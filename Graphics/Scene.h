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

#include "Math.h"
#include <d3d12.h>
#include "Core.h"
#include "Shaders.h"
#include "Texture.h"

#pragma warning( disable : 6387)
#pragma warning( disable : 26495)

// Structure for static vertex data (used for non-animated meshes)
struct STATIC_VERTEX
{
    Vec3 pos;       // Position of the vertex
    Vec3 normal;    // Normal at the vertex
    Vec3 tangent;   // Tangent vector at the vertex
    float tu;       // Texture coordinate (u)
    float tv;       // Texture coordinate (v)
};

// Structure for animated vertex data (includes bone influence information)
struct ANIMATED_VERTEX
{
    Vec3 pos;             // Position of the vertex
    Vec3 normal;          // Normal at the vertex
    Vec3 tangent;         // Tangent vector at the vertex
    float tu;             // Texture coordinate (u)
    float tv;             // Texture coordinate (v)
    unsigned int bonesIDs[4];  // IDs of influencing bones
    float boneWeights[4];      // Weights for each bone influence
};

// Structure for area light data (defined by three vertices and a normal)
struct AreaLightData
{
    Vec3 v1;      // First vertex of the light area
    Vec3 v2;      // Second vertex of the light area
    Vec3 v3;      // Third vertex of the light area
    Vec3 normal;  // Normal vector of the light surface
    float Le[3];  // Emission radiance (RGB)
};

// Structure for per-instance data used during rendering
struct InstanceData
{
    unsigned int startIndex = 0;   // Starting index for the instance mesh
    unsigned int bsdfAlbedoID = 0;   // Encodes BSDF type and texture ID
    float bsdfData[7] = {};        // BSDF parameters
    float coatingData[6] = {};     // Coating parameters

    // Update the BSDF type (stored in the upper 16 bits of bsdfAlbedoID)
    void updateBSDFType(int type)
    {
        bsdfAlbedoID = bsdfAlbedoID | (type << 16);
    }

    // Update the texture ID (stored in the lower 16 bits of bsdfAlbedoID)
    void updatetextureID(int ID)
    {
        bsdfAlbedoID = bsdfAlbedoID | (ID & 0xFFFF);
    }
};

// Represents a mesh with its vertex/index buffers and a BLAS for ray tracing.
class Mesh
{
public:
    ID3D12Resource* vertexBuffer;   // GPU resource for vertex data
    ID3D12Resource* indexBuffer;    // GPU resource for index data
    ID3D12Resource* blas;           // Bottom Level Acceleration Structure for ray tracing

    // Initialize mesh resources using provided vertex and index data.
    // This method creates GPU buffers, copies the data, and builds a BLAS.
    void init(Core* core, void* vertices, int vertexSizeInBytes, int numVertices, unsigned int* indices, int numIndices)
    {
        // Set up heap properties for upload heap using {} initializer
        D3D12_HEAP_PROPERTIES heapDesc{};
        heapDesc.Type = D3D12_HEAP_TYPE_UPLOAD;

        // Create resource description for vertex buffer using {} initializer
        D3D12_RESOURCE_DESC bd{};
        bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bd.Width = vertexSizeInBytes * numVertices;
        bd.Height = 1;
        bd.DepthOrArraySize = 1;
        bd.MipLevels = 1;
        bd.SampleDesc.Count = 1;
        bd.SampleDesc.Quality = 0;
        bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        // Create the vertex buffer resource
        core->device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_COMMON, NULL, IID_PPV_ARGS(&vertexBuffer));

        // Map and copy vertex data to the GPU buffer
        void* data;
        vertexBuffer->Map(0, NULL, &data);
        memcpy(data, vertices, vertexSizeInBytes * numVertices);
        vertexBuffer->Unmap(0, NULL);

