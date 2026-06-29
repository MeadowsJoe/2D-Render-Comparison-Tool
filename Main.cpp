/*
MIT License

Copyright (c) 2024 MSc Games Engineering Team

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "Graphics/Window.h"
#include "Graphics/Core.h"
#include "Graphics/Scene.h"
#include "Graphics/Shaders.h"
#include "Graphics/Timer.h"
#include "Graphics/Texture.h"
#include "Graphics/GEMLoader.h"
#include "Graphics/RTSceneLoader.h"

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Define the scene to load.
    std::string sceneName = "cornell-box";
    // std::string sceneName = "bathroom";
    // std::string sceneName = "bathroom2";
    // std::string sceneName = "bedroom";
    // std::string sceneName = "car2";
    // std::string sceneName = "classroom";
    // std::string sceneName = "coffee";
    // std::string sceneName = "dining-room";
    // std::string sceneName = "glass-of-water";
    // std::string sceneName = "house";
    // std::string sceneName = "kitchen";
    // std::string sceneName = "living-room";
    // std::string sceneName = "living-room-2";
    // std::string sceneName = "living-room-3";
    // std::string sceneName = "MaterialsScene";
    // std::string sceneName = "Sibenik";
    // std::string sceneName = "staircase";
    // std::string sceneName = "staircase2";
    // std::string sceneName = "teapot-full";
    // std::string sceneName = "Terrain";
    // std::string sceneName = "veach-bidir";
    // std::string sceneName = "veach-mis";

    // Retrieve the scene dimensions
    int width = 0;
    int height = 0;
    loadWidthAndHeight(sceneName, width, height);

    // Create the application window
    Window win;
    win.create(width, height, "GEGPUPathtracer");

    // Initialize core graphics and shaders
    std::string shaderName = "PT.hlsl";
    Core core;
    core.init(win.hwnd, width, height);

    Shaders shaders;
    shaders.init(&core);
    shaders.load(&core, shaderName);

    // Initialize scene, textures, and camera
    Scene scene;
    scene.init(&core, 1048576); // Pre-allocate memory for scene
    Textures textures;
    Camera camera;

    // Load and build the scene
    scene.reset();
    loadScene(&core, &scene, &textures, &camera, sceneName);
    scene.build(&core);

    // Update scene drawing information with the current shader
    scene.updateDrawInfo(&core, shaders.find(shaderName));

    // Update shader constants for lighting and environment settings
    unsigned int nLights = (unsigned int)scene.lights.size();
    shaders.updateConstant(shaderName, "CBuffer", "nLights", &nLights);
    unsigned int useEnv = scene.envLum > 0 ? 1 : 0;
    shaders.updateConstant(shaderName, "CBuffer", "useEnvironmentMap", &useEnv);

    // Set up timer and initialize control variables
    Timer timer;
    bool running = true;
    float t = 0;         // Total elapsed time
    unsigned int SPP = 0; // Samples per pixel counter

    // Main loop
    while (running)
    {
        // Process input events
        win.checkInput();
        float dt = timer.dt();  // Delta time for this frame

        // Camera movement controls
        if (win.keyPressed('W'))
        {
            camera.moveForward();
            SPP = 0;
        }
        if (win.keyPressed('S'))
        {
            camera.moveBackward();
            SPP = 0;
        }
        if (win.keyPressed('A'))
        {
            camera.moveLeft();
            SPP = 0;
        }
        if (win.keyPressed('D'))
        {
            camera.moveRight();
            SPP = 0;
        }
        // Camera orientation control using mouse input
        if (win.mouseButtons[0] == true)
        {
            float dx = (float)win.mousedx;
            float dy = (float)win.mousedy;
            camera.updateLookDirection(dx, dy, 0.001f);
            SPP = 0;
        }
        if (win.keyPressed(VK_ESCAPE))
        {
            break;
        }

        // Begin a new frame
        core.beginFrame();

        // Update time
        t += dt;

        // Update shader constants with current camera matrices
        shaders.updateConstant(shaderName, "CBuffer", "inverseView", &camera.inverseView);
        shaders.updateConstant(shaderName, "CBuffer", "inverseProjection", &camera.inverseProjection);

        // Update samples per pixel counter and pass it to the shader
        SPP++;
        float SPPf = static_cast<float>(SPP);
        shaders.updateConstant(shaderName, "CBuffer", "SPP", &SPPf);

        // Apply shader changes and bind resources for the render target
        shaders.apply(&core, shaderName);
        core.bindRTUAV();

        // Reapply shader and render the scene
        shaders.apply(&core, shaderName);
        scene.draw(&core);

        // Finish and present the frame
        core.finishFrame();
    }
    core.flushGraphicsQueue();

    return 0;
}
