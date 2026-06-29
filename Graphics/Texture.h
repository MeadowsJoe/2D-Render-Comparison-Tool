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
#include "Core.h"
#include <string>
#include <map>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Texture Class
// Responsible for creating a GPU texture resource and handling its data upload.
class Texture
{
public:
    // Direct3D texture resource
    ID3D12Resource* tex;
    // Descriptor heap offset for the texture
    int heapOffset;

    // Uploads texture data from CPU memory to the GPU texture resource.
    void uploadData(Core* core, void* data, unsigned long long size, unsigned int widthInBytes, unsigned int rowPitchInBytes, unsigned int slicePitch, D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint)
    {
        ID3D12Resource* uploadBuffer;

        // Set up heap properties for an upload heap (CPU-accessible)
        D3D12_HEAP_PROPERTIES heapDesc;
        memset(&heapDesc, 0, sizeof(D3D12_HEAP_PROPERTIES));
        heapDesc.Type = D3D12_HEAP_TYPE_UPLOAD;

        // Define a resource description for a buffer resource
        D3D12_RESOURCE_DESC bd;
        memset(&bd, 0, sizeof(D3D12_RESOURCE_DESC));
        bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bd.Width = size;
        bd.Height = 1;
        bd.DepthOrArraySize = 1;
        bd.MipLevels = 1;
        bd.SampleDesc.Count = 1;
        bd.SampleDesc.Quality = 0;
        bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        // Create a committed resource for the upload buffer
        core->device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&uploadBuffer));

        // Map the upload buffer to CPU memory and copy the texture data into it
        char* texData;
        D3D12_RANGE readRange;
        readRange.Begin = 0;
        readRange.End = 0;
        uploadBuffer->Map(0, &readRange, (void**)&texData);
        if (widthInBytes == rowPitchInBytes)
        {
            memcpy(texData, data, size);
        } else
        {
            unsigned char* dataP = (unsigned char*)data;
            for (UINT y = 0; y < footprint.Footprint.Height; ++y)
            {
                memcpy(texData + y * footprint.Footprint.RowPitch, &dataP[y * widthInBytes], widthInBytes);
            }
        }
        uploadBuffer->Unmap(0, NULL);

        // Set up the source location for the texture copy (from the upload buffer)
        D3D12_TEXTURE_COPY_LOCATION srcLocation;
        srcLocation.pResource = uploadBuffer;
        srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLocation.PlacedFootprint = footprint;

        // Set up the destination location for the texture copy (into the texture resource)
        D3D12_TEXTURE_COPY_LOCATION dstLocation;
        dstLocation.pResource = tex;
        dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLocation.SubresourceIndex = 0;

        // Reset command allocator and command list to record copy commands
        core->graphicsCommandAllocator->Reset();
        core->graphicsCommandList->Reset(core->graphicsCommandAllocator, nullptr);

        // Record the command to copy data from the upload buffer to the texture resource
        core->graphicsCommandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, NULL);

        // Transition the texture from COPY_DEST to PIXEL_SHADER_RESOURCE for shader access
        Barrier::add(tex, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, core->graphicsCommandList);

        // Close the command list and execute it
        core->graphicsCommandList->Close();
        core->graphicsQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&core->graphicsCommandList);

        // Wait for the GPU to finish processing the copy commands
        core->flushGraphicsQueue();

        uploadBuffer->Release();
    }

    // Initializes the texture resource on the GPU and uploads the texture data.
    void init(Core* core, int width, int height, int channels, unsigned int bytesPerChannel, DXGI_FORMAT format, void* data, DescriptorHeap* srvHeap)
    {
        // Set up heap properties for default (GPU) memory
        D3D12_HEAP_PROPERTIES heapDesc;
        memset(&heapDesc, 0, sizeof(D3D12_HEAP_PROPERTIES));
        heapDesc.Type = D3D12_HEAP_TYPE_DEFAULT;

        // Define resource description for a 2D texture
        D3D12_RESOURCE_DESC textureDesc;
        memset(&textureDesc, 0, sizeof(D3D12_RESOURCE_DESC));
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        textureDesc.Width = width;
        textureDesc.Height = height;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.MipLevels = 1;
        textureDesc.Format = format;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        // Create the texture resource in the COPY_DEST state for data upload
        core->device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&tex));

        // Retrieve the resource description to calculate the upload footprint and size
        D3D12_RESOURCE_DESC desc = tex->GetDesc();
        unsigned long long size;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
        core->device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, NULL, NULL, &size);

        // Calculate the aligned row width (must be a multiple of 256 bytes)
        unsigned int alignedWidth = ((width * channels * bytesPerChannel) + 255) & ~255;
        // Upload the texture data from CPU memory to the GPU texture resource
        uploadData(core, data, size, width * channels * bytesPerChannel, alignedWidth, alignedWidth * height, footprint);

        // Create a shader resource view (SRV) for the texture so that it can be accessed in shaders
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = srvHeap->getNextCPUHandle();
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
        memset(&srvDesc, 0, sizeof(D3D12_SHADER_RESOURCE_VIEW_DESC));
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        core->device->CreateShaderResourceView(tex, &srvDesc, srvHandle);

        // Record the descriptor heap offset for this texture
        heapOffset = srvHeap->used - 3;
    }

    // Releases the texture resource if it exists.
    void free()
    {
        if (tex != NULL)
        {
            tex->Release();
        }
    }

    // Ensures that the texture resource is freed when the Texture object is destroyed.
    ~Texture()
    {
        free();
    }
};

