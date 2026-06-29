#pragma once


#include "Core.h"
#include "Scene.h"

#include <vector>
#include <string>

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


struct Sprite
{
	Vec3 pos;
	float w, h;
	int textureID;
};

class SpriteSystem
{
public:
	Mesh* quad = nullptr;
	std::string key;
	std::vector<Sprite> sprites;


	// Create shared quad
	// Store in global buffers
	// Build instanced BLAS
	void init(Core* core, Scene* scene)
	{
		key = "unit_quad";

		std::vector<STATIC_VERTEX> verts;
		std::vector<unsigned int> indices;
		buildUnitQuad(verts, indices);

		scene->addMeshData(key, verts, indices);

		quad = new Mesh;
		quad->init(core, verts, indices);
	}


	// Add one sprite instance
	void addSprite(Scene* scene, const Sprite& s)
	{
		sprites.push_back(s);
		TLASTransform t;

		t.w[0][0] = s.w; t.w[0][1] = 0.0f;   t.w[0][2] = 0.0f; t.w[0][3] = s.pos.x;
		t.w[1][0] = 0.0f;  t.w[1][1] = s.h; t.w[1][2] = 0.0f; t.w[1][3] = s.pos.y;
		t.w[2][0] = 0.0f;  t.w[2][1] = 0.0f;   t.w[2][2] = 1.0f; t.w[2][3] = s.pos.z;

		scene->meshes.push_back(quad);
		scene->transforms.push_back(t);

		InstanceData inst;
		inst.updateBSDFType(1);
		inst.startIndex = scene->indexOffset[key];
		inst.bsdfData[0] = 1;
		inst.bsdfData[1] = 0;
		inst.bsdfData[2] = 0;
		scene->instanceData.push_back(inst);
	}
};