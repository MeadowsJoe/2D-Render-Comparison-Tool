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

#include <cmath>
#include <algorithm>

#define SQ(x) ((x) * (x))

template<typename T>
static T clamp(const T value, const T minValue, const T maxValue)
{
	return std::max(std::min(value, maxValue), minValue);
}

class Vec3
{
public:
	union{
	struct{
	float x;
	float y;
	float z;
	};
	float coords[3];
	};
	Vec3() { x = 0; y = 0; z = 0; }
	Vec3(float _x, float _y, float _z) {x = _x; y = _y; z = _z; }
	Vec3 operator+(const Vec3 &v) const { return Vec3(x + v.x, y + v.y, z + v.z); }
	Vec3 operator-(const Vec3& v) const { return Vec3(x - v.x, y - v.y, z - v.z); }
	Vec3 operator*(const Vec3& v) const { return Vec3(x * v.x, y * v.y, z * v.z); }
	Vec3 operator/(const Vec3& v) const { return Vec3(x / v.x, y / v.y, z / v.z); }
	Vec3& operator+=(const Vec3& v) { x += v.x, y += v.y, z += v.z; return *this; }
	Vec3& operator-=(const Vec3& v) { x -= v.x, y -= v.y, z -= v.z; return *this; }
	Vec3& operator*=(const Vec3& v) { x *= v.x, y *= v.y, z *= v.z; return *this; }
	Vec3& operator/=(const Vec3& v) { x /= v.x, y /= v.y, z /= v.z; return *this; }
	Vec3 operator*(const float v) const { return Vec3(x * v, y * v, z * v); }
	Vec3 operator/(const float v) const { float iv = 1.0f / v; return Vec3(x * iv, y * iv, z * iv); }
	Vec3& operator*=(const float v) { x *= v, y *= v, z *= v; return *this; }
	Vec3& operator/=(const float v) { float iv = 1.0f / v; x *= iv, y *= iv, z *= iv; return *this; }
	Vec3 operator-() const { return Vec3(-x, -y, -z); }
	float length() const { return sqrtf(SQ(x) + SQ(y) + SQ(z)); }
	float lengthSq() const { return (SQ(x) + SQ(y) + SQ(z)); }
	Vec3 normalize() const { float l = 1.0f / sqrtf(SQ(x) + SQ(y) + SQ(z)); return Vec3(x * l, y * l, z * l); }
	float normalize_getLength() { float l = sqrtf(SQ(x) + SQ(y) + SQ(z)); float il = 1.0f / l; x *= il; y *= il; z *= il; return l; }
};

static float Dot(const Vec3& v1, const Vec3& v2) { return ((v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z)); }
static Vec3 Cross(const Vec3 &v1, const Vec3 &v2) { return Vec3((v1.y * v2.z) - (v1.z * v2.y), (v1.z * v2.x) - (v1.x * v2.z), (v1.x * v2.y) - (v1.y * v2.x)); }
static Vec3 Max(const Vec3& v1, const Vec3& v2) { return Vec3(std::max(v1.x, v2.x), std::max(v1.y, v2.y), std::max(v1.z, v2.z)); }
static Vec3 Min(const Vec3& v1, const Vec3& v2) { return Vec3(std::min(v1.x, v2.x), std::min(v1.y, v2.y), std::min(v1.z, v2.z)); }

