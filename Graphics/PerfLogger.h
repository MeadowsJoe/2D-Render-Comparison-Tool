#pragma once

#include <fstream>
#include <iomanip>
#include <ctime>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define __STDC_LIB_EXT1__
#include "stb_image_write.h"

struct Params
{
	int nSprites;
	int nSamples;
	int nLights;
};

class PerfLogger
{
public:
	std::ofstream outfile;
	char timeStamp[32];

	void setTimestamp()
	{
		timeStamp[32] = NULL;
		std::time_t now = std::time(nullptr);
		std::tm my_time;
		localtime_s(&my_time, &now);
		std::strftime(timeStamp, sizeof(timeStamp), "%Y-%m-%d_%H-%M-%S", &my_time);
	}

	void open(std::string mode)
	{
		std::string prefix = "PerformanceLogs/" + mode + "_Log_";
		setTimestamp();
		std::string filename = prefix + timeStamp + ".csv";

		outfile.open(filename);
		outfile << std::fixed << std::setprecision(4);

		outfile << "Frames" << "," << "ms per frame" << "," << "Number of Sprites"
			<< "," << "Number of Samples" << "," << "Number of Lights" << "\n";
	}

	void log(float ms, int frames, Params params)
	{
		if (!outfile.is_open()) return;

		outfile << frames << "," << ms << "," << params.nSprites << ","
			<< params.nSamples << "," << params.nLights << "\n";
	}

	void close()
	{
		outfile.flush();
		outfile.close();
	}

	void screenCapture(Core* core, std::string mode)
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

		std::string prefix = "PerformanceLogs/"+ mode + "_SS_";
		std::string filename = prefix + timeStamp + ".png";

		stbi_write_png(filename.c_str(), width, height, 3, rgb.data(), width * 3);
	}
};
