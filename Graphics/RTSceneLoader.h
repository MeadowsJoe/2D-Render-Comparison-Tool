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

#pragma once

// This file implements functions for loading scene data including static models,
// area lights and instances using the GEMLoader
// It sets up scene geometry, camera parameters and material properties for rendering

#include "GEMLoader.h"
#include "Math.h"
#include "Scene.h"
#include "Camera.h"
#include "Texture.h"

class SceneBounds
{
public:
	Vec3 max;
	Vec3 min;
	SceneBounds()
	{
		reset();
	}
	void extend(Vec3 p)
	{
		max = Max(max, p);
		min = Min(min, p);
	}
	void reset()
	{
		max = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		min = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
	}
};

// Class for loading and storing static model meshes and updating the scene with them
class StaticModel
{
public:
	// Vector of pointers to mesh objects belonging to the static model
	std::vector<Mesh*> meshes;

	// Loads a static model from a file using the GEMLoader and initializes mesh objects from the loaded data
	void load(Core* core, std::string filename, Scene* scene)
	{
		GEMLoader::GEMModelLoader loader;
		std::vector<GEMLoader::GEMMesh> gemmeshes;
		// Load GEM mesh data from file
		loader.load(filename, gemmeshes);
		// Loop through each loaded GEM mesh
		for (int i = 0; i < gemmeshes.size(); i++)
		{
			Mesh* mesh = new Mesh();
			std::vector<STATIC_VERTEX> vertices;
			// Copy vertex data from the GEM mesh to a vector of STATIC_VERTEX
			for (int n = 0; n < gemmeshes[i].verticesStatic.size(); n++)
			{
				STATIC_VERTEX v;
				memcpy(&v, &gemmeshes[i].verticesStatic[n], sizeof(STATIC_VERTEX));
				vertices.push_back(v);
				use<SceneBounds>().extend(v.pos);
			}
			// Initialize the mesh with the core context, vertices and indices
			mesh->init(core, vertices, gemmeshes[i].indices);
			// Store the mesh pointer in the vector
			meshes.push_back(mesh);
			// Add mesh data (vertices and indices) to the scene
			scene->addMeshData(filename + std::to_string(i), vertices, gemmeshes[i].indices);
		}
	}
	// Updates the world transformation for each mesh and adds them to the scene
	void updateWorld(Scene* scene, Matrix& w)
	{
		// Loop through each stored mesh and add it to the scene with the transformation
		for (int i = 0; i < meshes.size(); i++)
		{
			scene->addMesh(meshes[i], w);
		}
	}
};

// Class for managing loading and caching of static models so that each model is loaded
// only once and then reused for multiple scene instances
class StaticModelManager
{
public:
	// Map from filenames to pointers of StaticModel objects
	std::map<std::string, StaticModel*> meshes;

	// Loads a static model if not already loaded, then adds its instances to the scene
	void load(Core* core, std::string filename, Scene* scene, Textures* textures, InstanceData meshInstanceData, Matrix& w)
	{
		// Check if the model is not already loaded
		if (meshes.find(filename) == meshes.end())
		{
			StaticModel* model = new StaticModel();
			model->load(core, filename, scene);
			meshes.insert({ filename, model });
		}
		StaticModel* model = meshes[filename];
		// Add each mesh instance to the scene
		for (int i = 0; i < model->meshes.size(); i++)
		{
			scene->addInstance(filename + std::to_string(i), meshInstanceData);
			scene->addMesh(model->meshes[i], w);
		}
	}
};

// Loads mesh data from a file and converts triangles into area light data
void loadAsAreaLights(std::string filename, Matrix transform, std::vector<AreaLightData>& lightData)
{
	GEMLoader::GEMModelLoader loader;
	std::vector<GEMLoader::GEMMesh> gemmeshes;
	// Load GEM mesh data from file
	loader.load(filename, gemmeshes);
	// Process each mesh
	for (int i = 0; i < gemmeshes.size(); i++)
	{
		// Process each triangle (assumed 3 indices per triangle)
		for (int n = 0; n < gemmeshes[i].indices.size(); n = n + 3)
		{
			AreaLightData data;
			// Copy triangle vertex positions into area light data
			memcpy(&data.v1, &gemmeshes[i].verticesStatic[gemmeshes[i].indices[n]].position, sizeof(Vec3));
			memcpy(&data.v2, &gemmeshes[i].verticesStatic[gemmeshes[i].indices[n + 1]].position, sizeof(Vec3));
			memcpy(&data.v3, &gemmeshes[i].verticesStatic[gemmeshes[i].indices[n + 2]].position, sizeof(Vec3));
			// Compute edge vectors
			Vec3 e1 = data.v3 - data.v2;
			Vec3 e2 = data.v1 - data.v3;
			// Calculate the face normal and normalize it
			data.normal = Cross(e1, e2).normalize();
			Vec3 v1n;
			// Get the stored normal from the first vertex
			memcpy(&v1n, &gemmeshes[i].verticesStatic[gemmeshes[i].indices[n]].normal, sizeof(Vec3));
			// Ensure the computed normal faces the same direction as the stored vertex normal
			data.normal = data.normal * (Dot(v1n, data.normal) > 0 ? 1.0f : -1.0f);
			// Apply the transformation to the vertices and normal
			data.v1 = transform.mulPoint(data.v1);
			data.v2 = transform.mulPoint(data.v2);
			data.v3 = transform.mulPoint(data.v3);
			data.normal = transform.mulVec(data.normal);
			data.normal = data.normal.normalize();
			// Add the area light data to the vector
			lightData.push_back(data);
		}
	}
}