class alignas(64) Matrix
{
public:
	union
	{
		float a[4][4];
		float m[16];
	};
	Matrix() { identity(); }
	Matrix(float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13, float m20, float m21, float m22, float m23, float m30, float m31, float m32, float m33)
	{
		a[0][0] = m00;
		a[0][1] = m01;
		a[0][2] = m02;
		a[0][3] = m03;
		a[1][0] = m10;
		a[1][1] = m11;
		a[1][2] = m12;
		a[1][3] = m13;
		a[2][0] = m20;
		a[2][1] = m21;
		a[2][2] = m22;
		a[2][3] = m23;
		a[3][0] = m30;
		a[3][1] = m31;
		a[3][2] = m32;
		a[3][3] = m33;
	}
	void identity()
	{
		memset(m, 0, 16 * sizeof(float));
		m[0] = 1.0f;
		m[5] = 1.0f;
		m[10] = 1.0f;
		m[15] = 1.0f;
	}
	Matrix transpose()
	{
		return Matrix(a[0][0], a[1][0], a[2][0], a[3][0],
			a[0][1], a[1][1], a[2][1], a[3][1],
			a[0][2], a[1][2], a[2][2], a[3][2],
			a[0][3], a[1][3], a[2][3], a[3][3]);
	}
	float& operator[](int index)
	{
		return m[index];
	}
	static Matrix translation(const Vec3& v)
	{
		Matrix mat;
		mat.a[0][3] = v.x;
		mat.a[1][3] = v.y;
		mat.a[2][3] = v.z;
		return mat;
	}
	static Matrix scaling(const Vec3& v)
	{
		Matrix mat;
		mat.m[0] = v.x;
		mat.m[5] = v.y;
		mat.m[10] = v.z;
		return mat;
	}
	static Matrix rotateX(float theta)
	{
		Matrix mat;
		float ct = cosf(theta);
		float st = sinf(theta);
		mat.m[5] = ct;
		mat.m[6] = st;
		mat.m[9] = -st;
		mat.m[10] = ct;
		return mat;
	}
	static Matrix rotateY(float theta)
	{
		Matrix mat;
		float ct = cosf(theta);
		float st = sinf(theta);
		mat.m[0] = ct;
		mat.m[2] = -st;
		mat.m[8] = st;
		mat.m[10] = ct;
		return mat;
	}
	static Matrix rotateZ(float theta)
	{
		Matrix mat;
		float ct = cosf(theta);
		float st = sinf(theta);
		mat.m[0] = ct;
		mat.m[1] = st;
		mat.m[4] = -st;
		mat.m[5] = ct;
		return mat;
	}
	Matrix mul(const Matrix& matrix) const
	{
		Matrix ret;

		ret.m[0] = m[0] * matrix.m[0] + m[4] * matrix.m[1] + m[8] * matrix.m[2] + m[12] * matrix.m[3];
		ret.m[1] = m[1] * matrix.m[0] + m[5] * matrix.m[1] + m[9] * matrix.m[2] + m[13] * matrix.m[3];
		ret.m[2] = m[2] * matrix.m[0] + m[6] * matrix.m[1] + m[10] * matrix.m[2] + m[14] * matrix.m[3];
		ret.m[3] = m[3] * matrix.m[0] + m[7] * matrix.m[1] + m[11] * matrix.m[2] + m[15] * matrix.m[3];

		ret.m[4] = m[0] * matrix.m[4] + m[4] * matrix.m[5] + m[8] * matrix.m[6] + m[12] * matrix.m[7];
		ret.m[5] = m[1] * matrix.m[4] + m[5] * matrix.m[5] + m[9] * matrix.m[6] + m[13] * matrix.m[7];
		ret.m[6] = m[2] * matrix.m[4] + m[6] * matrix.m[5] + m[10] * matrix.m[6] + m[14] * matrix.m[7];
		ret.m[7] = m[3] * matrix.m[4] + m[7] * matrix.m[5] + m[11] * matrix.m[6] + m[15] * matrix.m[7];

		ret.m[8] = m[0] * matrix.m[8] + m[4] * matrix.m[9] + m[8] * matrix.m[10] + m[12] * matrix.m[11];
		ret.m[9] = m[1] * matrix.m[8] + m[5] * matrix.m[9] + m[9] * matrix.m[10] + m[13] * matrix.m[11];
		ret.m[10] = m[2] * matrix.m[8] + m[6] * matrix.m[9] + m[10] * matrix.m[10] + m[14] * matrix.m[11];
		ret.m[11] = m[3] * matrix.m[8] + m[7] * matrix.m[9] + m[11] * matrix.m[10] + m[15] * matrix.m[11];

		ret.m[12] = m[0] * matrix.m[12] + m[4] * matrix.m[13] + m[8] * matrix.m[14] + m[12] * matrix.m[15];
		ret.m[13] = m[1] * matrix.m[12] + m[5] * matrix.m[13] + m[9] * matrix.m[14] + m[13] * matrix.m[15];
		ret.m[14] = m[2] * matrix.m[12] + m[6] * matrix.m[13] + m[10] * matrix.m[14] + m[14] * matrix.m[15];
		ret.m[15] = m[3] * matrix.m[12] + m[7] * matrix.m[13] + m[11] * matrix.m[14] + m[15] * matrix.m[15];

		return ret;
	}
	Matrix operator*(const Matrix& matrix)
	{
		return mul(matrix);
	}
	Vec3 mulVec(const Vec3& v)
	{
		return Vec3(
			(v.x * m[0] + v.y * m[1] + v.z * m[2]),
			(v.x * m[4] + v.y * m[5] + v.z * m[6]),
			(v.x * m[8] + v.y * m[9] + v.z * m[10]));
	}
	Vec3 mulPoint(const Vec3& v)
	{
		Vec3 v1 = Vec3(
			(v.x * m[0] + v.y * m[1] + v.z * m[2]) + m[3],
			(v.x * m[4] + v.y * m[5] + v.z * m[6]) + m[7],
			(v.x * m[8] + v.y * m[9] + v.z * m[10]) + m[11]);
		float w;
		w = (m[12] * v.x) + (m[13] * v.y) + (m[14] * v.z) + m[15];
		w = 1.0f / w;
		return (v1 * w);
	}
	Matrix &operator=(const Matrix& matrix)
	{
		memcpy(m, matrix.m, sizeof(float) * 16);
		return (*this);
	}
	Matrix invert() // Unrolled inverse from MESA library
	{
		Matrix inv;
		inv[0] = m[5] * m[10] * m[15] -
			m[5] * m[11] * m[14] -
			m[9] * m[6] * m[15] +
			m[9] * m[7] * m[14] +
			m[13] * m[6] * m[11] -
			m[13] * m[7] * m[10];
		inv[4] = -m[4] * m[10] * m[15] +
			m[4] * m[11] * m[14] +
			m[8] * m[6] * m[15] -
			m[8] * m[7] * m[14] -
			m[12] * m[6] * m[11] +
			m[12] * m[7] * m[10];
		inv[8] = m[4] * m[9] * m[15] -
			m[4] * m[11] * m[13] -
			m[8] * m[5] * m[15] +
			m[8] * m[7] * m[13] +
			m[12] * m[5] * m[11] -
			m[12] * m[7] * m[9];
		inv[12] = -m[4] * m[9] * m[14] +
			m[4] * m[10] * m[13] +
			m[8] * m[5] * m[14] -
			m[8] * m[6] * m[13] -
			m[12] * m[5] * m[10] +
			m[12] * m[6] * m[9];
		inv[1] = -m[1] * m[10] * m[15] +
			m[1] * m[11] * m[14] +
			m[9] * m[2] * m[15] -
			m[9] * m[3] * m[14] -
			m[13] * m[2] * m[11] +
			m[13] * m[3] * m[10];
		inv[5] = m[0] * m[10] * m[15] -
			m[0] * m[11] * m[14] -
			m[8] * m[2] * m[15] +
			m[8] * m[3] * m[14] +
			m[12] * m[2] * m[11] -
			m[12] * m[3] * m[10];
		inv[9] = -m[0] * m[9] * m[15] +
			m[0] * m[11] * m[13] +
			m[8] * m[1] * m[15] -
			m[8] * m[3] * m[13] -
			m[12] * m[1] * m[11] +
			m[12] * m[3] * m[9];
		inv[13] = m[0] * m[9] * m[14] -
			m[0] * m[10] * m[13] -
			m[8] * m[1] * m[14] +
			m[8] * m[2] * m[13] +
			m[12] * m[1] * m[10] -
			m[12] * m[2] * m[9];
		inv[2] = m[1] * m[6] * m[15] -
			m[1] * m[7] * m[14] -
			m[5] * m[2] * m[15] +
			m[5] * m[3] * m[14] +
			m[13] * m[2] * m[7] -
			m[13] * m[3] * m[6];
		inv[6] = -m[0] * m[6] * m[15] +
			m[0] * m[7] * m[14] +
			m[4] * m[2] * m[15] -
			m[4] * m[3] * m[14] -
			m[12] * m[2] * m[7] +
			m[12] * m[3] * m[6];
		inv[10] = m[0] * m[5] * m[15] -
			m[0] * m[7] * m[13] -
			m[4] * m[1] * m[15] +
			m[4] * m[3] * m[13] +
			m[12] * m[1] * m[7] -
			m[12] * m[3] * m[5];
		inv[14] = -m[0] * m[5] * m[14] +
			m[0] * m[6] * m[13] +
			m[4] * m[1] * m[14] -
			m[4] * m[2] * m[13] -
			m[12] * m[1] * m[6] +
			m[12] * m[2] * m[5];
		inv[3] = -m[1] * m[6] * m[11] +
			m[1] * m[7] * m[10] +
			m[5] * m[2] * m[11] -
			m[5] * m[3] * m[10] -
			m[9] * m[2] * m[7] +
			m[9] * m[3] * m[6];
		inv[7] = m[0] * m[6] * m[11] -
			m[0] * m[7] * m[10] -
			m[4] * m[2] * m[11] +
			m[4] * m[3] * m[10] +
			m[8] * m[2] * m[7] -
			m[8] * m[3] * m[6];
		inv[11] = -m[0] * m[5] * m[11] +
			m[0] * m[7] * m[9] +
			m[4] * m[1] * m[11] -
			m[4] * m[3] * m[9] -
			m[8] * m[1] * m[7] +
			m[8] * m[3] * m[5];
		inv[15] = m[0] * m[5] * m[10] -
			m[0] * m[6] * m[9] -
			m[4] * m[1] * m[10] +
			m[4] * m[2] * m[9] +
			m[8] * m[1] * m[6] -
			m[8] * m[2] * m[5];
		float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
		if (det == 0)
		{
			exit(0); // Perhaps add logging here. This should not happen in a games context
		}
		det = 1.0f / det;
		for (int i = 0; i < 16; i++)
		{
			inv[i] = inv[i] * det;
		}
		return inv;
	}
	static Matrix lookAt(const Vec3& from, const Vec3& to, const Vec3& up)
	{
		Matrix mat;
		Vec3 dir = (from - to).normalize();
		Vec3 left = Cross(up, dir).normalize();
		Vec3 newUp = Cross(dir, left);
		mat.a[0][0] = left.x;
		mat.a[0][1] = left.y;
		mat.a[0][2] = left.z;
		mat.a[1][0] = newUp.x;
		mat.a[1][1] = newUp.y;
		mat.a[1][2] = newUp.z;
		mat.a[2][0] = dir.x;
		mat.a[2][1] = dir.y;
		mat.a[2][2] = dir.z;
		mat.a[0][3] = -Dot(from, left);
		mat.a[1][3] = -Dot(from, newUp);
		mat.a[2][3] = -Dot(from, dir);
		mat.a[3][3] = 1;
		return mat;
	}
	static Matrix perspective(const float n, const float f, float aspect, const float fov) // FOV in degrees, outputs transposed Matrix for DX
	{
		Matrix pers;
		memset(pers.m, 0, sizeof(float) * 16);
		float t = 1.0f / (tanf(fov * 0.5f * 3.141592654f / 180.0f));
		pers.a[0][0] = t / aspect;
		pers.a[1][1] = t;
		pers.a[2][2] = -f / (f - n);
		pers.a[2][3] = -(f * n) / (f - n);
		pers.a[3][2] = -1.0f;
		return pers;
	}
	static Matrix rotateAxis(const Vec3& axis, float angle)
	{
		Vec3 u = axis.normalize();
		float c = cosf(angle);
		float s = sinf(angle);
		float t = 1.0f - c;

		return Matrix(
			t * u.x * u.x + c, t * u.x * u.y + s * u.z, t * u.x * u.z - s * u.y, 0,
			t * u.x * u.y - s * u.z, t * u.y * u.y + c, t * u.y * u.z + s * u.x, 0,
			t * u.x * u.z + s * u.y, t * u.y * u.z - s * u.x, t * u.z * u.z + c, 0,
			0, 0, 0, 1
		);
	}
	Vec3 extractScale() const
	{
		auto findL = [](float x, float y, float z) { return sqrtf(SQ(x) + SQ(y) + SQ(z)); };
		return Vec3(findL(m[0], m[1], m[2]), findL(m[4], m[5], m[6]), findL(m[8], m[9], m[10]));
	}
	Vec3 extractPosition() const
	{
		return Vec3(m[12], m[13], m[14]);
	}
	void extractAngleAxis(Vec3& axis, float& angle) // Add this
	{

	}
};

