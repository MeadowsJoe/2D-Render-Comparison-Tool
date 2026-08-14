#pragma once

#include <fstream>
#include <iomanip>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define __STDC_LIB_EXT1__
#include "stb_image_write.h"

#include "Params.h"

class PerfLogger
{
public:
	std::ofstream outfile;

	std::filesystem::path buildDir(const Params& params)
	{
		std::string resFolder = std::to_string(params.width) + "x" + std::to_string(params.height);
		std::string spriteFolder = std::to_string(params.nOnScreen + params.nOffScreen) + "sprites";

		std::filesystem::path dir = std::filesystem::path("PerformanceLogs") / resFolder / spriteFolder;
		std::filesystem::create_directories(dir);
		return dir;
	}

	void open(int scene, const std::string& mode, const Params& params)
	{
		std::filesystem::path filepath = buildDir(params) / ("Scene" + std::to_string(scene) + "_" + mode + "_" + "SS" + std::to_string(params.nShadowSamples) + "_" + "Glass" + std::to_string(params.usingGlass) + ".csv");

		outfile.open(filepath);
		outfile << std::fixed << std::setprecision(4);

		outfile << "Mode" << "," << "Frames" << "," << "ms per frame" << ","
			<< "Width" << "," << "Height" << ","
			<< "Prims On Screen" << "," << "Prims Off Screen" << ","
			<< "Shadow Samples" << "\n";
	}

	void log(const std::string& mode, float ms, int frames, const Params& params)
	{
		if (!outfile.is_open()) return;

		outfile << mode << "," << frames << "," << ms << ","
			<< params.width << "," << params.height << ","
			<< params.nOnScreen << "," << params.nOffScreen << ","
			<< params.nShadowSamples << "\n";
	}

	void close()
	{
		outfile.flush();
		outfile.close();
	}

	void screenCapture(Core* core, int scene, const std::string& mode, const Params& params)
	{
		int width = core->width;
		int height = core->height;
		int rowPitch = (width * 4 + 255) & ~255;

		core->finishCommandList();
		core->flushGraphicsQueue();
		core->resetCommandList();

		D3D12_RESOURCE_DESC readbackDesc = {};
		readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		readbackDesc.Width = rowPitch * height;
		readbackDesc.Height = 1;
		readbackDesc.DepthOrArraySize = 1;
		readbackDesc.MipLevels = 1;
		readbackDesc.Format = DXGI_FORMAT_UNKNOWN;
		readbackDesc.SampleDesc.Count = 1;
		readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		D3D12_HEAP_PROPERTIES readbackHeap = { D3D12_HEAP_TYPE_READBACK };
		ID3D12Resource* readbackBuffer;
		core->device->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &readbackDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readbackBuffer));

		Barrier::add(core->rendertarget, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE, core->graphicsCommandList);

		D3D12_TEXTURE_COPY_LOCATION dst = {};
		dst.pResource = readbackBuffer;
		dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dst.PlacedFootprint.Footprint.Width = width;
		dst.PlacedFootprint.Footprint.Height = height;
		dst.PlacedFootprint.Footprint.Depth = 1;
		dst.PlacedFootprint.Footprint.RowPitch = rowPitch;

		D3D12_TEXTURE_COPY_LOCATION src = {};
		src.pResource = core->rendertarget;
		src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src.SubresourceIndex = 0;

		core->graphicsCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

		Barrier::add(core->rendertarget, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT, core->graphicsCommandList);

		core->finishCommandList();
		core->flushGraphicsQueue();
		core->resetCommandList();

		void* mapped;
		readbackBuffer->Map(0, nullptr, &mapped);

		std::vector<unsigned char> rgb(width * height * 3);
		unsigned char* source = (unsigned char*)mapped;
		for (int y = 0; y < height; y++)
		{
			unsigned char* row = source + y * rowPitch;
			for (int x = 0; x < width; x++)
			{
				rgb[(y * width + x) * 3 + 0] = row[x * 4 + 0];
				rgb[(y * width + x) * 3 + 1] = row[x * 4 + 1];
				rgb[(y * width + x) * 3 + 2] = row[x * 4 + 2];
			}
		}
		readbackBuffer->Unmap(0, nullptr);
		readbackBuffer->Release();

		std::filesystem::path filepath = buildDir(params) / ("Scene" + std::to_string(scene) + "_" + mode + "_" + "SS" + std::to_string(params.nShadowSamples) + "_" + "Glass" + std::to_string(params.usingGlass) + ".png");

		stbi_write_png(filepath.string().c_str(), width, height, 3, rgb.data(), width * 3);
	}
};
