# Jenova Runtime Build System Script
# Developed by Hamid.Memar (2024-2025)
# Usage : python3 ./Jenova.Builder.py --compiler clang --skip-banner

# Imports
import os
import subprocess
import hashlib
import json
import argparse
from concurrent.futures import ThreadPoolExecutor
from colored import fg, attr

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
    "Libs/liblzma-static-gcc-x86_64.a",
    "Libs/libxml2-static-gcc-x86_64.a",
    "Libs/libdwarf-static-gcc-x86_64.a"
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
    "task_system.cpp",
    "package_manager.cpp",
    "asset_monitor.cpp"
]

# Configuration
compiler = "clang++"
linker = "clang++"
OutputDir = "Linux64"
OutputName = "Jenova.Runtime.Linux64.so"
MapFileName = "Jenova.Runtime.Linux64.map"
CacheDir = f"{OutputDir}/Cache"
CacheDB = f"{CacheDir}/Build.db"

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
    rgb_print("#42f569", banner)
    rgb_print("#37b2fa", ".:: Jenova Build System v1.2 ::.\n")
def compute_md5(file_path):
    hasher = hashlib.md5()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hasher.update(chunk)
    return hasher.hexdigest()
def load_cache():
    if os.path.exists(CacheDB):
        with open(CacheDB, "r") as f:
            return json.load(f)
    return {}
def save_cache(cache):
    with open(CacheDB, "w") as f:
        json.dump(cache, f, indent=4)
def should_compile(source, cache):
    source_path = os.path.abspath(source)
    obj_file = f"{CacheDir}/{source.replace('.cpp', '.o')}"
    if source_path not in cache:
        return True
    cached_entry = cache[source_path]
    current_timestamp = os.path.getmtime(source)
    current_md5 = compute_md5(source)
    if cached_entry["timestamp"] != current_timestamp or cached_entry["md5"] != current_md5:
        return True
    return not os.path.exists(obj_file)
def update_cache_entry(source, cache):
    source_path = os.path.abspath(source)
    cache[source_path] = {
        "timestamp": os.path.getmtime(source),
        "md5": compute_md5(source)
    }
def run_compile_command(command, source_name):
    try:
        rgb_print("#ff2474", f"[ > ] Compiling Source File '{source_name}' Using {compiler}...")
        subprocess.check_call(command, shell=True)
        rgb_print("#38f227", f"[ + ] Source File '{source_name}' Compiled.")
    except subprocess.CalledProcessError as e:
        rgb_print("#e02626", f"[ x ] Source File '{source_name}' Failed to Compile, Error : {e}")
        exit(1)
def run_linker_command(command):
    try:
        rgb_print("#ff2474", f"[ > ] Linking Compiled Object Files using {linker}...")
        subprocess.check_call(command, shell=True)
        rgb_print("#38f227", f"[ + ] All Object Files Linked Successfully.")
    except subprocess.CalledProcessError as e:
        rgb_print("#e02626", f"[ x ] Failed to Link Object Files, Error : {e}")
        exit(1)
def get_compiler_choice():
    global compiler, linker
    rgb_print("#ff2474", "[ ? ] Select Supported Compiler:")
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
        rgb_print("#e02626", "[ x ] Invalid Choice. Defaulting to LLVM Clang (clang++).")

# Entrypoint
if __name__ == "__main__":

    # Create Arguments Parser
    parser = argparse.ArgumentParser(description="Jenova Runtime Build System")
    parser.add_argument('--compiler', type=str, help='Specify the compiler to use.')
    parser.add_argument('--skip-banner', action='store_true', help='Skip printing the banner')

    # Parser Arguments
    args = parser.parse_args()

    # Print Banner
    if not args.skip_banner:
        print_banner()

    # Set Compiler
    if args.compiler:
        if args.compiler == "clang":
            compiler = "clang++"
            linker = "clang++"
        elif args.compiler == "gcc":
            compiler = "g++"
            linker = "g++"
        else:
            compiler = "g++"
            linker = "g++"
            rgb_print("#e02626", "[ x ] Invalid Compiler Input. Defaulting to GNU Compiler Collection (g++).")
    else:
        # Ask for Compiler
        get_compiler_choice()

    # Ensure Required Directories Exist
    rgb_print("#367fff", "[ ^ ] Validating Paths...")
    os.makedirs(OutputDir, exist_ok=True)
    os.makedirs(CacheDir, exist_ok=True)
    with ThreadPoolExecutor() as executor:
        executor.map(os.makedirs, directories)

    # Load Cache
    rgb_print("#367fff", "[ ^ ] Loading Cache...")
    cache = load_cache()

    # Generate Compile Commands
    rgb_print("#367fff", "[ ^ ] Generating Compiler Commands...")
    compile_commands = {}
    object_files = []
    for source in sources:
        object_file = f"{CacheDir}/{source.replace('.cpp', '.o')}"
        object_files.append(object_file)
        compile_command = (
            f"{compiler} -m64 -O3 -fPIC -pipe -w -std=c++20 -pthread "
            f"{' '.join([f'-D{flag}' for flag in flags])} "
            f"{' '.join([f'-I{directory}' for directory in directories])} "
            f"-c {source} -o {object_file}"
        )
        compile_commands[source] = compile_command

    # Compile Source Code
    rgb_print("#367fff", "[ ^ ] Compiling Jenova Runtime Source Code...")
    with ThreadPoolExecutor(max_workers=len(sources)) as executor:
        futures = []
        for source, command in compile_commands.items():
            if should_compile(source, cache):
                futures.append(executor.submit(run_compile_command, command, source))
                update_cache_entry(source, cache)
            else:
                rgb_print("#f59b42", f"[ ! ] Skipping Compilation for {source}, No Changes Detected.")
        for future in futures:
            future.result()

    # Save Cache
    rgb_print("#367fff", "[ ^ ] Saving Cache...")
    save_cache(cache)

    # Generate Linker Command
    rgb_print("#367fff", "[ ^ ] Generating Linker Command...")
    link_command = (
        f"{linker} -shared -fPIC {' '.join(object_files)} -o {OutputDir}/{OutputName} -m64 "
        f"-static-libstdc++ -static-libgcc -pthread {' '.join(libs)} -lssl -lcrypto -ldl -lrt "
        f"-Wl,-Map,{OutputDir}/{MapFileName}"
    )

    # Link Object Files
    rgb_print("#367fff", "[ ^ ] Linking Jenova Runtime Binary...")
    run_linker_command(link_command)

    # Verbose Success
    rgb_print("#03fc6f", "[ + ] Jenova Runtime has been Successfully Compiled and Built.")
