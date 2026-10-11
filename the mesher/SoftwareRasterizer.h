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
struct CPUMesh {
	int realTriangleCount;
	int vertexCount;
	int triangleCount;
	int colorCount;
	float* MeshletAABB;
	float *meshletPosition;
	float* vertices;
	bool verticePrecision;
	uint32_t* packedVertex;
	uint32_t* packedNormal;
	Color* colors;
	float* faceNormals;
	uint32_t *gpuBasedColoring;
};
struct InstancedMesh {
	float* positions;
	int instanceTriangleCount;

};
//cube data
static const Vector3 faceNormals[6] =
{
	{ -1.0f,  0.0f,  0.0f }, // Face 0: x = 0  (left)
	{  1.0f,  0.0f,  0.0f }, // Face 1: x = 1  (right)
	{  0.0f,  0.0f, -1.0f }, // Face 2: z = 0  (back)
	{  0.0f,  0.0f,  1.0f }, // Face 3: z = 1  (front)
	{  0.0f,  1.0f,  0.0f }, // Face 4: y = 1  (top)
	{  0.0f, -1.0f,  0.0f }, // Face 5: y = 0  (bottom)
};
inline bool GetWorldToScreenClipped(const Vector3 position, Matrix matView, Vector2* outScreen)
{

	Vector4 clipPos;
	clipPos.x = (matView.m0 * position.x + matView.m8 * position.z + matView.m12) * matProj.m0;
	clipPos.y = (matView.m1 * position.x + matView.m5 * position.y + matView.m9 * position.z + matView.m13) * matProj.m5;
	clipPos.z = (matView.m2 * position.x + matView.m6 * position.y + matView.m10 * position.z + matView.m14);
	clipPos.w = matProj.m11 * clipPos.z;
	clipPos.z = matProj.m10 * clipPos.z + matProj.m14 * matView.m15;

	if (clipPos.w <= 0.0f)
		return false;

	Vector3 ndc = { clipPos.x / clipPos.w, -clipPos.y / clipPos.w, clipPos.z / clipPos.w };

	outScreen->x = (ndc.x + 1.0f) * halfW;
	outScreen->y = (ndc.y + 1.0f) * halfH;

	return true;
}
inline static void SwapVector2(Vector2* a, Vector2* b) {
	Vector2 tmp = *a;
	*a = *b;
	*b = tmp;
}
static inline void DrawLine(
    int x0, int y0,
    int x1, int y1,
    Color color,
    ScreenData* screenData,
    int screenW,
    int screenH
)
{
    int dx = x1 - x0;
    int dy = y1 - y0;

    int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    if (steps == 0) return;

    float xInc = dx / (float)steps;
    float yInc = dy / (float)steps;

    float x = (float)x0;
    float y = (float)y0;

    uint32_t packed = PackRGBA8(color);

    for (int i = 0; i <= steps; ++i)
    {
        int xi = (int)(x + 0.5f);
        int yi = (int)(y + 0.5f);

        if ((unsigned)xi < (unsigned)screenW &&
            (unsigned)yi < (unsigned)screenH)
        {
            screenData->image.pixels[yi * screenW + xi] = packed;
        }

        x += xInc;
        y += yInc;
    }
}
static inline void DrawLineLocal(
	int x0, int y0,
	int x1, int y1,
	Color color,
	ScreenData* screenData,
	int screenW,
	int screenH
)
{
	int dx = x1 - x0;
	int dy = y1 - y0;

	int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
	if (steps == 0) return;

	float xInc = dx / (float)steps;
	float yInc = dy / (float)steps;

	float x = (float)x0;
	float y = (float)y0;

	uint32_t packed = PackRGBA8(color);

	for (int i = 0; i <= steps; ++i)
	{
		int xi = (int)(x + 0.5f);
		int yi = (int)(y + 0.5f);

		if (xi < screenW &&
			yi < screenH && xi>=0 && yi>=0)
		{
			screenData->image.pixels[yi * screenW + xi] = packed;
		}

		x += xInc;
		y += yInc;
	}
}
inline void DrawTriangleSoftware(Vector2 v0, Vector2 v1, Vector2 v2, Color color, ScreenData* screenData, float depth)
{
	if (v0.y > v1.y) SwapVector2(&v0, &v1);
	if (v1.y > v2.y) SwapVector2(&v1, &v2);
	if (v0.y > v1.y) SwapVector2(&v0, &v1);

	
	int y0 = (int)v0.y;
	int y1 = (int)v1.y;
	int y2 = (int)v2.y;
	float dx01 = (y1 != y0) ? (v1.x - v0.x) / (float)(y1 - y0) : 0.0f;
	float dx02 = (y2 != y0) ? (v2.x - v0.x) / (float)(y2 - y0) : 0.0f;
	float dx12 = (y2 != y1) ? (v2.x - v1.x) / (float)(y2 - y1) : 0.0f;
	float x01 = v0.x;
	float x02 = v0.x;
	for (int y = y0; y <= y1; y++) {

		if ((y + screenData->frame) % 2 == 0) {

			int sx = (int)x01;
			int ex = (int)x02;
			if (sx > ex) { int t = sx; sx = ex; ex = t; }

			if (ex > 0 && sx < screenW && y < screenH && y >= 0) {

				if (sx < 0) sx = 0;
				if (ex > screenW) ex = screenW;

				int width = ex - sx;
				if (width > 0)
					for (int offx = 0; offx < width; offx++) {
						int x = sx + offx;

						if (screenData->depth[y * screenW + x] > depth) {
							screenData->image.pixels[y * screenW + x] = PackRGBA8(color);
							screenData->depth[y * screenW + x] = depth;
						}

					}
			}
		}

		x01 += dx01;
		x02 += dx02;
	}
	x01 = v1.x + dx12 * ((y1 + 1) - v1.y);

	for (int y = y1 + 1; y <= y2; y++) {

		if ((y + screenData->frame) % 2 == 0 && y >= 0 && y < screenH) {

			int sx = (int)x01;
			int ex = (int)x02;
			if (sx > ex) { int t = sx; sx = ex; ex = t; }

			if (ex > 0 && sx < screenW) {

				if (sx < 0) sx = 0;
				if (ex > screenW) ex = screenW;

				int width = ex - sx;
				if (width > 0)

					for (int offx = 0; offx < width; offx++) {
						int x = sx + offx;

						if (screenData->depth[y * screenW + x] > depth) {
							screenData->image.pixels[y * screenW + x] = PackRGBA8(color);
							screenData->depth[y * screenW + x] = depth;
						}

					}
			}
		}

		x01 += dx12;
		x02 += dx02;
	}
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

inline Vector3 Vector3NormalizeInline(Vector3 v)
{
	Vector3 result = v;

	float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	if (length != 0.0f)
	{
		float ilength = 1.0f / length;

		result.x *= ilength;
		result.y *= ilength;
		result.z *= ilength;
	}

	return result;
}
void PreGenMeshCullingData(CPUMesh* mesh, bool generateGPUData = false)
{
	int meshletCount = (int)ceil((float)mesh->triangleCount / 64.0f);

	mesh->MeshletAABB = (float*)MemAlloc((size_t)meshletCount * 6 * sizeof(float));
	memset(mesh->MeshletAABB, 0xFF, (size_t)meshletCount * 6 * sizeof(float));

	for (int i = 0; i < meshletCount; i++) {
		float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
		float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;

		for (int id = i * 64; id < i * 64 + 64 && id < mesh->triangleCount; id++) {
			if (id >= mesh->realTriangleCount) break;
			int base = id * 3;
			float x = mesh->vertices[base + 0];
			float y = mesh->vertices[base + 1];
			float z = mesh->vertices[base + 2];
			if (x < minX) minX = x;  if (y < minY) minY = y;  if (z < minZ) minZ = z;
			if (x > maxX) maxX = x;  if (y > maxY) maxY = y;  if (z > maxZ) maxZ = z;
		}

		// Rebase vertices to meshlet-local origin
		for (int id = i * 64; id < i * 64 + 64 && id < mesh->triangleCount; id++) {
			if (id >= mesh->realTriangleCount) break;
			int base = id * 3;
			mesh->vertices[base + 0] -= minX;
			mesh->vertices[base + 1] -= minY;
			mesh->vertices[base + 2] -= minZ;
		}

		mesh->MeshletAABB[i * 6 + 0] = minX;
		mesh->MeshletAABB[i * 6 + 1] = minY;
		mesh->MeshletAABB[i * 6 + 2] = minZ;
		mesh->MeshletAABB[i * 6 + 3] = maxX;
		mesh->MeshletAABB[i * 6 + 4] = maxY;
		mesh->MeshletAABB[i * 6 + 5] = maxZ;
	}
	
	mesh->packedVertex = (uint32_t*)MemAlloc(mesh->vertexCount * sizeof(uint32_t));

	for (int i = 0; i < mesh->triangleCount; i++){

		float x = mesh->vertices[i * 3 + 0];
		float y = mesh->vertices[i * 3 + 1];
		float z = mesh->vertices[i * 3 + 2];
		if (x < 0.0f) x = 0.0f;
		else if (x > 1024.0f) x = 1024.0f;
		if (y < 0.0f) y = 0.0f;
		else if (y > 1024.0f) y = 1024.0f;
		if (z < 0.0f) z = 0.0f;
		else if (z > 1024.0f) z = 1024.0f;

		uint32_t pp1 = (uint32_t)x & 0x3FF;
		uint32_t pp2 = (uint32_t)y & 0x3FF;
		uint32_t pp3 = (uint32_t)z & 0x3FF;


		mesh->packedVertex[i] = pp1 | (pp2 << 10) | (pp3 << 20);
		

	}
}
bool bitTheBullet = false;

void RenderMeshCPU(Vector3 camPos, Vector3 camForward, Matrix matView, int renderX, int renderZ, float scale, CPUMesh mesh, ScreenData* screenData, Plane planes[6], int lodLevel) {
	constexpr Color colors[5] = { RED,BLUE,WHITE,GREEN,BLACK };
	uint8_t map[3][16][3];
	for (int x = 0; x < 3; x++) {
		for (int z = 0; z < 3; z++) {
			for (int y = 0; y < 256 / 16; y++) {

				float minX = x * 16.0f + renderX;
				float minY = y * 16.0f;
				float minZ = z * 16.0f + renderZ;

				float maxX = (x + 1) * 16.0f + renderX;
				float maxY = (y + 1) * 16.0f;
				float maxZ = (z + 1) * 16.0f + renderZ;

				map[x][y][z] = BoxInFrustum({ minX, minY, minZ }, { maxX, maxY, maxZ }, planes);

			}
		}
	}
	float m15reproc = 1.0 / matView.m15;
#pragma omp parallel for schedule(static)
	for (int i = 0; i < mesh.triangleCount; i++)
	{
		int base = i * 9;
		int trianglebase = i * 3;
		if (map[int(mesh.vertices[base + 0]) / 16][int(mesh.vertices[base + 1]) / 16][int(mesh.vertices[base + 2]) / 16] == 0) {
			continue;
		}
		Vector3 vec1 = { mesh.vertices[base + 0] * scale + renderX, mesh.vertices[base + 1] * scale, mesh.vertices[base + 2] * scale + renderZ };
		Vector3 vec2 = { mesh.vertices[base + 3] * scale + renderX, mesh.vertices[base + 4] * scale, mesh.vertices[base + 5] * scale + renderZ };
		Vector3 vec3 = { mesh.vertices[base + 6] * scale + renderX, mesh.vertices[base + 7] * scale, mesh.vertices[base + 8] * scale + renderZ };

		Vector3 faceNormal = { mesh.faceNormals[trianglebase] ,mesh.faceNormals[trianglebase + 1] ,mesh.faceNormals[trianglebase + 2] };
		Vector3 c;
		c.x = vec1.x + vec2.x + vec3.x;
		c.y = vec1.y + vec2.y + vec3.y;
		c.z = vec1.z + vec2.z + vec3.z;
		c.x /= 3;
		c.y /= 3;
		c.z /= 3;

		//Vector3 c = { mesh.triangleCenters[trianglebase] * scale + renderX ,mesh.triangleCenters[trianglebase + 1] * scale ,mesh.triangleCenters[trianglebase + 2] * scale + renderZ };

		Vector3 viewDir = { camPos.x - c.x,camPos.y - c.y,camPos.z - c.z };

		float length = sqrtf(viewDir.x * viewDir.x + viewDir.y * viewDir.y + viewDir.z * viewDir.z);
		float ilength = 1.0f / length;

		viewDir.x *= ilength;
		viewDir.y *= ilength;
		viewDir.z *= ilength;

		if ((faceNormal.x * viewDir.x + faceNormal.y * viewDir.y + faceNormal.z * viewDir.z) <= 0.0f) continue;

		Vector3 toC = { c.x - camPos.x,c.y - camPos.y,c.z - camPos.z };
		float distance = (toC.x * camForward.x + toC.y * camForward.y + toC.z * camForward.z);

		if (distance > 100000.0) continue;
		Vector3 toV1 = { vec1.x - camPos.x, vec1.y - camPos.y, vec1.z - camPos.z };
		Vector3 toV2 = { vec2.x - camPos.x, vec2.y - camPos.y, vec2.z - camPos.z };
		Vector3 toV3 = { vec3.x - camPos.x, vec3.y - camPos.y, vec3.z - camPos.z };
		float z1 = toV1.x * camForward.x + toV1.y * camForward.y + toV1.z * camForward.z;
		float z2 = toV2.x * camForward.x + toV2.y * camForward.y + toV2.z * camForward.z;
		float z3 = toV3.x * camForward.x + toV3.y * camForward.y + toV3.z * camForward.z;
		if ((z1) <= 0 &&
			(z2) <= 0 &&
			(z3) <= 0) continue;

		Vector2 p1 = { (((matProj.m0 * (matView.m0 * vec1.x + matView.m12)) * m15reproc) + 1.0f) * reprocOfhalfW, ((-(matProj.m5) / matView.m15) + 1.0f) * reprocOfhalfH };
		Vector2 p2 = { (((matProj.m0 * (matView.m0 * vec2.x + matView.m12)) * m15reproc) + 1.0f) * reprocOfhalfW, ((-(matProj.m5) / matView.m15) + 1.0f) * reprocOfhalfH };
		Vector2 p3 = { (((matProj.m0 * (matView.m0 * vec3.x + matView.m12)) * m15reproc) + 1.0f) * reprocOfhalfW, ((-(matProj.m5) / matView.m15) + 1.0f) * reprocOfhalfH };

		if (!GetWorldToScreenClipped(vec2, matView, &p2) ||
			!GetWorldToScreenClipped(vec1, matView, &p1) ||
			!GetWorldToScreenClipped(vec3, matView, &p3)

			) continue;
		p1.x *= reproX;
		p2.x *= reproX;
		p3.x *= reproX;
		p1.y *= reproY;
		p2.y *= reproY;
		p3.y *= reproY;
		DrawTriangleSoftware(p1, p2, p3, mesh.colors[i], screenData, distance);
	}
}
void RenderMeshCPUOutline(Vector3 camPos, Vector3 camForward, Matrix matView, int renderX, int renderZ, float scale, CPUMesh mesh, ScreenData* screenData, Plane planes[6], int lodLevel) {
	constexpr Color colors[5] = { RED,BLUE,WHITE,GREEN,BLACK };
	uint8_t map[3][16][3];
	for (int x = 0; x < 3; x++) {
		for (int z = 0; z < 3; z++) {
			for (int y = 0; y < 256 / 16; y++) {

				float minX = x * 16.0f + renderX;
				float minY = y * 16.0f;
				float minZ = z * 16.0f + renderZ;

				float maxX = (x + 1) * 16.0f + renderX;
				float maxY = (y + 1) * 16.0f;
				float maxZ = (z + 1) * 16.0f + renderZ;

				map[x][y][z] = BoxInFrustum({ minX, minY, minZ }, { maxX, maxY, maxZ }, planes);

			}
		}
	}
	float m15reproc = 1.0 / matView.m15;
#pragma omp parallel for schedule(static)
	for (int i = 0; i < mesh.triangleCount; i++)
	{
		int base = i * 9;
		int trianglebase = i * 3;
		if (map[int(mesh.vertices[base + 0]) / 16][int(mesh.vertices[base + 1]) / 16][int(mesh.vertices[base + 2]) / 16] == 0) {
			continue;
		}
		Vector3 vec1 = { mesh.vertices[base + 0] * scale + renderX, mesh.vertices[base + 1] * scale, mesh.vertices[base + 2] * scale + renderZ };
		Vector3 vec2 = { mesh.vertices[base + 3] * scale + renderX, mesh.vertices[base + 4] * scale, mesh.vertices[base + 5] * scale + renderZ };
		Vector3 vec3 = { mesh.vertices[base + 6] * scale + renderX, mesh.vertices[base + 7] * scale, mesh.vertices[base + 8] * scale + renderZ };

		Vector3 faceNormal = { mesh.faceNormals[trianglebase] ,mesh.faceNormals[trianglebase + 1] ,mesh.faceNormals[trianglebase + 2] };
		Vector3 c;
		c.x = vec1.x + vec2.x + vec3.x;
		c.y = vec1.y + vec2.y + vec3.y;
		c.z = vec1.z + vec2.z + vec3.z;
		c.x /= 3;
		c.y /= 3;
		c.z /= 3;
		Vector3 viewDir = { camPos.x - c.x,camPos.y - c.y,camPos.z - c.z };

		float length = sqrtf(viewDir.x * viewDir.x + viewDir.y * viewDir.y + viewDir.z * viewDir.z);
		float ilength = 1.0f / length;

		viewDir.x *= ilength;
		viewDir.y *= ilength;
		viewDir.z *= ilength;

		if ((faceNormal.x * viewDir.x + faceNormal.y * viewDir.y + faceNormal.z * viewDir.z) <= 0.0f) continue;

		Vector3 toC = { c.x - camPos.x,c.y - camPos.y,c.z - camPos.z };
		float distance = (toC.x * camForward.x + toC.y * camForward.y + toC.z * camForward.z);

		if (distance > 100000.0) continue;
		Vector3 toV1 = { vec1.x - camPos.x, vec1.y - camPos.y, vec1.z - camPos.z };
		Vector3 toV2 = { vec2.x - camPos.x, vec2.y - camPos.y, vec2.z - camPos.z };
		Vector3 toV3 = { vec3.x - camPos.x, vec3.y - camPos.y, vec3.z - camPos.z };
		float z1 = toV1.x * camForward.x + toV1.y * camForward.y + toV1.z * camForward.z;
		float z2 = toV2.x * camForward.x + toV2.y * camForward.y + toV2.z * camForward.z;
		float z3 = toV3.x * camForward.x + toV3.y * camForward.y + toV3.z * camForward.z;
		if ((z1) <= 0 &&
			(z2) <= 0 &&
			(z3) <= 0) continue;

		Vector2 p1 = { (((matProj.m0 * (matView.m0 * vec1.x + matView.m12)) * m15reproc) + 1.0f) * reprocOfhalfW, ((-(matProj.m5) / matView.m15) + 1.0f) * reprocOfhalfH };
		Vector2 p2 = { (((matProj.m0 * (matView.m0 * vec2.x + matView.m12)) * m15reproc) + 1.0f) * reprocOfhalfW, ((-(matProj.m5) / matView.m15) + 1.0f) * reprocOfhalfH };
		Vector2 p3 = { (((matProj.m0 * (matView.m0 * vec3.x + matView.m12)) * m15reproc) + 1.0f) * reprocOfhalfW, ((-(matProj.m5) / matView.m15) + 1.0f) * reprocOfhalfH };

		if (!GetWorldToScreenClipped(vec2, matView, &p2) ||
			!GetWorldToScreenClipped(vec1, matView, &p1) ||
			!GetWorldToScreenClipped(vec3, matView, &p3)

			) continue;
		p1.x *= reproX;
		p2.x *= reproX;
		p3.x *= reproX;
		p1.y *= reproY;
		p2.y *= reproY;
		p3.y *= reproY;
		DrawLineLocal(p1.x, p1.y, p2.x, p2.y, mesh.colors[i], screenData, 1920, 1080);
		DrawLineLocal(p2.x, p2.y, p3.x, p3.y, mesh.colors[i],screenData,1920,1080);
		DrawLineLocal(p3.x, p3.y, p1.x, p1.y, mesh.colors[i], screenData, 1920, 1080);

	}
}
