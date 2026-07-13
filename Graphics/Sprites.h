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

enum Role { OFobject, OFoccluder, Mirror, Occluder, Background };

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

	TLASTransform makeTranform(Vec3 pos, float w, float h)
	{
		TLASTransform t;

		t.w[0][0] = w; t.w[0][1] = 0.0f;   t.w[0][2] = 0.0f; t.w[0][3] = pos.x;
		t.w[1][0] = 0.0f;  t.w[1][1] = h; t.w[1][2] = 0.0f; t.w[1][3] = pos.y;
		t.w[2][0] = 0.0f;  t.w[2][1] = 0.0f;   t.w[2][2] = 1.0f; t.w[2][3] = pos.z;

		return t;
	}

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


	// Update triangles pos, create new matrix and update scene transforms
	void update(Scene* scene, float t)
	{
		for (int i = 0; i < sprites.size(); i ++)
		{
			switch (sprites[i].role) {
			case OFobject:
				break;
			case OFoccluder:
				break;
			case Mirror:
				break;
			case Occluder:
			{
				sprites[i].pos.x = sprites[i].startPos.x + sinf(t);

				TLASTransform tr = makeTranform(sprites[i].pos, sprites[i].w, sprites[i].h);

				scene->transforms[i] = tr;
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
		
		TLASTransform t = makeTranform(s.pos, s.w, s.h);

		scene->meshes.push_back(quad);
		scene->transforms.push_back(t);

		InstanceData inst;
		inst.updatetextureID(s.textureID);
		inst.updateBSDFType(s.bsdfType);
		inst.startIndex = scene->indexOffset[key];
		scene->instanceData.push_back(inst);
	}
};