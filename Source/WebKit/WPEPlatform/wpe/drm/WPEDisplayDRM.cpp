/*
 * Copyright (C) 2023 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEDisplayDRM.h"

#include "DRMUniquePtr.h"
#include "GRefPtrWPE.h"
#include "RefPtrUdev.h"
#include "WPEDRMDevice.h"
#include "WPEDRMSession.h"
#include "WPEDisplayDRMPrivate.h"
#include "WPEExtensions.h"
#include "WPEScreenDRMPrivate.h"
#include "WPESettings.h"
#include "WPEToplevelDRM.h"
#include "WPEViewDRM.h"
#include <fcntl.h>
#include <gio/gio.h>
#include <libudev.h>
#include <wtf/ASCIICType.h>
#include <wtf/Scope.h>
#include <wtf/dtoa.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringView.h>
#include <wtf/unix/UnixFileDescriptor.h>

/**
 * WPEDisplayDRM:
 *
 */
struct _WPEDisplayDRMPrivate {
    std::unique_ptr<WPE::DRM::Session> session;
    GRefPtr<WPEDRMDevice> displayDevice;
    GRefPtr<WPEDRMDevice> renderDevice;
    UnixFileDescriptor fd;
    struct gbm_device* device;
    bool atomicSupported;
    bool modifiersSupported;
    uint32_t cursorWidth;
    uint32_t cursorHeight;
    std::unique_ptr<WPE::DRM::Connector> connector;
    GRefPtr<WPEScreen> screen;
    std::unique_ptr<WPE::DRM::Plane> primaryPlane;
    std::unique_ptr<WPE::DRM::Cursor> cursor;
    std::unique_ptr<WPE::DRM::Seat> seat;
};
WEBKIT_DEFINE_FINAL_TYPE_WITH_CODE(WPEDisplayDRM, wpe_display_drm, WPE_TYPE_DISPLAY, WPEDisplay,
    wpeEnsureExtensionPointsRegistered();
    g_io_extension_point_implement(WPE_DISPLAY_EXTENSION_POINT_NAME, g_define_type_id, "wpe-display-drm", -100))

static void wpeDisplayDRMConstructed(GObject* object)
{
    G_OBJECT_CLASS(wpe_display_drm_parent_class)->constructed(object);

    auto* settings = wpe_display_get_settings(WPE_DISPLAY(object));
    RELEASE_ASSERT(wpe_settings_register(settings, WPE_SETTING_DRM_SCALE, G_VARIANT_TYPE_DOUBLE, g_variant_new_double(1.0), nullptr));
}

static void wpeDisplayDRMDispose(GObject* object)
{
    auto* priv = WPE_DISPLAY_DRM(object)->priv;

    if (priv->screen) {
        wpe_screen_invalidate(priv->screen.get());
        priv->screen = nullptr;
    }

    priv->cursor = nullptr;
    g_clear_pointer(&priv->device, gbm_device_destroy);

    if (priv->fd) {
        drmDropMaster(priv->fd.value());
        priv->fd = { };
    }

    G_OBJECT_CLASS(wpe_display_drm_parent_class)->dispose(object);
}

struct DisplayDevice {
    bool isNull() const
    {
        return !fd && !drmDevice;
    }

    UnixFileDescriptor fd;
    GRefPtr<WPEDRMDevice> drmDevice;
};

static GRefPtr<WPEDRMDevice> createDisplayDevice(const UnixFileDescriptor& fd, const char* filename)
{
    std::unique_ptr<char, decltype(free)*> renderNodePath(drmGetRenderDeviceNameFromFd(fd.value()), free);
    return adoptGRef(wpe_drm_device_new(filename, renderNodePath.get()));
}

// This is based on weston function find_primary_gpu(). It tries to find the boot VGA device that is KMS capable, or the first KMS device.
static struct DisplayDevice findTargetDevice(struct udev* udev, const char* seatID)
{
    RefPtr<struct udev_enumerate> udevEnumerate = adoptRef(udev_enumerate_new(udev));
    udev_enumerate_add_match_subsystem(udevEnumerate.get(), "drm");
    udev_enumerate_add_match_sysname(udevEnumerate.get(), "card[0-9]*");
    udev_enumerate_scan_devices(udevEnumerate.get());

