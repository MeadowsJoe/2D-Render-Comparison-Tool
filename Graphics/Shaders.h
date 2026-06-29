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

#include <D3D12.h>
#include <dxcapi.h>
#include <D3Dcompiler.h>
#include <d3d12shader.h>
#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <vector>

#include "Core.h"

// Link necessary libraries
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "D3DCompiler.lib")

#pragma warning( disable : 26495)

// Vertex type definitions
#define VERTEXTYPE_NONE 0
#define VERTEXTYPE_STATIC 1
#define VERTEXTYPE_ANIMATED 2

// Macro for four-character codes (DFCC)
#ifndef DFCC
#define DFCC(ch0, ch1, ch2, ch3)                                \
    ((static_cast<uint32_t>(ch0))        |                     \
    (static_cast<uint32_t>(ch1) << 8)    |                     \
    (static_cast<uint32_t>(ch2) << 16)   |                     \
    (static_cast<uint32_t>(ch3) << 24))
#endif

#ifndef DFCC_DXIL
#define DFCC_DXIL DFCC('D','X','I','L')
#endif

// Structure representing a variable within a constant buffer.
struct ConstantBufferVariable
{
    unsigned int offset;
    unsigned int size;
};

// Class for managing a constant buffer.
class ConstantBuffer
{
public:
    std::string name;
    std::map<std::string, ConstantBufferVariable> constantBufferData;
    ID3D12Resource* cb;
    unsigned char* buffer;
    unsigned int cbSizeInBytes;
    int dirty;
    unsigned int shaderStage;

    // Initialize the constant buffer with the given size (in bytes)
    // The size is aligned to 256 bytes.
    void init(Core* core, unsigned int sizeInBytes)
    {
        // Align to 256 bytes
        unsigned int alignedSize = (sizeInBytes + 255) & ~255;

        // Initialize heap properties for an upload heap
        D3D12_HEAP_PROPERTIES heapDesc;
        memset(&heapDesc, 0, sizeof(D3D12_HEAP_PROPERTIES));
        heapDesc.Type = D3D12_HEAP_TYPE_UPLOAD;

        // Define the resource description for a buffer resource
        D3D12_RESOURCE_DESC bd;
        memset(&bd, 0, sizeof(D3D12_RESOURCE_DESC));
        bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bd.Width = alignedSize;
        bd.Height = 1;
        bd.DepthOrArraySize = 1;
        bd.MipLevels = 1;
        bd.SampleDesc.Count = 1;
        bd.SampleDesc.Quality = 0;
        bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        // Create the constant buffer resource
        core->device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&cb));

        // Create a constant buffer view for the resource
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = cb->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = alignedSize;
        core->device->CreateConstantBufferView(&cbvDesc, core->uavsrvHeap.getNextCPUHandle());

        // Allocate system memory to mirror the constant buffer data
        buffer = new unsigned char[alignedSize];
        cbSizeInBytes = alignedSize;
        dirty = 1;
    }

    // Update a specific variable in the constant buffer.
    void update(std::string name, void* data)
    {
        ConstantBufferVariable cbVariable = constantBufferData[name];
        memcpy(&buffer[cbVariable.offset], data, cbVariable.size);
        dirty = 1;
    }

    // Upload the constant buffer data to the GPU if it has been modified.
    // Returns 1 if an upload occurred, 0 otherwise.
    int upload(Core* core)
    {
        if (dirty == 1)
        {
            void* mapped;
            D3D12_RANGE readRange{ 0, 0 };
            cb->Map(0, &readRange, &mapped);
            memcpy(mapped, buffer, cbSizeInBytes);
            cb->Unmap(0, NULL);
            dirty = 0;
            return 1;
        }
        return 0;
    }

    // Free the GPU resource.
    void free()
    {
        cb->Release();
    }
};

// Class representing a ray tracing shader and its associated resources.
class RTShader
{
public:
    ID3D12Resource* shaderList;
    ID3D12StateObject* pso;
    std::vector<ConstantBuffer> constantBuffers;
    std::map<std::string, int> textureBindPoints;

