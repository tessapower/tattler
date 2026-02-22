#pragma once

#include <Windows.h>

#include <string>

namespace Tattler
{

/// <summary>
/// Launches a target application with our hook injected via Detours. Handles
/// the file picker and injection in one shot.
/// </summary>
class ProcessLauncher
{
  public:
    /// <summary>
    /// Shows a file picker dialog, then launches the selected executable with
    /// the hook injected.
    /// </summary>
    /// <returns>true if the process was started successfully, false if the
    /// user cancelled or launch failed.</returns>
    static auto Launch(HWND parentHwnd) -> bool;

  private:
    /// <summary>
    /// Opens a Win32 "Open File" dialog filtered to .exe files.
    /// </summary>
    /// <returns>The selected path or an empty string if cancelled.</returns>
    static auto PickExecutable(HWND parentHwnd) -> std::wstring;

    /// <summary>
    /// Returns the full path to the hook dll, which lives in the same
    /// directory as the viewer executable.
    /// </summary>
    static auto GetHookDllPath() -> std::wstring;

    /// <summary>
    /// Creates the target process with the hook DLL injected before the
    /// entry point runs.
    /// </summary>
    /// <returns>true if the process was created successfully.</returns>
    static auto InjectAndLaunch(const std::wstring& exePath,
                                const std::wstring& dllPath) -> bool;
};

} // namespace Tattler
