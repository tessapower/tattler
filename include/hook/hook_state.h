#pragma once

#include "hook/capture_controller.h"
#include "hook/timestamp_manager.h"

/// <summary>
/// Global singletons shared across all hook files.
///
/// Hook wrappers: include this header to access the capture controller and
/// timestamp manager without needing to pass them around as parameters.
/// </summary>
namespace Tattler
{

/// The central capture controller that receives events and manages the pipe.
/// Hook wrappers: call g_captureController.IsCapturing() and SubmitEvent().
extern CaptureController g_captureController;

/// Manages the D3D12 timestamp query heap for GPU event timing.
/// Hook wrappers: call g_timestampManager.AllocatePair() and InsertTimestamp().
extern TimestampManager g_timestampManager;

/// Command queue used for timestamp frequency queries. Set by
/// ExecuteCommandLists hook, read by the Present hook via
/// g_timestampManager.GetFrequency(g_commandQueue).
extern ID3D12CommandQueue* g_commandQueue;

} // namespace Tattler
