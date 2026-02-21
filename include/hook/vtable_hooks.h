#pragma once

/// <summary>
/// Utilities for patching COM interface vtables at runtime
/// </summary>
namespace VTableHooks
{

/// NOTE TO SELF:
// COM objects store a pointer to their vtable as the first field. Each entry
// in the vtable is a raw function pointer. By overwriting a slot we redirect
// every call to that method on every instance sharing the same vtable,
// which in practice means all instances of the same concrete class.
//
// VirtualProtect is required because vtables live in **read-only** memory.

/// <summary>
/// Returns the vtable of a COM object as an array of raw function pointers.
/// </summary>
/// <param name="pObject">Any COM object pointer</param>
inline void** GetVTable(void* pObject)
{
    // The first field of any COM object is a pointer to its vtable.
    // We cast to void*** (3x!) and dereference once to get the vtable array.
    return *reinterpret_cast<void***>(pObject);
}

/// <summary>
/// Replaces one vtable slot with a hook function and returns the original.
/// </summary>
/// <typeparam name="T">Function pointer type of the slot</typeparam>
/// <param name="vtable">Vtable array obtained from GetVTable()</param>
/// <param name="slot">Zero-based slot index (see vtable_slots.h)</param>
/// <param name="hookFn">Replacement function, which must match the original
/// signature</param>
/// <returns>The original function pointer that was in the slot.</returns>
template <typename T> T HookVTableEntry(void** vtable, int slot, void* hookFn)
{
    // Save the original function pointer at vtable[slot]
    void* origFn = vtable[slot];

    // Use VirtualProtect to change vtable[slot]'s memory to PAGE_READWRITE,
    // this lets us redirect the pointer to the hook function!
    DWORD oldProtect;
    VirtualProtect(&vtable[slot], sizeof(void*), PAGE_READWRITE, &oldProtect);

    // Write hookFn into vtable[slot]
    vtable[slot] = hookFn;

    // Restore the original memory protection using the saved old protection
    VirtualProtect(&vtable[slot], sizeof(void*), oldProtect, &oldProtect);

    // Return the saved original function pointer cast to T
    return T(origFn);
}

} // namespace VTableHooks