    // Parse the shader code to initialize constant buffers.
    void initConstantBuffers(Core* core, IDxcBlob* code, std::vector<ConstantBuffer>& buffers)
    {
        // Create a container reflection instance for the DXIL container.
        IDxcContainerReflection* containerReflection;
        DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&containerReflection));
        containerReflection->Load(code);

        UINT dxilIndex;
        containerReflection->FindFirstPartKind(DFCC_DXIL, &dxilIndex);

        // Get a reflection interface for the DXIL library
        ID3D12LibraryReflection* libraryReflection;
        containerReflection->GetPartReflection(dxilIndex, IID_PPV_ARGS(&libraryReflection));

        D3D12_LIBRARY_DESC libDesc;
        libraryReflection->GetDesc(&libDesc);

        // Iterate over all functions in the shader library
        for (unsigned int index = 0; index < libDesc.FunctionCount; index++)
        {
            ID3D12FunctionReflection* functionData = libraryReflection->GetFunctionByIndex(index);
            D3D12_FUNCTION_DESC funcDesc;
            functionData->GetDesc(&funcDesc);

            // Iterate over all bound resources in the function
            for (unsigned int resourceIndex = 0; resourceIndex < funcDesc.BoundResources; resourceIndex++)
            {
                D3D12_SHADER_INPUT_BIND_DESC bindDesc;
                functionData->GetResourceBindingDesc(resourceIndex, &bindDesc);

                // If the resource is a constant buffer
                if (bindDesc.Type == D3D_SIT_CBUFFER)
                {
                    bool found = false;
                    for (int i = 0; i < buffers.size(); i++)
                    {
                        if (strcmp(buffers[i].name.c_str(), bindDesc.Name) == 0)
                        {
                            found = true;
                        }
                    }
                    if (found == false)
                    {
                        ConstantBuffer buffer;
                        ID3D12ShaderReflectionConstantBuffer* cb = functionData->GetConstantBufferByName(bindDesc.Name);
                        D3D12_SHADER_BUFFER_DESC cbDesc;
                        cb->GetDesc(&cbDesc);

                        // Set the constant buffer name
                        buffer.name = cbDesc.Name;
                        unsigned int totalSize = 0;

                        // Iterate over each variable in the constant buffer
                        for (UINT varIndex = 0; varIndex < cbDesc.Variables; varIndex++)
                        {
                            ID3D12ShaderReflectionVariable* cbVariable = cb->GetVariableByIndex(varIndex);
                            D3D12_SHADER_VARIABLE_DESC vDesc;
                            cbVariable->GetDesc(&vDesc);

                            ConstantBufferVariable bufferVariable;
                            bufferVariable.offset = vDesc.StartOffset;
                            bufferVariable.size = vDesc.Size;

                            // Insert the variable info into the constant buffer's data map
                            buffer.constantBufferData.insert({ vDesc.Name, bufferVariable });
                            totalSize += bufferVariable.size;
                        }

                        // Initialize the constant buffer with the total size
                        buffer.init(core, totalSize);
                        buffers.push_back(buffer);
                    }
                }
            }
        }
    }

    // Load the shader from the given DXC blob and create the state object.
    // Also initializes the shader list and constant buffers.
    void load(Core* core, IDxcBlob* code, ID3D12RootSignature* rootSignature)
    {
        std::vector<D3D12_STATE_SUBOBJECT> subobjects;

        // Define exports for shader entry points
        D3D12_EXPORT_DESC exports[] =
        {
            { L"RayGeneration", NULL, D3D12_EXPORT_FLAG_NONE },
            { L"Miss", NULL, D3D12_EXPORT_FLAG_NONE },
            { L"ClosestHit", NULL, D3D12_EXPORT_FLAG_NONE }
        };

        // Initialize DXIL library description using an initializer list
        D3D12_DXIL_LIBRARY_DESC libraryDesc{};
        libraryDesc.DXILLibrary.pShaderBytecode = code->GetBufferPointer();
        libraryDesc.DXILLibrary.BytecodeLength = code->GetBufferSize();
        // Add exports if names need to be made explicit
        // libraryDesc.pExports = exports;
        // libraryDesc.NumExports = 3;

        // Create a subobject for the DXIL library
        D3D12_STATE_SUBOBJECT librarySubobject{};
        librarySubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
        librarySubobject.pDesc = &libraryDesc;
        subobjects.push_back(librarySubobject);

        // Define the hit group for ray tracing
        D3D12_HIT_GROUP_DESC hitGroupDesc{};
        hitGroupDesc.ClosestHitShaderImport = L"ClosestHit";
        hitGroupDesc.HitGroupExport = L"HitGroup";
        hitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;

        D3D12_STATE_SUBOBJECT hitGroupSubobject{};
        hitGroupSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
        hitGroupSubobject.pDesc = &hitGroupDesc;
        subobjects.push_back(hitGroupSubobject);

        // Configure the shader with payload and attribute size limits
        D3D12_RAYTRACING_SHADER_CONFIG shaderConfig{};
        shaderConfig.MaxPayloadSizeInBytes = 36;  // Colour(3*4) + Throughput(3*4) + depth(4) + flags(4) + rndState(4)
        shaderConfig.MaxAttributeSizeInBytes = 16;

        D3D12_STATE_SUBOBJECT shaderConfigSubobject{};
        shaderConfigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
        shaderConfigSubobject.pDesc = &shaderConfig;
        subobjects.push_back(shaderConfigSubobject);

        // Set the maximum recursion depth for ray tracing
        D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig{};
        pipelineConfig.MaxTraceRecursionDepth = 8; // Update this value as needed

        D3D12_STATE_SUBOBJECT pipelineConfigSubobject{};
        pipelineConfigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
        pipelineConfigSubobject.pDesc = &pipelineConfig;
        subobjects.push_back(pipelineConfigSubobject);

        // Associate the global root signature with the pipeline
        D3D12_GLOBAL_ROOT_SIGNATURE globalRSDesc{};
        globalRSDesc.pGlobalRootSignature = rootSignature;

        D3D12_STATE_SUBOBJECT globalRSSubobject{};
        globalRSSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
        globalRSSubobject.pDesc = &globalRSDesc;
        subobjects.push_back(globalRSSubobject);

        // Describe the overall state object for the ray tracing pipeline
        D3D12_STATE_OBJECT_DESC psoDesc{};
        psoDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
        psoDesc.NumSubobjects = static_cast<unsigned int>(subobjects.size());
        psoDesc.pSubobjects = &subobjects[0];

        // Create the state object (pipeline state object)
        core->device->CreateStateObject(&psoDesc, IID_PPV_ARGS(&pso));

        // Create the shader table resource for ray tracing shaders

        // Initialize heap properties for an upload heap
        D3D12_HEAP_PROPERTIES heapDesc;
        memset(&heapDesc, 0, sizeof(D3D12_HEAP_PROPERTIES));
        heapDesc.Type = D3D12_HEAP_TYPE_UPLOAD;

        // Define the resource description for a buffer resource
        D3D12_RESOURCE_DESC bd;
        memset(&bd, 0, sizeof(D3D12_RESOURCE_DESC));
        bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bd.Width = 3 * D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
        bd.Height = 1;
        bd.DepthOrArraySize = 1;
        bd.MipLevels = 1;
        bd.SampleDesc.Count = 1;
        bd.SampleDesc.Quality = 0;
        bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        core->device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COMMON, NULL, IID_PPV_ARGS(&shaderList));

        // Map the shader table and copy shader identifiers into it
        char* data;
        shaderList->Map(0, NULL, reinterpret_cast<void**>(&data));

        ID3D12StateObjectProperties* psoProps;
        pso->QueryInterface(IID_PPV_ARGS(&psoProps));

        // Copy the RayGeneration shader identifier
        void* rayGenID = psoProps->GetShaderIdentifier(L"RayGeneration");
        memcpy(data, rayGenID, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        // Copy the Miss shader identifier
        void* missID = psoProps->GetShaderIdentifier(L"Miss");
        memcpy(data + D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, missID, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        // Copy the HitGroup shader identifier
        void* hitGroupID = psoProps->GetShaderIdentifier(L"HitGroup");
        memcpy(data + (2 * D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT), hitGroupID, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        psoProps->Release();
        shaderList->Unmap(0, NULL);

        // Initialize constant buffers based on shader reflection data
        initConstantBuffers(core, code, constantBuffers);
    }

    // Update a specific constant variable in a given constant buffer.
    void updateConstant(std::string constantBufferName, std::string variableName, void* data, std::vector<ConstantBuffer>& buffers)
    {
        for (int i = 0; i < buffers.size(); i++)
        {
            if (buffers[i].name == constantBufferName)
            {
                buffers[i].update(variableName, data);
                return;
            }
        }
    }

    // Overload to update constant variable using the object's constantBuffers.
    void updateConstant(std::string constantBufferName, std::string variableName, void* data)
    {
        updateConstant(constantBufferName, variableName, data, constantBuffers);
    }

    // Upload all constant buffers that have been modified.
    // Also sets the constant buffer view for the compute shader.
    void upload(Core* core)
    {
        int uploaded = 0;
        for (int i = 0; i < constantBuffers.size(); i++)
        {
            uploaded += constantBuffers[i].upload(core);
            core->graphicsCommandList->SetComputeRootConstantBufferView(2, constantBuffers[i].cb->GetGPUVirtualAddress());
        }
        if (uploaded > 0)
        {
            core->finishCommandList();
            core->flushGraphicsQueue();
            core->resetCommandList();
        }
    }

    // Free shader resources.
    void free()
    {
        shaderList->Release();
        pso->Release();
        for (auto cb : constantBuffers)
        {
            cb.free();
        }
    }
};

// Class to manage multiple shaders.
class Shaders
{
public:
    IDxcLibrary* library;
    IDxcCompiler3* compiler;
    IDxcIncludeHandler* includeHandler;
    IDxcUtils* utils;
    std::map<std::string, RTShader> shaders;

    // Initialize the DXC components.
    void init(Core* core)
    {
        DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
        utils->CreateDefaultIncludeHandler(&includeHandler);
    }

    // Read a file and return its contents as a wide string.
    std::wstring readFile(std::string filename)
    {
        std::ifstream file(filename);
        std::wstringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    // Load and compile an HLSL shader file, then store its RTShader instance.
    void load(Core* core, std::string filename)
    {
        // Check if the shader is already loaded
        if (shaders.find(filename) != shaders.end())
        {
            return;
        }

        IDxcBlobEncoding* source;
        std::wstring wfilename(filename.begin(), filename.end());
        HRESULT hr = utils->LoadFile(wfilename.c_str(), NULL, &source);
        if (FAILED(hr))
        {
            MessageBoxA(NULL, "Couldn't find HLSL file", "Error", 0);
            exit(0);
        }

        // Set shader compilation arguments (targeting library profile)
        const wchar_t* args[] =
        {
            L"-T", L"lib_6_3"
        };

        DxcBuffer sourceBuffer;
        sourceBuffer.Ptr = source->GetBufferPointer();
        sourceBuffer.Size = source->GetBufferSize();
        sourceBuffer.Encoding = 0;

        // Compile the shader source code
        IDxcOperationResult* res;
        hr = compiler->Compile(&sourceBuffer, args, 2, includeHandler, IID_PPV_ARGS(&res));
        if (FAILED(hr))
        {
            if (res)
            {
                IDxcBlobEncoding* errors;
                res->GetErrorBuffer(&errors);
                MessageBoxA(NULL, (const char*)errors->GetBufferPointer(), "Compilation Error", 0);
                exit(0);
            }
        }
        res->GetStatus(&hr);
        if (FAILED(hr))
        {
            if (res)
            {
                IDxcBlobEncoding* errors;
                res->GetErrorBuffer(&errors);
                MessageBoxA(core->windowHandle, (const char*)errors->GetBufferPointer(), "Compilation Error", 0);
                exit(0);
            }
        }
        IDxcBlob* code;
        res->GetResult(&code);

        // Create an RTShader instance and load it
        RTShader shader;
        shader.load(core, code, core->rootSignature);
        shaders.insert({ filename, shader });
    }

    // Find a loaded shader by filename.
    RTShader* find(std::string filename)
    {
        auto it = shaders.find(filename);
        if (it == shaders.end())
        {
            return NULL;
        }
        return &it->second;
    }

    // Update a constant variable in a specific shader's constant buffer.
    void updateConstant(std::string filename, std::string constantBufferName, std::string variableName, void* data)
    {
        if (shaders.find(filename) == shaders.end())
        {
            return;
        }
        shaders[filename].updateConstant(constantBufferName, variableName, data);
    }

    // Apply a shader by setting up the root signature and pipeline state.
    void apply(Core* core, std::string filename)
    {
        if (shaders.find(filename) == shaders.end())
        {
            return;
        }
        core->graphicsCommandList->SetComputeRootSignature(core->rootSignature);
        shaders[filename].upload(core);
        core->graphicsCommandList->SetComputeRootSignature(core->rootSignature);
        core->graphicsCommandList->SetPipelineState1(shaders[filename].pso);
    }

    // Destructor releases all DXC resources and shaders.
    ~Shaders()
    {
        library->Release();
        compiler->Release();
        includeHandler->Release();
        utils->Release();
        for (auto it = shaders.begin(); it != shaders.end(); )
        {
            it->second.free();
            shaders.erase(it++);
        }
    }
};
