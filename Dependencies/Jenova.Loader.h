#pragma once

// Windows Module Loader
#ifdef TARGET_PLATFORM_WINDOWS

#include <Windows.h>

typedef HMODULE HMEMORYMODULE;

#define MemoryModuleToModule(_hMemoryModule_) (_hMemoryModule_)

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

#define LOAD_FLAGS_NOT_MAP_DLL						0x10000000
#define LOAD_FLAGS_NOT_FAIL_IF_HANDLE_TLS			0x20000000
#define LOAD_FLAGS_PASS_IMAGE_CHECK					0x40000000
#define LOAD_FLAGS_NOT_ADD_INVERTED_FUNCTION		0x00000001
#define LOAD_FLAGS_NOT_USE_REFERENCE_COUNT			0x00000002
#define LOAD_FLAGS_USE_DLL_NAME						0x00000004
#define LOAD_FLAGS_NOT_HANDLE_TLS					0x00000008
#define LOAD_FLAGS_HOOK_DOT_NET						0x00000010

HMEMORYMODULE WINAPI LoadLibraryMemory(_In_ PVOID BufferAddress);

HMEMORYMODULE WINAPI LoadLibraryMemoryExA(
	_In_ PVOID BufferAddress,
	_In_ size_t Reserved,
	_In_opt_ LPCSTR DllBaseName,
	_In_opt_ LPCSTR DllFullName,
	_In_ DWORD Flags
);

HMEMORYMODULE WINAPI LoadLibraryMemoryExW(
	_In_ PVOID BufferAddress,
	_In_ size_t Reserved,
	_In_opt_ LPCWSTR DllBaseName,
	_In_opt_ LPCWSTR DllFullName,
	_In_ DWORD Flags
);

BOOL WINAPI FreeLibraryMemory(_In_ HMEMORYMODULE hMemoryModule);

NTSTATUS NTAPI InitializeMemoryModuleLoader();
NTSTATUS NTAPI ReleaseMemoryModuleLoader();
VOID SetAgressiveReleaseMode(bool agrState);

#define NtLoadDllMemory						LdrLoadDllMemory
#define NtLoadDllMemoryExA					LdrLoadDllMemoryExA
#define NtLoadDllMemoryExW					LdrLoadDllMemoryExW
#define NtUnloadDllMemory					LdrUnloadDllMemory
#define NtUnloadDllMemoryAndExitThread		LdrUnloadDllMemoryAndExitThread
#define FreeLibraryMemoryAndExitThread		LdrUnloadDllMemoryAndExitThread
#define NtQuerySystemMemoryModuleFeatures	LdrQuerySystemMemoryModuleFeatures

#ifdef UNICODE
#define LdrLoadDllMemoryEx LdrLoadDllMemoryExW
#define LoadLibraryMemoryEx LoadLibraryMemoryExW
#else
#define LdrLoadDllMemoryEx LdrLoadDllMemoryExA
#define LoadLibraryMemoryEx LoadLibraryMemoryExA
#endif
#define NtLoadDllMemoryEx LdrLoadDllMemoryEx

// Loader Interface [Windows]
class JenovaLoader
{
public:
	static bool Initialize()
	{
		return NT_SUCCESS(InitializeMemoryModuleLoader());
	}
	static bool Release()
	{
		return NT_SUCCESS(ReleaseMemoryModuleLoader());
	}
	static bool SetAgressiveMode(bool agrState)
	{
		SetAgressiveReleaseMode(agrState);
		return true;
	}
	static jenova::ModuleHandle LoadModule(void* bufferPtr, size_t bufferSize, int flags = 0)
	{
		// Update Flags
		loaderFlags = flags;
		return LoadLibraryMemory(bufferPtr);
	}
	static jenova::ModuleHandle LoadModuleAsVirtual(void* bufferPtr, size_t bufferSize, const char* moduleName, const char* modulePath, int flags = 0)
	{
		// Update Flags
		loaderFlags = flags;

		// If Debug Mode Required Load From Disk
		if ((loaderFlags & jenova::LoaderFlag::LoadInDebugMode) != 0)
		{
			return LoadLibraryA(jenova::CreateTemporaryModuleCache((uint8_t*)bufferPtr, bufferSize).c_str());
		}
		return LoadLibraryMemoryExA(bufferPtr, bufferSize, moduleName, modulePath, LOAD_FLAGS_USE_DLL_NAME);
	}
	static jenova::ModuleAddress GetModuleBaseAddress(jenova::ModuleHandle moduleHandle)
	{
		return jenova::ModuleAddress(moduleHandle);
	}
	static void* GetVirtualFunction(jenova::ModuleHandle moduleHandle, const char* functionName)
	{
		return jenova::GetModuleFunction(moduleHandle, functionName);
	}
	static bool ReleaseModule(jenova::ModuleHandle moduleHandle)
	{
		return FreeLibraryMemory(HMEMORYMODULE(moduleHandle));
	}

private:
	static inline jenova::LoaderFlags loaderFlags = 0;
};

