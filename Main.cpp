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
    std::string shaderName = "Sprite.hlsl";
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
    //loadScene(&core, &scene, &textures, &camera, sceneName); // Replaced by initQuad
    // Initialize Sprite
    SpriteSystem spriteSys;
    spriteSys.init(&core, &scene);
    
    //BG
    textures.load(&core, "Sprites/colored_desert.png");
    int bgTexID = textures.find("Sprites/colored_desert.png");
    Sprite bg; bg.startPos = bg.pos = Vec3(0.0f, 0.0f, -150.0f); bg.w = 16.0f; bg.h = 16.0f; bg.textureID = bgTexID, bg.role = Background;
    spriteSys.addSprite(&scene, bg);

    //BG offscreen
    textures.load(&core, "Sprites/colored_grass.png");
    int offBGTexID = textures.find("Sprites/colored_grass.png");
    Sprite offbg; offbg.startPos = offbg.pos = Vec3(10.0f, 0.0f, 950.0f); offbg.w = 20.0f; offbg.h = 20.0f; offbg.textureID = offBGTexID, offbg.role = Background;
    spriteSys.addSprite(&scene, offbg);

    // Create sprites in the scene
    textures.load(&core, "Sprites/alienGreen_stand.png");
    int spriteTexID = textures.find("Sprites/alienGreen_stand.png");
    Sprite a; a.startPos = a.pos = Vec3(-1.5f, 0.0f, -50.0f); a.w = 1.0f; a.h = 1.0f; a.textureID = spriteTexID, a.role = Occluder;
    Sprite b; b.startPos = b.pos = Vec3(-0.5f, 0.0f, -50.0f); b.w = 1.0f; b.h = 1.0f; b.textureID = spriteTexID, b.role = Occluder;
    Sprite c; c.startPos = c.pos = Vec3(0.5f, 0.0f, -50.0f); c.w = 1.0f; c.h = 1.0f; c.textureID = spriteTexID, c.role = Occluder;
    Sprite d; d.startPos = d.pos = Vec3(1.5f, 0.0f, -50.0f); d.w = 1.0f; d.h = 1.0f; d.textureID = spriteTexID, d.role = Occluder;
    spriteSys.addSprite(&scene, a);
    spriteSys.addSprite(&scene, b);
    spriteSys.addSprite(&scene, c);
    spriteSys.addSprite(&scene, d);

    // Color source
    textures.load(&core, "Sprites/gemRed.png");
    int colourTexID = textures.find("Sprites/gemRed.png");
    Sprite col; col.startPos = col.pos = Vec3(-3.0f, 0.0f, -55.0f); col.w = 1.0f; col.h = 1.0f; col.textureID = colourTexID; col.role = Colour;
    spriteSys.addSprite(&scene, col);

    // Mirror object
    textures.load(&core, "Sprites/mirror.png");
    int mirrorTexID = textures.find("Sprites/mirror.png");
    Sprite mir; mir.startPos = mir.pos = Vec3(5.0f, 0.0f, -40.0f); mir.w = 5.0f; mir.h = 5.0f; mir.textureID = mirrorTexID; mir.role = Mirror; mir.bsdfType = 3;
    spriteSys.addSprite(&scene, mir);

    // Off screen Occluder
    textures.load(&core, "Sprites/alienPink_stand.png");
    int ofOccTexID = textures.find("Sprites/alienPink_stand.png");
    Sprite ofOcc; ofOcc.startPos = ofOcc.pos = Vec3(3.0f, 0.0f, 900.0f); ofOcc.w = 1.0f; ofOcc.h = 1.0f; ofOcc.textureID = ofOccTexID; ofOcc.role = Mirror;
    spriteSys.addSprite(&scene, ofOcc);

    // Off screen object
    textures.load(&core, "Sprites/keyYellow.png");
    int ofObjTexID = textures.find("Sprites/keyYellow.png");
    Sprite ofObj; ofObj.startPos = ofObj.pos = Vec3(4.5f, 0.0f, 100.0f); ofObj.w = 1.0f; ofObj.h = 1.0f; ofObj.textureID = ofObjTexID; ofObj.role = Mirror;
    spriteSys.addSprite(&scene, ofObj);

    // Camera Setup
    Matrix P = Matrix::perspective(0.1f, 100.0f, (float)width / (float)height, 1.0f);
    camera.init(P, width, height);
    Matrix V = Matrix::lookAt(Vec3(0.0f, 0.0f, 500.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    camera.initView(V);
    camera.moveSpeed = 0.1f;

    // Point light source
    Vec3 lightPos = Vec3(10.0f, 1.0f, 900.0f);
    shaders.updateConstant(shaderName, "CBuffer", "lightPosition", &lightPos);

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

        spriteSys.update(&scene, t);
        updateTLAS(&core, &scene);

        // Reapply shader and render the scene
        shaders.apply(&core, shaderName);
        scene.draw(&core);

        // Finish and present the frame
        core.finishFrame();
    }
    core.flushGraphicsQueue();

    return 0;
}
