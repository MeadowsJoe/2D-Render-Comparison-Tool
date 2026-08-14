#pragma once


#include "Core.h"
#include "Scene.h"
#include "Texture.h"
#include "Sprites.h"
#include "Camera.h"
#include "Shaders.h"

#include <vector>
#include <string>
#include <wrl/client.h>

struct RasterInstanceData
{
	ThreeFourTransform transform;
	unsigned int textureID;
	unsigned int pad[3];
};

struct CameraCB
{
	Matrix viewProj;
};

class RasterSystem
{
public:
	ID3D12RootSignature* rootSignature;
	ID3D12PipelineState* pso;
	ID3D12Resource* depthBuffer;
	ID3D12DescriptorHeap* dsvHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
	ID3D12Resource* instanceBuffer;
	void* mappedPtr;
	ID3D12Resource* quadVertexBuffer;
	ID3D12Resource* quadIndexBuffer;
	D3D12_VERTEX_BUFFER_VIEW quadVBV;
	D3D12_INDEX_BUFFER_VIEW quadIBV;
	int instanceCount;
	int quadIndexCount;
	int maxInstances = 32768;
	D3D12_GPU_DESCRIPTOR_HANDLE ibHandle;

	// Camera
	ID3D12Resource* cameraBuffer;
	void* cameraMappedPtr;
	D3D12_GPU_VIRTUAL_ADDRESS cameraCBV;
	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;
	
	const FLOAT bgColor[4]{ 0.0f, 0.0f, 0.0f, 1.0f };
	SpriteSystem* spriteSys;

	void init(Core* core, Shaders* shaders, SpriteSystem* _spriteSys)
	{
		spriteSys = _spriteSys;
		createDepthBuffer(core); // need onResize() path for adapting to screen size changes
		createRootSignature(core);
		createQuadBuffers(core);
		IDxcBlob* vsBlob = shaders->compileGraphicsShader("RasSprite.hlsl", L"VSMain", L"vs_6_3");
		IDxcBlob* psBlob = shaders->compileGraphicsShader("RasSprite.hlsl", L"PSMain", L"ps_6_3");
		createPSO(core, vsBlob, psBlob);
		createInstanceBuffers(core, maxInstances);
		createViewport(core); // need onResize() path for adapting to screen size changes
		createCameraBuffer(core);
	}

	void createDepthBuffer(Core* core) 
	{
		D3D12_RESOURCE_DESC depthDesc = {};
		depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthDesc.Width = core->width;
		depthDesc.Height = core->height;
		depthDesc.DepthOrArraySize = 1;
		depthDesc.MipLevels = 1;
		depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.Format = DXGI_FORMAT_D32_FLOAT;
		clearValue.DepthStencil.Depth = 1.0f;

		D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_DEFAULT };

