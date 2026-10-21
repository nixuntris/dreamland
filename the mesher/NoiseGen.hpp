#pragma once
#include "raylib.h"
#ifdef _MSC_VER
#  include <intrin.h>
#  define __builtin_popcount __popcnt
#endif

int wx = 100;
int wz = 100;
int ww = wx * wz;
int wy = 32;
const int chunkSize = 32;
#include "PerlinAVX2.hpp"
struct GenerationData {
	uint16_t* heightMap;
	uint16_t lowestHeight;
	uint8_t highestChunkY;
	uint8_t lowestChunkY;
	void GenerateMountains(int gx, int gz) {
		heightMap = (uint16_t*)MemAlloc(chunkSize * chunkSize * sizeof(uint16_t));
		this->lowestHeight = wy * chunkSize;
		int highestHeight = 0;

		const int sampleRate = 4;
		const int res = (chunkSize / sampleRate) + 1;
		float* samples = (float*)alloca(res * res * sizeof(float));

		for (int z = 0; z < res; z++) {
			for (int x = 0; x < res; x += 8) {
				__m256 worldX = _mm256_set_ps(
					(float)(gx * chunkSize + ((x + 7) * sampleRate)),
					(float)(gx * chunkSize + ((x + 6) * sampleRate)),
					(float)(gx * chunkSize + ((x + 5) * sampleRate)),
					(float)(gx * chunkSize + ((x + 4) * sampleRate)),
					(float)(gx * chunkSize + ((x + 3) * sampleRate)),
					(float)(gx * chunkSize + ((x + 2) * sampleRate)),
					(float)(gx * chunkSize + ((x + 1) * sampleRate)),
					(float)(gx * chunkSize + (x * sampleRate))
				);
				__m256 worldZ = _mm256_set1_ps((float)(gz * chunkSize + (z * sampleRate)));

				__m256 noise = _mm256_setzero_ps();
				__m256 amplitude = _mm256_set1_ps(1.0f);
				__m256 frequency = _mm256_set1_ps(0.002f);

				for (int i = 0; i < 6; i++) {
					__m256 oct = perlin2d_avx2(_mm256_mul_ps(worldX, frequency), _mm256_mul_ps(worldZ, frequency));
					noise = _mm256_add_ps(noise, _mm256_mul_ps(oct, amplitude));
					amplitude = _mm256_mul_ps(amplitude, _mm256_set1_ps(0.5f));
					frequency = _mm256_mul_ps(frequency, _mm256_set1_ps(2.0f));
				}
				_mm256_storeu_ps(&samples[x + z * res], noise);
			}
		}
		for (int z = 0; z < chunkSize; z++) {
			for (int x = 0; x < chunkSize; x++) {
				int x0 = x / sampleRate; int x1 = x0 + 1;
				int z0 = z / sampleRate; int z1 = z0 + 1;
				float tx = (float)(x % sampleRate) / (float)sampleRate;
				float tz = (float)(z % sampleRate) / (float)sampleRate;

				float c00 = samples[x0 + z0 * res];
				float c10 = samples[x1 + z0 * res];
				float c01 = samples[x0 + z1 * res];
				float c11 = samples[x1 + z1 * res];

				float nx0 = c00 + tx * (c10 - c00);
				float nx1 = c01 + tx * (c11 - c01);
				float noise = nx0 + tz * (nx1 - nx0);

				int terrainHeight = (int)(noise * 800.0f);
				if (terrainHeight < 0) terrainHeight = 0;

				if (terrainHeight < 150) {
					float worldX = (float)(gx * chunkSize + x);
					float worldZ = (float)(gz * chunkSize + z);
					// Note: If you need extreme performance, you could batch this too, 
					// but per-pixel detail is usually light enough for scalar.
					float valleyDetail = perlin_noise_2d(worldX * 0.05f, worldZ * 0.05f) * 4.0f;
					terrainHeight = 150 + (int)((terrainHeight * 0.1f) + valleyDetail);
				}

				heightMap[x + z * chunkSize] = (uint16_t)terrainHeight;
				if (terrainHeight < this->lowestHeight) this->lowestHeight = terrainHeight;
				if (terrainHeight > highestHeight) highestHeight = terrainHeight;
			}
		}
		this->lowestChunkY = this->lowestHeight / chunkSize;
		this->highestChunkY = (highestHeight / chunkSize) + 1;
	}
};
float fbm3(Math::Vector3 p) {
	float v = 0.0, a = 0.5;
	for (int i = 0; i < 5; i++) {
		v += a * perlin_noise_3d(p.x,p.y,p.z);
		p.x *= 2.0;
		p.y *= 2.0;
		p.z *= 2.0;
		a *= 0.5;
	}
	return v;
}

static inline uint32_t SavHash(int x, int z, uint32_t seed) {
	uint32_t h = (uint32_t)x * 374761393u + (uint32_t)z * 668265263u + seed * 2246822519u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}
static inline float SavRand01(int x, int z, uint32_t seed) {
	return (SavHash(x, z, seed) & 0xFFFFu) / 65535.0f;
}