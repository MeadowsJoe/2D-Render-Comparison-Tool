#pragma once


#include "Core.h"
#include "Scene.h"
#include "Camera.h"
#include "Params.h"

#include <vector>
#include <string>

enum class RenderMode { RayTracing, Raster };

//Geometry for unit quad centred on 0,0 in XY, facing z, uv 0-1
inline void buildUnitQuad(std::vector<STATIC_VERTEX>& verts, std::vector<unsigned int>& indices)
{
	auto vertex = [](float px, float py, float pz, float u, float v)
	{
		STATIC_VERTEX sv{};
		sv.pos = Vec3(px, py, pz);
		sv.normal = Vec3(0.0f, 0.0f, 1.0f);
		sv.tangent = Vec3(1.0f, 0.0f, 0.0f);
		sv.tu = u;
		sv.tv = v;
		return sv;
	};

	verts = { vertex(-0.5f, -0.5f, 0.0f, 0.0f, 1.0f),
		vertex(0.5f, -0.5f, 0.0f, 1.0f, 1.0f),
		vertex(0.5f, 0.5f, 0.0f, 1.0f, 0.0f),
		vertex(-0.5f, 0.5f, 0.0f, 0.0f, 0.0f)
	};
	indices = { 0, 1, 2, 0, 2, 3 };
}

// TLAS rewrite on sprite transform update
void updateTLAS(Core* core, Scene* scene)
{
	// Map the instance buffer and update instance descriptors for each sprite.
	D3D12_RAYTRACING_INSTANCE_DESC* instanceDataDesc;
	scene->instances->Map(0, NULL, reinterpret_cast<void**>(&instanceDataDesc));
	for (int i = 0; i < scene->meshes.size(); i++)
	{
		memcpy(&instanceDataDesc[i].Transform, &scene->transforms[i], sizeof(float) * 12);
	}
	// Optionally unmap the instance buffer if needed
	scene->instances->Unmap(0, NULL);

	// Set up the TLAS build description
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
	buildDesc.DestAccelerationStructureData = scene->tlas->GetGPUVirtualAddress();
	buildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	buildDesc.Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE |
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
	buildDesc.Inputs.NumDescs = (unsigned int)scene->meshes.size();
	buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	buildDesc.Inputs.InstanceDescs = scene->instances->GetGPUVirtualAddress();
	buildDesc.ScratchAccelerationStructureData = scene->tlasBuildResource->GetGPUVirtualAddress();
	buildDesc.SourceAccelerationStructureData = scene->tlas->GetGPUVirtualAddress();


	// Build the TLAS using the command list
	core->graphicsCommandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, NULL);

	// Insert a UAV barrier to ensure TLAS build is complete before proceeding
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.UAV.pResource = scene->tlas;
	core->graphicsCommandList->ResourceBarrier(1, &barrier);
}

enum Role { OnStationary, OnMoving, OffStationary, OffMoving, OnMirror, OffMirror, OnBackground, OffBackground, OnSemi, OffSemi };

inline ThreeFourTransform makeTransform(Vec3 pos, float w, float h)
{
	ThreeFourTransform t;

	t.w[0][0] = w; t.w[0][1] = 0.0f;   t.w[0][2] = 0.0f; t.w[0][3] = pos.x;
	t.w[1][0] = 0.0f;  t.w[1][1] = h; t.w[1][2] = 0.0f; t.w[1][3] = pos.y;
	t.w[2][0] = 0.0f;  t.w[2][1] = 0.0f;   t.w[2][2] = 1.0f; t.w[2][3] = pos.z;

	return t;
}

struct Sprite
{
	Vec3 startPos;
	Vec3 pos;
	float w, h;
	int textureID;
	Role role;
	int bsdfType = 0; // 0 = normal, 3 = mirror
};

class SpriteSystem
{
public:
	Mesh* quad = nullptr;
	std::string key;
	std::vector<Sprite> sprites;
	RenderMode ACTIVE_MODE;


	// Create shared quad
	// Store in global buffers
	// Build instanced BLAS
	void init(Core* core, Scene* scene, RenderMode active_mode)
	{
		key = "unit_quad";

		std::vector<STATIC_VERTEX> verts;
		std::vector<unsigned int> indices;
		buildUnitQuad(verts, indices);

		scene->addMeshData(key, verts, indices);

		quad = new Mesh;
		quad->init(core, verts, indices);

		ACTIVE_MODE = active_mode;
	}