    struct DisplayDevice displayDevice;
    struct udev_list_entry* entry;
    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(udevEnumerate.get())) {
        RefPtr<struct udev_device> udevDevice = adoptRef(udev_device_new_from_syspath(udev, udev_list_entry_get_name(entry)));
        if (!udevDevice)
            continue;

        const char* deviceSeatID = udev_device_get_property_value(udevDevice.get(), "ID_SEAT");
        if (!deviceSeatID)
            deviceSeatID = "seat0";
        if (g_strcmp0(deviceSeatID, seatID))
            continue;

        bool isBootVGA = false;
        if (auto* pciDevice = udev_device_get_parent_with_subsystem_devtype(udevDevice.get(), "pci", nullptr)) {
            const char* id = udev_device_get_sysattr_value(pciDevice, "boot_vga");
            isBootVGA = id && !strcmp(id, "1");
        }

        if (!isBootVGA && !displayDevice.isNull())
            continue;

        const char* filename = udev_device_get_devnode(udevDevice.get());
        auto fd = UnixFileDescriptor { open(filename, O_RDWR | O_CLOEXEC), UnixFileDescriptor::Adopt };
        if (!fd)
            continue;

        WPE::DRM::UniquePtr<drmModeRes> resources(drmModeGetResources(fd.value()));
        if (!resources || !resources->count_crtcs || !resources->count_connectors || !resources->count_encoders)
            continue;

        auto drmDevice = createDisplayDevice(fd, filename);
        displayDevice = { WTFMove(fd), WTFMove(drmDevice) };
        if (isBootVGA)
            return displayDevice;
    }

    return displayDevice;
}

static GRefPtr<WPEDRMDevice> findFirstDeviceWithRenderNode()
{
    std::array<drmDevicePtr, 64> devices = { };

    int numDevices = drmGetDevices2(0, devices.data(), std::size(devices));
    if (numDevices <= 0)
        return { };

    GRefPtr<WPEDRMDevice> drmDevice;
    for (int i = 0; i < numDevices; ++i) {
        const auto* device = devices[i];
        const auto nodes = unsafeMakeSpan(device->nodes, DRM_NODE_MAX);

        bool hasPrimaryNode = device->available_nodes & (1 << DRM_NODE_PRIMARY);
        if (!hasPrimaryNode)
            continue;

        bool hasRenderNode = device->available_nodes & (1 << DRM_NODE_RENDER);
        if (!hasRenderNode)
            continue;

        drmDevice = adoptGRef(wpe_drm_device_new(nodes[DRM_NODE_PRIMARY], nodes[DRM_NODE_RENDER]));
        break;
    }

    drmFreeDevices(devices.data(), numDevices);
    return drmDevice;
}

static bool wpeDisplayDRMInitializeCapabilities(WPEDisplayDRM* display, int fd, GError** error)
{
    if (drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "DRM universal planes not supported");
        return false;
    }

    if (!getenv("WPE_DRM_DISABLE_ATOMIC"))
        display->priv->atomicSupported = !drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);

    uint64_t capability;
    display->priv->modifiersSupported = !drmGetCap(fd, DRM_CAP_ADDFB2_MODIFIERS, &capability) && capability;

    if (!drmGetCap(fd, DRM_CAP_CURSOR_WIDTH, &capability))
        display->priv->cursorWidth = capability;
    else
        display->priv->cursorWidth = 64;

    if (!drmGetCap(fd, DRM_CAP_CURSOR_HEIGHT, &capability))
        display->priv->cursorHeight = capability;
    else
        display->priv->cursorHeight = 64;

    return true;
}

