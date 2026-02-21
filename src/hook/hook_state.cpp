#include "stdafx.h"

#include "hook/hook_state.h"

namespace Tattler
{

// Define the global singletons declared in hook_state.h.
// All hook files reference these via the extern declarations.
CaptureController  g_captureController;
TimestampManager   g_timestampManager;
ID3D12CommandQueue* g_commandQueue = nullptr;

} // namespace Tattler