// DXGIFormatTraits Template
// Maps specific data types to their corresponding DXGI_FORMAT values.
template<typename T>
struct DXGIFormatTraits;

template<>
struct DXGIFormatTraits<unsigned char>
{
    static constexpr DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
};

template<>
struct DXGIFormatTraits<float>
{
    static constexpr DXGI_FORMAT format = DXGI_FORMAT_R32G32B32_FLOAT;
};

// Textures Class
// Manages a collection of textures, including loading from memory and file, and handling their cleanup.
class Textures
{
public:
    // Map to store textures by filename
    std::map<std::string, Texture*> textures;

    // Loads a texture from memory. If the provided data rows are not aligned, it
    // performs a row-by-row copy to align them.
    template<typename T>
    Texture* loadFromMemory(Core* core, int width, int height, int channels, T* data)
    {
        Texture* texture = new Texture();
        // Calculate aligned row width (must be a multiple of 256 bytes)
        unsigned int alignedWidth = ((width * channels) + 255) & ~255;
        const DXGI_FORMAT format = DXGIFormatTraits<T>::format;
        texture->init(core, width, height, channels, sizeof(T), format, data, &core->uavsrvHeap);
        return texture;
    }

    // Loads a texture from a file. Supports HDR images and standard images.
    Texture* loadFromFile(Core* core, std::string filename)
    {
        int width = 0;
        int height = 0;
        int channels = 0;
        Texture* texture;
        // Check if the file is an HDR image
        if (filename.find(".hdr") != std::string::npos)
        {
            float* textureData = stbi_loadf(filename.c_str(), &width, &height, &channels, 0);
            texture = loadFromMemory(core, width, height, channels, textureData);
            stbi_image_free(textureData);
            return texture;
        }
        // Load a standard image using stb_image
        unsigned char* img = stbi_load(filename.c_str(), &width, &height, &channels, 0);
        if (channels == 3)
        {
            // Convert a 3-channel image to 4 channels by adding an alpha channel
            channels = 4;
            unsigned char* dataAll = new unsigned char[width * height * channels];
            for (int i = 0; i < (width * height); i++)
            {
                dataAll[i * 4] = img[i * 3];
                dataAll[(i * 4) + 1] = img[(i * 3) + 1];
                dataAll[(i * 4) + 2] = img[(i * 3) + 2];
                dataAll[(i * 4) + 3] = 255;
            }
            texture = loadFromMemory(core, width, height, channels, dataAll);
            delete[] dataAll;
        } else
        {
            texture = loadFromMemory(core, width, height, channels, img);
        }
        stbi_image_free(img);
        return texture;
    }

    // Loads a texture from a file if it has not already been loaded.
    void load(Core* core, std::string filename)
    {
        if (textures.find(filename) == textures.end())
        {
            Texture* texture = loadFromFile(core, filename);
            textures.insert({ filename, texture });
        }
    }

    // Returns the descriptor heap offset of a texture given its name.
    unsigned int find(std::string name)
    {
        if (textures.find(name) != textures.end())
        {
            return textures[name]->heapOffset;
        }
        return 0;
    }

    // Checks if a texture has already been loaded.
    int contains(std::string filename)
    {
        if (textures.find(filename) != textures.end())
        {
            return 1;
        }
        return 0;
    }

    // Unloads a texture by freeing its resource and removing it from the collection.
    void unload(std::string name)
    {
        textures[name]->free();
        textures.erase(name);
    }

    // Frees all loaded textures upon destruction.
    ~Textures()
    {
        for (auto it = textures.cbegin(); it != textures.cend(); )
        {
            it->second->free();
            textures.erase(it++);
        }
    }
};
