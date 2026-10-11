#include <immintrin.h>
#include <stdint.h>
#include <math.h>

// Permutation table (standard Perlin, doubled to avoid wrapping)
static const int perm[512] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,
    140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,
    247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,
    57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
    74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,
    60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,
    65,25,63,161,1,216,80,73,209,76,132,187,208, 89,18,169,
    200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,
    52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,
    207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,
    119,248,152,2,44,154,163, 70,221,153,101,155,167, 43,172,9,
    129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,
    218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,
    81,51,145,235,249,14,239,107,49,192,214, 31,181,199,106,157,
    184, 84,204,176,215,121,50,45,127, 4,150,254,138,236,205,93,
    222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,
    140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,
    247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,
    57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
    74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,
    60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,
    65,25,63,161,1,216,80,73,209,76,132,187,208, 89,18,169,
    200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,
    52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,
    207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,
    119,248,152,2,44,154,163, 70,221,153,101,155,167, 43,172,9,
    129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,
    218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,
    81,51,145,235,249,14,239,107,49,192,214, 31,181,199,106,157,
    184, 84,204,176,215,121,50,45,127, 4,150,254,138,236,205,93,
    222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

static inline __m256 fade(__m256 t) {
    // 6t^5 - 15t^4 + 10t^3
    __m256 t3 = _mm256_mul_ps(t, _mm256_mul_ps(t, t));
    __m256 six = _mm256_set1_ps(6.0f);
    __m256 fifteen = _mm256_set1_ps(15.0f);
    __m256 ten = _mm256_set1_ps(10.0f);
    __m256 a = _mm256_fmsub_ps(six, t, fifteen);     // 6t - 15
    __m256 b = _mm256_fmadd_ps(a, t, ten);           // (6t-15)t + 10
    return _mm256_mul_ps(t3, b);                      // t^3 * b
}

static inline __m256 lerp(__m256 t, __m256 a, __m256 b) {
    return _mm256_fmadd_ps(t, _mm256_sub_ps(b, a), a);
}

// Gradient function for 8 lanes at once, using integer hash + lookup gather
static inline __m256 grad8(__m256i hash, __m256 x, __m256 y) {
    // h & 7 gives one of 8 gradient directions
    __m256i h = _mm256_and_si256(hash, _mm256_set1_epi32(7));

    // Precompute masks for selecting components
    __m256i is4 = _mm256_cmpgt_epi32(_mm256_set1_epi32(4), h); // h < 4 -> use x as u, y as v (else swap)
    __m256 u = _mm256_blendv_ps(y, x, _mm256_castsi256_ps(is4));
    __m256 v = _mm256_blendv_ps(x, y, _mm256_castsi256_ps(is4));

    // sign bits based on h bits 0 and 1
    __m256i h_and1 = _mm256_and_si256(h, _mm256_set1_epi32(1));
    __m256i h_and2 = _mm256_and_si256(h, _mm256_set1_epi32(2));
    __m256 negu = _mm256_castsi256_ps(_mm256_cmpeq_epi32(h_and1, _mm256_set1_epi32(1)));
    __m256 negv = _mm256_castsi256_ps(_mm256_cmpeq_epi32(h_and2, _mm256_set1_epi32(2)));

    __m256 su = _mm256_blendv_ps(u, _mm256_sub_ps(_mm256_setzero_ps(), u), negu);
    __m256 sv = _mm256_blendv_ps(v, _mm256_sub_ps(_mm256_setzero_ps(), v), negv);

    return _mm256_add_ps(su, sv);
}

// Gather permutation values for 8 integer indices (scalar fallback, since perm table is tiny and cache-resident)
static inline __m256i perm_lookup8(__m256i idx) {
    int32_t tmp[8], out[8];
    _mm256_storeu_si256((__m256i*)tmp, idx);
    for (int i = 0; i < 8; i++) out[i] = perm[tmp[i] & 511];
    return _mm256_loadu_si256((__m256i*)out);
}