	// Update triangles pos, create new matrix and update scene transforms
	void update(Scene* scene, float t)
	{
		for (int i = 0; i < sprites.size(); i ++)
		{
			switch (sprites[i].role) {
			case OnStationary:
				break; 
			case OffStationary:
					break;
			case OnMoving || OffMoving:
			{
				sprites[i].pos.x = sprites[i].startPos.x + sinf(t);

				if (ACTIVE_MODE == RenderMode::RayTracing)
				{
					ThreeFourTransform tr = makeTransform(sprites[i].pos, sprites[i].w, sprites[i].h);
					scene->transforms[i] = tr;
				}
			}
				break;
			case OnMirror:
				break;
			case OffMirror:
				break;
			case OnBackground:
				break;
			case OffBackground:
				break;
			case OnSemi:
				break;
			case OffSemi:
				break;
			}
		}
	}

	// Add one sprite instance
	void addSprite(Scene* scene, const Sprite& s)
	{
		sprites.push_back(s);
		
		if (ACTIVE_MODE == RenderMode::RayTracing)
		{
			ThreeFourTransform t = makeTransform(s.pos, s.w, s.h);

			scene->meshes.push_back(quad);
			scene->transforms.push_back(t);

			InstanceData inst;
			inst.updatetextureID(s.textureID);
			inst.updateBSDFType(s.bsdfType);
			inst.startIndex = scene->indexOffset[key];
			scene->instanceData.push_back(inst);
		}
	}

	void addSpriteGrid(Core* core, Scene* scene, Textures* textures, Camera* camera, 
		int count, float z, Role role, const std::string& texPath, float fillFraction = 0.8f)
	{
		textures->load(core, texPath);
		int texID = textures->find(texPath);

		int cols = (int)ceilf(sqrtf((float)count));
		int rows = (int)ceilf((float)count / cols);

		float distance = camera->position.z - z;

		float fovRadians = camera->fov * 3.141592654f / 180.0f;
		float halfHeight = tanf(fovRadians * 0.5f) * distance;
		float halfWidth = halfHeight * ((float)camera->width / (float)camera->height);

		float usableWidth = 2.0f * halfWidth * fillFraction;
		float usableHeight = 2.0f * halfHeight * fillFraction;

		float spacingX = cols > 1 ? usableWidth / (cols - 1) : 0.0f;
		float spacingY = rows > 1 ? usableHeight / (rows - 1) : 0.0f;

		Vec3 origin(-usableWidth * 0.5f, -usableHeight * 0.5f, z);

		for (int i = 0; i < count; i++)
		{
			int row = i / cols;
			int col = i % cols;
			Sprite s;
			s.pos = s.startPos = Vec3(origin.x + col * spacingX, origin.y + row * spacingY, z + i * 0.001f);
			s.w = 1.0f; s.h = 1.0f;
			s.textureID = texID;
			s.role = role;
			addSprite(scene, s);
		}
	}

	void addSpriteRing(Core* core, Scene* scene, Textures* textures, Camera* camera,
		int count, float zPlane, Role role, const std::string& texPath, float radiusMargin = 1.5f)
	{
		textures->load(core, texPath);
		int texID = textures->find(texPath);

		float distance = camera->position.z - zPlane;
		float fovRadians = camera->fov * 3.141592654f / 180.0f;
		float halfHeight = tanf(fovRadians * 0.5f) * distance;
		float halfWidth = halfHeight * ((float)camera->width / (float)camera->height);

		float diagonal = sqrtf(halfWidth * halfWidth + halfHeight * halfHeight);
		float radius = diagonal * radiusMargin;

		for (int i = 0; i < count; i++)
		{
			float angle = (2.0f * 3.141592654f * i) / (float)count;
			Sprite s;
			s.pos = s.startPos = Vec3(
				camera->position.x + cosf(angle) * radius,
				camera->position.y + sinf(angle) * radius,
				zPlane + i * 0.001f);
			s.w = 1.0f; s.h = 1.0f;
			s.textureID = texID;
			s.role = role;
			addSprite(scene, s);
		}
	}

	void countRoles(Params& params)
	{
		params.nOnScreen = 0;
		params.nOffScreen = 0;

		for (int i = 0; i < sprites.size(); i++)
		{
			Role r = sprites[i].role;
			if (r == OnStationary || r == OnMoving || r == OnMirror || r == OnBackground || r == OnSemi )
				params.nOnScreen++;
			else if (r == OffStationary || r == OffMoving || r == OffMirror || r == OffBackground || r == OffSemi )
				params.nOffScreen++;
		}
	}

