list(APPEND WTF_SOURCES
    android/LoggingAndroid.cpp
    android/RefPtrAndroid.cpp

    generic/MainThreadGeneric.cpp
    generic/MemoryFootprintGeneric.cpp
    generic/WorkQueueGeneric.cpp

    glib/Application.cpp
    glib/ChassisType.cpp
    glib/FileSystemGlib.cpp
    glib/GRefPtr.cpp
    glib/GResources.cpp
    glib/GSocketMonitor.cpp
    glib/GSpanExtras.cpp
    glib/RunLoopGLib.cpp
    glib/Sandbox.cpp
    glib/SocketConnection.cpp
    glib/URLGLib.cpp

    linux/CurrentProcessMemoryStatus.cpp
    linux/RealTimeThreads.cpp

    posix/CPUTimePOSIX.cpp
    posix/FileHandlePOSIX.cpp
    posix/FileSystemPOSIX.cpp
    posix/MappedFileDataPOSIX.cpp
    posix/OSAllocatorPOSIX.cpp
    posix/ThreadingPOSIX.cpp

    text/unix/TextBreakIteratorInternalICUUnix.cpp

    unix/LanguageUnix.cpp
    unix/LoggingUnix.cpp
    unix/MemoryPressureHandlerUnix.cpp
    unix/UniStdExtrasUnix.cpp
)

list(APPEND WTF_PUBLIC_HEADERS
    android/RefPtrAndroid.h

    glib/Application.h
    glib/ChassisType.h
    glib/GMutexLocker.h
    glib/GRefPtr.h
    glib/GResources.h
    glib/GSocketMonitor.h
    glib/GSpanExtras.h
    glib/GThreadSafeWeakPtr.h
    glib/GTypedefs.h
    glib/GUniquePtr.h
    glib/GWeakPtr.h
    glib/RunLoopSourcePriority.h
    glib/Sandbox.h
    glib/SocketConnection.h
    glib/SysprofAnnotator.h
    glib/WTFGType.h

    linux/CurrentProcessMemoryStatus.h
    linux/ProcessMemoryFootprint.h
    linux/RealTimeThreads.h

    posix/SocketPOSIX.h

    unix/UnixFileDescriptor.h
)

list(APPEND WTF_LIBRARIES
    GLib::Gio
    Threads::Threads
    ZLIB::ZLIB
)

list(APPEND WTF_PRIVATE_DEFINITIONS
    PKGDATADIR="${CMAKE_INSTALL_FULL_DATADIR}/wpe-webkit-${WPE_API_VERSION}"
)

if (ENABLE_JOURNALD_LOG)
    list(APPEND WTF_LIBRARIES Journald::Journald)
endif ()

if (ANDROID)
    list(APPEND WTF_LIBRARIES Android::Android Android::Log)
endif ()

if (USE_LIBBACKTRACE)
    list(APPEND WTF_LIBRARIES
        LIBBACKTRACE::LIBBACKTRACE
    )
endif ()

if (USE_SYSPROF_CAPTURE)
    list(APPEND WTF_LIBRARIES
        SysProfCapture::SysProfCapture
    )
endif ()
