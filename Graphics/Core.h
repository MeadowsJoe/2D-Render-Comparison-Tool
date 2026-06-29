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

#include <d3d12.h>
#include <dxgi1_4.h>
#include <vector>

// Link necessary libraries
#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxgi")

#pragma warning( disable : 6387)

// Enable debug layer if defined
#define ENGINERT_DEBUG

// GPUFence: Wraps a D3D12 fence to help synchronize GPU operations
class GPUFence
{
public:
    ID3D12Fence* fence;
    long long value;

    // Creates the fence with an initial value of 0
    void create(ID3D12Device5* device)
    {
        value = 0;
        device->CreateFence(value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    }

    // Signals the fence on the given command queue
    void signal(ID3D12CommandQueue* queue)
    {
        static UINT64 value1 = 1;
        queue->Signal(fence, value1);
        fence->SetEventOnCompletion(value1++, nullptr);
    }

    // Destructor: Releases the fence
    ~GPUFence()
    {
        if (fence)
        {
            fence->Release();
        }
    }
};

// DescriptorHeap: Manages a D3D12 descriptor heap
class DescriptorHeap
{
public:
    ID3D12DescriptorHeap* heap;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
    unsigned int size;
    int used;

    // Initializes the descriptor heap with 'num' descriptors
    void init(ID3D12Device5* device, int num)
    {
        D3D12_DESCRIPTOR_HEAP_DESC uavcbvHeapDesc = {};
        uavcbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        uavcbvHeapDesc.NumDescriptors = num;
        uavcbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        device->CreateDescriptorHeap(&uavcbvHeapDesc, IID_PPV_ARGS(&heap));
        cpuHandle = heap->GetCPUDescriptorHandleForHeapStart();
        gpuHandle = heap->GetGPUDescriptorHandleForHeapStart();
        size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        used = 0;
    }

    // Returns the next available CPU descriptor handle
    D3D12_CPU_DESCRIPTOR_HANDLE getNextCPUHandle()
    {
        if (used > 0)
        {
            cpuHandle.ptr += size;
        }
        used++;
        return cpuHandle;
    }
};

// Barrier: Provides a static helper to add resource transition barriers
class Barrier
{
public:
    // Adds a transition barrier for 'res' from 'first' state to 'second' state
    static void add(ID3D12Resource* res, D3D12_RESOURCE_STATES first, D3D12_RESOURCE_STATES second, ID3D12GraphicsCommandList4* commandList)
    {
        D3D12_RESOURCE_BARRIER rb = {};
        rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        rb.Transition.pResource = res;
        rb.Transition.StateBefore = first;
        rb.Transition.StateAfter = second;
        commandList->ResourceBarrier(1, &rb);
    }
};

// Core: Manages device, queues, swap chain, render target, and other key resources
class Core
{
public:
    ID3D12Device5* device;
    ID3D12CommandQueue* graphicsQueue;
    ID3D12CommandQueue* copyQueue;
    ID3D12CommandQueue* computeQueue;
    IDXGISwapChain3* swapchain;
    DescriptorHeap uavsrvHeap;
    ID3D12Resource* rendertarget;
    ID3D12CommandAllocator* graphicsCommandAllocator;
    ID3D12GraphicsCommandList4* graphicsCommandList;
    ID3D12RootSignature* rootSignature;
    GPUFence graphicsQueueFence;
    int width;
    int height;
    HWND windowHandle;

    // Initializes the Direct3D device, command queues, swap chain, and related resources
    void init(HWND hwnd, int _width, int _height)
    {
        // Create DXGI Factory
        IDXGIFactory4* factory;
        CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory));

#ifdef ENGINERT_DEBUG
        // Enable the debug layer
        ID3D12Debug* debug;
        D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
        debug->EnableDebugLayer();
        debug->Release();
#endif

        // Enumerate available adapters
        int i = 0;
        IDXGIAdapter1* adapterf;
        std::vector<IDXGIAdapter1*> adapters;
        while (factory->EnumAdapters1(i, &adapterf) != DXGI_ERROR_NOT_FOUND)
        {
            adapters.push_back(adapterf);
            i++;
        }

        // Choose the adapter with the most dedicated video memory
        unsigned long long maxVideoMemory = 0;
        int useAdapterIndex = 0;
        for (int i = 0; i < adapters.size(); i++)
        {
            DXGI_ADAPTER_DESC desc;
            adapters[i]->GetDesc(&desc);
            if (desc.DedicatedVideoMemory > maxVideoMemory)
            {
                maxVideoMemory = desc.DedicatedVideoMemory;
                useAdapterIndex = i;
            }
        }
        IDXGIAdapter* adapter = adapters[useAdapterIndex];

