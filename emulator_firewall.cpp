
/*-------------------------------------------------------------+
|                                                              |
|                   _________   ______ _    _____              |
|                  / / ____/ | / / __ \ |  / /   |             |
|             __  / / __/ /  |/ / / / / | / / /| |             |
|            / /_/ / /___/ /|  / /_/ /| |/ / ___ |             |
|            \____/_____/_/ |_/\____/ |___/_/  |_|             |
|                                                              |
|                        Jenova Runtime                        |
|                   Developed by Hamid.Memar                   |
|                                                              |
+-------------------------------------------------------------*/

// Network SDK
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

// Jenova SDK
#include "Jenova.hpp"

// Mini Hook SDK
#include <MinHook/MinHook.h>

// Log Macros
#define net_log(fmt,...) printf("jenova::Firewall > " fmt "\n",__VA_ARGS__);

// Helper Macros
#define ValidateHookModules(modules) if (!ValidateModules(modules, _ReturnAddress()))

// Config Macros
#define NETWORK_HOOK_ALLOWED_MODULES { "Jenova.Emu.x64.dll" }

// Helpers
static std::string GetModuleName(const char* moduleName)
{
    const char* dllName = strrchr(moduleName, '\\');
    if (dllName)
    {
        return dllName + 1;
    }
    return std::string(dllName);
}

// Jenova Emulator Firewall Original Function Pointers
static int (WINAPI* originalSend)(SOCKET s, const char* buf, int len, int flags) = nullptr;
static int (WINAPI* originalRecv)(SOCKET s, char* buf, int len, int flags) = nullptr;
static int (WINAPI* originalWSASend)(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, LPDWORD lpTotalBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) = nullptr;
static int (WINAPI* originalWSARecv)(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpTotalBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) = nullptr;
static int (WINAPI* originalSendTo)(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen) = nullptr;
static int (WINAPI* originalRecvFrom)(SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen) = nullptr;
static int (WINAPI* originalWSASendTo)(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, LPDWORD lpTotalBytesSent, DWORD dwFlags, const struct sockaddr* to, int tolen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) = nullptr;
static int (WINAPI* originalWSARecvFrom)(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpTotalBytesRecvd, LPDWORD lpFlags, struct sockaddr* from, int* fromlen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) = nullptr;

// Singleton Instance
JenovaFirewall* singleton = nullptr;

// Initializer/Deinitializer
void JenovaFirewall::init()
{
    // Register Class
    ClassDB::register_class<JenovaFirewall>();

    // Initialize Singleton
    singleton = memnew(JenovaFirewall);

    // Initialize Default Network Rules
    singleton->AddDefaultNetworkRules();

    // Verbose
    jenova::Output("Jenova Firewall Initialized.");
}
void JenovaFirewall::deinit()
{
    // Release Singleton
    if (singleton) memdelete(singleton);
}

// Bindings
void JenovaFirewall::_bind_methods()
{
    ClassDB::bind_static_method("JenovaFirewall", D_METHOD("GetInstance"), &JenovaFirewall::get_singleton);
    ClassDB::bind_method(D_METHOD("ActivateFirewall"), &JenovaFirewall::ActivateFirewall);
    ClassDB::bind_method(D_METHOD("DeactivateFirewall"), &JenovaFirewall::DeactivateFirewall);
}

// Singleton Handling
JenovaFirewall* JenovaFirewall::get_singleton()
{
    return singleton;
}

// Jenova Emulator Firewall Storage
static std::vector<std::string> networkWhitelistedAddresses;
static std::vector<std::string> networkBlacklistedAddresses;

// Jenova Emulator Firewall Module Awareness
static std::string GetCallerModule(PVOID callerAddress)
{
    HMODULE hModule = NULL;

    // Get Module Handle by the Address inside its mapped range
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)callerAddress, &hModule);

    if (hModule)
    {
        char moduleName[MAX_PATH];
        GetModuleFileNameA(hModule, moduleName, MAX_PATH);
        return GetModuleName(moduleName);
    }

    // Module Undefiend
    return "Unknown";
}
static bool ValidateModules(std::initializer_list<std::string> allowedModules, PVOID callerAddress)
{
    // Get the module name of the caller
    std::string callerModule = GetCallerModule(callerAddress);

    // Iterate over the allowed modules
    for (const auto& module : allowedModules)
    {
        // If the caller module matches any of the allowed ones, return true
        if (callerModule == module)
        {
            return true;
        }
    }

    // No match found, return false
    return false;
}