// Compute 2D Perlin noise for 8 (x,y) points packed in AVX registers
inline __m256 perlin2d_avx2(__m256 x, __m256 y) {
    __m256 floorx = _mm256_floor_ps(x);
    __m256 floory = _mm256_floor_ps(y);

    __m256i X = _mm256_and_si256(_mm256_cvtps_epi32(floorx), _mm256_set1_epi32(255));
    __m256i Y = _mm256_and_si256(_mm256_cvtps_epi32(floory), _mm256_set1_epi32(255));

    __m256 xf = _mm256_sub_ps(x, floorx);
    __m256 yf = _mm256_sub_ps(y, floory);

    __m256 xf1 = _mm256_sub_ps(xf, _mm256_set1_ps(1.0f));
    __m256 yf1 = _mm256_sub_ps(yf, _mm256_set1_ps(1.0f));

    __m256i one = _mm256_set1_epi32(1);
    __m256i X1 = _mm256_and_si256(_mm256_add_epi32(X, one), _mm256_set1_epi32(255));
    __m256i Y1 = _mm256_and_si256(_mm256_add_epi32(Y, one), _mm256_set1_epi32(255));

    // hash corners: perm[perm[X]+Y] etc.
    __m256i pX  = perm_lookup8(X);
    __m256i pX1 = perm_lookup8(X1);

    __m256i aa = perm_lookup8(_mm256_add_epi32(pX, Y));
    __m256i ab = perm_lookup8(_mm256_add_epi32(pX, Y1));
    __m256i ba = perm_lookup8(_mm256_add_epi32(pX1, Y));
    __m256i bb = perm_lookup8(_mm256_add_epi32(pX1, Y1));

    __m256 u = fade(xf);
    __m256 v = fade(yf);

    __m256 g_aa = grad8(aa, xf, yf);
    __m256 g_ba = grad8(ba, xf1, yf);
    __m256 g_ab = grad8(ab, xf, yf1);
    __m256 g_bb = grad8(bb, xf1, yf1);

    __m256 x1 = lerp(u, g_aa, g_ba);
    __m256 x2 = lerp(u, g_ab, g_bb);

    return lerp(v, x1, x2); // result in [-1, 1]
}

// Helper: generate a row/array of noise values
inline void perlin2d_array_avx2(const float* xs, const float* ys, float* out, int n) {
    int i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 x = _mm256_loadu_ps(xs + i);
        __m256 y = _mm256_loadu_ps(ys + i);
        __m256 r = perlin2d_avx2(x, y);
        _mm256_storeu_ps(out + i, r);
    }
    // scalar tail (fallback for n % 8 != 0) - omitted for brevity, or pad input arrays to multiple of 8
}
static inline float fade_scalar(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static inline float lerp_scalar(float t, float a, float b) {
    return a + t * (b - a);
}

static inline float grad_scalar(int hash, float x, float y) {
    int h = hash & 7;
    float u = (h < 4) ? x : y;
    float v = (h < 4) ? y : x;
    float su = (h & 1) ? -u : u;
    float sv = (h & 2) ? -v : v;
    return su + sv;
}

inline float perlin_noise_2d(float x, float y) {
    float floorx = floorf(x);
    float floory = floorf(y);

    int X = ((int)floorx) & 255;
    int Y = ((int)floory) & 255;

    float xf = x - floorx;
    float yf = y - floory;

    float xf1 = xf - 1.0f;
    float yf1 = yf - 1.0f;

    int X1 = (X + 1) & 255;
    int Y1 = (Y + 1) & 255;

    int pX = perm[X];
    int pX1 = perm[X1];

    int aa = perm[(pX + Y) & 511];
    int ab = perm[(pX + Y1) & 511];
    int ba = perm[(pX1 + Y) & 511];
    int bb = perm[(pX1 + Y1) & 511];

    float u = fade_scalar(xf);
    float v = fade_scalar(yf);

    float g_aa = grad_scalar(aa, xf, yf);
    float g_ba = grad_scalar(ba, xf1, yf);
    float g_ab = grad_scalar(ab, xf, yf1);
    float g_bb = grad_scalar(bb, xf1, yf1);

    float x1 = lerp_scalar(u, g_aa, g_ba);
    float x2 = lerp_scalar(u, g_ab, g_bb);

    return lerp_scalar(v, x1, x2);
}
// 3D gradient hash
inline static float grad3d(int hash, float x, float y, float z) {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

inline float perlin_noise_3d(float x, float y, float z) {
    int X = (int)floorf(x) & 255;
    int Y = (int)floorf(y) & 255;
    int Z = (int)floorf(z) & 255;

    float xf = x - floorf(x);
    float yf = y - floorf(y);
    float zf = z - floorf(z);

    float u = fade_scalar(xf);
    float v = fade_scalar(yf);
    float w = fade_scalar(zf);

    // Hash coordinates
    int A = perm[X] + Y, AA = perm[A] + Z, AB = perm[A + 1] + Z;
    int B = perm[X + 1] + Y, BA = perm[B] + Z, BB = perm[B + 1] + Z;

    // Interpolate 8 corners
    float res = lerp_scalar(w,
        lerp_scalar(v,
            lerp_scalar(u, grad3d(perm[AA], xf, yf, zf), grad3d(perm[BA], xf - 1, yf, zf)),
            lerp_scalar(u, grad3d(perm[AB], xf, yf - 1, zf), grad3d(perm[BB], xf - 1, yf - 1, zf))),
        lerp_scalar(v,
            lerp_scalar(u, grad3d(perm[AA + 1], xf, yf, zf - 1), grad3d(perm[BA + 1], xf - 1, yf, zf - 1)),
            lerp_scalar(u, grad3d(perm[AB + 1], xf, yf - 1, zf - 1), grad3d(perm[BB + 1], xf - 1, yf - 1, zf - 1)))
    );

    return res;
}