	void sceneOneSetup(Core* core, Scene* scene, Textures* textures, Params& params)
	{
		//BG
		textures->load(core, "Sprites/colored_desert.png");
		int bgTexID = textures->find("Sprites/colored_desert.png");
		Sprite bg; bg.startPos = bg.pos = Vec3(0.0f, 0.0f, -150.0f); bg.w = 16.0f; bg.h = 16.0f; bg.textureID = bgTexID, bg.role = OnBackground;

		//BG offscreen
		textures->load(core, "Sprites/colored_grass.png");
		int offBGTexID = textures->find("Sprites/colored_grass.png");
		Sprite offbg; offbg.startPos = offbg.pos = Vec3(10.0f, 0.0f, 950.0f); offbg.w = 20.0f; offbg.h = 20.0f; offbg.textureID = offBGTexID, offbg.role = OffBackground;

		// Create sprites in the scene
		textures->load(core, "Sprites/alienGreen_stand.png");
		int spriteTexID = textures->find("Sprites/alienGreen_stand.png");
		Sprite a; a.startPos = a.pos = Vec3(-1.5f, 0.0f, -50.0f); a.w = 1.0f; a.h = 1.0f; a.textureID = spriteTexID, a.role = OnMoving;
		Sprite b; b.startPos = b.pos = Vec3(-0.5f, 0.0f, -50.0f); b.w = 1.0f; b.h = 1.0f; b.textureID = spriteTexID, b.role = OnMoving;
		Sprite c; c.startPos = c.pos = Vec3(0.5f, 0.0f, -50.0f); c.w = 1.0f; c.h = 1.0f; c.textureID = spriteTexID, c.role = OnMoving;
		Sprite d; d.startPos = d.pos = Vec3(1.5f, 0.0f, -50.0f); d.w = 1.0f; d.h = 1.0f; d.textureID = spriteTexID, d.role = OnMoving;

		// Semi transparent sprite
		textures->load(core, "Sprites/redGlass.png");
		int semiTexID = textures->find("Sprites/redGlass.png");
		Sprite semi; semi.startPos = semi.pos = Vec3(1.0f, -1.0f, 0.0f); semi.w = 4.0f; semi.h = 4.0f; semi.textureID = semiTexID; semi.role = OnSemi; semi.bsdfType = 4;


		// Mirror object
		textures->load(core, "Sprites/mirror.png");
		int mirrorTexID = textures->find("Sprites/mirror.png");
		Sprite mir; mir.startPos = mir.pos = Vec3(5.0f, 0.0f, -40.0f); mir.w = 5.0f; mir.h = 5.0f; mir.textureID = mirrorTexID; mir.role = OnMirror; mir.bsdfType = 3;

		// Off screen Occluder
		textures->load(core, "Sprites/alienPink_stand.png");
		int ofOccTexID = textures->find("Sprites/alienPink_stand.png");
		Sprite ofOcc; ofOcc.startPos = ofOcc.pos = Vec3(8.0f, 0.0f, 250.0f); ofOcc.w = 1.0f; ofOcc.h = 1.0f; ofOcc.textureID = ofOccTexID; ofOcc.role = OffStationary;

		addSprite(scene, bg);
		addSprite(scene, offbg);
		addSprite(scene, a);
		addSprite(scene, b);
		addSprite(scene, c);
		addSprite(scene, d);
		addSprite(scene, mir);	
		addSprite(scene, semi);
		addSprite(scene, ofOcc);

		for (int i = 0; i < sprites.size(); i++)
		{
			if (sprites[i].role == OnSemi) params.usingGlass++;
		}

		countRoles(params);
	}

