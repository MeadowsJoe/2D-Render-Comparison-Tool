/********************************************************************************
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
********************************************************************************/

#pragma once

#include <Windows.h>

#pragma warning( disable : 26495)

// Timer class used to measure elapsed time between calls
class Timer
{
public:
	LARGE_INTEGER prev;
	LARGE_INTEGER now;
	LARGE_INTEGER freq;

	Timer()
	{
		// Retrieves the frequency of the high-resolution performance counter
		QueryPerformanceFrequency(&freq);

		// Initializes the previous counter value
		QueryPerformanceCounter(&prev);
	}

	float dt()
	{
		// Retrieves the current counter value
		QueryPerformanceCounter(&now);

		// Calculates the elapsed time in seconds since the last dt call
		float dtn = ((float)(now.QuadPart - prev.QuadPart) / (float)freq.QuadPart);

		// Updates the previous counter to the current one
		prev = now;

		// Returns the elapsed time
		return dtn;
	}
};