// Loads an instance of a model, sets up material properties and adds the instance
// and its associated lights (if any) to the scene
void loadInstance(Core* core, std::string sceneName, GEMLoader::GEMInstance& instance, Scene* scene, Textures* textures)
{
	std::vector<GEMLoader::GEMMesh> meshes;
	InstanceData meshInstanceData;
	// Construct the file name for the reflectance texture
	std::string reflectanceTextureFilename = sceneName + "/" + instance.material.find("reflectance").getValue("");
	// Load the texture if it is not already present
	if (textures->contains(reflectanceTextureFilename) == 0)
	{
		textures->load(core, reflectanceTextureFilename);
	}
	// Update the texture ID in the instance data
	meshInstanceData.updatetextureID(textures->find(reflectanceTextureFilename));
	// Set BSDF type based on material properties
	if (instance.material.find("bsdf").getValue("") == "diffuse")
	{
		meshInstanceData.updateBSDFType(0);
	}
	if (instance.material.find("emission").getValue("") != "")
	{
		meshInstanceData.updateBSDFType(1);
		// Retrieve emission color values and store them
		instance.material.find("emission").getValuesAsVector3(meshInstanceData.bsdfData[0], meshInstanceData.bsdfData[1], meshInstanceData.bsdfData[2]);
	}
	if (instance.material.find("bsdf").getValue("") == "orennayar")
	{
		meshInstanceData.updateBSDFType(2);
		// Set the alpha value for the Oren-Nayar model
		meshInstanceData.bsdfData[0] = instance.material.find("alpha").getValue(1.0f);
	}
	if (instance.material.find("bsdf").getValue("") == "mirror")
	{
		meshInstanceData.updateBSDFType(3);
	}
	if (instance.material.find("bsdf").getValue("") == "glass")
	{
		meshInstanceData.updateBSDFType(4);
		// Set internal and external index of refraction for glass
		meshInstanceData.bsdfData[0] = instance.material.find("intIOR").getValue(1.33f);
		meshInstanceData.bsdfData[1] = instance.material.find("extIOR").getValue(1.0f);
	}
	if (instance.material.find("bsdf").getValue("") == "plastic")
	{
		meshInstanceData.updateBSDFType(5);
		// Set IOR values and roughness for plastic
		meshInstanceData.bsdfData[0] = instance.material.find("intIOR").getValue(1.33f);
		meshInstanceData.bsdfData[1] = instance.material.find("extIOR").getValue(1.0f);
		meshInstanceData.bsdfData[2] = instance.material.find("roughness").getValue(1.0f);
	}
	if (instance.material.find("bsdf").getValue("") == "dielectric")
	{
		meshInstanceData.updateBSDFType(6);
		// Set IOR values and roughness for dielectric materials
		meshInstanceData.bsdfData[0] = instance.material.find("intIOR").getValue(1.33f);
		meshInstanceData.bsdfData[1] = instance.material.find("extIOR").getValue(1.0f);
		meshInstanceData.bsdfData[2] = instance.material.find("roughness").getValue(1.0f);
	}
	if (instance.material.find("bsdf").getValue("") == "conductor")
	{
		meshInstanceData.updateBSDFType(7);
		// Retrieve complex refractive index values (eta and k) and roughness for conductor materials
		instance.material.find("eta").getValuesAsVector3(meshInstanceData.bsdfData[0], meshInstanceData.bsdfData[1], meshInstanceData.bsdfData[2]);
		instance.material.find("k").getValuesAsVector3(meshInstanceData.bsdfData[3], meshInstanceData.bsdfData[4], meshInstanceData.bsdfData[5]);
		meshInstanceData.bsdfData[6] = instance.material.find("roughness").getValue(1.0f);
	}
	// Handle coating properties if coating thickness is greater than zero
	if (instance.material.find("coatingThickness").getValue(0) > 0)
	{
		instance.material.find("coatingSigmaA").getValuesAsVector3(meshInstanceData.coatingData[0], meshInstanceData.coatingData[1], meshInstanceData.coatingData[2]);
		meshInstanceData.coatingData[3] = instance.material.find("coatingIntIOR").getValue(1.33f);
		meshInstanceData.coatingData[4] = instance.material.find("coatingExtIOR").getValue(1.0f);
		meshInstanceData.coatingData[5] = instance.material.find("coatingThickness").getValue(0.0f);
	}
	// Copy the transformation matrix from the instance
	Matrix transform;
	memcpy(transform.m, instance.w.m, 16 * sizeof(float));
	// Load the static model using the StaticModelManager and add it to the scene
	use<StaticModelManager>().load(core, sceneName + "/" + instance.meshFilename, scene, textures, meshInstanceData, transform);
	// If the material has emission properties, create area lights from the mesh
	if (instance.material.find("emission").getValue("") != "")
	{
		std::vector<AreaLightData> lightData;
		loadAsAreaLights(sceneName + "/" + instance.meshFilename, transform, lightData);
		// Set the emission data and add each light to the scene
		for (int i = 0; i < lightData.size(); i++)
		{
			memcpy(&lightData[i].Le, &meshInstanceData.bsdfData[0], 3 * sizeof(float));
			scene->addLight(lightData[i]);
		}
	}
}

