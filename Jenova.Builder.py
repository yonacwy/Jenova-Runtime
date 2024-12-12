# Imports
import os
import subprocess
from colored import fg, bg, attr

# Database
flags = [
"JENOVA_RUNTIME",
"JENOVA_SDK_BUILD",
"TYPED_METHOD_BIND"
]
directories = [
"Libs",
"Libs/GodotSDK",
"Libs/Archive",
"Libs/Curl"
]
libs = [
"Libs/libzlib-static-gcc-x86_64.a",
"Libs/libtcc-static-gcc-x86_64.a",
"Libs/libgodotcpp-static-gcc-x86_64.a",
"Libs/libcurl-static-gcc-x86_64.a",
"Libs/libasmjit-static-gcc-x86_64.a",
"Libs/libarchive-static-gcc-x86_64.a",
"Libs/liblzma-static-gcc-x86_64.a"
]
sources = [
"jenova.cpp",
"jenovaSDK.cpp",
"script_instance_base.cpp",
"script_object.cpp",
"script_instance.cpp",
"script_resource.cpp",
"script_interpreter.cpp",
"script_templates.cpp",
"script_language.cpp",
"script_compiler.cpp",
"script_manager.cpp",
"tiny_profiler.cpp",
"package_manager.cpp",
"asset_monitor.cpp"
]

# Configuration
compiler = "clang++"
linker = "clang++"
OutputDir = "linux64"
OutputName = "Jenova.Runtime.Linux64.so"
CacheDir = f"{OutputDir}/Cache"


# Functions
def rgb_print(hex_color, output):
    hex_color = hex_color.lstrip('#')
    rgb = tuple(int(hex_color[i:i+2], 16) for i in (0, 2, 4))
    ansi_color = 16 + (36 * (rgb[0] // 51) + 6 * (rgb[1] // 51) + (rgb[2] // 51))
    color_code = fg(ansi_color)
    reset_code = attr('reset')
    print(f"{color_code}{output}{reset_code}");
def print_banner():
    banner = """
===========================================================================
       _________   ______ _    _____       ____        _ __    __         
      / / ____/ | / / __ \ |  / /   |     / __ )__  __(_) /___/ /__  _____
 __  / / __/ /  |/ / / / / | / / /| |    / __  / / / / / / __  / _ \/ ___/
/ /_/ / /___/ /|  / /_/ /| |/ / ___ |   / /_/ / /_/ / / / /_/ /  __/ /    
\____/_____/_/ |_/\____/ |___/_/  |_|  /_____/\__,_/_/_/\__,_/\___/_/     
                                                                          
===========================================================================
                        Developed by Hamid.Memar
===========================================================================
    """
    rgb_print("#f59b42", banner)
    rgb_print("#37b2fa", ".:: Jenova Build System v1.0 ::.\n")
def run_compile_command(command, source_name):
    try:
        rgb_print("#ff2474", f"Compiling Source File {source_name} Using {compiler}...")
        subprocess.check_call(command, shell=True)
        rgb_print("#03fc6f", f"Source File {source_name} Compiled.")
    except subprocess.CalledProcessError as e:
        rgb_print("#e02626", f"Source File {source_name} Failed to Compile, Error : {e}")
        exit(1)
def run_linker_command(command):
    try:
        rgb_print("#367fff", f"Linking Object Files using {linker}...")
        subprocess.check_call(command, shell=True)
        rgb_print("#03fc6f", f"All Object Files Linked Successfully.")
    except subprocess.CalledProcessError as e:
        rgb_print("#e02626", f"Failed to Link Object Files, Error : {e}")
        exit(1)
def get_compiler_choice():
    global compiler, linker
    rgb_print("#ff2474", "Select Supported Compiler:")
    print("1. LLVM Clang (clang++) [Recommended]")
    print("2. GNU Compiler Collection (g++)")
    rgb_print("#666666", "Enter the number of your choice (1 or 2): ")
    choice = input("")
    if choice == '1':
        compiler = "clang++"
        linker = "clang++"
    elif choice == '2':
        compiler = "g++"
        linker = "g++"
    else:
        rgb_print("#e02626", "Invalid Choice. Defaulting to LLVM Clang (clang++).")

# Entrypoint
if __name__ == "__main__":

    # Print Banner
    print_banner()

    # Ask for Compiler
    get_compiler_choice()

    # Ensure Required Directories Exist
    rgb_print("#367fff", "Validating Paths...")
    os.makedirs(OutputDir, exist_ok=True)
    os.makedirs(CacheDir, exist_ok=True)
    for directory in directories:
        os.makedirs(directory, exist_ok=True)


    # Generate Compile Commands
    rgb_print("#367fff", "Generating Compiler Commands...")
    compile_commands = {}
    object_files = []
    for source in sources:
        object_file = f"{CacheDir}/{source.replace('.cpp', '.o')}"
        object_files.append(object_file)
        compile_command = (
            f"{compiler} -m64 -O2 -w -std=c++20 -pthread "
            f"{' '.join([f'-D{flag}' for flag in flags])} "
            f"{' '.join([f'-I{directory}' for directory in directories])} "
            f"-c {source} -o {object_file}"
        )
        compile_commands[source] = compile_command

    # Compile Source Code
    rgb_print("#367fff", "Compiling Source Code Files...")
    for source, command in compile_commands.items():
        run_compile_command(command, source)

    # Generate Linker Command
    rgb_print("#367fff", "Generating Linker Command...")
    link_command = (
        f"{linker} -shared {' '.join(object_files)} -o {OutputDir}/{OutputName} -m64 "
        f"-static-libstdc++ -static-libgcc -static -pthread {' '.join(libs)} -lssl -lcrypto -ldl -lrt"
    )

    # Link Object Files
    rgb_print("#367fff", "Linking Object Files...")
    run_linker_command(link_command)

    # Verbose Success
    rgb_print("#03fc6f", "Jenova Runtime has been Successfully Compiled and Built.")
