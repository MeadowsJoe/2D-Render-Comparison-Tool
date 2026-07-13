#pragma once

#include <fstream>
#include <iomanip>
#include <ctime>

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

	void open()
	{
		std::string prefix = "PerformanceLogs/Performance_Log_";

		std::time_t now = std::time(nullptr);
		std::tm my_time;
		localtime_s(&my_time, &now);
		char stamp[32];
		std::strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", &my_time);

		std::string filename = prefix + stamp + ".csv";

		outfile.open(filename);
		outfile << std::fixed << std::setprecision(4);

		outfile << "Frames" << "," << "ms per frame" << "," << "Number of Sprites" << "," << "Number of Samples" << "," << "Number of Lights" << "\n";
	}

	void log(float ms, int frames, Params params, bool breakLog = false)
	{
		if (!outfile.is_open()) return;

		if (breakLog)
		{
			outfile << "----" << "," << "----" << "," << "----" << ","
				<< "----" << "," << "----" << "\n";
			return;
		}

		outfile << frames << "," << ms << "," << params.nSprites << ","
			<< params.nSamples << "," << params.nLights << "\n";
	}

	void close()
	{
		outfile.flush();
		outfile.close();
	}
};
