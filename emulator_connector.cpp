
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

/*
+ WARNING :
  Do not use Emulator, Proprietary code is removed.
*/

// Jenova SDK
#include "Jenova.hpp"

// Jenova Emulator SDK
#include "JenovaVM.h"

// Static Macro Configurations
#define JENOVA_DEFAULT_VM_NAME              "JenovaDefaultVM"
#define JENOVA_RAW_INPUT_CLASS_NAME         L"JenovaRawInputHandler"
#define JENOVA_EMULATOR_MODULE_PATH         L"Jenova.Emulator.x64.dll"

// Internal Types
struct VirtualMachineInitialData
{
    void*       interfacePtr;
};

// Static Value Configurations/Flags
static Color emulatorDefaultBackground      = Color(0, 0, 0, 1.0f);
static Color emulatorInactiveBorderColor    = Color(0.326f, 0.129f, 0.837f, 0.5f);
static Color emulatorInputBlockBorderColor  = Color(0.231f, 0.671f, 0.929f, 0.4f);
static Color emulatorActiveBorderColor      = Color(0.537f, 0.929f, 0.196f, 0.8f);
static Vector2i emulatorMainResolution      = Vector2i(1920, 1080);
static Vector2i emulatorMinimumResolution   = Vector2i(1024, 768);
static int emulatorBorderWidth              = 2;
static bool enableEmulatorHotkeys           = true;
static bool enableEmulatorVerbose           = false;
static bool startEmulatorOnInitialization   = false;
static bool executeInEditor                 = false;

// Dynamic Value Configurations/Flags
static int emulatorExitCode                 = -1;
static bool isEmulatorRunning               = false;
static bool isEmulatorInputAllowed          = false;
static bool exitEmulatorOnProcess           = false;

// Virtual Machine Handles
static HMODULE jenovaEmuModule              = nullptr;
static HANDLE jenovaEmuThreadHandle         = nullptr;
static HHOOK jenovaEmuMouseHook             = nullptr;
static HHOOK jenovaEmuKeyboardHook          = nullptr;
static HWND jenovaRawInputDeviceHandle      = nullptr;
static WNDPROC originalWindowHandler        = nullptr;

// Global Values
static Ref<Image> vmDisplayImage;
static Ref<ImageTexture> vmDisplayTexture;
static Vector2i vmDisplayTextureSize(1280, 720);

// Virtual Machine Settings
static JenovaEmulator* currentActiveEmulator = nullptr;
static Dictionary* currentVMSettings = nullptr;
static VirtualMachineInitialData vmInitialData = { 0 };

// Constructor
JenovaEmulator::JenovaEmulator()
{
    emulatorMutex.instantiate();
}

// Bindings
void JenovaEmulator::_bind_methods()
{
    ClassDB::bind_static_method("JenovaEmulator", D_METHOD("StartEmulator"), &JenovaEmulator::StartEmulator);
    ClassDB::bind_static_method("JenovaEmulator", D_METHOD("StopEmulator"), &JenovaEmulator::StopEmulator);
    ClassDB::bind_method(D_METHOD("GetEnabledState"), &JenovaEmulator::GetEnabledState);
    ClassDB::bind_method(D_METHOD("SetEnabledState", "value"), &JenovaEmulator::SetEnabledState);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "Emulator Enabled"), "SetEnabledState", "GetEnabledState");
}

// Initializer/Deinitializer
void JenovaEmulator::init() 
{
    // Register Class
	ClassDB::register_class<JenovaEmulator>();

    // Initialize Objects
    currentVMSettings = new Dictionary();

    // Check for Engine Exectuion Mode
    if (IsEditorExecutionAllowed()) return;

    // Initialize Virtual Machine/Emulator Module
    if (!InitializeEmulatorModule())
    {
        jenova::Error("Emulator", "Failed to Initialize Emulator Module.");
    }

    // Initialize Default Virtual Machine Configuration
    if (!InitializeDefaultVMConfiguration())
    {
        jenova::Error("Emulator", "Failed to Initialize Default Virtual Machine Configuration.");
    }

    // Start Default Emulator (Autostart)
    if (startEmulatorOnInitialization)
    {
        if (!StartEmulator(JENOVA_DEFAULT_VM_NAME))
        {
            jenova::Error("Emulator", "Failed to Start Default Emulator.");
        }
    }
}
void JenovaEmulator::deinit()
{
    // Free Objects
    if (currentVMSettings) 
    {
        delete currentVMSettings;
        currentVMSettings = nullptr;
    }

    // Check for Engine Exectuion Mode
    if (IsEditorExecutionAllowed()) return;

    // Release Virtual Machine Module
    if (jenovaEmuModule) FreeLibrary(jenovaEmuModule);
}

