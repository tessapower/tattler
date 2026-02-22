#include "stdafx.h"

#include "hook/capture_controller.h"
#include "hook/device_hooks.h"
#include "hook/hook_state.h"

namespace Tattler
{

/// <summary>
/// Background thread entry point. Runs off the loader lock so it is safe
/// to call Detours, create threads, and do blocking I/O here.
/// </summary>
static DWORD WINAPI HookThreadProc(LPVOID)
{
    // Attach Detours hooks on D3D12CreateDevice and CreateDXGIFactory2.
    // ⚠️ Must happen before the game creates any D3D12 objects!
    InstallInitialHooks();

    // This blocks until the pipe closes, it connects to the viewer,
    // then loops receiving StartCapture / StopCapture messages.
    g_captureController.Run();

    // Cleanly remove Detours hooks before the thread exits (good hygiene even
    // if the process is ending)
    UninstallInitialHooks();

    return 0;
}

} // namespace Tattler

/// <summary>
/// DLL entry point called by Windows when the DLL is loaded or unloaded.
/// The loader lock is held during DllMain, so creating threads or calling Detours directly here can cause deadlocks.
/// </summary>
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD reason, LPVOID)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        // Suppress per-thread DLL_THREAD_ATTACH / DLL_THREAD_DETACH calls,
        // we don't need them and they add unnecessary overhead
        DisableThreadLibraryCalls(hInstance);

        {
            HANDLE handle = CreateThread(nullptr, 0, Tattler::HookThreadProc,
                                         nullptr, 0, nullptr);
            if (!handle) return FALSE;
            CloseHandle(handle);
        }
        break;

    case DLL_PROCESS_DETACH:
        Tattler::g_timestampManager.Shutdown();
        break;
    }

    return TRUE;
}