class Quaternion
{
public:
	float q[4];
	Quaternion()
	{
		q[0] = 0;
		q[1] = 0;
		q[2] = 0;
		q[3] = 0;
	}
	Quaternion(float _x, float _y, float _z, float _w)
	{
		q[0] = _x;
		q[1] = _y;
		q[2] = _z;
		q[3] = _w;
	}
	float norm()
	{
		return sqrtf(SQ(q[0]) + SQ(q[1]) + SQ(q[2]) + SQ(q[3]));
	}
	void Normalize()
	{
		float n = 1.0f / sqrtf(SQ(q[0]) + SQ(q[1]) + SQ(q[2]) + SQ(q[3]));
		q[0] *= n;
		q[1] *= n;
		q[2] *= n;
		q[3] *= n;
	}
	void Conjugate()
	{
		q[0] = -q[0];
		q[1] = -q[1];
		q[2] = -q[2];
	}
	void invert()
	{
		Conjugate();
		Normalize();
	}
	Quaternion operator*(Quaternion q1)
	{
		Quaternion v;
		v.q[0] = ((q[3] * q1.q[0]) + (q[0] * q1.q[3]) + (q[1] * q1.q[2]) - (q[2] * q1.q[1]));
		v.q[1] = ((q[3] * q1.q[1]) - (q[0] * q1.q[2]) + (q[1] * q1.q[3]) + (q[2] * q1.q[0]));
		v.q[2] = ((q[3] * q1.q[2]) + (q[0] * q1.q[1]) - (q[1] * q1.q[0]) + (q[2] * q1.q[3]));
		v.q[3] = ((q[3] * q1.q[3]) - (q[0] * q1.q[0]) - (q[1] * q1.q[1]) - (q[2] * q1.q[2]));
		return v;
	}
	Matrix toMatrix()
	{
		float xx = q[0] * q[0];
		float xy = q[0] * q[1];
		float xz = q[0] * q[2];
		float yy = q[1] * q[1];
		float zz = q[2] * q[2];
		float yz = q[1] * q[2];
		float wx = q[3] * q[0];
		float wy = q[3] * q[1];
		float wz = q[3] * q[2];
		Matrix matrix;
		matrix[0] = 1.0f - 2.0f * (yy + zz);
		matrix[1] = 2.0f * (xy - wz);
		matrix[2] = 2.0f * (xz + wy);
		matrix[3] = 0.0;
		matrix[4] = 2.0f * (xy + wz);
		matrix[5] = 1.0f - 2.0f * (xx + zz);
		matrix[6] = 2.0f * (yz - wx);
		matrix[7] = 0.0;
		matrix[8] = 2.0f * (xz - wy);
		matrix[9] = 2.0f * (yz + wx);
		matrix[10] = 1.0f - 2.0f * (xx + yy);
		matrix[11] = 0.0;
		matrix[12] = 0;
		matrix[13] = 0;
		matrix[14] = 0;
		matrix[15] = 1;
		return matrix;
	}
	void rotateAboutAxis(Vec3 pt, float angle, Vec3 axis)
	{
		Quaternion q1, p, qinv;
		q1.q[0] = sinf(0.5f * angle) * axis.x;
		q1.q[1] = sinf(0.5f * angle) * axis.y;
		q1.q[2] = sinf(0.5f * angle) * axis.z;
		q1.q[3] = cosf(0.5f * angle);
		p.q[0] = pt.x;
		p.q[1] = pt.y;
		p.q[2] = pt.z;
		p.q[3] = 0;
		qinv = q1;
		qinv.invert();
		q1 = q1 * p;
		q1 = q1 * qinv;
		q[0] = q1.q[0];
		q[1] = q1.q[1];
		q[2] = q1.q[2];
		q[3] = q1.q[3];
	}
	static Quaternion slerp(Quaternion q1, Quaternion q2, float t)
	{
		Quaternion qr;
		float dp = q1.q[0] * q2.q[0] + q1.q[1] * q2.q[1] + q1.q[2] * q2.q[2] + q1.q[3] * q2.q[3];
		Quaternion q11 = dp < 0 ? -q1 : q1;
		dp = dp > 0 ? dp : -dp;
		float theta = acosf(clamp(dp, -1.0f, 1.0f));
		if (theta == 0)
		{
			return q1;
		}
		float d = sinf(theta);
		float a = sinf((1 - t) * theta);
		float b = sinf(t * theta);
		float coeff1 = a / d;
		float coeff2 = b / d;
		qr.q[0] = coeff1 * q11.q[0] + coeff2 * q2.q[0];
		qr.q[1] = coeff1 * q11.q[1] + coeff2 * q2.q[1];
		qr.q[2] = coeff1 * q11.q[2] + coeff2 * q2.q[2];
		qr.q[3] = coeff1 * q11.q[3] + coeff2 * q2.q[3];
		qr.Normalize();
		return qr;
	}
	Quaternion operator-()
	{
		return Quaternion(-q[0], -q[1], -q[2], -q[3]);
	}
};

