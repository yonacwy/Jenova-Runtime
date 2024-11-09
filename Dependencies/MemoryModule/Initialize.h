#pragma once

NTSTATUS NTAPI InitializeMemoryModuleLoader();
NTSTATUS NTAPI ReleaseMemoryModuleLoader();

//
// This function is available only if the MMPP is compiled as a DLL.
//
BOOL WINAPI ReflectiveMapDll(HMODULE hModule);
