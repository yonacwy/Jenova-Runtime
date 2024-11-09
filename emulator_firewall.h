#pragma once

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

// Jenova SDK
#include "Jenova.hpp""

// Jenova Firewall Values & Types
namespace jenova::Firewall
{
	// Enumerators
	enum class ConnectionDirection
	{
		In,
		Out,
		Unknown
	};
	enum class NetworkRuleType
	{
		Whitelist,
		Blacklist,
		Unknown
	};
}

// Jenova Emulator Firewall Difinition
class JenovaFirewall : public RefCounted
{
	GDCLASS(JenovaFirewall, RefCounted);

public:
	static JenovaFirewall* get_singleton();

protected:
	static void _bind_methods();

private:
	bool isFirewallActive = false;

public:
	static void init();
	static void deinit();

public:
	bool ActivateFirewall();
	bool DeactivateFirewall();
	bool AddNetworkRule(std::string vmName, jenova::Firewall::NetworkRuleType networkRuleType = jenova::Firewall::NetworkRuleType::Unknown);
	void AddDefaultNetworkRules();
};