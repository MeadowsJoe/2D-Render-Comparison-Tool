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
#include <cmath>

// The Camera class manages 3D transformations for a scene
class Camera
{
public:
	Matrix projection;
	Matrix inverseProjection;
	Matrix view;
	Matrix inverseView;
	Vec3 position;
	Vec3 forward;
	Vec3 up;
	float moveSpeed;
	int width;
	int height;

	// Initializes the projection and related values
	void init(Matrix P, int _width, int _height)
	{
		projection = P;
		inverseProjection = P.invert();
		width = _width;
		height = _height;
		inverseProjection = inverseProjection.transpose();
	}

	// Sets the view matrix and calculates the necessary inverse transforms
	void initView(Matrix _v)
	{
		view = _v;
		inverseView = view.invert();
		inverseView = inverseView.transpose();
		position = inverseView.extractPosition();
		forward = -Vec3(view.a[2][0], view.a[2][1], view.a[2][2]).normalize();
		up = Vec3(view.a[1][0], view.a[1][1], view.a[1][2]).normalize();
		updateViewMatrix();
	}

	// Moves the camera forward
	void moveForward()
	{
		position += forward * moveSpeed;
		updateViewMatrix();
	}

	// Moves the camera backward
	void moveBackward()
	{
		position -= forward * moveSpeed;
		updateViewMatrix();
	}

	// Moves the camera to the right
	void moveRight()
	{
		Vec3 right = Cross(forward, up).normalize();
		position += right * moveSpeed;
		updateViewMatrix();
	}

	// Moves the camera to the left
	void moveLeft()
	{
		Vec3 right = Cross(forward, up).normalize();
		position -= right * moveSpeed;
		updateViewMatrix();
	}

	// Adjusts the camera orientation based on mouse movement
	void updateLookDirection(float dx, float dy, float sensitivity)
	{
		Vec3 right = Cross(forward, up).normalize();
		float pitchAngle = dy * sensitivity;
		Matrix pitchMatrix = Matrix::rotateAxis(right, pitchAngle);
		forward = pitchMatrix.mulVec(forward).normalize();

		float yawAngle = dx * sensitivity;
		Matrix yawMatrix = Matrix::rotateAxis(up, yawAngle);
		forward = yawMatrix.mulVec(forward).normalize();

		updateViewMatrix();
	}

	// Returns the current forward direction
	Vec3 dir()
	{
		return forward;
	}

	// Updates the view matrix and its inverse
	void updateViewMatrix()
	{
		view = Matrix::lookAt(position, position + forward, up);
		inverseView = view.invert();
		inverseView = inverseView.transpose();
	}
};