static std::unique_ptr<WPE::DRM::Connector> chooseConnector(int fd, drmModeRes* resources, GError** error)
{
    for (int i = 0; i < resources->count_connectors; ++i) {
        WPE::DRM::UniquePtr<drmModeConnector> connector(drmModeGetConnector(fd, resources->connectors[i]));
        if (!connector || connector->connection != DRM_MODE_CONNECTED || connector->connector_type == DRM_MODE_CONNECTOR_WRITEBACK)
            continue;

        if (auto conn = WPE::DRM::Connector::create(fd, connector.get()))
            return conn;
    }

    g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to find a DRM connector");
    return nullptr;
}

static std::unique_ptr<WPE::DRM::Crtc> chooseCrtcForConnector(int fd, drmModeRes* resources, const WPE::DRM::Connector& connector, GError** error)
{
    // Try the currently connected encoder+crtc.
    WPE::DRM::UniquePtr<drmModeEncoder> encoder(drmModeGetEncoder(fd, connector.encoderID()));
    if (encoder) {
        for (int i = 0; i < resources->count_crtcs; ++i) {
            if (resources->crtcs[i] == encoder->crtc_id) {
                WPE::DRM::UniquePtr<drmModeCrtc> drmCrtc(drmModeGetCrtc(fd, resources->crtcs[i]));
                return WPE::DRM::Crtc::create(fd, drmCrtc.get(), i);
            }
        }
    }

    // If no active crtc was found, pick the first possible crtc, then try the first possible for crtc.
    for (int i = 0; i < resources->count_encoders; ++i) {
        WPE::DRM::UniquePtr<drmModeEncoder> encoder(drmModeGetEncoder(fd, resources->encoders[i]));
        if (!encoder)
            continue;

        for (int j = 0; j < resources->count_crtcs; ++j) {
            const uint32_t crtcMask = 1 << j;
            if (encoder->possible_crtcs & crtcMask) {
                WPE::DRM::UniquePtr<drmModeCrtc> drmCrtc(drmModeGetCrtc(fd, resources->crtcs[j]));
                return WPE::DRM::Crtc::create(fd, drmCrtc.get(), j);
            }
        }
    }

    g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to find a DRM crtc");
    return nullptr;
}

static std::unique_ptr<WPE::DRM::Plane> choosePlaneForCrtc(int fd, WPE::DRM::Plane::Type type, const WPE::DRM::Crtc& crtc, bool modifiersSupported, GError** error)
{
    WPE::DRM::UniquePtr<drmModePlaneRes> planeResources(drmModeGetPlaneResources(fd));
    if (!planeResources) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "No DRM plane resources found");
        return nullptr;
    }

    for (uint32_t i = 0; i < planeResources->count_planes; ++i) {
        WPE::DRM::UniquePtr<drmModePlane> drmPlane(drmModeGetPlane(fd, planeResources->planes[i]));
        auto plane = WPE::DRM::Plane::create(fd, type, drmPlane.get(), modifiersSupported);
        if (!plane)
            continue;

        if (plane->canBeUsedWithCrtc(crtc))
            return plane;
    }

    g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "No DRM planes found");
    return nullptr;
}