        // Create the Direct3D device
        D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device));

        // Create command queues for graphics, copy, and compute tasks
        D3D12_COMMAND_QUEUE_DESC graphicsQueueDesc = {};
        graphicsQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        device->CreateCommandQueue(&graphicsQueueDesc, IID_PPV_ARGS(&graphicsQueue));

        D3D12_COMMAND_QUEUE_DESC copyQueueDesc = {};
        copyQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
        device->CreateCommandQueue(&copyQueueDesc, IID_PPV_ARGS(&copyQueue));

        D3D12_COMMAND_QUEUE_DESC computeQueueDesc = {};
        computeQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        device->CreateCommandQueue(&computeQueueDesc, IID_PPV_ARGS(&computeQueue));

        // Create the swap chain
        DXGI_SWAP_CHAIN_DESC1 scDesc = {};
        scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scDesc.SampleDesc.Count = 1;
        scDesc.SampleDesc.Quality = 0;
        scDesc.BufferCount = 2;
        scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        IDXGISwapChain1* swapChain1;
        factory->CreateSwapChainForHwnd(graphicsQueue, hwnd, &scDesc, nullptr, nullptr, &swapChain1);
        swapChain1->QueryInterface(&swapchain);
        swapChain1->Release();

        // Release the factory
        factory->Release();

        // Initialize the descriptor heap for UAV/SRV with space for 16384 descriptors
        uavsrvHeap.init(device, 16384);

        rendertarget = nullptr;

        // Update screen resources based on the given width and height
        updateScreenResources(_width, _height);

        // Create the command allocator and command list
        device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&graphicsCommandAllocator));
        device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&graphicsCommandList));

        // Create a GPU fence for synchronization
        graphicsQueueFence.create(device);

        // Create the root signature
        createRootSignature();

        windowHandle = hwnd;
    }

    // Updates resources that depend on the screen size
    void updateScreenResources(int _width, int _height)
    {
        width = _width;
        height = _height;
        swapchain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

        // Release the existing render target if it exists
        if (rendertarget != nullptr)
        {
            rendertarget->Release();
        }

        // Create a new render target resource for the HDR accumulation texture
        D3D12_HEAP_PROPERTIES heapDesc = {};
        heapDesc.Type = D3D12_HEAP_TYPE_DEFAULT;

        // Create a new render target resource for the tonr mapped texture
        D3D12_RESOURCE_DESC rtDesc = {};
        rtDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        rtDesc.Width = width;
        rtDesc.Height = height;
        rtDesc.DepthOrArraySize = 1;
        rtDesc.MipLevels = 1;
        rtDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        rtDesc.SampleDesc.Count = 1;
        rtDesc.SampleDesc.Quality = 0;
        rtDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &rtDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&rendertarget));

        // Create an unordered access view (UAV) for the render target
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(rendertarget, nullptr, &uavDesc, uavsrvHeap.getNextCPUHandle());
    }

    // Creates the root signature for the pipeline
    void createRootSignature()
    {
        // Descriptor range for a UAV
        D3D12_DESCRIPTOR_RANGE uavRange = {};
        uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavRange.NumDescriptors = 1;
        uavRange.BaseShaderRegister = 0;

        // Root parameter for the UAV descriptor table
        D3D12_ROOT_PARAMETER uavParam = {};
        uavParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        uavParam.DescriptorTable.NumDescriptorRanges = 1;
        uavParam.DescriptorTable.pDescriptorRanges = &uavRange;

        // Root parameter for a shader resource view (SRV)
        D3D12_ROOT_PARAMETER asParam = {};
        asParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        asParam.Descriptor.ShaderRegister = 0;
        asParam.Descriptor.RegisterSpace = 0;

        // Descriptor range for textures
        D3D12_DESCRIPTOR_RANGE textureRange = {};
        textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        textureRange.NumDescriptors = 4096; // Maximum textures
        textureRange.BaseShaderRegister = 0;
        textureRange.RegisterSpace = 1;
        textureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        // Root parameter for the texture descriptor table
        D3D12_ROOT_PARAMETER textureParam = {};
        textureParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        textureParam.DescriptorTable.NumDescriptorRanges = 1;
        textureParam.DescriptorTable.pDescriptorRanges = &textureRange;
        textureParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        // Constant buffer view (CBV) for general use
        D3D12_ROOT_PARAMETER cbvParam = {};
        cbvParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        cbvParam.Descriptor.ShaderRegister = 0;
        cbvParam.Descriptor.RegisterSpace = 0;
        cbvParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        // Constant buffer view (CBV) for scene data
        D3D12_ROOT_PARAMETER cbvParamScene = {};
        cbvParamScene.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        cbvParamScene.Descriptor.ShaderRegister = 1;
        cbvParamScene.Descriptor.RegisterSpace = 0;
        cbvParamScene.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        // Static sampler for texture sampling
        D3D12_STATIC_SAMPLER_DESC staticSampler = {};
        staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSampler.MipLODBias = 0;
        staticSampler.MaxAnisotropy = 1;
        staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
        staticSampler.MinLOD = 0.0f;
        staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
        staticSampler.ShaderRegister = 0;
        staticSampler.RegisterSpace = 0;
        staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        // Additional SRV parameters for buffers
        D3D12_ROOT_PARAMETER lightBufferParam = {};
        lightBufferParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        lightBufferParam.Descriptor.ShaderRegister = 4; // Corresponds to register t4
        lightBufferParam.Descriptor.RegisterSpace = 0;
        lightBufferParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_PARAMETER vertexBufferParam = {};
        vertexBufferParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        vertexBufferParam.Descriptor.ShaderRegister = 1; // Corresponds to register t1
        vertexBufferParam.Descriptor.RegisterSpace = 0;
        vertexBufferParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_PARAMETER indexBufferParam = {};
        indexBufferParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        indexBufferParam.Descriptor.ShaderRegister = 2; // Corresponds to register t2
        indexBufferParam.Descriptor.RegisterSpace = 0;
        indexBufferParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_PARAMETER instanceBufferParam = {};
        instanceBufferParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        instanceBufferParam.Descriptor.ShaderRegister = 3; // Corresponds to register t3
        instanceBufferParam.Descriptor.RegisterSpace = 0;
        instanceBufferParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_DESCRIPTOR_RANGE envTextureRange = {};
        envTextureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        envTextureRange.NumDescriptors = 1;
        envTextureRange.BaseShaderRegister = 5; // Corresponds to register t5
        envTextureRange.RegisterSpace = 0;
        envTextureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER envTextureParam = {};
        envTextureParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        envTextureParam.DescriptorTable.NumDescriptorRanges = 1;
        envTextureParam.DescriptorTable.pDescriptorRanges = &envTextureRange;
        envTextureParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        // Array of all root parameters
        D3D12_ROOT_PARAMETER params[] =
        {
            uavParam,
            asParam,
            cbvParam,
            textureParam,
            vertexBufferParam,
            indexBufferParam,
            instanceBufferParam,
            lightBufferParam,
            envTextureParam
        };

        D3D12_ROOT_SIGNATURE_DESC desc = {};
        desc.NumParameters = _countof(params);
        desc.pParameters = params;
        desc.NumStaticSamplers = 1;
        desc.pStaticSamplers = &staticSampler;

        // Serialize and create the root signature
        ID3DBlob* blob;
        ID3DBlob* error;
        D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, &error);
        // TODO: If 'error' is non-null, log the error message
        device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
        blob->Release();

        // Optionally, set the root signature on the command list:
        // graphicsCommandList->SetComputeRootSignature(rootSignature);
    }

    // Resets the command list for recording new commands
    void resetCommandList()
    {
        graphicsCommandAllocator->Reset();
        graphicsCommandList->Reset(graphicsCommandAllocator, nullptr);
    }

    // Closes the command list and submits it to the graphics queue
    void finishCommandList()
    {
        graphicsCommandList->Close();
        graphicsQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&graphicsCommandList);
    }

    // Begins a new frame by resetting the command list
    void beginFrame()
    {
        graphicsCommandAllocator->Reset();
        graphicsCommandList->Reset(graphicsCommandAllocator, nullptr);
    }

    // Binds the render target UAV and texture descriptor tables
    void bindRTUAV()
    {
        graphicsCommandList->SetDescriptorHeaps(1, &uavsrvHeap.heap);
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = uavsrvHeap.heap->GetGPUDescriptorHandleForHeapStart();
        graphicsCommandList->SetComputeRootDescriptorTable(0, gpuHandle);
        UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_GPU_DESCRIPTOR_HANDLE textureGpuHandle = gpuHandle;
        textureGpuHandle.ptr += descriptorSize * 2;
        graphicsCommandList->SetComputeRootDescriptorTable(3, textureGpuHandle);
    }

    // Completes the frame by copying the render target to the swap chain backbuffer and presenting
    void finishFrame()
    {
        ID3D12Resource* backbuffer;
        swapchain->GetBuffer(swapchain->GetCurrentBackBufferIndex(), IID_PPV_ARGS(&backbuffer));

        // Transition states for copy operations
        Barrier::add(rendertarget, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE, graphicsCommandList);
        Barrier::add(backbuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST, graphicsCommandList);

        // Copy render target content to the backbuffer
        graphicsCommandList->CopyResource(backbuffer, rendertarget);

        // Transition resources back to their original states
        Barrier::add(backbuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT, graphicsCommandList);
        Barrier::add(rendertarget, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, graphicsCommandList);

        backbuffer->Release();

        graphicsCommandList->Close();
        graphicsQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&graphicsCommandList);

        flushGraphicsQueue();
        swapchain->Present(1, 0);
    }

    // Flushes the graphics queue by signaling the fence
    void flushGraphicsQueue()
    {
        graphicsQueueFence.signal(graphicsQueue);
    }

    // Destructor: Releases all allocated Direct3D resources
    ~Core()
    {
        if (rootSignature)
        {
            rootSignature->Release();
        }
        if (graphicsCommandList)
        {
            graphicsCommandList->Release();
        }
        if (graphicsCommandAllocator)
        {
            graphicsCommandAllocator->Release();
        }
        if (rendertarget)
        {
            rendertarget->Release();
        }
        if (swapchain)
        {
            swapchain->Release();
        }
        if (computeQueue)
        {
            computeQueue->Release();
        }
        if (copyQueue)
        {
            copyQueue->Release();
        }
        if (graphicsQueue)
        {
            graphicsQueue->Release();
        }
        if (device)
        {
            device->Release();
        }
    }
};

