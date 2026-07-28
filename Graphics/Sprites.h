#pragma once


#include "Core.h"
#include "Scene.h"
#include "Camera.h"

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

enum Role { Stationary, Mirror, Moving, Background };

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
			case Stationary:
				break;
			case Mirror:
				break;
			case Moving:
			{
				sprites[i].pos.x = sprites[i].startPos.x + sinf(t);

				if (ACTIVE_MODE == RenderMode::RayTracing)
				{
					ThreeFourTransform tr = makeTransform(sprites[i].pos, sprites[i].w, sprites[i].h);
					scene->transforms[i] = tr;
				}
			}
				break;
			case Background:
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

	void sceneOneSetup(Core* core, Scene* scene, Textures* textures)
	{
		//BG
		textures->load(core, "Sprites/colored_desert.png");
		int bgTexID = textures->find("Sprites/colored_desert.png");
		Sprite bg; bg.startPos = bg.pos = Vec3(0.0f, 0.0f, -150.0f); bg.w = 16.0f; bg.h = 16.0f; bg.textureID = bgTexID, bg.role = Background;

		//BG offscreen
		textures->load(core, "Sprites/colored_grass.png");
		int offBGTexID = textures->find("Sprites/colored_grass.png");
		Sprite offbg; offbg.startPos = offbg.pos = Vec3(10.0f, 0.0f, 950.0f); offbg.w = 20.0f; offbg.h = 20.0f; offbg.textureID = offBGTexID, offbg.role = Background;

		// Create sprites in the scene
		textures->load(core, "Sprites/alienGreen_stand.png");
		int spriteTexID = textures->find("Sprites/alienGreen_stand.png");
		Sprite a; a.startPos = a.pos = Vec3(-1.5f, 0.0f, -50.0f); a.w = 1.0f; a.h = 1.0f; a.textureID = spriteTexID, a.role = Moving;
		Sprite b; b.startPos = b.pos = Vec3(-0.5f, 0.0f, -50.0f); b.w = 1.0f; b.h = 1.0f; b.textureID = spriteTexID, b.role = Moving;
		Sprite c; c.startPos = c.pos = Vec3(0.5f, 0.0f, -50.0f); c.w = 1.0f; c.h = 1.0f; c.textureID = spriteTexID, c.role = Moving;
		Sprite d; d.startPos = d.pos = Vec3(1.5f, 0.0f, -50.0f); d.w = 1.0f; d.h = 1.0f; d.textureID = spriteTexID, d.role = Moving;

		// Mirror object
		textures->load(core, "Sprites/mirror.png");
		int mirrorTexID = textures->find("Sprites/mirror.png");
		Sprite mir; mir.startPos = mir.pos = Vec3(5.0f, 0.0f, -40.0f); mir.w = 5.0f; mir.h = 5.0f; mir.textureID = mirrorTexID; mir.role = Mirror; mir.bsdfType = 3;

		// Off screen Occluder
		textures->load(core, "Sprites/alienPink_stand.png");
		int ofOccTexID = textures->find("Sprites/alienPink_stand.png");
		Sprite ofOcc; ofOcc.startPos = ofOcc.pos = Vec3(3.0f, 0.0f, 250.0f); ofOcc.w = 1.0f; ofOcc.h = 1.0f; ofOcc.textureID = ofOccTexID; ofOcc.role = Stationary;

		addSprite(scene, bg);
		addSprite(scene, offbg);
		addSprite(scene, a);
		addSprite(scene, b);
		addSprite(scene, c);
		addSprite(scene, d);
		addSprite(scene, mir);
		addSprite(scene, ofOcc);
	}

	void sceneTwoSetup(Core* core, Scene* scene, Textures* textures, Camera* camera, int count)
	{
		//BG
		textures->load(core, "Sprites/colored_grass.png");
		int bgTexID = textures->find("Sprites/colored_grass.png");
		Sprite bg; bg.startPos = bg.pos = Vec3(0.0f, 0.0f, -150.0f); bg.w = 16.0f; bg.h = 16.0f; bg.textureID = bgTexID, bg.role = Background;

		//BG offscreen
		textures->load(core, "Sprites/colored_desert.png");
		int offBGTexID = textures->find("Sprites/colored_desert.png");
		Sprite offbg; offbg.startPos = offbg.pos = Vec3(10.0f, 0.0f, 950.0f); offbg.w = 20.0f; offbg.h = 20.0f; offbg.textureID = offBGTexID, offbg.role = Background;

		// Create sprites in the scene
		addSpriteGrid(core, scene, textures, camera, count, -35.0f, Stationary, "Sprites/alienGreen_stand.png");

		// Mirror object
		textures->load(core, "Sprites/mirror.png");
		int mirrorTexID = textures->find("Sprites/mirror.png");
		Sprite mir; mir.startPos = mir.pos = Vec3(5.0f, 0.0f, -40.0f); mir.w = 5.0f; mir.h = 5.0f; mir.textureID = mirrorTexID; mir.role = Mirror; mir.bsdfType = 3;

		addSprite(scene, bg);
		addSprite(scene, offbg);
		addSprite(scene, mir);
	}

	void sceneThreeSetup(Core* core, Scene* scene, Textures* textures, Camera* camera, int count)
	{
		//BG
		textures->load(core, "Sprites/colored_grass.png");
		int bgTexID = textures->find("Sprites/colored_grass.png");
		Sprite bg; bg.startPos = bg.pos = Vec3(0.0f, 0.0f, -150.0f); bg.w = 16.0f; bg.h = 16.0f; bg.textureID = bgTexID, bg.role = Background;

		//BG offscreen
		textures->load(core, "Sprites/colored_desert.png");
		int offBGTexID = textures->find("Sprites/colored_desert.png");
		Sprite offbg; offbg.startPos = offbg.pos = Vec3(10.0f, 0.0f, 950.0f); offbg.w = 20.0f; offbg.h = 20.0f; offbg.textureID = offBGTexID, offbg.role = Background;

		// Create sprites in the scene
		addSpriteRing(core, scene, textures, camera, count, -50.0f, Stationary, "Sprites/alienPink_stand.png");


		// Mirror object
		textures->load(core, "Sprites/mirror.png");
		int mirrorTexID = textures->find("Sprites/mirror.png");
		Sprite mir; mir.startPos = mir.pos = Vec3(5.0f, 0.0f, -40.0f); mir.w = 5.0f; mir.h = 5.0f; mir.textureID = mirrorTexID; mir.role = Mirror; mir.bsdfType = 3;

		addSprite(scene, bg);
		addSprite(scene, offbg);
		addSprite(scene, mir);
	}
};