static gboolean wpeDisplayDRMSetup(WPEDisplayDRM* displayDRM, const char* deviceName, GError** error)
{
    RefPtr<struct udev> udev = adoptRef(udev_new());
    if (!udev) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to create udev context");
        return FALSE;
    }

    auto session = WPE::DRM::Session::create();
    DisplayDevice displayDevice;
    if (!deviceName) {
        displayDevice = findTargetDevice(udev.get(), session->seatID());
        if (displayDevice.isNull()) {
            g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "No suitable DRM device found");
            return FALSE;
        }
    } else {
        int fd = open(deviceName, O_RDWR | O_CLOEXEC);
        if (fd < 0) {
            g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to open DRM device");
            return FALSE;
        }
        WTF::UnixFileDescriptor unixFd(fd, WTF::UnixFileDescriptor::Adopt);
        auto drmDevice = createDisplayDevice(unixFd, deviceName);
        displayDevice = { WTFMove(unixFd), WTFMove(drmDevice) };
    }
    auto fd = WTFMove(displayDevice.fd);
    if (drmSetMaster(fd.value()) == -1) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to become DRM master");
        return FALSE;
    }

    if (!wpeDisplayDRMInitializeCapabilities(displayDRM, fd.value(), error))
        return FALSE;

    WPE::DRM::UniquePtr<drmModeRes> resources(drmModeGetResources(fd.value()));
    if (!resources) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to get device resources");
        return FALSE;
    }

    auto connector = chooseConnector(fd.value(), resources.get(), error);
    if (!connector)
        return FALSE;

    auto crtc = chooseCrtcForConnector(fd.value(), resources.get(), *connector, error);
    if (!crtc)
        return FALSE;

    auto primaryPlane = choosePlaneForCrtc(fd.value(), WPE::DRM::Plane::Type::Primary, *crtc, displayDRM->priv->modifiersSupported, error);
    if (!primaryPlane)
        return FALSE;

    auto cursorPlane = choosePlaneForCrtc(fd.value(), WPE::DRM::Plane::Type::Cursor, *crtc, displayDRM->priv->modifiersSupported, nullptr);

    auto* device = gbm_create_device(fd.value());
    if (!device) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to create GBM device");
        return FALSE;
    }

    auto seat = WPE::DRM::Seat::create(udev.get(), *session);
    if (!seat) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Failed to initialize input");
        return FALSE;
    }

    displayDRM->priv->session = WTFMove(session);
    displayDRM->priv->fd = WTFMove(fd);
    displayDRM->priv->displayDevice = WTFMove(displayDevice.drmDevice);
    if (!wpe_drm_device_get_render_node(displayDRM->priv->displayDevice.get()))
        displayDRM->priv->renderDevice = findFirstDeviceWithRenderNode();
    displayDRM->priv->device = device;
    displayDRM->priv->connector = WTFMove(connector);

    static const auto scaleIsInBounds = [](double scale) {
        return (scale >= 0.05) && (scale <= 20.0);
    };

    std::optional<double> scaleFromEnvironment;
    if (const auto scaleString = StringView::fromLatin1(getenv("WPE_DRM_SCALE"))) {
        RELEASE_ASSERT(scaleString.is8Bit());
        auto trimmedScaleString = scaleString.trim(isASCIIWhitespace<LChar>);
        size_t parsedLength = 0;
        auto scale = parseDouble(trimmedScaleString, parsedLength);
        if (parsedLength == trimmedScaleString.length() && scaleIsInBounds(scale))
            scaleFromEnvironment = scale;
        else
            g_warning("Invalid WPE_DRM_SCALE='%*s' value, or out of bounds.", static_cast<int>(scaleString.span8().size()), scaleString.span8().data());
    }

    int x = crtc->x();
    int y = crtc->y();
    int width = crtc->width();
    int height = crtc->height();
    displayDRM->priv->screen = wpeScreenDRMCreate(WTFMove(crtc), *displayDRM->priv->connector);
    if (!width || !height) {
        auto* mode = wpeScreenDRMGetMode(WPE_SCREEN_DRM(displayDRM->priv->screen.get()));
        width = mode->hdisplay;
        height = mode->vdisplay;
    }

    double scale = scaleFromEnvironment.value_or(wpeScreenDRMGuessScale(WPE_SCREEN_DRM(displayDRM->priv->screen.get())));
    RELEASE_ASSERT(wpe_settings_set_double(wpe_display_get_settings(WPE_DISPLAY(displayDRM)), WPE_SETTING_DRM_SCALE, scale, WPE_SETTINGS_SOURCE_PLATFORM, nullptr));

    GUniqueOutPtr<GError> settingsError;
    double scaleFromSettings = wpe_settings_get_double(wpe_display_get_settings(WPE_DISPLAY(displayDRM)), WPE_SETTING_DRM_SCALE, &settingsError.outPtr());
    RELEASE_ASSERT(!settingsError);
    if (scaleIsInBounds(scaleFromSettings))
        scale = scaleFromSettings;
    else
        g_warning("Ignoring out of bounds WPE_SETTING_DRM_SCALE value '%.2f'.", scaleFromSettings);
    RELEASE_ASSERT(scaleIsInBounds(scale));

    wpe_screen_set_position(displayDRM->priv->screen.get(), x / scale, y / scale);
    wpe_screen_set_size(displayDRM->priv->screen.get(), width / scale, height / scale);
    wpe_screen_set_scale(displayDRM->priv->screen.get(), scale);

    displayDRM->priv->primaryPlane = WTFMove(primaryPlane);
    displayDRM->priv->seat = WTFMove(seat);
    wpe_display_set_available_input_devices(WPE_DISPLAY(displayDRM), displayDRM->priv->seat->availableInputDevices());
    displayDRM->priv->seat->setAvailableInputDevicesChangedCallback([weakDisplay = GWeakPtr { displayDRM }](WPEAvailableInputDevices devices) {
        if (!weakDisplay)
            return;

        wpe_display_set_available_input_devices(WPE_DISPLAY(weakDisplay.get()), devices);
    });
    if (cursorPlane)
        displayDRM->priv->cursor = makeUnique<WPE::DRM::Cursor>(WTFMove(cursorPlane), device, displayDRM->priv->cursorWidth, displayDRM->priv->cursorHeight);

    return TRUE;
}

