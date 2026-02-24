# Tattler

A lightweight, real-time GPU profiler that captures and visualizes DirectX 12
rendering workloads. It injects a hook DLL into target applications,
intercepts D3D12 API calls via vtable patching, and streams captured frame
data to a standalone viewer with timeline visualization and texture inspection.

<!-- TODO: Add screenshots of the viewer UI, timeline, and texture preview panels -->

<!-- TODO: Add example capture session showing GPU timeline and frame breakdown -->

---

## Build

### Prerequisites

- Windows 10/11 (64-bit)
- Visual Studio 2022 or later (with C++ Desktop Development workload)
- CMake 3.28+
- vcpkg for dependency management

### Dependencies (managed via vcpkg)

- [Microsoft Detours](https://github.com/microsoft/Detours) — API hooking
- [Dear ImGui](https://github.com/ocornut/imgui) — Viewer UI
- [Font Awesome](https://fontawesome.com/) — Icons (automatically fetched)

### Build Steps

1. Clone the repo:
   ```bash
   git clone https://github.com/tessapower/tattler.git
   cd tattler
   ```

2. Set up vcpkg integration (if not already configured):
   ```bash
   vcpkg integrate install
   ```

3. **Configure with CMake:**
   ```bash
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[path-to-vcpkg]/scripts/buildsystems/vcpkg.cmake
   ```

4. **Build:**
   ```bash
   cmake --build build --config Release
   ```

5. **Output:** Binaries will be in `build/Release/`:
   - `tattler.exe`: Standalone viewer application
   - `tattler_hook.dll`: Injection DLL (must stay alongside the viewer)

---

## Features

- **Non-Intrusive Injection**: Attach to any D3D12 application via DLL injection (no source code modification required)
- **Real-Time Capture**: Start/stop capture on demand from the viewer UI
- **GPU Timeline Visualization**: Per-event GPU timing with color-coded command types
- **Texture Inspection**: Readback and preview render targets, depth buffers, and intermediate textures
- **Frame Tree Navigation**: Hierarchical view of captured frames and events
- **Named Pipe IPC**: Low-latency streaming of capture data from hook to viewer
- **Automatic Memory Management**: First 100 frames cached in memory, subsequent frames spilled to disk to prevent OOM

### D3D12 Captured Events

- `DrawInstanced` / `DrawIndexedInstanced`
- `Dispatch`
- `CopyResource`
- `ResourceBarrier`
- `ClearRenderTargetView` / `ClearDepthStencilView`
- `Present`
- TODO: PSO creation, descriptor heap updates, etc.

---

## Directory Structure

```
tattler/
│
├── include/
│   ├── common/  # Shared types (capture events, pipe protocol, serialization)
│   ├── hook/    # Hook DLL headers (vtable patching, D3D12 interception)
│   └── viewer/  # Viewer app headers (UI, rendering, IPC client)
│
├── src/
│   ├── common/  # Shared implementation (serialization, pipe utilities)
│   ├── hook/    # Hook DLL implementation (dllmain, device/queue/cmdlist hooks)
│   └── viewer/  # Viewer app implementation (main, UI panels, process launcher)
│
├── resources/      # Fonts, icons, shaders (copied to build dir at compile time)
├── tests/          # Google Test unit tests
├── CMakeLists.txt  # Top-level build config
└── README.md       # This file :)
```

### Key Components

- `tattler.exe`: Standalone Dear ImGui viewer. Launches target apps, receives capture data via named pipe, renders timeline and texture previews.
- `tattler_hook.dll`: Injected DLL. Patches D3D12/DXGI vtables at runtime, intercepts API calls, timestamps GPU events, and streams data to viewer.
- `common/`: Protocol definitions (`MessageHeader`, `CapturedEvent`, `StagedTexture`) shared between hook and viewer.

---

## How It Works

### Injection Flow

```
┌──────────────────┐
│  tattler.exe     │
│  (Viewer App)    │
└────────┬─────────┘
         │ 1. User clicks "Launch Target"
         │    → Opens file picker dialog
         │
         ▼
┌────────────────────────────────────┐
│  ProcessLauncher::InjectAndLaunch  │
│ ────────────────────────────────── │
│  • CreateProcess(SUSPENDED)        │
│  • VirtualAllocEx + WriteMemory    │
│  • CreateRemoteThread(LoadLibraryW)│
│  • ResumeThread                    │
└────────┬───────────────────────────┘
         │ 2. target.exe loads tattler_hook.dll
         │    → DllMain called in target process
         ▼
┌─────────────────────────────┐
│  tattler_hook.dll           │
│ ─────────────────────────── │
│  DLL_PROCESS_ATTACH:        │
│  • Spawn background  thread │
│    (off loader lock)        │
│  • InstallInitialHooks()    │
│    ├─ Hook D3D12            │
│    │  CreateDevice          │
│    └─ Hook DXGI             │
│       CreateFactory2        │
└─────────────────────────────┘
```

### VTable Hooking Chain

```
Target App Creates D3D12 Device
        │
        ▼
┌─────────────────────────────────────┐
│  Hooked D3D12CreateDevice           │
│ ─────────────────────────────────── │
│  1. Call original D3D12CreateDevice │
│  2. GetVTable(device)               │
│  3. Hook vtable slots:              │
│     • CreateCommandQueue → 8        │
│     • CreateCommandList  → 12       │
└─────────┬───────────────────────────┘
          │
          ▼
┌───────────────────────────────────┐
│  Hooked CreateCommandQueue        │
│  ──────────────────────────────── │
│  1. Call original                 │
│  2. GetVTable(queue)              │
│  3. Hook ExecuteCommandLists → 10 │
└─────────┬─────────────────────────┘
          │
          ▼
┌───────────────────────────────────┐
│  Hooked CreateCommandList         │
│  ──────────────────────────────── │
│  1. Call original                 │
│  2. GetVTable(cmdList)            │
│  3. Hook drawing/dispatch slots:  │
│     • DrawInstanced         → 12  │
│     • DrawIndexedInstanced  → 13  │
│     • Dispatch              → 14  │
│     • CopyResource          → 17  │
│     • ResourceBarrier       → 26  │
│     • ClearDSV              → 47  │
│     • ClearRTV              → 48  │
└───────────────────────────────────┘
```

### 3. Capture & Data Flow

```
┌──────────────┐                       ┌──────────┐
│ tattler.exe  │◄──── Named Pipe ──────┤ hook.dll │
│ (Viewer)     │(Bi-directional IPC)   │          │
└──────┬───────┘                       └────┬─────┘
       │                                    │
       │ Send: StartCapture                 │
       ├───────────────────────────────────►│
       │                                    │ Capture Loop:
       │                                    │ ┌─────────────────┐
       │                                    │ │ Hook intercepts │
       │                                    │ │ Draw/Dispatch/  │
       │                                    │ │ Present calls   │
       │                                    │ └────────┬────────┘
       │                                    │          │
       │                                    │ ┌────────▼────────┐
       │                                    │ │ Insert GPU      │
       │                                    │ │ timestamp query │
       │                                    │ │ before & after  │
       │                                    │ └────────┬────────┘
       │                                    │          │
       │                                    │ ┌────────▼───────┐
       │                                    │ │ Store event in │
       │                                    │ │ CaptureBuffer  │
       │                                    │ └────────────────┘
       │                                    │
       │ Send: StopCapture                  │
       ├───────────────────────────────────►│
       │                                    │ ┌────────────────┐
       │                                    │ │ Resolve GPU    │
       │                                    │ │ timestamps     │
       │                                    │ └────────┬───────┘
       │                                    │          │
       │                                    │ ┌────────▼───────┐
       │                                    │ │ Readback       │
       │                                    │ │ textures       │
       │                                    │ └────────┬───────┘
       │                                    │          │
       │                                    │ ┌────────▼───────┐
       │ Receive: CaptureData               │ │ Serialize      │
       │◄───────────────────────────────────┤ │ Send to pipe   │
       │ (MessageHeader + serialized events)│ └────────────────┘
       │                                    │
┌──────▼────────┐
│ Parse events  │
│ Render UI:    │
│ • Timeline    │
│ • Frame tree  │
│ • Textures    │
└───────────────┘
```

### VTable Patching Internals

```cpp
void** vtable = GetVTable(pDevice); // vtable = *(void***)pDevice
void* originalFn = vtable[slot];   // Save original function pointer

DWORD oldProtect;
VirtualProtect(&vtable[slot], sizeof(void*), PAGE_READWRITE, &oldProtect);
vtable[slot] = (void*)MyHookFunction; // Redirect to hook
VirtualProtect(&vtable[slot], sizeof(void*), oldProtect, &oldProtect);

// All future calls to this method route through MyHookFunction,
// which can call originalFn to forward to the real implementation.
```

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## Attributions

- [Microsoft Detours](https://github.com/microsoft/Detours): Robust API hooking library
- [Dear ImGui](https://github.com/ocornut/imgui): for immediate-mode GUI framework
- [Font Awesome](https://fontawesome.com/): Icon font for UI elements

---

## Future Work

- PSO state inspection (shaders, blend modes, depth settings).
- Call stack capture for event attribution.
- Export captures to JSON/binary format for offline analysis.
- Frame playback and scrubbing in viewer.
