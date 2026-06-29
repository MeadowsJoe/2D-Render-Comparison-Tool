# GPU Path Tracer

This repository contains a simple pedagogic interactive path tracer built on top of DirectX 12 and DXR for the the Games Engineering course at Warwick. It demonstrates how to:
- Create and manage a **Direct3D 12** device
- Load and build scenes (e.g., Cornell box, living room, etc.)
- Implement a real-time path tracer with an HLSL shader
- Load and use textures
- Read from a shader file
- Interactively control the camera and scene parameters

This repository is for the lab session on GPU Pathtracing for the Games Engineering course at Warwick. In the lab session, students on the course are expected to:

- Add a HDR UAV to render to and tonemap the output
- Replace the path tracing integrator with an ambient occlusion integrator

The scenes are in the [GEMScene](https://github.com/MSCGamesTom/GEM) format used by the Games Engineering course at Warwick.

## Table of Contents
- [Features](#features)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Building the Project](#building-the-project)
- [Running the Application](#running-the-application)
- [Directory Structure](#directory-structure)
- [Supported Scenes](#supported-scenes)
- [Controls](#controls)
- [License](#license)

## Features
- **Real-time GPU Path Tracing**: Uses DirectX 12 compute and graphics queues to render scenes via path tracing.
- **Multiple Scenes**: Load various 3D scenes, from the Cornell box to more complex environments.
- **Interactive Camera**: Move around the scene with WASD keys and adjust viewing angles using mouse input.
- **Shader Hot-Reload** (optional): Easily switch or adjust HLSL shaders if you want to experiment with different materials or shading models.
- **Texture Management**: Utilize [stb_image](https://github.com/nothings/stb) to load textures from files and convert them into GPU resources.

## Getting Started

### Prerequisites
1. **Windows 10 or later** (64-bit)
2. **Visual Studio 2019/2022** with the C++ development environment installed, or another compiler toolchain that supports C++17 (or newer).
3. **DirectX 12-compatible GPU** and updated graphics drivers.

### Building the Project
1. Clone or download this repository to your local machine.
2. Open the project in Visual Studio (or your preferred environment). If you have a `.sln` file, double-click it. Otherwise, create a new Visual Studio solution or CMake project that includes all `.cpp` and `.h` files in the `Graphics` directory, plus the main `Main.cpp`.
3. Make sure you link against the necessary **Windows SDK** libraries (e.g., `d3d12.lib`, `dxgi.lib`, and `dxguid.lib`).
4. Build the solution in **Debug** or **Release** mode.

> **Note:** If you are using CMake, ensure you set up `find_package(DirectX12 REQUIRED)` or manually link the required DirectX 12 libraries.

## Running the Application
1. After building, run the generated executable.  
2. The application will open a new window with the **Cornell box** scene by default.  
3. Use the controls below to move the camera or switch scenes (by modifying the `sceneName` variable in `Main.cpp`).

## Directory Structure
```
Graphics/
??? Camera.h          // Camera class and logic
??? Core.cpp          // Core initialization for D3D12
??? Core.h
??? GEMLoader.h       // Geometry and mesh loading functionality
??? Math.h            // Basic math utilities
??? RTSceneLoader.h   // Scene loading logic for path tracer
??? Scene.h           // Scene class - manages objects, lights, etc.
??? Shaders.h         // Shader management class
??? stb_image.h       // External library for loading textures
??? Texture.h         // GPU texture handling and SRV creation
??? Timer.h           // High-resolution timing utilities
??? Window.h          // Window creation, input handling

Main.cpp              // Entry point (WinMain), sets up everything
```

## Supported Scenes
In `Main.cpp`, you will see a series of commented lines with different scene names (e.g., `"cornell-box"`, `"bathroom"`, `"kitchen"`, etc.).  
- To change the scene, simply modify:
  ```cpp
  std::string sceneName = "cornell-box";
  ```
  to the desired scene (e.g., `"kitchen"`). Ensure that the scene assets or files exist in the expected location.

## Controls
- **W**: Move camera forward  
- **S**: Move camera backward  
- **A**: Strafe camera left  
- **D**: Strafe camera right  
- **Left Mouse Button**: Click and drag to rotate the camera  
- **Esc**: Exit application  

Each time you move or look around, the path tracer resets the sample accumulator (so it starts at SPP = 0 again) and accumulates samples over time.

## Acknowledgements
Some of this code is inspired by this fantastic [article](https://landelare.github.io/2023/02/18/dxr-tutorial.html). Scenes converted from [https://benedikt-bitterli.me/resources/](https://benedikt-bitterli.me/resources/)

## License
Unless specified otherwise in individual files, this code is available under the MIT License.  
Some components (e.g., `stb_image.h`) may be under different licenses (MIT or public domain). See those headers for details.