static gboolean wpeDisplayDRMConnect(WPEDisplay* display, GError** error)
{
    return wpeDisplayDRMSetup(WPE_DISPLAY_DRM(display), nullptr, error);
}

static WPEView* wpeDisplayDRMCreateView(WPEDisplay* display)
{
    auto* displayDRM = WPE_DISPLAY_DRM(display);
    auto* view = WPE_VIEW(g_object_new(WPE_TYPE_VIEW_DRM, "display", display, nullptr));

    if (wpe_settings_get_boolean(wpe_display_get_settings(display), WPE_SETTING_CREATE_VIEWS_WITH_A_TOPLEVEL, nullptr)) {
        GRefPtr<WPEToplevel> toplevel = adoptGRef(wpe_toplevel_drm_new(displayDRM));
        wpe_view_set_toplevel(view, toplevel.get());
    }

    displayDRM->priv->seat->setView(view);
    return view;
}

static WPEBufferDMABufFormats* wpeDisplayDRMGetPreferredDMABufFormats(WPEDisplay* display)
{
    auto* displayDRM = WPE_DISPLAY_DRM(display);
    auto* builder = wpe_buffer_dma_buf_formats_builder_new(displayDRM->priv->displayDevice.get());
    wpe_buffer_dma_buf_formats_builder_append_group(builder, nullptr, WPE_BUFFER_DMA_BUF_FORMAT_USAGE_SCANOUT);
    for (const auto& format : displayDRM->priv->primaryPlane->formats()) {
        for (auto modifier : format.modifiers)
            wpe_buffer_dma_buf_formats_builder_append_format(builder, format.format, modifier);
    }

    return wpe_buffer_dma_buf_formats_builder_end(builder);
}

static guint wpeDisplayDRMGetNScreens(WPEDisplay*)
{
    return 1;
}

static WPEScreen* wpeDisplayDRMGetScreen(WPEDisplay* display, guint index)
{
    if (index)
        return nullptr;
    return WPE_DISPLAY_DRM(display)->priv->screen.get();
}

static WPEDRMDevice* wpeDisplayDRMGetDRMDevice(WPEDisplay* display)
{
    auto* priv = WPE_DISPLAY_DRM(display)->priv;
    return priv->renderDevice ? priv->renderDevice.get() : priv->displayDevice.get();
}

static gboolean wpeDisplayDRMUseExplicitSync(WPEDisplay* display)
{
    return WPE_DISPLAY_DRM(display)->priv->atomicSupported;
}