		core->device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &depthDesc,D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&depthBuffer));

		// DSV descriptor heap
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		core->device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		core->device->CreateDepthStencilView(depthBuffer, &dsvDesc, dsvHandle);
	}

	void createRootSignature(Core* core) {
		D3D12_DESCRIPTOR_RANGE ranges[2] = {};
		ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		ranges[0].NumDescriptors = 1;
		ranges[0].BaseShaderRegister = 0;
		ranges[0].RegisterSpace = 0;

		ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		ranges[1].NumDescriptors = 4096;
		ranges[1].BaseShaderRegister = 0;
		ranges[1].RegisterSpace = 1;

		D3D12_ROOT_PARAMETER params[3] = {};
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[0].Descriptor = { 0, 0 };
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].DescriptorTable = { 1, &ranges[0] };
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[2].DescriptorTable = { 1, &ranges[1] };
		params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_STATIC_SAMPLER_DESC sampler = {};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.ShaderRegister = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = 3;
		desc.pParameters = params;
		desc.NumStaticSamplers = 1;
		desc.pStaticSamplers = &sampler;
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ID3DBlob* blob;
		ID3DBlob* error;
		D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, &error);
		core->device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
		blob->Release();
	}

	void createPSO(Core* core, IDxcBlob* vsBlob, IDxcBlob* psBlob)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = rootSignature;
		psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
		psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

		D3D12_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		psoDesc.InputLayout = { layout, _countof(layout)};
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
		psoDesc.RasterizerState.DepthClipEnable = TRUE;

		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;
		psoDesc.SampleMask = UINT_MAX;

		core->device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
	}

	ID3D12Resource* createDefaultBuffer(Core* core, void* data, int sizeInBytes)
	{
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

		ID3D12Resource* buffer;

		// Create the GPU buffer resource
		core->device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buffer));

		// Upload the initial data
		ID3D12Resource* uploadBuffer;

		D3D12_HEAP_PROPERTIES uploadHeapDesc = {};
		uploadHeapDesc.Type = D3D12_HEAP_TYPE_UPLOAD;

		// Create an upload buffer
		core->device->CreateCommittedResource(&uploadHeapDesc, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer));

		// Map and copy the data to the upload buffer
		void* mappedData;
		uploadBuffer->Map(0, nullptr, &mappedData);
		memcpy(mappedData, data, bd.Width);
		uploadBuffer->Unmap(0, nullptr);

		// Record commands to copy data from the upload buffer to the GPU buffer
		core->resetCommandList();
		Barrier::add(buffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST, core->graphicsCommandList);
		core->graphicsCommandList->CopyBufferRegion(buffer, 0, uploadBuffer, 0, bd.Width);
		Barrier::add(buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ, core->graphicsCommandList);
		core->finishCommandList();

		core->flushGraphicsQueue();

		uploadBuffer->Release();

		return buffer;
	}

	void createQuadBuffers(Core* core)
	{
		std::vector<STATIC_VERTEX> verts;
		std::vector<unsigned int> indices;
		buildUnitQuad(verts, indices);

		quadVertexBuffer = createDefaultBuffer(core, verts.data(), static_cast<int>(verts.size() * sizeof(STATIC_VERTEX)));
		quadIndexBuffer = createDefaultBuffer(core, indices.data(), static_cast<int>(indices.size() * sizeof(unsigned int)));

		quadVBV.BufferLocation = quadVertexBuffer->GetGPUVirtualAddress();
		quadVBV.StrideInBytes = sizeof(STATIC_VERTEX);
		quadVBV.SizeInBytes = (UINT)(verts.size() * sizeof(STATIC_VERTEX));

		quadIBV.BufferLocation = quadIndexBuffer->GetGPUVirtualAddress();
		quadIBV.Format = DXGI_FORMAT_R32_UINT;
		quadIBV.SizeInBytes = (UINT)(indices.size() * sizeof(unsigned int));
		quadIndexCount = (int)indices.size();
	}

	void uploadInstanceBuffer(Core*)
	{
		std::vector<RasterInstanceData> frameArray;
		frameArray.resize(spriteSys->sprites.size());
		for (int i = 0; i < frameArray.size(); i++)
		{
			frameArray[i].transform = makeTransform(spriteSys->sprites[i].pos, spriteSys->sprites[i].w, spriteSys->sprites[i].h);
			frameArray[i].textureID = spriteSys->sprites[i].textureID;
		}

		memcpy(mappedPtr, frameArray.data(), frameArray.size() * sizeof(RasterInstanceData));
		instanceCount = static_cast<int>(spriteSys->sprites.size());
	}

	void createInstanceBuffers(Core* core, int _maxInstances)
	{
		D3D12_RESOURCE_DESC bd = {};
		bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bd.Width = _maxInstances * sizeof(RasterInstanceData);
		bd.Height = 1;
		bd.DepthOrArraySize = 1;
		bd.MipLevels = 1;
		bd.Format = DXGI_FORMAT_UNKNOWN;
		bd.SampleDesc.Count = 1;
		bd.SampleDesc.Quality = 0;
		bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		bd.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES uploadHeapDesc = {};
		uploadHeapDesc.Type = D3D12_HEAP_TYPE_UPLOAD;

		// Create an upload buffer
		core->device->CreateCommittedResource(&uploadHeapDesc, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&instanceBuffer));

		// Map and copy the data to the upload buffer
		instanceBuffer->Map(0, nullptr, &mappedPtr);

		// Create a shader resource view (SRV) for this buffer
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = _maxInstances;
		srvDesc.Buffer.StructureByteStride = sizeof(RasterInstanceData);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		D3D12_CPU_DESCRIPTOR_HANDLE srv = core->uavsrvHeap.getNextCPUHandle();
		int srvIndex = core->uavsrvHeap.used - 1;
		ibHandle.ptr = core->uavsrvHeap.gpuHandle.ptr + srvIndex * core->uavsrvHeap.size;
		core->device->CreateShaderResourceView(instanceBuffer, &srvDesc, srv);
	}

	void createCameraBuffer(Core* core)
	{
		D3D12_RESOURCE_DESC bd = {};
		bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bd.Width = sizeof(CameraCB);
		bd.Height = 1;
		bd.DepthOrArraySize = 1;
		bd.MipLevels = 1;
		bd.Format = DXGI_FORMAT_UNKNOWN;
		bd.SampleDesc.Count = 1;
		bd.SampleDesc.Quality = 0;
		bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		bd.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES uploadHeapDesc = {};
		uploadHeapDesc.Type = D3D12_HEAP_TYPE_UPLOAD;

		// Create an upload buffer
		core->device->CreateCommittedResource(&uploadHeapDesc, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&cameraBuffer));

		// Map and copy the data to the upload buffer
		cameraBuffer->Map(0, nullptr, &cameraMappedPtr);

		cameraCBV = cameraBuffer->GetGPUVirtualAddress();
	}

	void updateCameraBuffer(Camera* camera)
	{
		CameraCB camCB;
		camCB.viewProj = (camera->view * camera->projection).transpose();
		memcpy(cameraMappedPtr, &camCB, sizeof(CameraCB));
	}

	void createViewport(Core* core)
	{
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = (FLOAT)core->width;
		viewport.Height = (FLOAT)core->height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		scissorRect.left = 0;
		scissorRect.right = core->width;
		scissorRect.top = 0;
		scissorRect.bottom = core->height;
	}

	void draw(Core* core)
	{
		Barrier::add(core->rendertarget, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RENDER_TARGET, core->graphicsCommandList);
	
		core->graphicsCommandList->OMSetRenderTargets(1, &core->rtvHandle, FALSE, &dsvHandle);
		core->graphicsCommandList->ClearRenderTargetView(core->rtvHandle, bgColor, 0, nullptr);
		core->graphicsCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);
		core->graphicsCommandList->SetDescriptorHeaps(1, &core->uavsrvHeap.heap);
		core->graphicsCommandList->SetPipelineState(pso);
		core->graphicsCommandList->SetGraphicsRootSignature(rootSignature);

		core->graphicsCommandList->SetGraphicsRootConstantBufferView(0, cameraCBV);
		core->graphicsCommandList->SetGraphicsRootDescriptorTable(1, ibHandle);

		UINT descriptorSize = core->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_GPU_DESCRIPTOR_HANDLE textureGpuHandle = core->uavsrvHeap.gpuHandle;
		textureGpuHandle.ptr += descriptorSize * 2;
		core->graphicsCommandList->SetGraphicsRootDescriptorTable(2, textureGpuHandle);

		core->graphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		core->graphicsCommandList->IASetVertexBuffers(0, 1, &quadVBV);
		core->graphicsCommandList->IASetIndexBuffer(&quadIBV);

		core->graphicsCommandList->RSSetViewports(1, &viewport);
		core->graphicsCommandList->RSSetScissorRects(1, &scissorRect);

		core->graphicsCommandList->DrawIndexedInstanced(quadIndexCount, instanceCount, 0, 0, 0);

		Barrier::add(core->rendertarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, core->graphicsCommandList);
	}

	~RasterSystem()
	{
		if (pso) pso->Release();
		if (rootSignature) rootSignature->Release();
		if (depthBuffer) depthBuffer->Release();
		if (dsvHeap) dsvHeap->Release();
		if (instanceBuffer) instanceBuffer->Release();
		if (cameraBuffer) cameraBuffer->Release();
		if (quadVertexBuffer) quadVertexBuffer->Release();
		if (quadIndexBuffer) quadIndexBuffer->Release();
	}
};