#pragma once
#include "raylib.h"
#include "Anubis/Anubis.hpp"
#include "Anubis/AnubisShaders.hpp"
#include "Anubis/AnubisMath.hpp"
#include <cinttypes>
#include <queue>
#include <vector>
#include <cstdint>
#include <cstring> 
#include <bitset>
#include <bit> // Required for std::bit_cast (C++20)
#include <chrono>
const int screenW = 1920;
const int screenH = 1080;

using Clock = std::chrono::high_resolution_clock;
#define START_TIMER(name) auto name##_start = Clock::now();
#define END_TIMER(name) \
    auto name##_end = Clock::now(); \
    std::cout << #name << ": " \
    << std::chrono::duration<double, std::milli>(name##_end - name##_start).count() \
    << " ms\n";
const float halfW = (float(screenW) / 2.0);
const float halfH = (float(screenH) / 2.0);
const float reprocOfhalfW = 1.0 / halfW;
const float reprocOfhalfH = 1.0 / halfH;
int lesserTriangles = 0;
int lesserVoxels = 0;
int voxelizedTriangles = 0;

const int renderW = 1920;
const int renderH = 1080;
const float reproX = float(renderW) / float(screenW);
const float reproY = float(renderH) / float(screenH);
struct Vector3LowPrecision {
	uint8_t x;
	uint8_t y;
	uint8_t z;
};
const Vector3LowPrecision v000 = { 0, 0, 0 }; //0
const Vector3LowPrecision v001 = { 0, 0, 1 }; //1
const Vector3LowPrecision v010 = { 0,1, 0 }; //2
const Vector3LowPrecision v011 = { 0, 1, 1 }; //3 
const Vector3LowPrecision v100 = { 1, 0, 0 }; //3
const Vector3LowPrecision v101 = { 1, 0, 1 }; //4
const Vector3LowPrecision v110 = { 1, 1, 0 }; //5
const Vector3LowPrecision v111 = { 1, 1, 1 }; //6
const Vector3LowPrecision vertices[8] =
{
	{0,0,0}, // 0
	{0,0,1}, // 1
	{0,1,0}, // 2
	{0,1,1}, // 3
	{1,0,0}, // 4
	{1,0,1}, // 5
	{1,1,0}, // 6
	{1,1,1}  // 7
};
const uint8_t tris[6][6] =
{
	{ 2, 6, 4, 2, 4, 0 },
	{ 7, 3, 1, 7, 1, 5 },
	{ 3, 2, 1, 1, 2, 0 },
	{ 6, 7, 5, 6, 5, 4 },
	{ 3, 7, 6, 3, 6, 2 },
	{ 4, 5, 1, 4, 1, 0 }
};
const uint8_t quads[6][4] =
{
	{ 2, 6, 4, 0 }, // -X face
	{ 7, 3, 1, 5 }, // +X face
	{ 3, 2, 0, 1 }, // -Y face
	{ 6, 7, 5, 4 }, // +Y face
	{ 3, 7, 6, 2 }, // +Z face
	{ 4, 5, 1, 0 }  // -Z face
};
const Matrix matProj = MatrixPerspective(90 * DEG2RAD, ((double)1920 / (double)1080), 0.1, 100000.0);
const float f_pixels = matProj.m0 * 1280.0f*0.5f;
struct Frame {
	uint32_t pixels[1920 * 1080];
};
struct Vector3HP {
	float16 x;
	float16 y;
	float16 z;
};
inline uint32_t PackRGBA8(Color color)
{
	return
		(uint32_t(color.r)) |
		(uint32_t(color.g) << 8) |
		(uint32_t(color.b) << 16) |
		(uint32_t(color.a) << 24);
}
struct ScreenData {
	Frame image;

	float depth[1920 * 1080];
	float depthLowRes[960*540];
	int frame;
	void ClearBuffer() {
		for (int dy = 0; dy < screenH / 2; dy++) {
			int y = dy * 2 + (frame) % 2;
			for (int i = 0; i < screenW; i++) {
				image.pixels[y * screenW + i] = PackRGBA8(SKYBLUE);
				depth[y * screenW + i] = 100000;

			}

		}
	}
};
static const Vector3 faceNormals[6] =
{
	{ -1.0f,  0.0f,  0.0f }, // Face 0: x = 0  (left)
	{  1.0f,  0.0f,  0.0f }, // Face 1: x = 1  (right)
	{  0.0f,  0.0f, -1.0f }, // Face 2: z = 0  (back)
	{  0.0f,  0.0f,  1.0f }, // Face 3: z = 1  (front)
	{  0.0f,  1.0f,  0.0f }, // Face 4: y = 1  (top)
	{  0.0f, -1.0f,  0.0f }, // Face 5: y = 0  (bottom)
};
inline static void SwapVector2(Vector2* a, Vector2* b) {
	Vector2 tmp = *a;
	*a = *b;
	*b = tmp;
}
struct Plane {
	Vector3 normal;
	float d;
};

void ExtractFrustumPlanes(Matrix view, Plane *planes)
{
	Matrix m = MatrixMultiply(view, matProj);

	planes[0] = { { m.m3 + m.m0,  m.m7 + m.m4,  m.m11 + m.m8  }, m.m15 + m.m12 }; // Left
	planes[1] = { { m.m3 - m.m0,  m.m7 - m.m4,  m.m11 - m.m8  }, m.m15 - m.m12 }; // Right
	planes[2] = { { m.m3 + m.m1,  m.m7 + m.m5,  m.m11 + m.m9  }, m.m15 + m.m13 }; // Bottom
	planes[3] = { { m.m3 - m.m1,  m.m7 - m.m5,  m.m11 - m.m9  }, m.m15 - m.m13 }; // Top
	planes[4] = { { m.m3 + m.m2,  m.m7 + m.m6,  m.m11 + m.m10 }, m.m15 + m.m14 }; // Near
	planes[5] = { { m.m3 - m.m2,  m.m7 - m.m6,  m.m11 - m.m10 }, m.m15 - m.m14 }; // Far

	for (int i = 0; i < 6; i++)
	{
		float len = Vector3Length(planes[i].normal);
		planes[i].normal = Vector3Scale(planes[i].normal, 1.0f / len);
		planes[i].d /= len;
	}
}
inline bool TriangleInFrustum(Vector3 v1, Vector3 v2, Vector3 v3, Plane planes[6]) {
	for (int i = 0; i < 6; i++)
	{
		if ((planes[i].normal.x * v2.x + planes[i].normal.y * v2.y + planes[i].normal.z * v2.z) + planes[i].d < 0 &&
			(planes[i].normal.x * v1.x + planes[i].normal.y * v1.y + planes[i].normal.z * v1.z) + planes[i].d < 0 &&
			(planes[i].normal.x * v3.x + planes[i].normal.y * v3.y + planes[i].normal.z * v3.z) + planes[i].d < 0) {
			return false;
		}
	}
	return true;
}
inline bool BoxInFrustum(Vector3 min, Vector3 max, Plane planes[6]) {
	for (int i = 0; i < 6; i++) {
		const Plane& p = planes[i];
		Vector3 nVertex = {
			(p.normal.x >= 0) ? max.x : min.x,
			(p.normal.y >= 0) ? max.y : min.y,
			(p.normal.z >= 0) ? max.z : min.z
		};

		float distance = p.normal.x * nVertex.x +
			p.normal.y * nVertex.y +
			p.normal.z * nVertex.z +
			p.d;

		if (distance < 0) return false;
	}
	return true;
}

bool bitTheBullet = false;