class Frame
{
public:
	Vec3 u;
	Vec3 v;
	Vec3 w;
	void fromVector(const Vec3& n)
	{
		// Gram-Schmit
		w = n.normalize();
		if (fabsf(w.x) > fabsf(w.y))
		{
			float l = 1.0f / sqrtf(w.x * w.x + w.z * w.z);
			u = Vec3(w.z * l, 0.0f, -w.x * l);
		} else
		{
			float l = 1.0f / sqrtf(w.y * w.y + w.z * w.z);
			u = Vec3(0, w.z * l, -w.y * l);
		}
		v = Cross(w, u);
	}
	void fromVectorTangent(const Vec3& n, const Vec3 &t)
	{
		w = n.normalize();
		u = t.normalize();
		v = Cross(w, u);
	}
	Vec3 toLocal(const Vec3& vec) const
	{
		return Vec3(Dot(vec, u), Dot(vec, v), Dot(vec, w));
	}
	Vec3 toWorld(const Vec3& vec) const
	{
		return ((u * vec.x) + (v * vec.y) + (w * vec.z));
	}
};

Vec3 sphericalToVector(const float theta, const float phi)
{
	float ct = cosf(theta);
	float st = sqrtf(1.0f - (ct * ct));
	return Vec3(sinf(phi) * st, ct, cosf(phi) * st);
}