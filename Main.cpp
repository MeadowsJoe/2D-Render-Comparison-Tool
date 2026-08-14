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
#include "Graphics/Sprites.h"
#include "Graphics/PerfLogger.h"
#include "Graphics/Raster.h"
#include "Graphics/Params.h"



constexpr RenderMode ACTIVE_MODE = RenderMode::RayTracing;

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Retrieve the scene dimensions
    int width = 1280;
    int height = 720;

    // Create the application window
    Window win;
    win.create(width, height, "GEGPURenderer");

    // Initialize core graphics
    Core core;
    core.init(win.hwnd, width, height);

    // Initialize scene, textures, and camera
    Scene scene;
    scene.init(&core, 1048576); // Pre-allocate memory for scene
    Textures textures;
    Camera camera;

    // Initialise shaders and sprites
    Shaders shaders;
    std::string shaderName = "RTSprite.hlsl";
    shaders.init(&core);

    SpriteSystem spriteSys;
    RasterSystem rasterSys;
    if (ACTIVE_MODE == RenderMode::RayTracing)
    {
        shaders.load(&core, shaderName);
        spriteSys.init(&core, &scene, ACTIVE_MODE);
    }
    else
    {
        rasterSys.init(&core, &shaders, &spriteSys);
    }
  
    // Camera Setup
    float fov = 1.0f;
    Matrix P = Matrix::perspective(1000.0f, 0.1f, (float)width / (float)height, fov);
    camera.init(P, width, height, fov);
    Matrix V = Matrix::lookAt(Vec3(0.0f, 0.0f, 500.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    camera.initView(V);
    camera.moveSpeed = 0.1f;

    // Select scene
    Params params{ width, height };
    int sceneSelection = 2;

    if(sceneSelection == 1) 
        spriteSys.sceneOneSetup(&core, &scene, &textures, params);
    else if (sceneSelection == 2) 
        spriteSys.sceneTwoSetup(&core, &scene, &textures, &camera, params, 8);
    else if (sceneSelection == 3)
        spriteSys.sceneThreeSetup(&core, &scene, &textures, &camera, params, 4096);

    if (ACTIVE_MODE == RenderMode::RayTracing)
    {
        params.nShadowSamples = 64;
    }
    else
    {
        params.nShadowSamples = 0;
    }

    // Point light source
    Vec3 lightPos = Vec3(10.0f, 1.0f, 600.0f);
    shaders.updateConstant(shaderName, "CBuffer", "lightPosition", &lightPos);
    shaders.updateConstant(shaderName, "CBuffer", "shadowSamples", &params.nShadowSamples);

    if (ACTIVE_MODE == RenderMode::RayTracing)
    {
        // Use a default black environment
        float env[3] = { 0, 0, 0 };
        scene.environmentMap = textures.loadFromMemory(&core, 1, 1, 3, env);
        scene.envLum = 0;
        scene.build(&core);

        // Update scene drawing information with the current shader
        scene.updateDrawInfo(&core, shaders.find(shaderName));

        // Update shader constants for lighting and environment settings
        unsigned int nLights = (unsigned int)scene.lights.size();
        shaders.updateConstant(shaderName, "CBuffer", "nLights", &nLights);
        unsigned int useEnv = scene.envLum > 0 ? 1 : 0;
        shaders.updateConstant(shaderName, "CBuffer", "useEnvironmentMap", &useEnv);
    }

    // Set up timer and initialize control variables
    Timer timer;
    bool running = true;
    float t = 0;         // Total elapsed time
    unsigned int SPP = 0; // Samples per pixel counter

    float dtSmoothed = 0.016f;

    // Performance log
    PerfLogger perf;
    int frames = 0;
    int caputreFrame = 0;

    bool logStarted = false;

    // Main loop
    while (running)
    {
        // Process input events
        win.checkInput();
        float dt = timer.dt();  // Delta time for this frame

        dtSmoothed = dtSmoothed * 0.95f + dt * 0.05f;
        float ms = dtSmoothed * 1000.0f;
        float fps = 1.0f / dtSmoothed;

        char buf[64];
        if(ACTIVE_MODE == RenderMode::RayTracing) sprintf_s(buf, "2D RT - %.1f FPS (%.2f ms)", fps, ms);
        else sprintf_s(buf, "2D Ras - %.1f FPS (%.2f ms)", fps, ms);
        SetWindowTextA(win.hwnd, buf);

        // Camera movement controls
        if (win.keyPressed('W') && !logStarted)
        {
            camera.moveForward();
            SPP = 0;
        }
        if (win.keyPressed('S') && !logStarted)
        {
            camera.moveBackward();
            SPP = 0;
        }
        if (win.keyPressed('A') && !logStarted)
        {
            camera.moveLeft();
            SPP = 0;
        }
        if (win.keyPressed('D') && !logStarted)
        {
            camera.moveRight();
            SPP = 0;
        }
        // Camera orientation control using mouse input
        if (win.mouseButtons[0] == true && !logStarted)
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
        frames++;

        spriteSys.update(&scene, t);

        if (ACTIVE_MODE == RenderMode::RayTracing)
        {
            if(sceneSelection == 1)
                updateTLAS(&core, &scene);

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
        }
        else
        {
            rasterSys.uploadInstanceBuffer(&core);
            rasterSys.updateCameraBuffer(&camera);
            rasterSys.draw(&core);
        }

        // Record performance
        if ((win.keyPressed('C') || (frames == 1000)) && !logStarted)
        {
            logStarted = true;
            caputreFrame = 0;
            if (ACTIVE_MODE == RenderMode::RayTracing)
            {
                perf.open(sceneSelection, "RT", params);
                perf.screenCapture(&core, sceneSelection, "RT", params);
            }
            else
            {
                perf.open(sceneSelection, "Ras", params);
                perf.screenCapture(&core, sceneSelection, "Ras", params);
            }
        }
        if (caputreFrame < 1000 && logStarted)
        {
            caputreFrame++;
            if(ACTIVE_MODE == RenderMode::RayTracing)
                perf.log("RayTracing", dt * 1000, caputreFrame, params);
            else
                perf.log("Rasterizer", dt * 1000, caputreFrame, params);

            if (caputreFrame >= 999)
            {
                logStarted = false;
                perf.close();

                core.finishFrame();
                break;
            }
        }

        // Finish and present the frame
        core.finishFrame();
    }

    core.flushGraphicsQueue();
   
    return 0;
}