// Jenova Emulator Firewall Network Filtering
static bool MatchesIpWildcard(const std::string& pattern, const std::string& value)
{
    // If pattern is '*', it matches everything (entire IP)
    if (pattern == "*" || pattern == "x.x.x.x")
    {
        return true;
    }

    // Split the pattern and value into octets
    std::vector<std::string> patternParts;
    std::vector<std::string> valueParts;

    std::stringstream patternStream(pattern);
    std::stringstream valueStream(value);

    std::string segment;
    while (std::getline(patternStream, segment, '.'))
    {
        patternParts.push_back(segment);
    }

    while (std::getline(valueStream, segment, '.'))
    {
        valueParts.push_back(segment);
    }

    // Ensure both the pattern and value have exactly 4 parts (IPv4 address)
    if (patternParts.size() != 4 || valueParts.size() != 4)
    {
        return false;
    }

    // Compare each octet
    for (int i = 0; i < 4; ++i)
    {
        // Allow 'x' as a wildcard for any octet
        if (patternParts[i] != "x" && patternParts[i] != valueParts[i])
        {
            return false;
        }
    }

    return true;
}
static bool MatchesWildcard(const std::string& pattern, const std::string& value)
{
    // If pattern is '*', it matches everything
    if (pattern == "*")
    {
        return true;
    }

    size_t patLen = pattern.length();
    size_t valLen = value.length();
    size_t patPos = 0, valPos = 0, starIdx = -1, matchIdx = 0;

    while (valPos < valLen)
    {
        if (patPos < patLen && (pattern[patPos] == '?' || pattern[patPos] == value[valPos]))
        {
            ++patPos;
            ++valPos;
        }
        else if (patPos < patLen && pattern[patPos] == '*')
        {
            starIdx = patPos++;
            matchIdx = valPos;
        }
        else if (starIdx != -1)
        {
            patPos = starIdx + 1;
            valPos = ++matchIdx;
        }
        else
        {
            return false;
        }
    }

    while (patPos < patLen && pattern[patPos] == '*')
    {
        ++patPos;
    }

    return patPos == patLen;
}
static bool ValidateConnection(const struct sockaddr* addr, int addrlen, jenova::Firewall::ConnectionDirection conDir = jenova::Firewall::ConnectionDirection::Unknown)
{
    if (addr->sa_family == AF_INET) // IPv4
    {
        struct sockaddr_in* ipv4 = (struct sockaddr_in*)addr;
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(ipv4->sin_addr), ip, INET_ADDRSTRLEN);

        uint16_t port = ntohs(ipv4->sin_port);
        std::string addrStr = std::string(ip) + ":" + std::to_string(port);
        std::string directionStr = (conDir == jenova::Firewall::ConnectionDirection::In) ? "in" : "out";

        // Check all blacklist entries
        for (const auto& entry : networkBlacklistedAddresses)
        {
            size_t firstColon = entry.find(':');
            size_t secondColon = entry.find(':', firstColon + 1);

            std::string ipEntry = entry.substr(0, firstColon);
            std::string portEntry = entry.substr(firstColon + 1, secondColon - firstColon - 1);
            std::string directionEntry = entry.substr(secondColon + 1);

            if (MatchesIpWildcard(ipEntry, ip) && MatchesWildcard(portEntry, std::to_string(port)) && (MatchesWildcard(directionEntry, directionStr) || directionEntry == "*"))
            {
                net_log("Connection blocked: %s:%d:%s matches blacklist entry %s", ip, port, directionStr.c_str(), entry.c_str());
                return false; // Blocked by blacklist
            }
        }

        // Check all whitelist entries
        for (const auto& entry : networkWhitelistedAddresses)
        {
            size_t firstColon = entry.find(':');
            size_t secondColon = entry.find(':', firstColon + 1);

            // Split the entry into IP, Port, and Direction
            std::string ipEntry = entry.substr(0, firstColon);
            std::string portEntry = entry.substr(firstColon + 1, secondColon - firstColon - 1);
            std::string directionEntry = entry.substr(secondColon + 1);

            // Check if the entry matches IP, Port, and Direction
            if (MatchesIpWildcard(ipEntry, ip) && MatchesWildcard(portEntry, std::to_string(port)) && (MatchesWildcard(directionEntry, directionStr) || directionEntry == "*"))
            {
                net_log("Connection allowed: %s:%d:%s matches whitelist entry %s", ip, port, directionStr.c_str(), entry.c_str());
                return true; // Valid connection
            }
        }

        net_log("Connection blocked: %s:%s is not in the whitelist", addrStr.c_str(), directionStr.c_str());
        return false; // Invalid connection
    }
    else if (addr->sa_family == AF_INET6)
    {
        // Handle IPv6 or other address families (if required)
        net_log("Connection blocked: AF_INET6 not allowed.");
        return false;
    }
    else if (addr->sa_family == AF_UNSPEC)
    {
        return true;
    }

    // If it's not IPv4 or IPv6, reject it
    net_log("Connection blocked: Non-IPv4/IPv6 family not allowed. Requested Connection Family : %d", addr->sa_family);
    return false;
}