// Events Implementation
void JenovaEmulator::_enter_tree()
{
    // Alert
    jenova::Alert("Do not use Jenova Emulator, Proprietary Code is Removed.");
    this->queue_free();
    return;

    // Validations
    if (this->IsEditorExecutionAllowed()) return;

    // Create Default Material
    this->CreateDefaultMaterial();

    // Create Base Display Texture
    vmDisplayImage = Image::create(vmDisplayTextureSize.x, vmDisplayTextureSize.y, false, Image::FORMAT_RGBA8);
    vmDisplayImage->fill(Color(0.5f, 0.5f, 0.5f, 1.0f));
    vmDisplayTexture = ImageTexture::create_from_image(vmDisplayImage);

    // Update Flags
    this->isActive = true;
}
void JenovaEmulator::_exit_tree()
{
    if (this->IsEditorExecutionAllowed()) return;
    vmDisplayTexture.unref();
    vmDisplayImage.unref();

    // Deactivate Firewall
    if (JenovaFirewall::get_singleton()) JenovaFirewall::get_singleton()->DeactivateFirewall();

    // Terminate VM
    if (isEmulatorRunning && jenovaEmuThreadHandle)
    {
        // Send Force Shutdown Request to VM
        jenova::vm::JenovaAPI_ExecuteCommand(jenova::vm::JENOVA_VMCMD_KILL, jenova::vm::JenovaDataPacket());

        // Wait for VM to Shutdown
        while (isEmulatorRunning) { Sleep(100); }
    }

    // Update Flags
    this->isActive = false;
}
void JenovaEmulator::_ready()
{
    // Validate
    if (this->IsEditorExecutionAllowed()) return;

    // Set Default Tooltip
    this->set_tooltip_text("Click On Emulator to Enter Virtual Machine.");
}
void JenovaEmulator::_draw()
{
    // Get Viewport Rect (Control Size)
    Rect2 rect = Rect2(Vector2(0, 0), get_size());

    // Draw Default Background
    draw_rect(rect, emulatorDefaultBackground, true);

    // Draw Inactive Borders
    this->DrawEmulatorViewBorders(emulatorInactiveBorderColor, emulatorBorderWidth);

    // If Editor Support is Disabled, Draw Gray Rect
    if (this->IsEditorExecutionAllowed()) return;

    // Validate VM Display Data
    if (!this->isValid || vmDisplayTexture.is_null() || !isEmulatorRunning) return;

    // Get the size of the control and the VM texture
    Vector2 scaledSize = Vector2();
    Vector2 controlSize = get_size();
    Vector2 textureSize = vmDisplayTexture->get_size();

    // Calculate aspect ratios
    float controlAspectRatio = controlSize.x / controlSize.y;
    float textureAspectRatio = textureSize.x / textureSize.y;

    // If texture aspect ratio is larger, fit by width
    if (textureAspectRatio > controlAspectRatio)
    {
        // Scale by width, fit to control's width
        scaledSize.x = controlSize.x;
        scaledSize.y = controlSize.x / textureAspectRatio;
    }
    else
    {
        // Scale by height, fit to control's height
        scaledSize.y = controlSize.y;
        scaledSize.x = controlSize.y * textureAspectRatio;
    }

    // Calculate the position to center the texture in the control
    Vector2 position = (controlSize - scaledSize) / 2;

    // Draw the Texture, Centered and Scaled
    Rect2 textureRect = Rect2(position, scaledSize);
    draw_texture_rect(vmDisplayTexture, textureRect, false);

    // Draw Active Borders
    this->DrawEmulatorViewBorders(IsEmulatorInputAllowed(true) ? emulatorActiveBorderColor : emulatorInputBlockBorderColor, emulatorBorderWidth);
}
void JenovaEmulator::_gui_input(const Ref<InputEvent>& p_event)
{
    // Validate
    if (this->IsEditorExecutionAllowed()) return;

    // Ignore If Disabled
    if (!this->is_visible() || !this->GetEnabledState()) return;

    // Capture Inputs
    if (p_event->is_pressed() && isInsideMachine == false && IsEmulatorInputAllowed(true))
    {
        // Enter Emulator
        this->EnterEmulator();
    }
}
void JenovaEmulator::_unhandled_input(const Ref<InputEvent>& p_event)
{
    // Validate
    if (this->IsEditorExecutionAllowed()) return;

    // Ignore If Disabled
    if (!this->is_visible() || !this->GetEnabledState()) return;
}
void JenovaEmulator::_unhandled_key_input(const Ref<InputEvent>& p_event)
{
    // Validate
    if (this->IsEditorExecutionAllowed()) return;

    // Ignore If Disabled
    if (!this->is_visible() || !this->GetEnabledState()) return;

    // Capture Key
    Ref<InputEventKey> key_event = p_event;
    if (key_event.is_valid() && key_event->is_pressed() &&
        key_event->get_keycode() == KEY_F && key_event->is_ctrl_pressed() &&
        key_event->is_shift_pressed())
    {
        // Exit Emulator
        this->SwitchFullScreenMode();
    }
}
void JenovaEmulator::_process(double p_delta)
{
    __try
    {
        _safe_process(p_delta);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        jenova::Error("Emulator", "JenovaEmulator Instance (%p) Faced an Error : Memory Violation Happened.");
    }
}
void JenovaEmulator::_safe_process(double p_delta)
{
    // Validations
    if (!jenovaEmuModule) return;
    if (this->IsEditorExecutionAllowed()) return;
    if (vmDisplayImage.is_null() || !vmDisplayImage.is_valid() || vmDisplayTexture.is_null() || !vmDisplayTexture.is_valid()) return;
    if (!this->isActive && !isEmulatorRunning) return;

    // Process Remote Commands
    if (exitEmulatorOnProcess)
    {
        if (this->IsInsideMachine())
        {
            this->ExitEmulator();
            isEmulatorInputAllowed = false;
        }
        exitEmulatorOnProcess = false;
    }

    // Update Flags
    this->isValid = false;

    // Get Main VM Display Data
    jenova::vm::JenovaDisplayData displayData = jenova::vm::JenovaAPI_GetDisplayData(0);

    // Validate Display Data
    if (!displayData.valid || !displayData.framebuffer) return;

    // Cancel Minimum Size
    if (displayData.width < emulatorMinimumResolution.x || displayData.height < emulatorMinimumResolution.y) return;

    // Check for Input VM Display Image Size
    if (displayData.width != vmDisplayTextureSize.x || displayData.height != vmDisplayTextureSize.y)
    {
        // Dispose Current Textures
        if (vmDisplayImage.is_valid()) vmDisplayImage.unref();
        if (vmDisplayTexture.is_valid()) vmDisplayTexture.unref();

        // Update Size
        vmDisplayTextureSize.x = displayData.width;
        vmDisplayTextureSize.y = displayData.height;

        // Create New & Update Textures
        vmDisplayImage = Image::create(vmDisplayTextureSize.x, vmDisplayTextureSize.y, false, Image::FORMAT_RGBA8);
        vmDisplayImage->fill(Color(0.5f, 0.5f, 0.5f, 1.0f));
        vmDisplayTexture = ImageTexture::create_from_image(vmDisplayImage);

        // Update Input Allowance
        isEmulatorInputAllowed = vmDisplayTextureSize == emulatorMainResolution;
        if (!isEmulatorInputAllowed) exitEmulatorOnProcess = true;

        // Verbose
        jenova::Output("Jenova VM Display Textures Updated, New Size : %d/%d ", vmDisplayTextureSize.x, vmDisplayTextureSize.y);
    }

    // Update Image Data From Display Data
    PackedByteArray data = vmDisplayImage->get_data();
    uint8_t* dest = data.ptrw();
    uint8_t* src = displayData.framebuffer;
    size_t width = vmDisplayImage->get_width();
    size_t height = vmDisplayImage->get_height();
    size_t dataSize = width * height * 4;

    // Validate Buffers Size (Actual < Expected)
    if (data.size() < dataSize) return;

    // Disable VM Display Update
    jenova::vm::JenovaAPI_SetDisplayUpdateState(false);

    // Lock Mutex
    emulatorMutex->lock();

    // Copy Pixel Data
    if (dest && src) memcpy_s(dest, dataSize, src, dataSize);

    // Lock Mutex
    emulatorMutex->unlock();

    // Enable VM Display Update
    jenova::vm::JenovaAPI_SetDisplayUpdateState(true);

    // Update Texture Data
    vmDisplayImage->set_data(width, height, false, Image::FORMAT_RGBA8, data);
    vmDisplayTexture->update(vmDisplayImage);

    // Directly update the texture on the GPU
    // RenderingServer::get_singleton()->texture_2d_update(vmDisplayTexture->get_rid(), vmDisplayImage, 0);

    // Update Flags
    this->isValid = true;

    // Request Draw
    queue_redraw();
}

