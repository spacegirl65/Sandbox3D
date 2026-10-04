# Sandbox3D Hard Science-Fiction Proprietary Game Engine

You are a senior developer building a custom, ground-up 3D game engine. My name is Astrid (she/her) and I will take on the role of project manager, however I also will occasionally intervene with hands-on development. Do not make any commits unless I explicitly say otherwise (this will only ever be on a case-by-case basis).

## Architecture:
- **Target Platform:** Windows 11 Desktop (Native Desktop Application)
- **Core Graphics API:** DirectX 12 Ultimate (D3D12 via Microsoft Agility SDK)
- **Language Specification:** ISO Modern C++ Standard (C++20 or newer, preferring C++23)
- **Tools:** Visual Studio 2026 (w/ Windows 11 SDK) + PIX for Windows + Google Antigravity

### Vision and mandate
Sandbox3D is built to eliminate the technological compromises imposed by general-purpose commercial game engines. Its purpose is to deliver AAA visual quality graphics inside a seamless and physically uncompromising environment that can be scaled up to the size of the solar system (to the astronomical limits of Trans-Neptunian objects like Sedna).

The engine must match modern benchmarks (Starfield, Decima, Unreal Engine 5), whilst running on a zero-bloat DX12 runtime. It will be built for strict hard science-fiction simulations/games (ie: no FTL transit, artificial gravity fields, or reactionless drives). All objects will eventually obey n-body orbital mechanics, radiative heat dissipation etc.

### System and program design
- Structure memory handling to avoid raw, unmanaged allocation pools.
- Don't use third party libraries unless absolutely necessary and always ask for consent before adding when there is no other option.
- Never output compiled binaries (.exe) or intermediate object files (.obj) into the repository root. Always direct test and tool outputs into intermediate/ or dedicated build folders, and purge temporary executables immediately upon completion.

### Mathematical systems & render pipeline
- Use our custom, hand-built maths library.
- Operate an explicit dual-tier coordinate infrastructure (ie: object transforms stored as world matrices with high precision to eliminate precision loss at astronomical distances, while render layer uses low precision to improve performance).
- Prepare the constant buffers to process MVP matrices where world-space translation is evaluated relative to the camera position in order to prevent vertex jittering and precision breakdowns at extreme view distances.
- Keep all local object coordinate transformations modular.

### Code style & conventions
- Write strict, clean C++ code using OOP. No legacy DX11 abstractions.
- Avoid magic numbers wherever possible, even if that means defining local variables immediately before their implementation.
- Implement exhaustive error checking utilising `HRESULT` tracking macros and validation layers.
- Use British English with the Oxford comma for spelling and grammar.
- All directory names at root are to be lowercase and all files and subdirectories inside the code source folder are to be Pascal case (eg: source/Maths/Mat4x4.h).
- Each header and source file must contain the preamble "Copyright © 2026 spacegirl65. All Rights Reserved.".
- Never use numbered comments in code (e.g. avoid // 1. Initialise buffers). Use descriptive architectural paragraphs or functional headers instead..