// Jenova Emulator Firewall Hooks
static int WINAPI Send_Alter(SOCKET s, const char* buf, int len, int flags)
{
    // Validate Modules to Hook
    ValidateHookModules(NETWORK_HOOK_ALLOWED_MODULES)
    {
        // Module Needs No Hooks, Fallback to Original
        return originalSend(s, buf, len, flags);
    }

    // Validate Connection
    struct sockaddr_in addr;
    int addrlen = sizeof(addr);
    if (getpeername(s, (struct sockaddr*)&addr, &addrlen) == 0 && ValidateConnection((struct sockaddr*)&addr, addrlen, jenova::Firewall::ConnectionDirection::Out))
    {
        // Connection is valid
        return originalSend(s, buf, len, flags);
    }

    // Connection not valid
    return SOCKET_ERROR;
}
static int WINAPI Recv_Alter(SOCKET s, char* buf, int len, int flags)
{
    // Validate Modules to Hook
    ValidateHookModules(NETWORK_HOOK_ALLOWED_MODULES)
    {
        // Module Needs No Hooks, Fallback to Original
        return originalRecv(s, buf, len, flags);
    }

    // Validate Connection
    struct sockaddr_in addr;
    int addrlen = sizeof(addr);
    if (getpeername(s, (struct sockaddr*)&addr, &addrlen) == 0 && ValidateConnection((struct sockaddr*)&addr, addrlen, jenova::Firewall::ConnectionDirection::In))
    {
        // Connection is valid
        return originalRecv(s, buf, len, flags);
    }

    // Connection not valid
    return SOCKET_ERROR;
}
static int WINAPI WSASend_Alter(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, LPDWORD lpTotalBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    // Validate Modules to Hook
    ValidateHookModules(NETWORK_HOOK_ALLOWED_MODULES)
    {
        // Module Needs No Hooks, Fallback to Original
        return originalWSASend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, lpTotalBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
    }

    // Validate Connection
    struct sockaddr_in addr;
    int addrlen = sizeof(addr);
    if (getpeername(s, (struct sockaddr*)&addr, &addrlen) == 0 && ValidateConnection((struct sockaddr*)&addr, addrlen, jenova::Firewall::ConnectionDirection::Out))
    {
        // Connection is valid
        return originalWSASend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, lpTotalBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
    }

    // Connection not valid
    return SOCKET_ERROR;
}
static int WINAPI WSARecv_Alter(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpTotalBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    // Validate Modules to Hook
    ValidateHookModules(NETWORK_HOOK_ALLOWED_MODULES)
    {
        // Module Needs No Hooks, Fallback to Original
        return originalWSARecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpTotalBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);
    }

    // Validate Connection
    struct sockaddr_in addr;
    int addrlen = sizeof(addr);
    if (getpeername(s, (struct sockaddr*)&addr, &addrlen) == 0 && ValidateConnection((struct sockaddr*)&addr, addrlen, jenova::Firewall::ConnectionDirection::In))
    {
        // Connection is valid
        return originalWSARecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpTotalBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);
    }

    // Connection not valid
    return SOCKET_ERROR;
}
static int WINAPI SendTo_Alter(SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen)
{
    // Validate Modules to Hook
    ValidateHookModules(NETWORK_HOOK_ALLOWED_MODULES)
    {
        // Module Needs No Hooks, Fallback to Original
        return originalSendTo(s, buf, len, flags, to, tolen);
    }

    // Validate connection
    if (!ValidateConnection(to, tolen, jenova::Firewall::ConnectionDirection::Out))
    {
        // Return error code to block the connection
        WSASetLastError(WSAEACCES); // Access denied error
        return SOCKET_ERROR;
    }

    return originalSendTo(s, buf, len, flags, to, tolen);
}
static int WINAPI RecvFrom_Alter(SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen)
{
    // Validate Modules to Hook
    ValidateHookModules(NETWORK_HOOK_ALLOWED_MODULES)
    {
        // Module Needs No Hooks, Fallback to Original
        return originalRecvFrom(s, buf, len, flags, from, fromlen);
    }

    // Validate connection
    if (!ValidateConnection(from, *fromlen, jenova::Firewall::ConnectionDirection::In))
    {
        // Return error code to block the connection
        WSASetLastError(WSAEACCES); // Access denied error
        return SOCKET_ERROR;
    }

    return originalRecvFrom(s, buf, len, flags, from, fromlen);
}
static int WINAPI WSASendTo_Alter(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, LPDWORD lpTotalBytesSent, DWORD dwFlags, const struct sockaddr* to, int tolen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    // Validate Modules to Hook
    ValidateHookModules(NETWORK_HOOK_ALLOWED_MODULES)
    {
        // Module Needs No Hooks, Fallback to Original
        return originalWSASendTo(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, lpTotalBytesSent, dwFlags, to, tolen, lpOverlapped, lpCompletionRoutine);
    }

    // Validate connection
    if (!ValidateConnection(to, tolen, jenova::Firewall::ConnectionDirection::Out))
    {
        // Return error code to block the connection
        WSASetLastError(WSAEACCES); // Access denied error
        return SOCKET_ERROR;
    }

    return originalWSASendTo(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, lpTotalBytesSent, dwFlags, to, tolen, lpOverlapped, lpCompletionRoutine);
}
static int WINAPI WSARecvFrom_Alter(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpTotalBytesRecvd, LPDWORD lpFlags, struct sockaddr* from, int* fromlen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    // Validate Modules to Hook
    ValidateHookModules(NETWORK_HOOK_ALLOWED_MODULES)
    {
        // Module Needs No Hooks, Fallback to Original
        return originalWSARecvFrom(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpTotalBytesRecvd, lpFlags, from, fromlen, lpOverlapped, lpCompletionRoutine);
    }

    // Validate connection
    if (!ValidateConnection(from, *fromlen, jenova::Firewall::ConnectionDirection::In))
    {
        // Return error code to block the connection
        WSASetLastError(WSAEACCES); // Access denied error
        return SOCKET_ERROR;
    }

    return originalWSARecvFrom(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpTotalBytesRecvd, lpFlags, from, fromlen, lpOverlapped, lpCompletionRoutine);
}

