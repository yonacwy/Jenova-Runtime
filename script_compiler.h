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
#include "Jenova.hpp"

// Jenova Script Compiler Definitions
namespace jenova
{
	// Jenova Compiler Interface Definitions
	class IJenovaCompiler
	{
		/*
			Interface Description :
			JenovaCompiler is the base interface of all C++ Compilers Implemented in Jenova System.
			It's responsible for compiling and generating module mapping data. 
		*/

	public:
		virtual bool InitializeCompiler(String compilerInstanceName = "<JenovaCompiler>") = 0;
		virtual bool ReleaseCompiler() = 0;
		virtual String PreprocessScript(Ref<CPPScript> cppScript, const Dictionary& preprocessorSettings) = 0;
		virtual CompileResult CompileScript(const String sourceCode) = 0;
		virtual CompileResult CompileScriptWithCustomSettings(const String sourceCode, const Dictionary& compilerSettings) = 0;
		virtual CompileResult CompileScriptFromFile(const String scriptFilePath) = 0;
		virtual CompileResult CompileScriptFromFileWithCustomSettings(const String scriptFilePath, const Dictionary& compilerSettings) = 0;
		virtual CompileResult CompileScriptModuleContainer(const ScriptModuleContainer& scriptModulesContainer) = 0;
		virtual CompileResult CompileScriptModuleWithCustomSettingsContainer(const ScriptModuleContainer& scriptModulesContainer, const Dictionary& compilerSettings) = 0;
		virtual BuildResult BuildFinalModule(const jenova::ModuleList& scriptModules) = 0;
		virtual BuildResult BuildFinalModuleWithCustomSettings(const jenova::ModuleList& scriptModules, const Dictionary& linkerSettings) = 0;
		virtual bool SetCompilerOption(const StringName& optName, const Variant& optValue) = 0;
		virtual Variant GetCompilerOption(const StringName& optName) const = 0;
		virtual Variant ExecuteCommand(const StringName& commandName, const Dictionary& commandSettings) const = 0;
		virtual CompilerFeatures GetCompilerFeatures() const = 0;
		virtual CompilerModel GetCompilerModel() const = 0;
		virtual bool SolveCompilerSettings(const Dictionary& compilerSettings) = 0;
	};

	// Windows Compilers
	#ifdef TARGET_PLATFORM_WINDOWS

	// Jenova Microsoft Visual C++ Compiler Definitions
	class MicrosoftCompiler : public IJenovaCompiler
	{
	public:
		MicrosoftCompiler();
		~MicrosoftCompiler();

		bool InitializeCompiler(String compilerInstanceName = "<JenovaMSVCCompiler>");
		bool ReleaseCompiler();
		String PreprocessScript(Ref<CPPScript> cppScript, const Dictionary& preprocessorSettings);
		CompileResult CompileScript(const String sourceCode);
		CompileResult CompileScriptWithCustomSettings(const String sourceCode, const Dictionary& compilerSettings);
		CompileResult CompileScriptFromFile(const String scriptFilePath);
		CompileResult CompileScriptFromFileWithCustomSettings(const String scriptFilePath, const Dictionary& compilerSettings);
		CompileResult CompileScriptModuleContainer(const ScriptModuleContainer& scriptModulesContainer);
		CompileResult CompileScriptModuleWithCustomSettingsContainer(const ScriptModuleContainer& scriptModulesContainer, const Dictionary& compilerSettings);
		BuildResult BuildFinalModule(const jenova::ModuleList& scriptModules);
		BuildResult BuildFinalModuleWithCustomSettings(const jenova::ModuleList& scriptModules, const Dictionary& linkerSettings);
		bool SetCompilerOption(const StringName& optName, const Variant& optValue);
		Variant GetCompilerOption(const StringName& optName) const;
		Variant ExecuteCommand(const StringName& commandName, const Dictionary& commandSettings) const;
		CompilerFeatures GetCompilerFeatures() const;
		CompilerModel GetCompilerModel() const;
		bool SolveCompilerSettings(const Dictionary& compilerSettings);

	private:
		Dictionary internalDefaultSettings;
		std::string projectPath;
		std::string compilerBinaryPath;
		std::string linkerBinaryPath;
		std::string includePath;
		std::string libraryPath;
		std::string jenovaSDKPath;
		std::string godotSDKPath;
		std::string jenovaCachePath;
	};

	#endif
}