	void sceneTwoSetup(Core* core, Scene* scene, Textures* textures, Camera* camera, Params& params, int count)
	{
		//BG
		textures->load(core, "Sprites/colored_grass.png");
		int bgTexID = textures->find("Sprites/colored_grass.png");
		Sprite bg; bg.startPos = bg.pos = Vec3(0.0f, 0.0f, -150.0f); bg.w = 16.0f; bg.h = 16.0f; bg.textureID = bgTexID, bg.role = OnBackground;

		//BG offscreen
		textures->load(core, "Sprites/colored_desert.png");
		int offBGTexID = textures->find("Sprites/colored_desert.png");
		Sprite offbg; offbg.startPos = offbg.pos = Vec3(10.0f, 0.0f, 950.0f); offbg.w = 20.0f; offbg.h = 20.0f; offbg.textureID = offBGTexID, offbg.role = OffBackground;

		// Create sprites in the scene
		addSpriteGrid(core, scene, textures, camera, count, -35.0f, OnStationary, "Sprites/alienGreen_stand.png");

		// Semi transparent sprite
		textures->load(core, "Sprites/redGlass.png");
		int semiTexID = textures->find("Sprites/redGlass.png");
		Sprite semi; semi.startPos = semi.pos = Vec3(1.0f, -1.0f, 0.0f); semi.w = 4.0f; semi.h = 4.0f; semi.textureID = semiTexID; semi.role = OnSemi; semi.bsdfType = 4;
		Sprite semi1; semi1.startPos = semi1.pos = Vec3(-1.0f, -1.0f, -10.0f); semi1.w = 4.0f; semi1.h = 4.0f; semi1.textureID = semiTexID; semi1.role = OnSemi; semi1.bsdfType = 4;
		Sprite semi2; semi2.startPos = semi2.pos = Vec3(-1.0f, 1.0f, -20.0f); semi2.w = 4.0f; semi2.h = 4.0f; semi2.textureID = semiTexID; semi2.role = OnSemi; semi2.bsdfType = 4;
		Sprite semi3; semi3.startPos = semi3.pos = Vec3(1.0f, 1.0f, -30.0f); semi3.w = 4.0f; semi3.h = 4.0f; semi3.textureID = semiTexID; semi3.role = OnSemi; semi3.bsdfType = 4;
		Sprite semi4; semi4.startPos = semi4.pos = Vec3(1.2f, -1.2f, 10.0f); semi4.w = 4.0f; semi4.h = 4.0f; semi4.textureID = semiTexID; semi4.role = OnSemi; semi4.bsdfType = 4;
		Sprite semi5; semi5.startPos = semi5.pos = Vec3(-1.2f, -1.2f, 20.0f); semi5.w = 4.0f; semi5.h = 4.0f; semi5.textureID = semiTexID; semi5.role = OnSemi; semi5.bsdfType = 4; 
		Sprite semi6; semi6.startPos = semi6.pos = Vec3(-1.2f, 1.2f, 30.0f); semi6.w = 4.0f; semi6.h = 4.0f; semi6.textureID = semiTexID; semi6.role = OnSemi; semi6.bsdfType = 4;
		Sprite semi7; semi7.startPos = semi7.pos = Vec3(1.2f, 1.2f, 40.0f); semi7.w = 4.0f; semi7.h = 4.0f; semi7.textureID = semiTexID; semi7.role = OnSemi; semi7.bsdfType = 4;

		// Mirror object
		textures->load(core, "Sprites/mirror.png");
		int mirrorTexID = textures->find("Sprites/mirror.png");
		Sprite mir; mir.startPos = mir.pos = Vec3(5.0f, 0.0f, -40.0f); mir.w = 5.0f; mir.h = 5.0f; mir.textureID = mirrorTexID; mir.role = OnMirror; mir.bsdfType = 3;

		addSprite(scene, bg);
		addSprite(scene, offbg);
		addSprite(scene, semi);
		addSprite(scene, semi1);
		addSprite(scene, semi2);
		addSprite(scene, semi3);
		addSprite(scene, semi4);
		//addSprite(scene, semi5);
		//addSprite(scene, semi6);
		//addSprite(scene, semi7);
		addSprite(scene, mir);

		for (int i = 0; i < sprites.size(); i++)
		{
			if (sprites[i].role == OnSemi) params.usingGlass++;
		}

		countRoles(params);
	}

	void sceneThreeSetup(Core* core, Scene* scene, Textures* textures, Camera* camera, Params& params, int count)
	{
		//BG
		textures->load(core, "Sprites/colored_grass.png");
		int bgTexID = textures->find("Sprites/colored_grass.png");
		Sprite bg; bg.startPos = bg.pos = Vec3(0.0f, 0.0f, -150.0f); bg.w = 16.0f; bg.h = 16.0f; bg.textureID = bgTexID, bg.role = OnBackground;

		//BG offscreen
		textures->load(core, "Sprites/colored_desert.png");
		int offBGTexID = textures->find("Sprites/colored_desert.png");
		Sprite offbg; offbg.startPos = offbg.pos = Vec3(10.0f, 0.0f, 950.0f); offbg.w = 20.0f; offbg.h = 20.0f; offbg.textureID = offBGTexID, offbg.role = OffBackground;

		// Create sprites in the scene
		addSpriteRing(core, scene, textures, camera, count, 250.0f, OffStationary, "Sprites/alienPink_stand.png");

		// Semi transparent sprite
		textures->load(core, "Sprites/redGlass.png");
		int semiTexID = textures->find("Sprites/redGlass.png");
		Sprite semi; semi.startPos = semi.pos = Vec3(1.0f, -1.0f, -20.0f); semi.w = 4.0f; semi.h = 4.0f; semi.textureID = semiTexID; semi.role = OnSemi; semi.bsdfType = 4;

		// Mirror object
		textures->load(core, "Sprites/mirror.png");
		int mirrorTexID = textures->find("Sprites/mirror.png");
		Sprite mir; mir.startPos = mir.pos = Vec3(5.0f, 0.0f, -40.0f); mir.w = 5.0f; mir.h = 5.0f; mir.textureID = mirrorTexID; mir.role = OnMirror; mir.bsdfType = 3;

		addSprite(scene, bg);
		addSprite(scene, offbg);
		addSprite(scene, semi);
		addSprite(scene, mir);


		for (int i = 0; i < sprites.size(); i++)
		{
			if (sprites[i].role == OnSemi) params.usingGlass++;
		}

		countRoles(params);
	}
};