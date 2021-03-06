# RegEngine

Yet another approach to developing a personal 3D graphics engine. Windows + Direct3D 12.

**Work in progress...** Nothing to see here. Just playing around 😀 Features implemented:

Rendering:

- Direct3D 12 initialization and usage
- Loading 3D models (using Assimp library)
- Basic deferred shading

Other:

- GUI (using Dear ImGui library)
- Configuration loaded from and saved to a file (using RapidJSON library)

![RegEngine screenshot](Doc/Screenshot.jpg "RegEngine screenshot")

# License

The project is open source under MIT license. See file [LICENSE.txt](LICENSE.txt).

# Dependencies and third-party libraries

The project source code depends on:

- C++ standard library, including some of the latest C++11/14/17/20 features
- WinAPI from Windows 10 with some reasonably new Windows SDK, including Direct3D 12
- Visual Studio 2022

The project uses following thirt-party libraries:

- **[D3D12 Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator)** - easy to integrate memory allocation library for Direct3D 12, by AMD. License: MIT.
  - Directory: ThirdParty\D3D12MemoryAllocator
- **[d3dx12.h](https://github.com/microsoft/DirectX-Headers)** - D3D12 helpers, by Microsoft. License: MIT.
  - Directory: ThirdParty\d3dx12
- **[Dear ImGui](https://github.com/ocornut/imgui/)** by Omar Cornut. License: MIT.
  - Directory: ThirdParty\imgui-1.87
- **[DirectX Shader Compiler](https://github.com/microsoft/DirectXShaderCompiler)** by Microsoft. License: University of Illinois Open Source License.
  - Directory: ThirdParty\dxc_2021_12_08
- **[DirectXTex](URL)** - for reading and writing texture file formats, by Microsoft. License: MIT.
  - Directory: Source\packages\directxtex_desktop_win10.2021.11.8.1
- **[Font Awesome](https://fontawesome.com/)** - font with icons, by Fonticons, Inc. License: Font Awesome Free license.
  - File: WorkingDir\Data\Fonts\Font Awesome 6 Free-Solid-900.otf
- **[GLM](https://github.com/g-truc/glm)** - mathematics library for graphics software, by G-Truc Creation. License: The Happy Bunny License or MIT.
  - Directory: ThirdParty\glm
- **[IconFontCppHeaders](https://github.com/juliettef/IconFontCppHeaders)** - haader with constants for icons from Font Awesome, by Juliette Foucaut and Doug Binks. License: zlib.
  - Directory: ThirdParty\IconFontCppHeaders
- **[Open Asset Import Library (assimp)](https://github.com/assimp/assimp)** - for loading 3D model file formats, by assimp team. License: BSD.
  - Directory: Source\packages\AssimpCpp.5.0.1.6
- **[RapidJSON](https://rapidjson.org/)** - a fast JSON parser/generator, by Tencent. License: MIT.
  - Directory: ThirdParty\rapidjson
- **[str_view](https://github.com/sawickiap/str_view)** - null-termination-aware string-view class for C++, by Adam Sawicki. License: MIT.
  - Directory: ThirdParty\str_view
- **[WinPixEventRuntime](https://devblogs.microsoft.com/pix/winpixeventruntime/)** - a library for PIX events, by Microsoft. License: custom freeware.
  - Directory: Source\packages\WinPixEventRuntime.1.0.210818001

The project requires following hardware/software environment to run: PC, Windows 10+, Direct3D 12 compatible display adapter (graphics card).

Running the project in a system without beforementioned development environment installed or without Developer Mode enabled in Windows settings has not been tested yet and may not work properly.