        // Update resource description for index buffer (using size of int per index)
        bd.Width = sizeof(int) * numIndices;
        core->device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_COMMON, NULL, IID_PPV_ARGS(&indexBuffer));

        // Map and copy index data to the GPU buffer
        indexBuffer->Map(0, NULL, &data);
        memcpy(data, indices, sizeof(int) * numIndices);
        indexBuffer->Unmap(0, NULL);

        // Set up the geometry description for the BLAS using {} initializer
        D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc{};
        geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE; // Mark as opaque geometry
        geometryDesc.Triangles.Transform3x4 = 0;
        geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
        geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geometryDesc.Triangles.IndexCount = numIndices;
        geometryDesc.Triangles.VertexCount = numVertices;
        geometryDesc.Triangles.IndexBuffer = indexBuffer->GetGPUVirtualAddress();
        geometryDesc.Triangles.VertexBuffer.StartAddress = vertexBuffer->GetGPUVirtualAddress();
        geometryDesc.Triangles.VertexBuffer.StrideInBytes = vertexSizeInBytes;

        // Set up inputs for building the BLAS using {} initializer
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        inputs.NumDescs = 1;
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.pGeometryDescs = &geometryDesc;

        // Retrieve prebuild info for the BLAS
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo{};
        core->device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);

        // Create scratch and result buffers in default heap for BLAS build
        heapDesc.Type = D3D12_HEAP_TYPE_DEFAULT;

        // Create scratch resource for BLAS build
        ID3D12Resource* buildResouce;
        bd.Width = prebuildInfo.ScratchDataSizeInBytes;
        bd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        core->device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_COMMON, NULL, IID_PPV_ARGS(&buildResouce));

        // Create BLAS result resource
        bd.Width = prebuildInfo.ResultDataMaxSizeInBytes;
        bd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        core->device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, NULL, IID_PPV_ARGS(&blas));

        // Set up the BLAS build description using {} initializer
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
        buildDesc.DestAccelerationStructureData = blas->GetGPUVirtualAddress();
        buildDesc.Inputs = inputs;
        buildDesc.ScratchAccelerationStructureData = buildResouce->GetGPUVirtualAddress();

        // Build the BLAS using the graphics command list
        core->graphicsCommandAllocator->Reset();
        core->graphicsCommandList->Reset(core->graphicsCommandAllocator, NULL);
        core->graphicsCommandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, NULL);
        core->graphicsCommandList->Close();
        core->graphicsQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&core->graphicsCommandList);

        // Wait for the GPU to finish processing
        core->flushGraphicsQueue();

        // Release the temporary scratch resource
        buildResouce->Release();
    }

    // Overloaded init for static vertices using std::vector
    void init(Core* core, std::vector<STATIC_VERTEX> vertices, std::vector<unsigned int> indices)
    {
        init(core, &vertices[0], sizeof(STATIC_VERTEX), (int)vertices.size(), &indices[0], (int)indices.size());
    }

    // Overloaded init for animated vertices using std::vector
    void init(Core* core, std::vector<ANIMATED_VERTEX> vertices, std::vector<unsigned int> indices)
    {
        init(core, &vertices[0], sizeof(ANIMATED_VERTEX), (int)vertices.size(), &indices[0], (int)indices.size());
    }

    // Clean up GPU resources
    void cleanUp()
    {
        indexBuffer->Release();
        vertexBuffer->Release();
        blas->Release();
    }

    // Destructor automatically releases resources
    ~Mesh()
    {
        cleanUp();
    }
};

// Wrapper class for storing a 3x4 transformation matrix used in TLAS.
class TLASTransform
{
public:
    union
    {
        float w[3][4]; // 3x4 matrix representation
        float a[12];   // Flat array representation (alternative)
    };

    TLASTransform()
    {
    }

    // Construct TLASTransform from a Matrix object (copies 12 floats)
    TLASTransform(const Matrix& m)
    {
        memcpy(a, m.m, sizeof(float) * 12);
    }
};

// Represents the complete scene including meshes, lights, and acceleration structures.
class Scene
{
public:
    // Mesh data and file metadata
    std::vector<STATIC_VERTEX> allVertices;  // Combined vertex data from all meshes
    std::vector<unsigned int> allIndices;      // Combined index data from all meshes
    std::vector<std::string> filenames;        // Filenames corresponding to mesh data
    std::vector<InstanceData> instanceData;      // Instance-specific data for rendering
    std::vector<AreaLightData> lights;         // Area light data in the scene

    // Structured buffers for GPU consumption
    StructuredBuffer allVertexBuffer;
    StructuredBuffer allIndexBuffer;
    StructuredBuffer instanceBuffer;
    StructuredBuffer areaLightBuffer;

    // Mapping from filename to index offsets and sizes
    std::map<std::string, int> indexOffset;
    std::map<std::string, int> indexSize;

