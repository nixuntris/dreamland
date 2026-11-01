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

using Clock = std::chrono::high_resolution_clock;
#define START_TIMER(name) auto name##_start = Clock::now();
#define END_TIMER(name) \
    auto name##_end = Clock::now(); \
    std::cout << #name << ": " \
    << std::chrono::duration<double, std::milli>(name##_end - name##_start).count() \
    << " ms\n";
struct Plane {
	Vector3 normal;
	float d;
};

const Matrix matProj = MatrixPerspective(90 * DEG2RAD, ((double)1920 / (double)1080), 0.1, 100000.0);

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
