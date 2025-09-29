/*
 * Copyright (C) 2020 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FlatpakLauncher.h"

#if OS(LINUX)

#include <gio/gio.h>
#include <wtf/FileSystem.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/Sandbox.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GTK/WPE port

namespace WebKit {

GRefPtr<GSubprocess> flatpakSpawn(GSubprocessLauncher* launcher, const WebKit::ProcessLauncher::LaunchOptions& launchOptions, char** argv, int childProcessSocket, GError** error)
{
    ASSERT(launcher);

    // When we are running inside of flatpak's sandbox we do not have permissions to use the same
    // bubblewrap sandbox we do outside but flatpak offers the ability to create new sandboxes
    // for us using flatpak-spawn.

    GUniquePtr<char> childProcessSocketArg(g_strdup_printf("--forward-fd=%d", childProcessSocket));
    Vector<CString> flatpakArgs = {
        "flatpak-spawn",
        childProcessSocketArg.get(),
        "--expose-pids",
        "--watch-bus"
    };

    if (launchOptions.processType == ProcessLauncher::ProcessType::Web) {
        flatpakArgs.appendVector(Vector<CString>({
            "--sandbox",
            "--no-network",
            "--sandbox-flag=share-gpu",
            "--sandbox-flag=share-display",
            "--sandbox-flag=share-sound",
            "--sandbox-flag=allow-a11y",
            "--sandbox-flag=allow-dbus", // Note that this only allows portals and $appid.Sandbox.* access
        }));

        // GST_DEBUG_FILE points to an absolute file path, so we need write permissions for its parent directory.
        if (const char* debugFilePath = g_getenv("GST_DEBUG_FILE")) {
            auto parentDir = FileSystem::parentPath(FileSystem::stringFromFileSystemRepresentation(debugFilePath));
            GUniquePtr<gchar> pathArg(g_strdup_printf("--sandbox-expose-path=%s", parentDir.utf8().data()));
            flatpakArgs.append(pathArg.get());
        }

        // GST_DEBUG_DUMP_DOT_DIR might not exist when the application starts, so we need write
        // permissions for its parent directory.
        if (const char* dotDir = g_getenv("GST_DEBUG_DUMP_DOT_DIR")) {
            auto parentDir = FileSystem::parentPath(FileSystem::stringFromFileSystemRepresentation(dotDir));
            GUniquePtr<gchar> pathArg(g_strdup_printf("--sandbox-expose-path=%s", parentDir.utf8().data()));
            flatpakArgs.append(pathArg.get());
        }

        for (const auto& pathAndPermission : launchOptions.extraSandboxPaths) {
            const char* formatString = pathAndPermission.value == SandboxPermission::ReadOnly ? "--sandbox-expose-path-ro=%s": "--sandbox-expose-path=%s";
            GUniquePtr<gchar> pathArg(g_strdup_printf(formatString, pathAndPermission.key.data()));
            flatpakArgs.append(pathArg.get());
        }

#if USE(ATSPI)
        RELEASE_ASSERT(isInsideFlatpak());
        if (checkFlatpakPortalVersion(7)) {
            auto busName = launchOptions.extraInitializationData.get<HashTranslatorASCIILiteral>("accessibilityBusName"_s);
            GUniquePtr<gchar> a11yOwnNameArg(g_strdup_printf("--sandbox-a11y-own-name=%s", busName.utf8().data()));
            flatpakArgs.append(a11yOwnNameArg.get());
        }
#endif
    }

    // We need to pass our full environment to the subprocess.
    GUniquePtr<char*> environ(g_get_environ());
    for (char** variable = environ.get(); variable && *variable; variable++) {
        GUniquePtr<char> arg(g_strconcat("--env=", *variable, nullptr));
        flatpakArgs.append(arg.get());
    }

    char** newArgv = g_newa(char*, g_strv_length(argv) + flatpakArgs.size() + 1);
    size_t i = 0;

    for (const auto& arg : flatpakArgs)
        newArgv[i++] = const_cast<char*>(arg.data());
    for (size_t x = 0; argv[x]; x++)
        newArgv[i++] = argv[x];
    newArgv[i++] = nullptr;

    return adoptGRef(g_subprocess_launcher_spawnv(launcher, newArgv, error));
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

};

#endif // OS(LINUX)
