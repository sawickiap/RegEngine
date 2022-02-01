# RegEngine

Yet another approach to developing a personal 3D graphics engine. Windows + Direct3D 12.

**Work in progress...** Nothing to see here. I just rendered a first triangle 😀

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
- **[DirectX Shader Compiler](https://github.com/microsoft/DirectXShaderCompiler)** by Microsoft. License: University of Illinois Open Source License.
  - Directory: ThirdParty\dxc_2021_12_08
- **[DirectXTex](URL)** - for reading and writing texture file formats, by Microsoft. License: MIT.
  - Directory: Source\packages\directxtex_desktop_win10.2021.11.8.1
- **[GLM](https://github.com/g-truc/glm)** - mathematics library for graphics software, by G-Truc Creation. License: The Happy Bunny License or MIT.
  - Directory: ThirdParty\glm
- **[RapidJSON](https://rapidjson.org/)** - a fast JSON parser/generator, by Tencent. License: MIT.
  - Directory: ThirdParty\rapidjson
- **[str_view](https://github.com/sawickiap/str_view)** - null-termination-aware string-view class for C++, by Adam Sawicki. License: MIT.
  - Directory: ThirdParty\str_view
- **[WinFontRender](https://github.com/sawickiap/WinFontRender)** - a library that renders Windows fonts in graphics applications, by Adam Sawicki. License: Modified MIT.
  - Directory: ThirdParty\WinFontRender
- **[WinPixEventRuntime](https://devblogs.microsoft.com/pix/winpixeventruntime/)** - a library for PIX events, by Microsoft. License: custom freeware.
  - Directory: Source\packages\WinPixEventRuntime.1.0.210818001

The project requires following hardware/software environment to run: PC, Windows 10+, Direct3D 12 compatible display adapter (graphics card).

Running the project in a system without beforementioned development environment installed or without Developer Mode enabled in Windows settings has not been tested yet and may not work properly.