// Setters/Getters Implementation
bool JenovaEmulator::GetEnabledState() const
{
    return this->isEnabled;
}
void JenovaEmulator::SetEnabledState(bool value)
{
    this->isEnabled = value;
}

// Global APIs Implementation (Public)
bool JenovaEmulator::RegisterVirtualMachine(String vmName, const Dictionary& vmSettings)
{
    // Check if the VM name already exists in currentVMSettings
    if (currentVMSettings->has(vmName)) return false;

    // Add new VM settings
    (*currentVMSettings)[vmName] = vmSettings;

    // All Good
    jenova::Output("New Virtual Machine `%s` Registered.", AS_C_STRING(vmName));
    return true;
}
bool JenovaEmulator::UnregisterVirtualMachine(String vmName)
{
    // Check if the VM name exists in currentVMSettings
    if (!currentVMSettings->has(vmName)) return false;

    // Remove the VM settings
    currentVMSettings->erase(vmName);

    // All Good
    jenova::Output("Virtual Machine `%s` Unregistered.", AS_C_STRING(vmName));
    return true;
}
bool JenovaEmulator::StartEmulator(String vmName)
{
    // All Good
	return true;
}
bool JenovaEmulator::StopEmulator(String vmName)
{
    // All Good
	return true;
}
bool JenovaEmulator::IsEmulatorRunning(String vmName)
{
    // For Now Only Single Instance Emulator is Supported
    if (vmName == JENOVA_DEFAULT_VM_NAME) return isEmulatorRunning;
    return false;
}