    // Resources for top level acceleration structure (TLAS)
    ID3D12Resource* instances;         // GPU resource for instance descriptions
    ID3D12Resource* tlasBuildResource; // Scratch resource used during TLAS build
    ID3D12Resource* tlas;              // TLAS resource for ray tracing

    // Mesh pointers and their transforms
    std::vector<Mesh*> meshes;
    std::vector<TLASTransform> transforms;

    // Dispatch description for ray tracing
    D3D12_DISPATCH_RAYS_DESC dispatchDesc;

    // Environment map and its luminance
    Texture* environmentMap;
    float envLum;

    // Initialize TLAS and instance buffer with a maximum number of instances.
    void init(Core* core, int maxInstances)
    {
        // Set up heap properties for an upload heap using {} initializer
        D3D12_HEAP_PROPERTIES heapDesc{};
        heapDesc.Type = D3D12_HEAP_TYPE_UPLOAD;

        // Describe the instance buffer resource using {} initializer
        D3D12_RESOURCE_DESC bd{};
        bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bd.Width = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * maxInstances;
        bd.Height = 1;
        bd.DepthOrArraySize = 1;
        bd.MipLevels = 1;
        bd.SampleDesc.Count = 1;
        bd.SampleDesc.Quality = 0;
        bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        // Create the instance buffer resource
        core->device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_COMMON, NULL, IID_PPV_ARGS(&instances));

        // Set up TLAS build inputs using {} initializer
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
        inputs.NumDescs = maxInstances;
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.InstanceDescs = instances->GetGPUVirtualAddress();

