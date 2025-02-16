# Jenova Runtime Build System Script
# Developed by Hamid.Memar (2024-2025)
# Usage : python3 ./Jenova.Builder.py --compiler clang --skip-banner

# Imports
import os
import sys
import platform
import shutil
import subprocess
import hashlib
import json
import time
import argparse
from concurrent.futures import ThreadPoolExecutor
from colored import fg, attr

# Shared Database
flags = [
    "NDEBUG",
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
    "asset_monitor.cpp",
    "gdextension_exporter.cpp"
]

# Global Options
skip_deps = False
skip_cache = False

# Global Functions
def rgb_print(hex_color, output,):
    hex_color = hex_color.lstrip('#')
    rgb = tuple(int(hex_color[i:i+2], 16) for i in (0, 2, 4))
    ansi_color = 16 + (36 * (rgb[0] // 51) + 6 * (rgb[1] // 51) + (rgb[2] // 51))
    color_code = fg(ansi_color)
    reset_code = attr('reset')
    print(f"{color_code}{output}{reset_code}")
def format_duration(seconds):
    milliseconds = int((seconds - int(seconds)) * 1000)
    seconds = int(seconds)
    minutes, seconds = divmod(seconds, 60)
    hours, minutes = divmod(minutes, 60)
    return f"{hours}h {minutes}m {seconds}s {milliseconds}ms"
def guard_path(path):
    return '\"' + path + '\"'
def get_os_name():
    system_name = platform.system()
    architecture = "x64" if platform.machine() in ["AMD64", "x86_64"] else platform.machine()
    if system_name == "Windows":
        import sys
        win_ver = sys.getwindowsversion()
        if win_ver.build >= 26000: return f"Microsoft Windows 12 {architecture}"
        if win_ver.build >= 22000: return f"Microsoft Windows 11 {architecture}"
        if win_ver.build >= 19000: return f"Microsoft Windows 10 {architecture}"
        if win_ver.build >= 9600: return f"Microsoft Windows 8.1 {architecture}"
        if win_ver.build >= 9200: return f"Microsoft Windows 8 {architecture}"
        if win_ver.build >= 7600: return f"Microsoft Windows 7 {architecture}"
        return f"Microsoft Windows {platform.win32_ver()[0]} {architecture}"
    elif system_name == "Linux":
        try:
            import distro
            return f"{distro.name(pretty=True) or distro.id().capitalize() or 'Linux'} {architecture}"
        except ImportError:
            return f"{system_name} {platform.release()} {architecture}"
    return f"{system_name} {platform.release()} {architecture}"
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
    rgb_print("#2942ff", f".:: Jenova Build System v1.9 ::.\n")
def get_compiler_choice():
    global compiler, linker
    rgb_print("#ff2474", "[ ? ] Select Supported Compiler :\n")
    rgb_print("#4eb8ff", "----------------- Windows x64 -----------------")
    print("1. Microsoft Visual C++ (win-msvc) [Recommended]")
    print("2. Microsoft Visual C++ LLVM (win-clangcl)")
    print("3. LLVM Clang (win-clang)")
    print("4. Minimalist GNU for Windows (win-gcc)")
    rgb_print("#fff06e", "\n------------------ Linux x64 ------------------")
    print("5. LLVM Clang (linux-clang) [Recommended]")
    print("6. GNU Compiler Collection (linux-gcc)")
    rgb_print("#ff6666", f"\nYou are currently running {get_os_name()} using ({os.cpu_count()} Cores)") 
    rgb_print("#666666", "\nEnter Build Mode: ")
    choice = input("")
    if choice == '1':
        build_windows("cl.exe", "link.exe", "win-msvc", "Microsoft Visual C++")
    elif choice == '2':
        build_windows("clang-cl.exe", "lld-link.exe", "win-clangcl", "Microsoft Visual C++ LLVM")
    elif choice == '3':
        build_windows("clang++.exe", "clang++.exe", "win-clang", "LLVM Clang")
    elif choice == '4':
        build_windows("g++.exe", "g++.exe", "win-gcc", "Minimalist GNU")
    elif choice == '5':
        build_linux("g++", "g++", "linux-gcc", "GNU Compiler Collection")
    elif choice == '6':
        build_linux("clang++", "clang++", "linux-clang", "LLVM Clang")
    else:
        rgb_print("#e02626", "[ x ] Error : Invalid Choice.")
        exit(-1)

# Generic Build Functions
def compute_md5(file_path):
    hasher = hashlib.md5()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hasher.update(chunk)
    return hasher.hexdigest()
def load_cache(cacheDB):
    if skip_cache: return {}
    if os.path.exists(cacheDB):
        with open(cacheDB, "r") as f:
            return json.load(f)
    return {}
def save_cache(cache, cacheDB):
    with open(cacheDB, "w") as f:
        json.dump(cache, f, indent=4)
def should_compile(source, cache):
    source_path = os.path.abspath(source)
    if source_path not in cache:
        return True
    cached_entry = cache[source_path]
    current_timestamp = os.path.getmtime(source)
    current_md5 = compute_md5(source)
    return cached_entry["timestamp"] != current_timestamp or cached_entry["md5"] != current_md5
def update_cache_entry(source, cache):
    source_path = os.path.abspath(source)
    cache[source_path] = {
        "timestamp": os.path.getmtime(source),
        "md5": compute_md5(source)
    }
def run_compile_command(command, source_name, compiler):
    try:
        rgb_print("#ff2474", f"[ > ] Compiling Source File '{os.path.basename(source_name)}' Using {compiler}...")
        subprocess.check_call(command, shell=True)
        rgb_print("#38f227", f"[ + ] Source File '{os.path.basename(source_name)}' Compiled.")
    except subprocess.CalledProcessError as e:
        rgb_print("#e02626", f"[ x ] Source File '{os.path.basename(source_name)}' Failed to Compile, Error : {e}")
        exit(1)
def run_linker_command(command, linker):
    try:
        rgb_print("#ff2474", f"[ > ] Linking Compiled Object Files using {linker}...")
        subprocess.check_call(command, shell=True)
        rgb_print("#38f227", f"[ + ] All Object Files Linked Successfully.")
    except subprocess.CalledProcessError as e:
        rgb_print("#e02626", f"[ x ] Failed to Link Object Files, Error : {e}")
        exit(1)    
def build_with_ninja(buildPath):
    subprocess.run(["ninja.exe", "-C", buildPath, "-j", f"{os.cpu_count()}"], check=True)

# Linux Build Functions
def initialize_toolchain_linux():
    return
def build_dependencies_linux(compilerBinary, linkerBinary, buildMode, cacheDir):
    return
def build_linux(compilerBinary, linkerBinary, buildMode, buildSystem):

    # Verbose Build
    rgb_print("#fff06e", f"[ > ] Building Jenova Runtime for Linux using {buildSystem} Build System")

    # Dependencies
    libs = [
        "Libs/libzlib-static-x86_64.a",
        "Libs/libtcc-static-x86_64.a",
        "Libs/libgodotcpp-static-x86_64.a",
        "Libs/libcurl-static-x86_64.a",
        "Libs/libasmjit-static-x86_64.a",
        "Libs/libarchive-static-x86_64.a",
        "Libs/liblzma-static-x86_64.a",
        "Libs/libxml2-static-x86_64.a"
    ]

    # Configuration
    compiler = compilerBinary
    linker = linkerBinary
    outputDir = "Linux64"
    outputName = "Jenova.Runtime.Linux64.so"
    mapFileName = "Jenova.Runtime.Linux64.map"
    cacheDir = f"{outputDir}/Cache"
    CacheDB = f"{cacheDir}/Build.db"

    # Ensure Required Directories Exist
    rgb_print("#367fff", "[ ^ ] Validating Paths...")
    os.makedirs(outputDir, exist_ok=True)
    os.makedirs(cacheDir, exist_ok=True)

    # Load Cache
    rgb_print("#367fff", "[ ^ ] Loading Cache...")
    cache = load_cache(CacheDB)

    # Generate Compile Commands
    rgb_print("#367fff", "[ ^ ] Generating Compiler Commands...")
    compile_commands = {}
    object_files = []
    for source in sources:
        object_file = f"{cacheDir}/{source.replace('.cpp', '.o')}"
        object_files.append(object_file)
        compile_command = (
            f"{compiler} -m64 -O3 -fPIC -pipe -w -std=c++20 -pthread -fexceptions "
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
                futures.append(executor.submit(run_compile_command, command, source, compiler))
                update_cache_entry(source, cache)
            else:
                rgb_print("#f59b42", f"[ ! ] Skipping Compilation for {os.path.basename(source)}, No Changes Detected.")
        for future in futures: future.result()

    # Save Cache
    rgb_print("#367fff", "[ ^ ] Saving Cache...")
    save_cache(cache, CacheDB)

    # Generate Linker Command
    rgb_print("#367fff", "[ ^ ] Generating Linker Command...")
    link_command = (
        f"{linker} -shared -fPIC {' '.join(object_files)} -o {outputDir}/{outputName} -m64 "
        f"-static-libstdc++ -static-libgcc {' '.join(libs)} -lssl -lcrypto -ldl -lrt "
        f"-Wl,-Map,{outputDir}/{mapFileName}"
    )

    # Link Object Files
    rgb_print("#367fff", "[ ^ ] Linking Jenova Runtime Binary...")
    run_linker_command(link_command, linker)

# Windows Build Functions
def initialize_toolchain_windows():

    # Todo : If Toolchain doesn't exist download it

    # Configuring Toolchain
    rgb_print("#367fff", "[ ^ ] Configuring Toolchain...")
    toolchain_path = os.path.abspath("./Toolchain/bin")
    current_path = os.environ.get("PATH", "")
    if toolchain_path not in current_path: os.environ["PATH"] = f"{toolchain_path};{current_path}"
    
    # Set Toolchain Path
    os.environ['TOOLCHAIN_PATH'] = os.path.abspath("./Toolchain")

    # Verbose
    rgb_print("#38f227",f"[ + ] Toolchain ({toolchain_path}) has been Configured Successfully.")
def build_dependencies_windows(buildMode, cacheDir):

    # Create Library
    os.makedirs("Libs", exist_ok=True)
    shutil.copytree("./Dependencies/libjenova", "./Libs", dirs_exist_ok=True)

    # Build Dependencies for MSVC & LLVM
    if buildMode == "win-msvc" or buildMode == "win-clangcl":
        
        # Set Compiler/Linker Configuration
        os.environ['CC'] = 'cl.exe' if buildMode == "win-msvc" else 'clang-cl.exe'
        os.environ['CXX'] = 'cl.exe' if buildMode == "win-msvc" else 'clang-cl.exe'
        os.environ['CLFLAGS'] = '/MT /std:c17 /std:c++20 '
        os.environ['LDFLAGS'] = '/LTCG:incremental'
        os.environ["INCLUDE"] = f"{os.environ['TOOLCHAIN_PATH']}/x86_64-msvc/Include"
        os.environ["LIB"] = f"{os.environ['TOOLCHAIN_PATH']}/x86_64-msvc/Lib"
        os.environ["LIBPATH"] = f"{os.environ['TOOLCHAIN_PATH']}/x86_64-msvc/Lib"     

        # Set Specific Build System Configuration
        if buildMode == "win-clangcl":
            os.environ['CLFLAGS'] += '-w -Weverything'
            os.environ['CFLAGS'] = '-w -Weverything'
            os.environ['CXXFLAGS'] = '-w -Weverything'

        # Build AsmJIT
        if not os.path.exists("./Libs/libasmjit-static-x86_64.lib"):
            buildPath = cacheDir + "/Dependencies/asmjit"
            if os.path.exists(buildPath): shutil.rmtree(buildPath)
            subprocess.run([
                "cmake.exe",
                "-S", "./Dependencies/libasmjit",
                "-B", buildPath,
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                "-DBUILD_SHARED_LIBS=OFF",
                "-DASMJIT_STATIC=ON",
                "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded",
                f"-DCMAKE_CXX_FLAGS={os.environ['CLFLAGS']}"
            ], check=True)
            build_with_ninja(buildPath)
            shutil.copyfile(buildPath + "/asmjit.lib", "./Libs/libasmjit-static-x86_64.lib")
            rgb_print("#38f227", "[ + ] Jenova Runtime Dependency 'AsmJIT' Compiled Successfully.")
            
        # Build Curl
        if not os.path.exists("./Libs/libcurl-static-x86_64.lib"):
            buildPath = cacheDir + "/Dependencies/curl"
            if os.path.exists(buildPath): shutil.rmtree(buildPath)
            subprocess.run([
                "cmake.exe",
                "-S", "./Dependencies/libcurl",
                "-B", buildPath,
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                "-DBUILD_SHARED_LIBS=OFF",
                "-DCURL_STATICLIB=ON",
                "-DCURL_USE_LIBPSL=OFF",
                "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded",
                f"-DCMAKE_CXX_FLAGS={os.environ['CLFLAGS']}"
            ], check=True)

            build_with_ninja(buildPath)
            shutil.copyfile(buildPath + "/lib/libcurl.lib", "./Libs/libcurl-static-x86_64.lib")
            rgb_print("#38f227", "[ + ] Jenova Runtime Dependency 'Curl' Compiled Successfully.")

        # Build LZMA
        if not os.path.exists("./Libs/liblzma-static-x86_64.lib"):
            buildPath = cacheDir + "/Dependencies/lzma"
            if os.path.exists(buildPath): shutil.rmtree(buildPath)
            subprocess.run([
                "cmake.exe",
                "-S", "./Dependencies/liblzma",
                "-B", buildPath,
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                "-DBUILD_SHARED_LIBS=OFF",
                "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded",
                f"-DCMAKE_CXX_FLAGS={os.environ['CLFLAGS']}"
            ], check=True)
            build_with_ninja(buildPath)
            shutil.copyfile(buildPath + "/lzma.lib", "./Libs/liblzma-static-x86_64.lib")
            rgb_print("#38f227", "[ + ] Jenova Runtime Dependency 'LZMA' Compiled Successfully.")

        # Build Archive
        if not os.path.exists("./Libs/libarchive-static-x86_64.lib"):
            buildPath = cacheDir + "/Dependencies/archive"
            if os.path.exists(buildPath): shutil.rmtree(buildPath)
            subprocess.run([
                "cmake.exe",
                "-S", "./Dependencies/libarchive",
                "-B", buildPath,
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                "-DBUILD_SHARED_LIBS=OFF",
                "-DENABLE_LZMA=ON",
                "-DENABLE_TEST=OFF",
                "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded",
                f"-DCMAKE_CXX_FLAGS={os.environ['CLFLAGS']}"
            ], check=True)
            build_with_ninja(buildPath)
            shutil.copyfile(buildPath + "/libarchive/archive.lib", "./Libs/libarchive-static-x86_64.lib")
            rgb_print("#38f227", "[ + ] Jenova Runtime Dependency 'Archive' Compiled Successfully.")

        # Build XML2
        if not os.path.exists("./Libs/libxml2-static-x86_64.lib"):
            buildPath = cacheDir + "/Dependencies/xml2"
            if os.path.exists(buildPath): shutil.rmtree(buildPath)
            subprocess.run([
                "cmake.exe",
                "-S", "./Dependencies/libxml2",
                "-B", buildPath,
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                "-DBUILD_SHARED_LIBS=OFF",
                "-DLIBXML2_WITH_ICONV=OFF",
                "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded",
                f"-DCMAKE_CXX_FLAGS={os.environ['CLFLAGS']}"
            ], check=True)
            build_with_ninja(buildPath)
            shutil.copyfile(buildPath + "/libxml2s.lib", "./Libs/libxml2-static-x86_64.lib")
            rgb_print("#38f227", "[ + ] Jenova Runtime Dependency 'XML2' Compiled Successfully.")

        # Build ZLIB
        if not os.path.exists("./Libs/libzlib-static-x86_64.lib"):
            buildPath = cacheDir + "/Dependencies/zlib"
            if os.path.exists(buildPath): shutil.rmtree(buildPath)
            os.makedirs(buildPath)
            subprocess.run([
                "cmake.exe",
                "-S", "./Dependencies/libzlib",
                "-B", buildPath,
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                "-DBUILD_SHARED_LIBS=OFF",
                "-DZLIB_BUILD_TESTING=OFF",
                "-DZLIB_BUILD_STATIC=ON",
                "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded",
                f"-DCMAKE_CXX_FLAGS={os.environ['CLFLAGS']}"
            ], check=True)
            build_with_ninja(buildPath)
            shutil.copyfile(buildPath + "/zs.lib", "./Libs/libzlib-static-x86_64.lib")
            rgb_print("#38f227", "[ + ] Jenova Runtime Dependency 'ZLIB' Compiled Successfully.")

        # Build PThread
        if not os.path.exists("./Libs/libpthread-static-x86_64.lib"):
            buildPath = cacheDir + "/Dependencies/pthread"
            if os.path.exists(buildPath): shutil.rmtree(buildPath)
            subprocess.run([
                "cmake.exe",
                "-S", "./Dependencies/libpthread",
                "-B", buildPath,
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                "-DBUILD_SHARED_LIBS=OFF",
                "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded",
                f"-DCMAKE_CXX_FLAGS={os.environ['CLFLAGS']}"
            ], check=True)
            build_with_ninja(buildPath)
            shutil.copyfile(buildPath + "/pthreadVSE3.lib", "./Libs/libpthread-static-x86_64.lib")
            rgb_print("#38f227", "[ + ] Jenova Runtime Dependency 'PThread' Compiled Successfully.")
         
        # Build TinyCC
        if not os.path.exists("./Libs/libtcc-static-x86_64.lib"):
            buildPath = cacheDir + "/Dependencies/tinycc"
            if os.path.exists(buildPath): shutil.rmtree(buildPath)
            subprocess.run([
                "cmake.exe",
                "-S", "./Dependencies/libtinycc",
                "-B", buildPath,
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                "-DBUILD_SHARED_LIBS=OFF",
                "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded"
            ], check=True)
            build_with_ninja(buildPath)
            shutil.copyfile(buildPath + "/libtcc.lib", "./Libs/libtcc-static-x86_64.lib")
            rgb_print("#38f227", "[ + ] Jenova Runtime Dependency 'TinyCC' Compiled Successfully.")
     
        # Build GodotSDK
        if not os.path.exists("./Libs/libgodotcpp-static-x86_64.lib"):
            os.environ["GODOT_MSVC_STATIC_RUNTIME"] = "true"
            buildPath = cacheDir + "/Dependencies/godotcpp"
            sdkPath = "./Libs/GodotSDK"
            if os.path.exists(buildPath): shutil.rmtree(buildPath)
            subprocess.run([
                "cmake.exe",
                "-S", "./Dependencies/libgodot",
                "-B", buildPath,
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                "-DBUILD_SHARED_LIBS=OFF",
                "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded",
                f"-DCMAKE_CXX_FLAGS={os.environ['CLFLAGS']}"
            ], check=True)
            build_with_ninja(buildPath)
            shutil.copyfile(buildPath + "/bin/godot-cpp.windows.minsizerel.64.lib", "./Libs/libgodotcpp-static-x86_64.lib")
            if os.path.exists(sdkPath): shutil.rmtree(sdkPath)
            os.makedirs(sdkPath, exist_ok=True)
            shutil.copyfile("./Dependencies/libgodot/gdextension/gdextension_interface.h", sdkPath + "/gdextension_interface.h")
            shutil.copytree("./Dependencies/libgodot/include", sdkPath, dirs_exist_ok=True)
            shutil.copytree(buildPath + "/gen/include", sdkPath, dirs_exist_ok=True)
            rgb_print("#38f227", "[ + ] Jenova Runtime Dependency 'GodotSDK' Compiled Successfully.")

    # Build Dependencies for Clang & GNU
    if buildMode == "win-clang" or buildMode == "win-gcc":

        # Set Compiler/Linker Configuration
        os.environ['CC'] = 'clang.exe' if buildMode == "win-clang" else 'gcc.exe'
        os.environ['CXX'] = 'clang++.exe' if buildMode == "win-clang" else 'g++.exe'
        os.environ['CFLAGS'] = '-static -m64'
        os.environ['CXXFLAGS'] = '-static -std=c++20 -m64'
        os.environ['LDFLAGS'] = '-static -static-libgcc -static-libstdc++ -pthread -m64'
        os.environ['C_COMPILER'] = 'clang' if buildMode == "win-clang" else 'gcc'
        os.environ['CXX_COMPILER'] = 'clang++' if buildMode == "win-clang" else 'g++'

        # Set Specific Build System Configuration
        if buildMode == "win-clang":
            os.environ['CFLAGS'] += ' -Qunused-arguments'
            os.environ['CXXFLAGS'] += ' -Qunused-arguments -femulated-tls'

        # Build AsmJIT
        if not os.path.exists("./Libs/libasmjit-static-x86_64.a"):
            buildPath = cacheDir + "/Dependencies/asmjit"
            if os.path.exists(buildPath): shutil.rmtree(buildPath)
            subprocess.run([
                "cmake.exe",
                "-S", "./Dependencies/libasmjit",
                "-B", buildPath,
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                "-DBUILD_SHARED_LIBS=OFF",
                "-DASMJIT_STATIC=ON"
            ], check=True)
            build_with_ninja(buildPath)
            shutil.copyfile(buildPath + "/libasmjit.a", "./Libs/libasmjit-static-x86_64.a")
            rgb_print("#38f227", "[ + ] Jenova Runtime Dependency 'AsmJIT' Compiled Successfully.")

        # Build Curl
        if not os.path.exists("./Libs/libcurl-static-x86_64.a"):
            buildPath = cacheDir + "/Dependencies/curl"
            if os.path.exists(buildPath): shutil.rmtree(buildPath)
            subprocess.run([
                "cmake.exe",
                "-S", "./Dependencies/libcurl",
                "-B", buildPath,
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                "-DBUILD_SHARED_LIBS=OFF",
                "-DCURL_STATICLIB=ON",
                "-DCURL_USE_LIBPSL=OFF"
            ], check=True)
            build_with_ninja(buildPath)
            shutil.copyfile(buildPath + "/lib/libcurl.a", "./Libs/libcurl-static-x86_64.a")
            rgb_print("#38f227", "[ + ] Jenova Runtime Dependency 'Curl' Compiled Successfully.")

        # Build LZMA
        if not os.path.exists("./Libs/liblzma-static-x86_64.a"):
            buildPath = cacheDir + "/Dependencies/lzma"
            if os.path.exists(buildPath): shutil.rmtree(buildPath)
            subprocess.run([
                "cmake",
                "-S", "./Dependencies/liblzma",
                "-B", buildPath,
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                "-DBUILD_SHARED_LIBS=OFF"
            ], check=True)
            build_with_ninja(buildPath)
            shutil.copyfile(buildPath + "/liblzma.a", "./Libs/liblzma-static-x86_64.a")
            rgb_print("#38f227", "[ + ] Jenova Runtime Dependency 'LZMA' Compiled Successfully.")

        # Build Archive
        if not os.path.exists("./Libs/libarchive-static-x86_64.a"):
            buildPath = cacheDir + "/Dependencies/archive"
            if os.path.exists(buildPath): shutil.rmtree(buildPath)
            subprocess.run([
                "cmake.exe",
                "-S", "./Dependencies/libarchive",
                "-B", buildPath,
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                "-DBUILD_SHARED_LIBS=OFF",
                "-DENABLE_LZMA=ON",
                "-DENABLE_TEST=OFF"
            ], check=True)
            build_with_ninja(buildPath)
            shutil.copyfile(buildPath + "/libarchive/libarchive.a", "./Libs/libarchive-static-x86_64.a")
            rgb_print("#38f227", "[ + ] Jenova Runtime Dependency 'Archive' Compiled Successfully.")

        # Build XML2
        if not os.path.exists("./Libs/libxml2-static-x86_64.a"):
            buildPath = cacheDir + "/Dependencies/xml2"
            if os.path.exists(buildPath): shutil.rmtree(buildPath)
            subprocess.run([
                "cmake",
                "-S", "./Dependencies/libxml2",
                "-B", buildPath,
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                "-DBUILD_SHARED_LIBS=OFF",
                "-DLIBXML2_WITH_ICONV=OFF"
            ], check=True)
            build_with_ninja(buildPath)
            shutil.copyfile(buildPath + "/libxml2.a", "./Libs/libxml2-static-x86_64.a")
            rgb_print("#38f227", "[ + ] Jenova Runtime Dependency 'XML2' Compiled Successfully.")

        # Build ZLIB
        if not os.path.exists("./Libs/libzlib-static-x86_64.a"):
            buildPath = cacheDir + "/Dependencies/zlib"
            if os.path.exists(buildPath): shutil.rmtree(buildPath)
            os.makedirs(buildPath)
            subprocess.run([
                "cmake.exe",
                "-S", "./Dependencies/libzlib",
                "-B", buildPath,
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                "-DBUILD_SHARED_LIBS=OFF",
                "-DZLIB_BUILD_TESTING=OFF",
                "-DZLIB_BUILD_STATIC=ON"
            ], check=True)
            build_with_ninja(buildPath)
            shutil.copyfile(buildPath + "/libzs.a", "./Libs/libzlib-static-x86_64.a")
            rgb_print("#38f227", "[ + ] Jenova Runtime Dependency 'ZLIB' Compiled Successfully.")

        # Build TinyCC
        if not os.path.exists("./Libs/libtcc-static-x86_64.a"):
            buildPath = cacheDir + "/Dependencies/tinycc"
            if os.path.exists(buildPath): shutil.rmtree(buildPath)
            os.makedirs(buildPath, exist_ok=True)
            shutil.copytree("./Dependencies/libtinycc", buildPath, dirs_exist_ok=True)
            subprocess.run([
                "busybox.exe",
                "sh",
                "./configure",
                f"--cc={os.environ['C_COMPILER']}",
                "--config-mingw32",
                "--enable-static",
                "--cpu=x86_64"
            ], cwd=buildPath, check=True)
            subprocess.run(["mingw32-make.exe"], cwd=buildPath, env=os.environ.copy(), check=True)
            shutil.copyfile(os.path.join(buildPath, "libtcc.a"), "./Libs/libtcc-static-x86_64.a")
            rgb_print("#03fc6f", "[ + ] Jenova Runtime Dependency 'TinyCC' Compiled Successfully.")

        # Build GodotSDK
        if not os.path.exists("./Libs/libgodotcpp-static-x86_64.a"):
            buildPath = cacheDir + "/Dependencies/godotcpp"
            sdkPath = "./Libs/GodotSDK"
            if os.path.exists(buildPath): shutil.rmtree(buildPath)
            subprocess.run([
                "cmake.exe",
                "-S", "./Dependencies/libgodot",
                "-B", buildPath,
                "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=MinSizeRel",
                "-DBUILD_SHARED_LIBS=OFF"
            ], check=True)
            build_with_ninja(buildPath)
            shutil.copyfile(buildPath + "/bin/libgodot-cpp.windows.minsizerel.64.a", "./Libs/libgodotcpp-static-x86_64.a")
            if os.path.exists(sdkPath): shutil.rmtree(sdkPath)
            os.makedirs(sdkPath, exist_ok=True)
            shutil.copyfile("./Dependencies/libgodot/gdextension/gdextension_interface.h", sdkPath + "/gdextension_interface.h")
            shutil.copytree("./Dependencies/libgodot/include", sdkPath, dirs_exist_ok=True)
            shutil.copytree(buildPath + "/gen/include", sdkPath, dirs_exist_ok=True)
            rgb_print("#38f227", "[ + ] Jenova Runtime Dependency 'GodotSDK' Compiled Successfully.")
def build_windows(compilerBinary, linkerBinary, buildMode, buildSystem):

    # Verbose Build
    rgb_print("#4eb8ff", f"[ > ] Building Jenova Runtime for Windows using {buildSystem} Build System...")

    # Configuration
    compiler = compilerBinary
    linker = linkerBinary
    outputDir = "Win64"
    outputName = "Jenova.Runtime.Win64.dll"
    symbolFileName = "Jenova.Runtime.Win64.pdb"
    mapFileName = "Jenova.Runtime.Win64.map"
    cacheDir = f"{outputDir}/Cache"
    sdkDir = f"{outputDir}/JenovaSDK"
    CacheDB = f"{cacheDir}/Build.db"

    # Ensure Required Directories Exist
    rgb_print("#367fff", "[ ^ ] Validating Paths...")
    os.makedirs(outputDir, exist_ok=True)
    os.makedirs(sdkDir, exist_ok=True) 
    os.makedirs(cacheDir, exist_ok=True)

    # Initialize Toolchain
    initialize_toolchain_windows()

    # Build Dependencies
    if not skip_deps:
        rgb_print("#367fff", "[ ^ ] Building Dependencies...")
        build_dependencies_windows(buildMode, cacheDir)
    else:
        rgb_print("#367fff", "[ ^ ] Building Dependencies Skipped by User.")

    # Build Runtime Using MSVC & LLVM
    if buildMode == "win-msvc" or buildMode == "win-clangcl":

        # Verbose Build Start
        rgb_print("#367fff", "[ ^ ] Building Fully-Featured Jenova Runtime...")

        # Set Compiler/Linker Configuration
        os.environ["INCLUDE"] = f"{os.environ['TOOLCHAIN_PATH']}/x86_64-msvc/Include"
        os.environ["LIB"] = f"{os.environ['TOOLCHAIN_PATH']}/x86_64-msvc/Lib"
        os.environ["LIBPATH"] = f"{os.environ['TOOLCHAIN_PATH']}/x86_64-msvc/Lib"    

        # Dependencies
        libs = [
            "Libs/libgodotcpp-static-x86_64.lib",
            "Libs/libasmjit-static-x86_64.lib",
            "Libs/libzlib-static-x86_64.lib",
            "Libs/libarchive-static-x86_64.lib",
            "Libs/libcurl-static-x86_64.lib",
            "Libs/libtcc-static-x86_64.lib",
            "Libs/libpthread-static-x86_64.lib",
            "comctl32.lib",
            "Dbghelp.lib",
            "Ws2_32.lib",
            "Wldap32.lib"
        ]

        # Add Extra Directories
        directories.append("Libs/Threading")

        # Add Compatibility Sources
        if buildMode == "win-clangcl":
            sources.append("Libs/Misc/clangcl-compatibility.cpp")

        # Load Cache
        rgb_print("#367fff", "[ ^ ] Loading Cache...")
        cache = load_cache(CacheDB)

        # Determine Changed Sources
        changedSources = [source for source in sources if should_compile(source, cache)]

        # Check If Compile Required
        if changedSources:

            # Generate Compiler Command
            rgb_print("#367fff", "[ ^ ] Generating Compiler Command...")
            if buildMode == "win-msvc":
                compile_command = (
                    f"{compiler} /c /MP /permissive /ifcOutput:\"{cacheDir}/\" /GS /GL /W0 /Gy /Zc:wchar_t "
                    f"{' '.join([f'/I{guard_path(directory)}' for directory in directories])} "
                    f"/Zi /Gm- /O2 /sdl /Fd\"{cacheDir}/vc143.pdb\" /Zc:inline /fp:precise "
                    f"{' '.join([f'/D{flag}' for flag in flags])} "
                    f"/errorReport:prompt /WX- /Zc:forScope /GR /Gd /Oi /MT /openmp /std:c++20 /FC "
                    f"/EHa /nologo /Fo\"{cacheDir}/\" /permissive /diagnostics:column "
                    f"{' '.join(changedSources)}"
                )
            if buildMode == "win-clangcl":
                compile_commands = {}
                for source in sources:
                    object_file = f"{cacheDir}/{source.replace('.cpp', '.obj')}"
                    compile_command = (
                        f"{compiler} /c /permissive /GS /GL /W0 /Gy /Zc:wchar_t "
                        f"{' '.join([f'/I{guard_path(directory)}' for directory in directories])} "
                        f"/Zi /Gm- /O2 /sdl /Fd\"{cacheDir}/vc143.pdb\" /Zc:inline /fp:precise "
                        f"{' '.join([f'/D{flag}' for flag in flags])} "
                        f"/errorReport:prompt /WX- /Zc:forScope /GR /Gd /Oi /MT /openmp /std:c++20 /FC "
                        f"/EHa /nologo /Fo\"{cacheDir}/\" /permissive /diagnostics:column -Xclang -Wftime-trace "
                        f"{source}"
                    )
                    compile_commands[source] = compile_command

            # Compile Source Code
            rgb_print("#367fff", "[ ^ ] Compiling Jenova Runtime Source Code...")
            if buildMode == "win-msvc":
                run_compile_command(compile_command, "MSVC Compilation", compiler)
            if buildMode == "win-clangcl":
                with ThreadPoolExecutor(max_workers=len(sources)) as executor:
                    futures = []
                    for source, command in compile_commands.items():
                        if should_compile(source, cache):
                            futures.append(executor.submit(run_compile_command, command, source, compiler))
                            update_cache_entry(source, cache)
                        else:
                            rgb_print("#f59b42", f"[ ! ] Skipping Compilation for {os.path.basename(source)}, No Changes Detected.")
                    for future in futures: future.result()

            # Update Cache
            for source in sources: update_cache_entry(source, cache)

            # Save Cache
            rgb_print("#367fff", "[ ^ ] Saving Cache...")
            save_cache(cache, CacheDB)

            # Generate Object Files List
            object_files = []
            for source in sources:
                object_file = f"{cacheDir}/{os.path.basename(source).replace('.cpp', '.obj')}"
                object_files.append(object_file)

            # Generate Linker Command
            rgb_print("#367fff", "[ ^ ] Generating Linker Command...")
            link_command = (
                f"{linker} /OUT:\"{outputDir}/{outputName}\" /MANIFEST /LTCG:incremental /NXCOMPAT "
                f"/PDB:\"{outputDir}/{symbolFileName}\" /DYNAMICBASE "
                f"{' '.join(object_files)} "
                f"{' '.join(libs)} "
                f"comctl32.lib Dbghelp.lib Ws2_32.lib Wldap32.lib kernel32.lib user32.lib "
                f"gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib "
                f"msvcrt.lib vcruntime.lib ucrt.lib libcpmt.lib libcmt.lib "
                f"uuid.lib odbc32.lib odbccp32.lib delayimp.lib /IMPLIB:\"{sdkDir}/Jenova.SDK.x64.lib\" "
                f"/DEBUG:FULL /DLL /MACHINE:X64 /OPT:REF /INCREMENTAL:NO "
                f"/SUBSYSTEM:CONSOLE "
                f"/MANIFESTUAC:\"level='asInvoker' uiAccess='false'\" "
                f"/ManifestFile:\"{cacheDir}/{outputName}.intermediate.manifest\" "
                f"/LTCGOUT:\"{cacheDir}/{outputName}.iobj\" /OPT:ICF /ERRORREPORT:PROMPT "
                f"/ILK:\"{cacheDir}/{outputName}.ilk\" /NOLOGO "
                f"/DELAYLOAD:\"dbghelp.dll\" /DELAYLOAD:\"Wldap32.dll\" /DELAYLOAD:\"bcrypt.dll\" "
                f"/TLBID:1 /IGNORE:4098 /IGNORE:4286 /IGNORE:4099 "
            )

            # Link Object Files
            rgb_print("#367fff", "[ ^ ] Linking Jenova Runtime Binary...")
            run_linker_command(link_command, linker)
        else:
            rgb_print("#f59b42", "[ ! ] No Changes Detected, Skipping Compilation.")

        # Prepare Release
        shutil.copy2("./JenovaSDK.h", f"{sdkDir}/JenovaSDK.h")
        shutil.copy2("./Jenova.Runtime.gdextension", f"{outputDir}/Jenova.Runtime.gdextension")  
    
    # Build Runtime Using Clang & GNU
    if buildMode == "win-clang" or buildMode == "win-gcc":
        
        # Verbose Build Start
        rgb_print("#367fff", "[ ^ ] Building Jenova Runtime...")

        # Dependencies
        libs = [
            "Libs/libzlib-static-x86_64.a",
            "Libs/libtcc-static-x86_64.a",
            "Libs/libgodotcpp-static-x86_64.a",
            "Libs/libcurl-static-x86_64.a",
            "Libs/libasmjit-static-x86_64.a",
            "Libs/libarchive-static-x86_64.a",
            "Libs/liblzma-static-x86_64.a",
            "Libs/libxml2-static-x86_64.a"
        ]

        # Load Cache
        rgb_print("#367fff", "[ ^ ] Loading Cache...")
        cache = load_cache(CacheDB)

        # Generate Compile Commands
        rgb_print("#367fff", "[ ^ ] Generating Compiler Commands...")
        compile_commands = {}
        object_files = []
        if buildMode == "win-clang":
            for source in sources:
                object_file = f"{cacheDir}/{source.replace('.cpp', '.o')}"
                object_files.append(object_file)
                compile_command = (
                    f"{compiler} -m64 -O3 -g -gcodeview -fno-use-linker-plugin "
                    "-pipe -w -std=c++20 -pthread -fexceptions -femulated-tls "
                    f"{' '.join([f'-D{flag}' for flag in flags])} "
                    f"{' '.join([f'-I{directory}' for directory in directories])} "
                    f"-c {source} -o {object_file}"
                )
                compile_commands[source] = compile_command
        if buildMode == "win-gcc":
            for source in sources:
                object_file = f"{cacheDir}/{source.replace('.cpp', '.o')}"
                object_files.append(object_file)
                compile_command = (
                    f"{compiler} -m64 -O3 -g -gcodeview -fno-use-linker-plugin "
                    "-pipe -w -std=c++20 -pthread -fexceptions "
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
                    futures.append(executor.submit(run_compile_command, command, source, compiler))
                    update_cache_entry(source, cache)
                else:
                    rgb_print("#f59b42", f"[ ! ] Skipping Compilation for {os.path.basename(source)}, No Changes Detected.")
            for future in futures: future.result()

        # Save Cache
        rgb_print("#367fff", "[ ^ ] Saving Cache...")
        save_cache(cache, CacheDB)

        # Generate Linker Command
        rgb_print("#367fff", "[ ^ ] Generating Linker Command...")
        if buildMode == "win-clang":
            link_command = (
                f"{linker} -shared -static-libstdc++ -static-libgcc -fuse-ld=lld "
                f"{' '.join(object_files)} -o {outputDir}/{outputName} -m64 "
                f"{' '.join(libs)} -lws2_32 -lwinhttp -ldbghelp -lbcrypt -lwldap32" + " "
                "-Wl,-Bstatic -lpthread" + " "
                f"-Wl,-Map=\"{outputDir}/{mapFileName}\" "
                f"-Wl,--pdb=\"{outputDir}/{symbolFileName}\" "
                f"-Wl,--out-implib,\"{sdkDir}/Jenova.SDK.x64.a\" "
            )
        if buildMode == "win-gcc":
            link_command = (
                f"{linker} -shared -static-libstdc++ -static-libgcc -fuse-ld=lld "
                f"{' '.join(object_files)} -o {outputDir}/{outputName} -m64 "
                f"{' '.join(libs)} -lws2_32 -lwinhttp -ldbghelp -lbcrypt -lwldap32" + " "
                "-Wl,-Bstatic -lpthread" + " "
                f"-Wl,-Map=\"{outputDir}/{mapFileName}\" "
                f"-Wl,--pdb=\"{outputDir}/{symbolFileName}\" "
                f"-Wl,--out-implib,\"{sdkDir}/Jenova.SDK.x64.a\" "
            )

        # Link Object Files
        rgb_print("#367fff", "[ ^ ] Linking Jenova Runtime Binary...")
        run_linker_command(link_command, linker)

        # Prepare Release
        shutil.copy2("./JenovaSDK.h", f"{sdkDir}/JenovaSDK.h")
        shutil.copy2("./Jenova.Runtime.gdextension", f"{outputDir}/Jenova.Runtime.gdextension")  

# Entrypoint
if __name__ == "__main__":
    # Disable PyCache
    sys.dont_write_bytecode = True
    os.environ['PYTHONDONTWRITEBYTECODE'] = "1"

    # Create Arguments Parser
    parser = argparse.ArgumentParser(description="Jenova Runtime Build System")
    parser.add_argument('--compiler', type=str, help='Specify Compiler to Use.')
    parser.add_argument('--skip-banner', action='store_true', help='Skip Printing Banner')
    parser.add_argument('--skip-deps', action='store_true', help='Skip Building Dependencies')
    parser.add_argument('--no-cache', action='store_true', help='Skip Source Caching')
    
    # Parser Arguments
    args = parser.parse_args()

    # Print Banner
    if not args.skip_banner: print_banner()

    # Skip Building Dependencies
    if args.skip_deps: skip_deps = True

    # Skip Source Caching
    if args.no_cache: skip_cache = True

    # Set Compiler And Start Build
    start_time = time.time()
    if args.compiler:
        if args.compiler == "win-msvc":
            build_windows("cl.exe", "link.exe", "win-msvc", "Microsoft Visual C++")
        elif args.compiler == "win-clangcl":
            build_windows("clang-cl.exe", "lld-link.exe", "win-clangcl", "Microsoft Visual C++ LLVM")      
        elif args.compiler == "win-clang":
            build_windows("clang++.exe", "clang++.exe", "win-clang", "LLVM Clang")
        elif args.compiler == "win-gcc":
            build_windows("g++.exe", "g++.exe", "win-gcc", "Minimalist GNU")
        elif args.compiler == "linux-clang":
            build_linux("clang++", "clang++", "linux-clang", "LLVM Clang")
        elif args.compiler == "linux-gcc":
            build_linux("g++", "g++", "linux-gcc", "GNU Compiler Collection")
        else:
            rgb_print("#e02626", "[ x ] Error : Invalid Input.")
            exit(-1)
    else:
        # Ask for Compiler
        get_compiler_choice()

    # Verbose Success
    build_time = time.time() - start_time
    rgb_print("#03fc6f", f"[ + ] Jenova Runtime has been Successfully Compiled and Built in {format_duration(build_time)}.")