// Loads the scene's width and height from the JSON scene configuration file
void loadWidthAndHeight(std::string sceneName, int& width, int& height)
{
	GEMLoader::GEMScene gemscene;
	// Load the scene JSON file
	gemscene.load(sceneName + "/scene.json");
	// Retrieve width and height properties with default values if not found
	width = gemscene.findProperty("width").getValue(1920);
	height = gemscene.findProperty("height").getValue(1080);
}

// Loads the entire scene configuration including camera setup, scene geometry,
// environment maps and instance data from the JSON scene configuration file
void loadScene(Core* core, Scene* scene, Textures* textures, Camera* camera, std::string sceneName)
{
	GEMLoader::GEMScene gemscene;
	// Load scene configuration from JSON file
	gemscene.load(sceneName + "/scene.json");
	// Retrieve scene dimensions and camera field of view
	int width = gemscene.findProperty("width").getValue(1920);
	int height = gemscene.findProperty("height").getValue(1080);
	float fov = gemscene.findProperty("fov").getValue(45.0f);
	// Create a perspective projection matrix
	Matrix P = Matrix::perspective(0.001f, 10000.0f, (float)width / (float)height, fov);
	Vec3 from;
	Vec3 to;
	Vec3 up;
	// Retrieve camera position and orientation parameters
	gemscene.findProperty("from").getValuesAsVector3(from.x, from.y, from.z);
	gemscene.findProperty("to").getValuesAsVector3(to.x, to.y, to.z);
	gemscene.findProperty("up").getValuesAsVector3(up.x, up.y, up.z);
	// Create a view matrix using the camera parameters
	Matrix V = Matrix::lookAt(from, to, up);
	// Optionally flip the projection matrix along the X-axis if required
	int flip = gemscene.findProperty("flipX").getValue(0);
	if (flip == 1)
	{
		P.a[0][0] = -P.a[0][0];
	}
	// Initialize the camera with the projection matrix and viewport dimensions
	camera->init(P, width, height);
	camera->initView(V);

	// Load all model instances defined in the scene
	for (int i = 0; i < gemscene.instances.size(); i++)
	{
		loadInstance(core, sceneName, gemscene.instances[i], scene, textures);
	}
	// Load and assign the environment map if available
	if (gemscene.findProperty("envmap").getValue("") != "")
	{
		scene->environmentMap = textures->loadFromFile(core, sceneName + "/" + gemscene.findProperty("envmap").getValue(""));
		scene->envLum = 1.0f;
	} else
	{
		// Use a default black environment
		float env[3] = { 0, 0, 0 };
		scene->environmentMap = textures->loadFromMemory(core, 1, 1, 3, env);
		scene->envLum = 0;
	}
	// Set the camera movement speed
	//Vec3 size = use<SceneBounds>().max - use<SceneBounds>().min;
	camera->moveSpeed = 0.1f;// size.length() * 0.05f;
}
