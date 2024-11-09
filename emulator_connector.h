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

// Configuration
#define JENOVA_EMULATOR_TYPE Control

// Jenova Emulator Connector Difinition
class JenovaEmulator : public JENOVA_EMULATOR_TYPE
{
	GDCLASS(JenovaEmulator, JENOVA_EMULATOR_TYPE);

private:
	Ref<godot::Mutex> emulatorMutex;

private:
	bool isEnabled = false;
	bool isActive = false;
	bool isValid = false;
	bool isInsideMachine = false;
	bool isFullscreen = false;
	Rect2 emulatorDimension;

protected:
	static void _bind_methods();

public:
	static void init();
	static void deinit();

public:
	void _enter_tree() override;
	void _exit_tree() override;
	void _ready() override;
	void _draw() override;
	void _gui_input(const Ref<InputEvent>& p_event) override;
	void _unhandled_input(const Ref<InputEvent>& p_event) override;
	void _unhandled_key_input(const Ref<InputEvent>& p_event) override;
	void _process(double p_delta) override;
	void _safe_process(double p_delta);

public:
	bool GetEnabledState() const;
	void SetEnabledState(bool value);

public:
	static bool RegisterVirtualMachine(String vmName, const Dictionary& vmSettings);
	static bool UnregisterVirtualMachine(String vmName);
	static bool StartEmulator(String vmName);
	static bool StopEmulator(String vmName);
	static bool IsEmulatorRunning(String vmName);

private:
	static bool IsEditorExecutionAllowed();
	static HWND GetMainGameWindowHandle();
	static bool InitializeEmulatorModule();
	static bool InitializeDefaultVMConfiguration();
	static std::string GenerateArgumentsFromVMConfiguration(String vmName);
	static bool IsEmulatorInputAllowed(bool validateOnlyDisplay = false);
	static bool CreateRawInputDevice();
	static bool DestroyRawInputDevice();
	static void ProcessRawInput(LPARAM lParam);
	static LRESULT CALLBACK EmulatorMouseHook(int nCode, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK EmulatorKeyboardHook(int nCode, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK EmulatorWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static void SyncEmulatorInput();

private:
	static void EmulatorThread();

public:
	void SwitchFullScreenMode();

private:
	void CreateDefaultMaterial();
	void DrawEmulatorViewBorders(Color borderColor, int borderWidth);
	void EnterEmulator();
	void ExitEmulator();
	bool IsInsideMachine() const;

public:
	JenovaEmulator();
};