// Global APIs Implementation (Internal)
bool JenovaEmulator::IsEditorExecutionAllowed()
{
    return QUERY_ENGINE_MODE(Editor) && !executeInEditor;
}
HWND JenovaEmulator::GetMainGameWindowHandle()
{
    return (HWND)DisplayServer::get_singleton()->window_get_native_handle(DisplayServer::HandleType::WINDOW_HANDLE);
}
bool JenovaEmulator::InitializeEmulatorModule()
{
    // Load Emulator Module
    jenovaEmuModule = LoadLibraryW(JENOVA_EMULATOR_MODULE_PATH);
    if (!jenovaEmuModule)
    {
        jenova::Error("Emulator" ,"Failed to load the Emulator.");
        return false;
    }

    // Initialize Emulator APIs
    jenova::Output("Initializing Emulator And Virtual Machine APIs...");
    jenova::vm::JenovaAPI_InitializeEmulator = (jenova::vm::JenovaAPI_InitializeEmulatorFunc)GetProcAddress(jenovaEmuModule, "JenovaAPI_InitializeEmulator");
    if (!jenova::vm::JenovaAPI_InitializeEmulator)
    {
        jenova::Error("Emulator" ,"Failed to obtain the Jenova API Initialize Emulator.");
        FreeLibrary(jenovaEmuModule);
        return false;
    }
    jenova::vm::JenovaAPI_ExecuteCommand = (jenova::vm::JenovaAPI_ExecuteCommandFunc)GetProcAddress(jenovaEmuModule, "JenovaAPI_ExecuteCommand");
    if (!jenova::vm::JenovaAPI_ExecuteCommand)
    {
        jenova::Error("Emulator" ,"Failed to obtain the Jenova API Execute Command.");
        FreeLibrary(jenovaEmuModule);
        return false;
    }
    jenova::vm::JenovaAPI_SetDisplayUpdateState = (jenova::vm::JenovaAPI_SetDisplayUpdateStateFunc)GetProcAddress(jenovaEmuModule, "JenovaAPI_SetDisplayUpdateState");
    if (!jenova::vm::JenovaAPI_SetDisplayUpdateState)
    {
        jenova::Error("Emulator" ,"Failed to obtain the Jenova API Set Display Update State.");
        FreeLibrary(jenovaEmuModule);
        return false;
    }
    jenova::vm::JenovaAPI_GetDisplayData = (jenova::vm::JenovaAPI_GetDisplayDataFunc)GetProcAddress(jenovaEmuModule, "JenovaAPI_GetDisplayData");
    if (!jenova::vm::JenovaAPI_GetDisplayData)
    {
        jenova::Error("Emulator" ,"Failed to obtain the Jenova API Get Display Data.");
        FreeLibrary(jenovaEmuModule);
        return false;
    }
    jenova::vm::JenovaAPI_GetConsoleCount = (jenova::vm::JenovaAPI_GetConsoleCountFunc)GetProcAddress(jenovaEmuModule, "JenovaAPI_GetConsoleCount");
    if (!jenova::vm::JenovaAPI_GetConsoleCount)
    {
        jenova::Error("Emulator" ,"Failed to obtain the Jenova API Get Console Count.");
        FreeLibrary(jenovaEmuModule);
        return false;
    }
    jenova::vm::JenovaAPI_GetConsolePtr = (jenova::vm::JenovaAPI_GetConsolePtrFunc)GetProcAddress(jenovaEmuModule, "JenovaAPI_GetConsolePtr");
    if (!jenova::vm::JenovaAPI_GetConsolePtr)
    {
        jenova::Error("Emulator" ,"Failed to obtain the Jenova API Get Console Ptr.");
        FreeLibrary(jenovaEmuModule);
        return false;
    }
    jenova::vm::JenovaAPI_ExecuteDisplayCommand = (jenova::vm::JenovaAPI_ExecuteCommandFunc)GetProcAddress(jenovaEmuModule, "JenovaAPI_ExecuteDisplayCommand");
    if (!jenova::vm::JenovaAPI_ExecuteDisplayCommand)
    {
        jenova::Error("Emulator" ,"Failed to obtain the Jenova API Execute Display Command.");
        FreeLibrary(jenovaEmuModule);
        return false;
    }
    jenova::vm::JenovaAPI_GetRAMBaseAddress = (jenova::vm::JenovaAPI_GetRAMBaseAddressFunc)GetProcAddress(jenovaEmuModule, "JenovaAPI_GetRAMBaseAddress");
    if (!jenova::vm::JenovaAPI_GetRAMBaseAddress)
    {
        jenova::Error("Emulator" ,"Failed to obtain the Jenova API Get RAM Base Address.");
        FreeLibrary(jenovaEmuModule);
        return false;
    }
    jenova::vm::JenovaAPI_GetRAMSize = (jenova::vm::JenovaAPI_GetRAMSizeFunc)GetProcAddress(jenovaEmuModule, "JenovaAPI_GetRAMSize");
    if (!jenova::vm::JenovaAPI_GetRAMSize)
    {
        jenova::Error("Emulator" ,"Failed to obtain the Jenova API Get RAM Size.");
        FreeLibrary(jenovaEmuModule);
        return false;
    }

    // All Good
    jenova::Output("Jenova Emulator System Initialzied.");
    return true;
}
bool JenovaEmulator::InitializeDefaultVMConfiguration()
{
    // Validate Module
    if (!jenovaEmuModule) return false;

    // Create Dictionary
    godot::Dictionary defaultConfiguration;

    // Register New Virtual Machine
    return RegisterVirtualMachine(JENOVA_DEFAULT_VM_NAME, defaultConfiguration);
}
std::string JenovaEmulator::GenerateArgumentsFromVMConfiguration(String vmName)
{
    std::string result;

    // Check if currentVMSettings is initialized and contains the specified VM name
    if (currentVMSettings == nullptr || !currentVMSettings->has(vmName)) return result;

    // Get the VM settings
    godot::Dictionary vmSettings = currentVMSettings->operator[](vmName);

    // Iterate through the dictionary and build the argument string
    for (int i = 0; i < vmSettings.size(); ++i)
    {
        godot::Variant keyVariant = vmSettings.keys()[i];
        godot::Variant valueVariant = vmSettings.values()[i];
        godot::String key = keyVariant;
        godot::String value = valueVariant;

        // Remove @X suffix from key if present
        int at_pos = key.find("@");
        if (at_pos != -1) key = key.substr(0, at_pos);

        // Append the argument to the result string
        result += key.utf8().get_data();
        result += " ";
        result += value.utf8().get_data();
        result += " ";
    }

    // Trim trailing space
    if (!result.empty()) result.pop_back();

    return result;
}
bool JenovaEmulator::IsEmulatorInputAllowed(bool validateOnlyDisplay)
{
    // Validate Input
    if (!validateOnlyDisplay)
    {
        if (!currentActiveEmulator) return false;
        if (!currentActiveEmulator->isValid) return false;
    }
    if (!isEmulatorInputAllowed) return false;

    // All Good
    return true;
}
bool JenovaEmulator::CreateRawInputDevice()
{
    // Raw Input Handler
    static WNDPROC WndProc = [](HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) -> LRESULT
    {
        if (uMsg == WM_INPUT)
        {
            JenovaEmulator::ProcessRawInput(lParam);
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    };

    // Create Message Only Window for Raw Input Capture
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = JENOVA_RAW_INPUT_CLASS_NAME;
    if (!RegisterClass(&wc)) return false;
    jenovaRawInputDeviceHandle = CreateWindow(wc.lpszClassName, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    if (!jenovaRawInputDeviceHandle) return false;

    // Create Raw Input Devices
    RAWINPUTDEVICE rid[1] = { 0 };
    rid[0].usUsagePage = 0x01; // Generic Desktop Controls
    rid[0].usUsage = 0x02;     // Mouse
    rid[0].dwFlags = RIDEV_INPUTSINK;
    rid[0].hwndTarget = jenovaRawInputDeviceHandle;

    // Register Raw Input Device
    if (!RegisterRawInputDevices(rid, 1, sizeof(RAWINPUTDEVICE))) return false;

    // All Good
    return true;
}
bool JenovaEmulator::DestroyRawInputDevice()
{
    // Unregister Raw Input Devices (Mouse and Keyboard)
    RAWINPUTDEVICE rid[1] = { 0 };

    // For Mouse
    rid[0].usUsagePage = 0x01;
    rid[0].usUsage = 0x02; // Mouse
    rid[0].dwFlags = RIDEV_REMOVE;
    rid[0].hwndTarget = nullptr;

    // Unregister Raw Input Device
    if (!RegisterRawInputDevices(rid, 1, sizeof(RAWINPUTDEVICE))) return false;

    // Destroy Message-Only Window
    if (jenovaRawInputDeviceHandle)
    {
        DestroyWindow(jenovaRawInputDeviceHandle);
        jenovaRawInputDeviceHandle = nullptr;
    }

    // Unregister Window Class
    if (!UnregisterClass(JENOVA_RAW_INPUT_CLASS_NAME, GetModuleHandle(nullptr)))
    {
        return false;
    }

    // All Good
    return true;
}
void JenovaEmulator::ProcessRawInput(LPARAM lParam)
{
    // Calculate Raw Mouse Input Delta
    UINT dwSize;
    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
    LPBYTE lpb = new BYTE[dwSize];
    if (!lpb) return;
    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
    {
        delete[] lpb;
        return;
    }
    RAWINPUT* raw = (RAWINPUT*)lpb;
    if (raw->header.dwType == RIM_TYPEMOUSE)
    {
        // Validate Input Allowance
        if (IsEmulatorInputAllowed())
        {
            // Create and Send Mouse Movement Data
            jenova::vm::JenovaInputData inputData = {};
            inputData.console = jenova::vm::JenovaAPI_GetConsolePtr(jenova::vm::DEFAULT_EMULATOR_CONSOLE_INDEX);
            inputData.dataType = jenova::vm::JenovaInputDataType::QueueRelative;

            int deltaX = raw->data.mouse.lLastX;
            int deltaY = raw->data.mouse.lLastY;

            deltaX = std::clamp(deltaX, -60, 60);
            deltaY = std::clamp(deltaY, -60, 60);

            inputData.axis = jenova::vm::JenovaInputAxis::INPUT_AXIS_X;
            inputData.value = deltaX;
            jenova::vm::JenovaAPI_ExecuteDisplayCommand(jenova::vm::JENOVA_VMCMD_INPUT, { &inputData, sizeof(inputData) });

            inputData.axis = jenova::vm::JenovaInputAxis::INPUT_AXIS_Y;
            inputData.value = deltaY;
            jenova::vm::JenovaAPI_ExecuteDisplayCommand(jenova::vm::JENOVA_VMCMD_INPUT, { &inputData, sizeof(inputData) });

            // Sync Mouse Movement Input
            SyncEmulatorInput();

            // Safe Delay
            std::this_thread::yield();
            Sleep(1);
        }
    }
    delete[] lpb;
}
LRESULT JenovaEmulator::EmulatorMouseHook(int nCode, WPARAM wParam, LPARAM lParam)
{
    // Validate Input Allowance
    if (!IsEmulatorInputAllowed()) return 1;

    // Handle Inputs
    if (nCode >= 0)
    {
        MSLLHOOKSTRUCT* mouseStruct = (MSLLHOOKSTRUCT*)lParam;

        jenova::vm::JenovaInputData inputData = {};
        inputData.console = jenova::vm::JenovaAPI_GetConsolePtr(jenova::vm::DEFAULT_EMULATOR_CONSOLE_INDEX);

        switch (wParam)
        {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                inputData.dataType = jenova::vm::JenovaInputDataType::QueueButton;
                inputData.button = jenova::vm::JenovaInputButton::INPUT_BUTTON_LEFT;
                inputData.pressed = (wParam == WM_LBUTTONDOWN);
                jenova::vm::JenovaAPI_ExecuteDisplayCommand(jenova::vm::JENOVA_VMCMD_INPUT, { &inputData, sizeof(inputData) });
                break;

            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                inputData.dataType = jenova::vm::JenovaInputDataType::QueueButton;
                inputData.button = jenova::vm::JenovaInputButton::INPUT_BUTTON_RIGHT;
                inputData.pressed = (wParam == WM_RBUTTONDOWN);
                jenova::vm::JenovaAPI_ExecuteDisplayCommand(jenova::vm::JENOVA_VMCMD_INPUT, { &inputData, sizeof(inputData) });
                break;

            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                inputData.dataType = jenova::vm::JenovaInputDataType::QueueButton;
                inputData.button = jenova::vm::JenovaInputButton::INPUT_BUTTON_MIDDLE;
                inputData.pressed = (wParam == WM_MBUTTONDOWN);
                jenova::vm::JenovaAPI_ExecuteDisplayCommand(jenova::vm::JENOVA_VMCMD_INPUT, { &inputData, sizeof(inputData) });
                break;

            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
                inputData.dataType = jenova::vm::JenovaInputDataType::QueueButton;
                if (GET_XBUTTON_WPARAM(mouseStruct->mouseData) == XBUTTON1)
                {
                    inputData.button = jenova::vm::JenovaInputButton::INPUT_BUTTON_SIDE;
                }
                else if (GET_XBUTTON_WPARAM(mouseStruct->mouseData) == XBUTTON2)
                {
                    inputData.button = jenova::vm::JenovaInputButton::INPUT_BUTTON_EXTRA;
                }
                inputData.pressed = (wParam == WM_XBUTTONDOWN);
                jenova::vm::JenovaAPI_ExecuteDisplayCommand(jenova::vm::JENOVA_VMCMD_INPUT, { &inputData, sizeof(inputData) });
                break;

            case WM_MOUSEWHEEL:
                inputData.dataType = jenova::vm::JenovaInputDataType::QueueButton;
                int wheelDelta = GET_WHEEL_DELTA_WPARAM(mouseStruct->mouseData);
                if (wheelDelta > 0)
                {
                    inputData.button = jenova::vm::JenovaInputButton::INPUT_BUTTON_WHEEL_UP;
                }
                else
                {
                    inputData.button = jenova::vm::JenovaInputButton::INPUT_BUTTON_WHEEL_DOWN;
                }
                inputData.pressed = true;
                jenova::vm::JenovaAPI_ExecuteDisplayCommand(jenova::vm::JENOVA_VMCMD_INPUT, { &inputData, sizeof(inputData) });
                SyncEmulatorInput();
                inputData.pressed = false;
                jenova::vm::JenovaAPI_ExecuteDisplayCommand(jenova::vm::JENOVA_VMCMD_INPUT, { &inputData, sizeof(inputData) });
                SyncEmulatorInput();
                break;
        }

        SyncEmulatorInput();
    }

    // Block Host Mouse
    return 1;
}
LRESULT JenovaEmulator::EmulatorKeyboardHook(int nCode, WPARAM wParam, LPARAM lParam)
{
    // Global Modifier Keys State
    static bool ctrlPressed = false, shiftPressed = false, altPressed = false;

    // Validate Input Allowance
    if (!IsEmulatorInputAllowed()) return 1;

    // Handle Inputs
    if (nCode >= 0)
    {
        KBDLLHOOKSTRUCT* kbdStruct = (KBDLLHOOKSTRUCT*)lParam;

        // Update Modifier Keys State
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
        {
            if (kbdStruct->vkCode == VK_CONTROL || kbdStruct->vkCode == VK_LCONTROL || kbdStruct->vkCode == VK_RCONTROL)
            {
                ctrlPressed = true;
            }
            if (kbdStruct->vkCode == VK_SHIFT || kbdStruct->vkCode == VK_LSHIFT || kbdStruct->vkCode == VK_RSHIFT)
            {
                shiftPressed = true;
            }
            if (kbdStruct->vkCode == VK_MENU || kbdStruct->vkCode == VK_LMENU || kbdStruct->vkCode == VK_RMENU)
            {
                altPressed = true;
            }
        }
        if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
        {
            if (kbdStruct->vkCode == VK_CONTROL || kbdStruct->vkCode == VK_LCONTROL || kbdStruct->vkCode == VK_RCONTROL)
            {
                ctrlPressed = false;
            }
            if (kbdStruct->vkCode == VK_SHIFT || kbdStruct->vkCode == VK_LSHIFT || kbdStruct->vkCode == VK_RSHIFT)
            {
                shiftPressed = false;
            }
            if (kbdStruct->vkCode == VK_MENU || kbdStruct->vkCode == VK_LMENU || kbdStruct->vkCode == VK_RMENU)
            {
                altPressed = false;
            }
        }

        // Check For Emulator Hotkeys
        if (enableEmulatorHotkeys)
        {
            // Hotkey Detection Flag
            bool emulatorHotKeyDetected = false;

            // Execute Hotkeys on WM_KEYUP or WM_SYSKEYUP
            if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
            {
                // Exit Virtual Machine
                if (kbdStruct->vkCode == 'B' && ctrlPressed && shiftPressed && currentActiveEmulator)
                {
                    emulatorHotKeyDetected = true;
                    currentActiveEmulator->ExitEmulator();
                }

                // Switch Full Screen
                if (kbdStruct->vkCode == 'F' && ctrlPressed && shiftPressed && currentActiveEmulator)
                {
                    emulatorHotKeyDetected = true;
                    currentActiveEmulator->SwitchFullScreenMode();
                }
            }

            // Release Hotkey Data From Machine
            if (emulatorHotKeyDetected)
            {
                // Release Modifier Keys
                jenova::vm::JenovaInputData inputData = {};
                inputData.dataType = jenova::vm::JenovaInputDataType::ReleaseModifierKeys;
                jenova::vm::JenovaAPI_ExecuteDisplayCommand(jenova::vm::JENOVA_VMCMD_INPUT, { &inputData, sizeof(inputData) });
            }
        }

        // Process And Redirect Key Strokes
        if (nCode == HC_ACTION)
        {
            // Prepare Input Data
            jenova::vm::JenovaInputData inputData = {};
            inputData.console = jenova::vm::JenovaAPI_GetConsolePtr(jenova::vm::DEFAULT_EMULATOR_CONSOLE_INDEX);

            switch (wParam)
            {
                case WM_KEYDOWN:
                case WM_KEYUP:
                case WM_SYSKEYDOWN:
                case WM_SYSKEYUP:

                    // Update Input Data
                    inputData.dataType = jenova::vm::JenovaInputDataType::SendKey;
                    inputData.keyCode = kbdStruct->vkCode;
                    inputData.pressed = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);

                    // Handle Alt Key (Alternative : (kbdStruct->flags & LLKHF_ALTDOWN))
                    if (kbdStruct->vkCode == VK_MENU || kbdStruct->vkCode == VK_LMENU || kbdStruct->vkCode == VK_RMENU) inputData.pressed = altPressed;

                    // Send Key Data to Emulator
                    jenova::vm::JenovaAPI_ExecuteDisplayCommand(jenova::vm::JENOVA_VMCMD_INPUT, { &inputData, sizeof(inputData) });
                    break;

                default:
                    break;
            }

            // Block Host Keyboard Input
            return 1;
        }
    }

    // Call the next hook in the chain
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}
LRESULT JenovaEmulator::EmulatorWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (originalWindowHandler) return CallWindowProc(originalWindowHandler, hwnd, msg, wParam, lParam);
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
void JenovaEmulator::SyncEmulatorInput()
{
    // Sync Input
    if (!jenova::vm::JenovaAPI_ExecuteDisplayCommand) return;
    jenova::vm::JenovaInputData syncData = {};
    syncData.dataType = jenova::vm::JenovaInputDataType::EventSync;
    jenova::vm::JenovaAPI_ExecuteDisplayCommand(jenova::vm::JENOVA_VMCMD_INPUT, { &syncData, sizeof(syncData) });
}

// Emulator Thread
void JenovaEmulator::EmulatorThread()
{
    // Set Emulator Verbose
    if (enableEmulatorVerbose)
    {
        // Enable VM Console Verbose
        jenova::vm::JenovaAPI_ExecuteCommand(jenova::vm::JENOVA_ENABLE_CONSOLE, jenova::vm::JenovaDataPacket());
    }

    // Validate Function
    if (!jenova::vm::JenovaAPI_InitializeEmulator) return;

    // Activate Firewall
    if (JenovaFirewall::get_singleton()) JenovaFirewall::get_singleton()->ActivateFirewall();

    // Run Emulator
    emulatorExitCode = jenova::vm::JenovaAPI_InitializeEmulator(vmInitialData.interfacePtr);
    // Update Emulator State
    isEmulatorRunning = false;

    // Deactivate Firewall
    if (JenovaFirewall::get_singleton()) JenovaFirewall::get_singleton()->DeactivateFirewall();

    // Exit Emulator if Inside
    exitEmulatorOnProcess = true;

    // Verbose
    jenova::Output("Emulator Exited With Code : %d", emulatorExitCode);
}

// Instance APIs Implementation (Internal)
void JenovaEmulator::SwitchFullScreenMode()
{
    // Create a new Tween instance using the tree
    Ref<Tween> tween = get_tree()->create_tween();
    Ref<Tween> tween2 = get_tree()->create_tween();

    // Backup current size and position
    Rect2 current_rect = this->get_rect();
    Vector2 current_center = current_rect.position + current_rect.size / 2.0;

    if (isFullscreen)
    {
        // Restore Back to Size
        Rect2 target_rect = emulatorDimension;
        Vector2 target_center = target_rect.position + target_rect.size / 2.0;

        // Animate size and position
        tween->tween_property(this, "size", target_rect.size, 0.5f);
        tween2->tween_property(this, "position", target_rect.position, 0.5f);

        // Update Flags
        isFullscreen = false;
        
        // Tween Finish Callback
        class TweenFinishedCallback : public RefCounted
        {
        private:
            JenovaEmulator* emulatorInstance;
        public:
            TweenFinishedCallback(JenovaEmulator* _emulator) : emulatorInstance(_emulator) {}
            void OnTweenCompleted()
            {
                // Update Z Index
                emulatorInstance->set("z_index", 0);
                memdelete(this);
            }
        };
        tween->connect("finished", callable_mp(memnew(TweenFinishedCallback(this)), &TweenFinishedCallback::OnTweenCompleted));
    }
    else
    {
        // Update Z Index
        this->set("z_index", 200);

        // Backup current size and position
        emulatorDimension = current_rect;

        // Switch to Fullscreen
        Rect2 target_rect = Rect2(Vector2(0, 0), get_viewport_rect().size);
        Vector2 target_center = target_rect.position + target_rect.size / 2.0;

        // Animate size and position
        tween->tween_property(this, "size", target_rect.size, 0.5f);
        tween2->tween_property(this, "position", target_rect.position, 0.5f);

        // Update Flags
        isFullscreen = true;
    }

    // Start the tween animation
    tween->play();
    tween2->play();
}

// Instance APIs Implementation (Internal)
void JenovaEmulator::CreateDefaultMaterial()
{
    // Define the shader source code
    String shader_code = 
    R"(
        shader_type canvas_item;

        void fragment() 
        {
            vec4 Color = COLOR;
            vec4 Final = Color;
            Final.r = Color.b;
            Final.b = Color.r;
            Final.a = 1.0f;
            COLOR = Final;
        }
     )";

    // Create the Shader
    Ref<Shader> shader = (memnew(Shader));
    shader->set_code(shader_code);

    // Create the ShaderMaterial and assign the shader to it
    Ref<ShaderMaterial> shader_material = (memnew(ShaderMaterial));
    shader_material->set_shader(shader);

    // Assign the ShaderMaterial to the Control (CanvasItem)
    this->set_material(shader_material);
}
void JenovaEmulator::DrawEmulatorViewBorders(Color borderColor, int borderWidth)
{
    // Get Viewport Rect (Control Size)
    Rect2 rect = Rect2(Vector2(0, 0), get_size());

    // Draw Borders
    draw_line(rect.position, rect.position + Vector2(rect.size.x, 0), borderColor, borderWidth);
    draw_line(rect.position + Vector2(0, rect.size.y), rect.position + Vector2(rect.size.x, rect.size.y), borderColor, borderWidth);
    draw_line(rect.position, rect.position + Vector2(0, rect.size.y), borderColor, borderWidth);
    draw_line(rect.position + Vector2(rect.size.x, 0), rect.position + Vector2(rect.size.x, rect.size.y), borderColor, borderWidth);
}
void JenovaEmulator::EnterEmulator()
{
    // Verbose
    jenova::Output("Entering Machine...");

    // Lock the cursor and hide it
    Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);

    // Update Flags
    isInsideMachine = true;

    // Update Active Emulator
    currentActiveEmulator = this;

    // Swap Window Message Handler
    originalWindowHandler = (WNDPROC)SetWindowLongPtr(GetMainGameWindowHandle(), GWLP_WNDPROC, (LONG_PTR)JenovaEmulator::EmulatorWindowProc);

    // Hook Input System
    jenovaEmuMouseHook = SetWindowsHookEx(WH_MOUSE_LL, JenovaEmulator::EmulatorMouseHook, NULL, NULL);
    jenovaEmuKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, JenovaEmulator::EmulatorKeyboardHook, NULL, NULL);
    
    // Create Register new Raw Input Device
    if (!CreateRawInputDevice())
    {
        jenova::Error("Emulator", "Failed to Create New Raw Input Device.");
    }

    // Remove Focus From Game Window
    SetForegroundWindow(GetDesktopWindow());

    // Release Keys
    jenova::vm::JenovaAPI_ExecuteDisplayCommand(jenova::vm::JENOVA_VMCMD_RELEASE_KEYS, jenova::vm::JenovaDataPacket());

    // Update Tooltip
    this->set_tooltip_text("");
}
void JenovaEmulator::ExitEmulator()
{
    // Verbose
    jenova::Output("Exiting Machine...");

    // Release the cursor and make it visible again
    Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);

    // Update Flags
    isInsideMachine = false;

    // Update Active Emulator
    currentActiveEmulator = nullptr;

    // Remove Input Hooks
    UnhookWindowsHookEx(jenovaEmuKeyboardHook);
    jenovaEmuKeyboardHook = nullptr;
    UnhookWindowsHookEx(jenovaEmuMouseHook);
    jenovaEmuMouseHook = nullptr;

    // Revert Window Message Handler
    SetWindowLongPtr(GetMainGameWindowHandle(), GWLP_WNDPROC, (LONG_PTR)originalWindowHandler);
    originalWindowHandler = nullptr;

    // Create Register new Raw Input Device
    if (!DestroyRawInputDevice())
    {
        jenova::Error("Emulator", "Failed to Destroy Raw Input Device.");
    }

    // Recover Focus to Game Window
    SetForegroundWindow(GetMainGameWindowHandle());

    // Update Tooltip
    this->set_tooltip_text("Click On Emulator to Enter Virtual Machine.");
}
bool JenovaEmulator::IsInsideMachine() const
{
    return isInsideMachine;
}