// StructuredBuffer: Manages a GPU buffer for structured data
class StructuredBuffer
{
public:
    ID3D12Resource* buffer;
    D3D12_CPU_DESCRIPTOR_HANDLE srv;
    int srvIndex;

    // Uploads data to the GPU buffer using an intermediate upload buffer
    void upload(Core* core, D3D12_RESOURCE_DESC bd, void* data)
    {
        ID3D12Resource* uploadBuffer;

        D3D12_HEAP_PROPERTIES heapDesc = {};
        heapDesc.Type = D3D12_HEAP_TYPE_UPLOAD;

        // Create an upload buffer
        core->device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer));

        // Map and copy the data to the upload buffer
        void* mappedData;
        uploadBuffer->Map(0, nullptr, &mappedData);
        memcpy(mappedData, data, bd.Width);
        uploadBuffer->Unmap(0, nullptr);

        // Record commands to copy data from the upload buffer to the GPU buffer
        core->resetCommandList();
        Barrier::add(buffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST, core->graphicsCommandList);
        core->graphicsCommandList->CopyBufferRegion(buffer, 0, uploadBuffer, 0, bd.Width);
        Barrier::add(buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, core->graphicsCommandList);
        core->finishCommandList();

        core->flushGraphicsQueue();

        uploadBuffer->Release();
    }

    // Initializes the structured buffer with the given data and creates a corresponding SRV
    void init(Core* core, int elementSizeInBytes, int size, void* data, DescriptorHeap* srvHeap)
    {
        int sizeInBytes = elementSizeInBytes * size;
        D3D12_HEAP_PROPERTIES heapDesc = {};
        heapDesc.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC bd = {};
        bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bd.Width = sizeInBytes;
        bd.Height = 1;
        bd.DepthOrArraySize = 1;
        bd.MipLevels = 1;
        bd.Format = DXGI_FORMAT_UNKNOWN;
        bd.SampleDesc.Count = 1;
        bd.SampleDesc.Quality = 0;
        bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bd.Flags = D3D12_RESOURCE_FLAG_NONE;

        // Create the GPU buffer resource
        core->device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buffer));

        // Upload the initial data
        upload(core, bd, data);

        // Create a shader resource view (SRV) for this buffer
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = size;
        srvDesc.Buffer.StructureByteStride = elementSizeInBytes;
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        srv = srvHeap->getNextCPUHandle();
        srvIndex = srvHeap->used - 1;
        core->device->CreateShaderResourceView(buffer, &srvDesc, srv);
    }
};

// Template function 'use': Returns a static instance of type T
template<typename T>
T &use()
{
    static T t;
    return t;
}
