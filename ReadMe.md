# Projekt J.E.N.O.V.A :: Core (Public Access)

This repository contains the source code for **Jenova Runtime** developed for Godot 4+
![image](https://github.com/user-attachments/assets/013eed25-7047-407d-aef8-b964203e73b0)

## Overview

**J.E.N.O.V.A** is an extension library for the Godot 4 Game Engine that brings fully-featured C++ scripting directly into the Godot Editor. It allows the use of modern C++20 standards within the Godot Engine, similar to GDScript. With Jenova Framework there are no limits. You can integrate OpenCV, CUDA, Vulkan, OpenMP, and any other modern C++ features, all supported by the powerful MSVC backend.

> [!CAUTION]
> This source code is licensed under MIT license.

## Dependencies

Jenova Core has following dependencies :

- [AsmJIT](https://github.com/asmjit/asmjit)
- [LibArchive](https://github.com/libarchive/libarchive)
- [LibCurl](https://github.com/curl/curl)
- [LibFastZLib](https://github.com/timotejroiko/fast-zlib)
- [LibTinyCC](http://download.savannah.gnu.org/releases/tinycc/)
- [LibVSWhere](https://github.com/TheAenema/libvswhere)
- [MemoryModule++](https://github.com/bb107/MemoryModulePP)
- [MinHook](https://github.com/TsudaKageyu/minhook)
- [JSON++](https://github.com/nlohmann/json)
- [FileWatch](https://github.com/ThomasMonkman/filewatch)
- [ArgParse++](https://github.com/p-ranav/argparse)
- [Base64++](https://github.com/zaphoyd/websocketpp/blob/master/websocketpp/base64/base64.hpp)

## Build System
Jenova Core requires Visual Studio 2022 with C++20 Support

## Open Source vs Proprietary

While the public source code of Jenova is ~99% identical to the proprietary version, a few specific features have been removed or disabled.
### These changes include :

- **Jenova Emulator Connector**  is provided in full source code but parts of the integration code for proprietary emulators have been removed. It remains functional if connected to another emulator module.

- **A.K.I.R.A JIT** is removed from the public source code. This component was responsible for executing obfuscated code using a proprietary highly secured VM.

- **Code Encryption and Key System** has been omitted from the public version to protect critical proprietary algorithms. However, Code Compression is fully included, Developers can add their own encryption on top of the existing buffering system.

- **Jenova Code Virtualizer/Sandbox** removed due to reliance on the proprietary SecureAngel™ 2.0 technology.

  

----
Developed & Designed By **Hamid.Memar**