#endif

// Linux Module Loader
#ifdef TARGET_PLATFORM_LINUX

// Loader Interface [Linux]
class JenovaLoader
{
public:
	static bool Initialize()
	{
		return true;
	}
	static bool Release()
	{
		return true;
	}
	static bool SetAgressiveMode(bool agrState)
	{
		aggressiveMode = agrState;
		return true;
	}
	static jenova::ModuleHandle LoadModule(void* bufferPtr, size_t bufferSize, int flags = 0)
	{
		// Generate Memory-Mapped File Name
		pid_t pid = getpid();
		char moduleName[64];
		snprintf((char*)moduleName, sizeof(moduleName), "jenova_module_%d", pid);

		// Create Memory-Mapped File
		int fd = memfd_create(moduleName, MFD_CLOEXEC | MFD_ALLOW_SEALING);
		if (fd == -1)
		{
			perror("[Jenova Loader] memfd_create failed.");
			return nullptr;
		}

		// Resize Memory-Mapped File
		if (ftruncate(fd, bufferSize) == -1)
		{
			perror("[Jenova Loader] ftruncate failed.");
			close(fd);
			return nullptr;
		}

		// Write Module to Memory-Mapped File
		ssize_t written = write(fd, bufferPtr, bufferSize);
		if (written != static_cast<ssize_t>(bufferSize))
		{
			perror("[Jenova Loader] write failed.");
			close(fd);
			return nullptr;
		}

		// Seal Memory-Mapped File
		if (fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE) == -1)
		{
			perror("[Jenova Loader] fcntl failed.");
			close(fd);
			return nullptr;
		}

		// Construct File Descriptor Path for Memory-Mapped File
		char fd_path[64];
		snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", fd);

		// Load the Shared Object from Memory-Mapped File
		void* handle = dlopen(fd_path, RTLD_NOW | RTLD_GLOBAL | flags);
		if (!handle)
		{
			fprintf(stderr, "[Jenova Loader] dlopen failed: %s\n", dlerror());
			close(fd);
			return nullptr;
		}

		// Store Memory-Mapped File
		moduleFdMap[handle] = fd;

		// Return Loaded Module Handle
		return reinterpret_cast<jenova::ModuleHandle>(handle);
	}
	static jenova::ModuleHandle LoadModuleAsVirtual(void* bufferPtr, size_t bufferSize, const char* moduleName, const char* modulePath, int flags = 0)
	{
		return LoadModule(bufferPtr, bufferSize, flags);
	}
	static jenova::ModuleAddress GetModuleBaseAddress(jenova::ModuleHandle moduleHandle)
	{
		struct link_map* map;
		dlinfo(moduleHandle, RTLD_DI_LINKMAP, &map);
		return jenova::ModuleAddress(map->l_addr);
	}
	static void* GetVirtualFunction(jenova::ModuleHandle moduleHandle, const char* functionName)
	{
		return dlsym(moduleHandle, functionName);
	}
	static bool ReleaseModule(jenova::ModuleHandle moduleHandle)
	{
		// Validate
		if (!moduleHandle) return false;
		if (!aggressiveMode) return true;

		// Close Shared Object
		dlclose(moduleHandle);

		// Release Memory-Mapped File
		auto it = moduleFdMap.find(moduleHandle);
		if (it != moduleFdMap.end())
		{
			close(it->second);
			moduleFdMap.erase(it);
		}

		// All Good
		return true;
	}

private:
	static inline bool aggressiveMode = false;
	static inline std::unordered_map<jenova::ModuleHandle, int> moduleFdMap;
};

#endif