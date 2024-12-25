# Projekt J.E.N.O.V.A :: Jenova Runtime (Core)

This repository contains the source code for **Jenova Runtime** developed for Godot 4 and It's a part of **[Projekt J.E.N.O.V.A](https://github.com/Jenova-Framework/J.E.N.O.V.A)**

![image](https://github.com/user-attachments/assets/013eed25-7047-407d-aef8-b964203e73b0)

## Overview

**Projekt J.E.N.O.V.A** is a comprehensive series of extension libraries for the Godot 4 Game Engine that brings fully-featured C++ scripting directly into the Godot Editor. It allows the use of modern C++20 standards within the Godot Engine, similar to GDScript. 

By using Jenova Framework you can easily program your entire game or application in C/C++ without any issues. You can also integrate OpenCV, CUDA, Vulkan, OpenMP and any other modern C++ features seamlessly, All supported by the powerful backend.

### Structure of Projekt J.E.N.O.V.A
- **Jenova Framework**: The framework contains Jenova Runtime and JenovaSDK.
- **Jenova Runtime (Core)**: The heart of the framework, It includes the Jenova Interpreter, Jenova Compiler Interface, Jenova Packer, etc.
- **JenovaSDK**: An optional component of Jenova Runtime, providing helpers and APIs for hot-reloading, global allocation/access, etc.

Full feature list can be found [here](https://github.com/Jenova-Framework/J.E.N.O.V.A#%EF%B8%8F-current-features).

> [!IMPORTANT]
> This source code is licensed under MIT license.

## Issue/Bug Reports and Questions
If you want to report an issue or bug create a new thread at [Issues](https://github.com/Jenova-Framework/Jenova-Runtime/issues).

If you have any questions you can create a new thread at [Discussions](https://github.com/Jenova-Framework/J.E.N.O.V.A/discussions).

## Dependencies

Jenova Core has following dependencies :

- [AsmJIT](https://github.com/asmjit/asmjit)
- [LibArchive](https://github.com/libarchive/libarchive)
- [LibCurl](https://github.com/curl/curl)
- [LibFastZLib](https://github.com/gildor2/fast_zlib)
- [LibTinyCC](http://download.savannah.gnu.org/releases/tinycc/)
- [LibPThread](https://github.com/GerHobbelt/pthread-win32)
- [LibVSWhere](https://github.com/TheAenema/libvswhere/tree/jenova-edition)
- [MemoryModule++](https://github.com/bb107/MemoryModulePP)
- [MinHook](https://github.com/TsudaKageyu/minhook)
- [JSON++](https://github.com/nlohmann/json)
- [FileWatch](https://github.com/ThomasMonkman/filewatch)
- [ArgParse++](https://github.com/p-ranav/argparse)
- [Base64++](https://github.com/zaphoyd/websocketpp/blob/master/websocketpp/base64/base64.hpp)

> [!NOTE]
> - Edit **base64.hpp** namespace to `base64`
> - Only header file `libtcc.h` is required from TinyCC beside static library
> - In **FileWatch.hpp** change `_callback(file.first, file.second);` to `_callback(_path + file.first, file.second);`

> - **Pre-Built Windows x64 Dependencies can be obtained from [here](https://jenova-framework.github.io/archive/dependencies/Jenova-Runtime-v0.3.4.3-Dependencies-Win64.7z).**
>
> - **Pre-Built Linux x64 Dependencies can be obtained from [here](https://jenova-framework.github.io/archive/dependencies/Jenova-Runtime-v0.3.4.3-Dependencies-Linux64.7z).**

## Build System
### Windows x64

Building Jenova Runtime for Windows x64 requires Visual Studio 2022 with C++20 Support.

Proprietary Pre-built Windows x64 binaries can be obtained from [here](https://github.com/Jenova-Framework/J.E.N.O.V.A/releases).

### Linux x64

Building Jenova Runtime for Linux x64 can be done through Code::Blocks or Custom Builder. To build with Code::Blocks, simply open `Jenova.workspace` and build. To build using Custom Builder, Install Python 3 and then use the following commands from VSCode or Terminal:

```bash
# Running Builder
python3 ./Jenova.Builder.py

# Build using Clang++
python3 ./Jenova.Builder.py --compiler clang --skip-banner

# Build using G++
python3 ./Jenova.Builder.py --compiler gcc --skip-banner
```

The Jenova Runtime source code is fully compatible with both Clang++ and G++. Additionally, [Visual Studio Code](https://code.visualstudio.com/) compatibility files are provided.

## Open Source vs Proprietary
While the public source code of Jenova is ~90% identical to the proprietary version, a few specific features have been removed or disabled.
### These changes include :
- **Jenova Emulator Connector**  is provided in full source code but parts of the integration code for proprietary emulators have been removed. It remains functional if connected to another emulator module.
- **A.K.I.R.A JIT** is removed from the public source code. This component was responsible for executing obfuscated code using a proprietary highly secured VM.
- **Code Encryption and Key System** has been omitted from the public version to protect critical proprietary algorithms. However, Code Compression is fully included, Developers can add their own encryption on top of the existing buffering system.
- **Jenova Code Virtualizer/Sandbox** removed due to reliance on the proprietary SecureAngel™ 2.0 technology.

  

----
Developed & Designed By **Hamid.Memar (MemarDesign™ LLC.)**