// Jenova Emulator Firewall Implementation
bool JenovaFirewall::ActivateFirewall()
{
    // Validate Firewall State
    if (isFirewallActive == true) return true;

    // Initialize MinHook
    if (MH_Initialize() != MH_STATUS::MH_OK)
    {
        return false;
    }

    // Create hooks
    if (MH_CreateHook(&send, &Send_Alter, reinterpret_cast<LPVOID*>(&originalSend)) != MH_STATUS::MH_OK ||
        MH_CreateHook(&recv, &Recv_Alter, reinterpret_cast<LPVOID*>(&originalRecv)) != MH_STATUS::MH_OK ||
        MH_CreateHook(&WSASend, &WSASend_Alter, reinterpret_cast<LPVOID*>(&originalWSASend)) != MH_STATUS::MH_OK ||
        MH_CreateHook(&WSARecv, &WSARecv_Alter, reinterpret_cast<LPVOID*>(&originalWSARecv)) != MH_STATUS::MH_OK ||
        MH_CreateHook(&sendto, &SendTo_Alter, reinterpret_cast<LPVOID*>(&originalSendTo)) != MH_STATUS::MH_OK ||
        MH_CreateHook(&recvfrom, &RecvFrom_Alter, reinterpret_cast<LPVOID*>(&originalRecvFrom)) != MH_STATUS::MH_OK ||
        MH_CreateHook(&WSASendTo, &WSASendTo_Alter, reinterpret_cast<LPVOID*>(&originalWSASendTo)) != MH_STATUS::MH_OK ||
        MH_CreateHook(&WSARecvFrom, &WSARecvFrom_Alter, reinterpret_cast<LPVOID*>(&originalWSARecvFrom)) != MH_STATUS::MH_OK)
    {
        return false;
    }

    // Enable hooks
    if (MH_EnableHook(&send) != MH_STATUS::MH_OK ||
        MH_EnableHook(&recv) != MH_STATUS::MH_OK ||
        MH_EnableHook(&WSASend) != MH_STATUS::MH_OK ||
        MH_EnableHook(&WSARecv) != MH_STATUS::MH_OK ||
        MH_EnableHook(&sendto) != MH_STATUS::MH_OK ||
        MH_EnableHook(&recvfrom) != MH_STATUS::MH_OK ||
        MH_EnableHook(&WSASendTo) != MH_STATUS::MH_OK ||
        MH_EnableHook(&WSARecvFrom) != MH_STATUS::MH_OK)
    {
        return false;
    }

    // All Good
    isFirewallActive = true;
    return true;
}
bool JenovaFirewall::DeactivateFirewall()
{
    // Validate Firewall State
    if (isFirewallActive == false) return true;

    // Disable hooks
    if (MH_DisableHook(&send) != MH_STATUS::MH_OK ||
        MH_DisableHook(&recv) != MH_STATUS::MH_OK ||
        MH_DisableHook(&WSASend) != MH_STATUS::MH_OK ||
        MH_DisableHook(&WSARecv) != MH_STATUS::MH_OK ||
        MH_DisableHook(&sendto) != MH_STATUS::MH_OK ||
        MH_DisableHook(&recvfrom) != MH_STATUS::MH_OK ||
        MH_DisableHook(&WSASendTo) != MH_STATUS::MH_OK ||
        MH_DisableHook(&WSARecvFrom) != MH_STATUS::MH_OK)
    {
        return false;
    }

    // Remove hooks
    if (MH_RemoveHook(&send) != MH_STATUS::MH_OK ||
        MH_RemoveHook(&recv) != MH_STATUS::MH_OK ||
        MH_RemoveHook(&WSASend) != MH_STATUS::MH_OK ||
        MH_RemoveHook(&WSARecv) != MH_STATUS::MH_OK ||
        MH_RemoveHook(&sendto) != MH_STATUS::MH_OK ||
        MH_RemoveHook(&recvfrom) != MH_STATUS::MH_OK ||
        MH_RemoveHook(&WSASendTo) != MH_STATUS::MH_OK ||
        MH_RemoveHook(&WSARecvFrom) != MH_STATUS::MH_OK)
    {
        return false;
    }

    // Uninitialize MinHook
    if (MH_Uninitialize() != MH_STATUS::MH_OK)
    {
        return false;
    }

    // All Good
    isFirewallActive = false;
    return true;
}
bool JenovaFirewall::AddNetworkRule(std::string networkRule, jenova::Firewall::NetworkRuleType networkRuleType)
{
    switch (networkRuleType)
    {
    case jenova::Firewall::NetworkRuleType::Whitelist:
        networkWhitelistedAddresses.push_back(networkRule);
        break;
    case jenova::Firewall::NetworkRuleType::Blacklist:
        networkBlacklistedAddresses.push_back(networkRule);
        break;
    case jenova::Firewall::NetworkRuleType::Unknown:
        break;
    default:
        break;
    }

    return true;
}
void JenovaFirewall::AddDefaultNetworkRules()
{
    // Whitelist
    // AddNetworkRule("*:*:*", jenova::Firewall::NetworkRuleType::Whitelist); // Allow All
    // AddNetworkRule("x.x.x.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist); // Allow All
    // AddNetworkRule("*:80:*", jenova::Firewall::NetworkRuleType::Whitelist); // Allow HTTP
    // AddNetworkRule("*:443:*", jenova::Firewall::NetworkRuleType::Whitelist); // Allow HTTPS
    AddNetworkRule("*:53:*", jenova::Firewall::NetworkRuleType::Whitelist); // Allow DNS
    AddNetworkRule("127.0.0.1:7271:*", jenova::Firewall::NetworkRuleType::Whitelist); // Jenova Controller

    // Google IP ranges
    AddNetworkRule("8.8.4.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("8.8.8.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("8.34.208.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("8.35.192.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("23.236.48.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("23.251.128.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("34.x.x.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("35.x.x.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("57.140.192.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("64.15.112.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("64.233.160.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("66.22.228.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("66.102.x.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("66.249.64.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("70.32.128.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("72.14.192.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("74.125.x.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("104.154.x.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("104.196.x.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("104.237.160.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("107.167.160.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("107.178.192.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("108.59.80.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("108.170.192.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("108.177.x.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("130.211.x.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("136.22.160.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("136.22.176.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("136.22.184.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("136.22.186.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("142.250.x.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("146.148.x.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("152.65.208.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("152.65.214.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("152.65.218.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("152.65.222.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("152.65.224.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("162.120.128.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("162.216.148.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("162.222.176.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("172.110.32.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("172.217.x.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("172.253.x.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("173.194.x.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("173.255.112.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("192.158.28.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("192.178.x.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("193.186.4.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("199.36.154.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("199.36.156.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("199.192.112.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("199.223.232.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("207.223.160.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("209.85.x.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("216.58.x.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("216.239.x.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("216.239.32.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("216.239.34.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("216.239.36.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);
    AddNetworkRule("216.239.38.x:*:*", jenova::Firewall::NetworkRuleType::Whitelist);

    // BlackList
    AddNetworkRule("1.1.1.1:*:*", jenova::Firewall::NetworkRuleType::Blacklist); // Block 1.1.1.1 DNS
}