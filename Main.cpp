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
#include "Graphics/Sprites.h"
#include "Graphics/PerfLogger.h"
#include "Graphics/Raster.h"

enum class RenderMode { RayTracing, Raster };
constexpr RenderMode ACTIVE_MODE = RenderMode::RayTracing;

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Retrieve the scene dimensions
    int width = 1024;
    int height = 1024;

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
        spriteSys.init(&core, &scene);
    }
    else
    {
        rasterSys.init(&core, &shaders);
    }
   
    // Load and build the scene
    scene.reset();
    
    //BG
    textures.load(&core, "Sprites/colored_desert.png");
    int bgTexID = textures.find("Sprites/colored_desert.png");
    Sprite bg; bg.startPos = bg.pos = Vec3(0.0f, 0.0f, -150.0f); bg.w = 16.0f; bg.h = 16.0f; bg.textureID = bgTexID, bg.role = Background;

    //BG offscreen
    textures.load(&core, "Sprites/colored_grass.png");
    int offBGTexID = textures.find("Sprites/colored_grass.png");
    Sprite offbg; offbg.startPos = offbg.pos = Vec3(10.0f, 0.0f, 950.0f); offbg.w = 20.0f; offbg.h = 20.0f; offbg.textureID = offBGTexID, offbg.role = Background;

    // Create sprites in the scene
    textures.load(&core, "Sprites/alienGreen_stand.png");
    int spriteTexID = textures.find("Sprites/alienGreen_stand.png");
    Sprite a; a.startPos = a.pos = Vec3(-1.5f, 0.0f, -50.0f); a.w = 1.0f; a.h = 1.0f; a.textureID = spriteTexID, a.role = Occluder;
    Sprite b; b.startPos = b.pos = Vec3(-0.5f, 0.0f, -50.0f); b.w = 1.0f; b.h = 1.0f; b.textureID = spriteTexID, b.role = Occluder;
    Sprite c; c.startPos = c.pos = Vec3(0.5f, 0.0f, -50.0f); c.w = 1.0f; c.h = 1.0f; c.textureID = spriteTexID, c.role = Occluder;
    Sprite d; d.startPos = d.pos = Vec3(1.5f, 0.0f, -50.0f); d.w = 1.0f; d.h = 1.0f; d.textureID = spriteTexID, d.role = Occluder;
    
    // Mirror object
    textures.load(&core, "Sprites/mirror.png");
    int mirrorTexID = textures.find("Sprites/mirror.png");
    Sprite mir; mir.startPos = mir.pos = Vec3(5.0f, 0.0f, -40.0f); mir.w = 5.0f; mir.h = 5.0f; mir.textureID = mirrorTexID; mir.role = Mirror; mir.bsdfType = 3;

    // Off screen Occluder
    textures.load(&core, "Sprites/alienPink_stand.png");
    int ofOccTexID = textures.find("Sprites/alienPink_stand.png");
    Sprite ofOcc; ofOcc.startPos = ofOcc.pos = Vec3(3.0f, 0.0f, 250.0f); ofOcc.w = 1.0f; ofOcc.h = 1.0f; ofOcc.textureID = ofOccTexID; ofOcc.role = OFoccluder;

    if (ACTIVE_MODE == RenderMode::RayTracing)
    {
        spriteSys.addSprite(&scene, bg);
        spriteSys.addSprite(&scene, offbg);
        spriteSys.addSprite(&scene, a);
        spriteSys.addSprite(&scene, b);
        spriteSys.addSprite(&scene, c);
        spriteSys.addSprite(&scene, d);
        spriteSys.addSprite(&scene, mir);
        spriteSys.addSprite(&scene, ofOcc);
    }
    else
    {
        rasterSys.sprites.push_back(bg);
        rasterSys.sprites.push_back(offbg);
        rasterSys.sprites.push_back(a);
        rasterSys.sprites.push_back(b);
        rasterSys.sprites.push_back(c);
        rasterSys.sprites.push_back(d);
        rasterSys.sprites.push_back(mir);
        rasterSys.sprites.push_back(ofOcc);
    }

    // Camera Setup
    Matrix P = Matrix::perspective(0.1f, 1000.0f, (float)width / (float)height, 1.0f);
    camera.init(P, width, height);
    Matrix V = Matrix::lookAt(Vec3(0.0f, 0.0f, 500.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    camera.initView(V);
    camera.moveSpeed = 0.1f;

    // Point light source
    Vec3 lightPos = Vec3(10.0f, 1.0f, 600.0f);
    unsigned int shadowSamples = 64;
    shaders.updateConstant(shaderName, "CBuffer", "lightPosition", &lightPos);
    shaders.updateConstant(shaderName, "CBuffer", "shadowSamples", &shadowSamples);

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
    Params params;
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
        sprintf_s(buf, "2D RT - %.1f FPS (%.2f ms)", fps, ms);
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

        if (ACTIVE_MODE == RenderMode::RayTracing)
        {
            if (!logStarted)
                spriteSys.update(&scene, t);
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
            if (!logStarted)
                rasterSys.update(t);
            rasterSys.uploadInstanceBuffer(&core);
            rasterSys.updateCameraBuffer(&camera);
            rasterSys.draw(&core);
        }

        // Record performance
        if (win.keyPressed('C') && !logStarted)
        {
            logStarted = true;
            caputreFrame = 0;
            perf.open();
        }
        if (caputreFrame < 1000 && logStarted)
        {
            caputreFrame++;

            params.nSprites = (ACTIVE_MODE == RenderMode::RayTracing) ? spriteSys.sprites.size() : rasterSys.sprites.size(); 
            params.nSamples = shadowSamples; 
            params.nLights = 1;
            perf.log(dt * 1000, caputreFrame, params);
            if (caputreFrame == 999)
            {
                logStarted = false;
                perf.close();
            }
        }

        // Finish and present the frame
        core.finishFrame();
    }

    core.flushGraphicsQueue();
   
    return 0;
}