static void wpe_display_drm_class_init(WPEDisplayDRMClass* displayDRMClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(displayDRMClass);
    objectClass->constructed = wpeDisplayDRMConstructed;
    objectClass->dispose = wpeDisplayDRMDispose;

    WPEDisplayClass* displayClass = WPE_DISPLAY_CLASS(displayDRMClass);
    displayClass->connect = wpeDisplayDRMConnect;
    displayClass->create_view = wpeDisplayDRMCreateView;
    displayClass->get_preferred_dma_buf_formats = wpeDisplayDRMGetPreferredDMABufFormats;
    displayClass->get_n_screens = wpeDisplayDRMGetNScreens;
    displayClass->get_screen = wpeDisplayDRMGetScreen;
    displayClass->get_drm_device = wpeDisplayDRMGetDRMDevice;
    displayClass->use_explicit_sync = wpeDisplayDRMUseExplicitSync;
}

const WPE::DRM::Connector& wpeDisplayDRMGetConnector(WPEDisplayDRM* display)
{
    return *display->priv->connector;
}

WPEDRMDevice* wpeDisplayDRMGetDisplayDevice(WPEDisplayDRM* display)
{
    return display->priv->displayDevice.get();
}

WPEScreen* wpeDisplayDRMGetScreen(WPEDisplayDRM* display)
{
    return display->priv->screen.get();
}

const WPE::DRM::Plane& wpeDisplayDRMGetPrimaryPlane(WPEDisplayDRM* display)
{
    return *display->priv->primaryPlane;
}

WPE::DRM::Cursor* wpeDisplayDRMGetCursor(WPEDisplayDRM* display)
{
    return display->priv->cursor.get();
}

const WPE::DRM::Seat& wpeDisplayDRMGetSeat(WPEDisplayDRM* display)
{
    return *display->priv->seat;
}

/**
 * wpe_display_drm_new:
 *
 * Create a new #WPEDisplayDRM
 *
 * Returns: (transfer full): a #WPEDisplay
 */
WPEDisplay* wpe_display_drm_new(void)
{
    return WPE_DISPLAY(g_object_new(WPE_TYPE_DISPLAY_DRM, nullptr));
}

/**
 * wpe_display_drm_connect:
 * @display: a #WPEDisplayDRM
 * @name: (nullable): the name of the DRM device to connect to, or %NULL
 * @error: return location for error or %NULL to ignore
 *
 * Connect to the DRM device named @name. If @name is %NULL it
 * connects to the default device.
 *
 * Returns: %TRUE if connection succeeded, or %FALSE in case of error.
 */
gboolean wpe_display_drm_connect(WPEDisplayDRM* display, const char* name, GError** error)
{
    g_return_val_if_fail(WPE_IS_DISPLAY_DRM(display), FALSE);
    return wpeDisplayDRMSetup(display, name, error);
}

/**
 * wpe_display_drm_get_device: (skip)
 * @display: a #WPEDisplayDRM
 *
 * Get the GBM device for @display
 *
 * Returns: (transfer none) (nullable): a `struct gbm_device`,
     or %NULL if display is not connected
 */
struct gbm_device* wpe_display_drm_get_device(WPEDisplayDRM* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY_DRM(display), nullptr);

    return display->priv->device;
}

/**
 * wpe_display_drm_supports_atomic:
 * @display: a #WPEDisplayDRM
 *
 * Get whether the DRM driver supports atomic mode-setting
 *
 * Returns: %TRUE if atomic mode-setting is supported, or %FALSE otherwise
 */
gboolean wpe_display_drm_supports_atomic(WPEDisplayDRM* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY_DRM(display), FALSE);

    return display->priv->atomicSupported;
}

/**
 * wpe_display_drm_supports_modifiers:
 * @display: a #WPEDisplayDRM
 *
 * Get whether the DRM driver supports format modifiers
 *
 * Returns: %TRUE if format modifiers are supported, or %FALSE otherwise
 */
gboolean wpe_display_drm_supports_modifiers(WPEDisplayDRM* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY_DRM(display), FALSE);

    return display->priv->modifiersSupported;
}