        // Get prebuild info for TLAS
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo{};
        core->device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);

        // Create scratch and result buffers for the TLAS
        heapDesc.Type = D3D12_HEAP_TYPE_DEFAULT;
        bd.Width = prebuildInfo.ScratchDataSizeInBytes;
        bd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        core->device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_COMMON, NULL, IID_PPV_ARGS(&tlasBuildResource));

        bd.Width = prebuildInfo.ResultDataMaxSizeInBytes;
        bd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        core->device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &bd,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, NULL, IID_PPV_ARGS(&tlas));
    }

    // Reset scene meshes (clears the list of meshes)
    void reset()
    {
        meshes.clear();
    }

    // Add mesh data from a file.
    // This function prevents duplicate data by checking the filename.
    void addMeshData(std::string filename, std::vector<STATIC_VERTEX> vertices, std::vector<unsigned int> indices)
    {
        // Check if the mesh data for this file already exists
        for (int i = 0; i < filenames.size(); i++)
        {
            if (filenames[i] == filename)
            {
                return;
            }
        }
        int offset = (int)allVertices.size();
        // Append the new vertices
        for (int i = 0; i < vertices.size(); i++)
        {
            allVertices.push_back(vertices[i]);
        }
        int initialIndexOffset = (int)allIndices.size();
        // Append the new indices, adjusting for the vertex offset
        for (int i = 0; i < indices.size(); i++)
        {
            allIndices.push_back(indices[i] + offset);
        }
        filenames.push_back(filename);
        indexOffset[filename] = initialIndexOffset;
        indexSize[filename] = (int)indices.size();
    }

    // Add an instance of a mesh to the scene.
    // The instance's start index is determined from the filename mapping.
    void addInstance(std::string filename, InstanceData meshInstanceData)
    {
        meshInstanceData.startIndex = indexOffset[filename];
        instanceData.push_back(meshInstanceData);
    }

    // Add an area light to the scene.
    void addLight(AreaLightData lightData)
    {
        lights.push_back(lightData);
    }

    // Find instance data by filename; if not found, returns the first instance.
    InstanceData find(std::string filename)
    {
        for (int i = 0; i < filenames.size(); i++)
        {
            if (filenames[i] == filename)
            {
                return instanceData[i];
            }
        }
        return instanceData[0];
    }

    // Add a mesh along with its transformation matrix to the scene.
    void addMesh(Mesh* mesh, const Matrix& transform)
    {
        meshes.push_back(mesh);
        transforms.push_back(TLASTransform(transform));
    }

    // Build the TLAS and initialize structured buffers for rendering.
    void build(Core* core)
    {
        // Map the instance buffer and populate instance descriptors for each mesh.
        D3D12_RAYTRACING_INSTANCE_DESC* instanceDataDesc;
        instances->Map(0, NULL, reinterpret_cast<void**>(&instanceDataDesc));
        for (int i = 0; i < meshes.size(); i++)
        {
            instanceDataDesc[i] = {};
            instanceDataDesc[i].InstanceID = i;
            instanceDataDesc[i].InstanceMask = 1;
            instanceDataDesc[i].AccelerationStructure = meshes[i]->blas->GetGPUVirtualAddress();
            // Copy the 3x4 transform for this instance
            memcpy(&instanceDataDesc[i].Transform, &transforms[i], sizeof(float) * 12);
        }
        // Optionally unmap the instance buffer if needed (currently commented out)
        instances->Unmap(0, NULL);

        // Set up the TLAS build description
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
        buildDesc.DestAccelerationStructureData = tlas->GetGPUVirtualAddress();
        buildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        buildDesc.Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
        buildDesc.Inputs.NumDescs = (unsigned int)meshes.size();
        buildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        buildDesc.Inputs.InstanceDescs = instances->GetGPUVirtualAddress();
        buildDesc.ScratchAccelerationStructureData = tlasBuildResource->GetGPUVirtualAddress();

        // Build the TLAS using the command list
        core->resetCommandList();
        core->graphicsCommandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, NULL);

        // Insert a UAV barrier to ensure TLAS build is complete before proceeding
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = tlas;
        core->graphicsCommandList->ResourceBarrier(1, &barrier);

        core->graphicsCommandList->Close();
        core->graphicsQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&core->graphicsCommandList);

        core->flushGraphicsQueue();

        // Initialize structured buffers for vertex, index, instance, and light data
        allVertexBuffer.init(core, sizeof(STATIC_VERTEX), (int)allVertices.size(), &allVertices[0], &core->uavsrvHeap);
        allIndexBuffer.init(core, sizeof(unsigned int), (int)allIndices.size(), &allIndices[0], &core->uavsrvHeap);
        instanceBuffer.init(core, sizeof(InstanceData), (int)instanceData.size(), &instanceData[0], &core->uavsrvHeap);
        if (lights.size() > 0)
        {
            areaLightBuffer.init(core, sizeof(AreaLightData), (int)lights.size(), &lights[0], &core->uavsrvHeap);
        }
    }

    // Update the ray tracing dispatch description with current shader and viewport parameters.
    void updateDrawInfo(Core* core, RTShader* shader)
    {
        dispatchDesc = {};
        dispatchDesc.RayGenerationShaderRecord.StartAddress = shader->shaderList->GetGPUVirtualAddress();
        dispatchDesc.RayGenerationShaderRecord.SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        dispatchDesc.MissShaderTable.StartAddress = shader->shaderList->GetGPUVirtualAddress() + D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
        dispatchDesc.MissShaderTable.SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        dispatchDesc.HitGroupTable.StartAddress = shader->shaderList->GetGPUVirtualAddress() + (2 * D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
        dispatchDesc.HitGroupTable.SizeInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        dispatchDesc.Width = core->width;
        dispatchDesc.Height = core->height;
        dispatchDesc.Depth = 1;
    }

    // Bind resources and dispatch ray tracing commands to draw the scene.
    void draw(Core* core)
    {
        core->graphicsCommandList->SetComputeRootShaderResourceView(1, tlas->GetGPUVirtualAddress());
        core->graphicsCommandList->SetComputeRootShaderResourceView(4, allVertexBuffer.buffer->GetGPUVirtualAddress());
        core->graphicsCommandList->SetComputeRootShaderResourceView(5, allIndexBuffer.buffer->GetGPUVirtualAddress());
        core->graphicsCommandList->SetComputeRootShaderResourceView(6, instanceBuffer.buffer->GetGPUVirtualAddress());
        if (lights.size() > 0)
        {
            core->graphicsCommandList->SetComputeRootShaderResourceView(7, areaLightBuffer.buffer->GetGPUVirtualAddress());
        }
        // Calculate descriptor offset for the environment map
        unsigned int descriptorSize = core->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_GPU_DESCRIPTOR_HANDLE offset;
        offset.ptr = core->uavsrvHeap.heap->GetGPUDescriptorHandleForHeapStart().ptr + ((environmentMap->heapOffset + 2) * descriptorSize);
        core->graphicsCommandList->SetComputeRootDescriptorTable(8, offset);
        core->graphicsCommandList->DispatchRays(&dispatchDesc);
    }
};
