#version 460
#extension GL_ARB_bindless_texture : require
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;




layout(std430, binding = 9) buffer MeshletCounter {
    int meshletCounter;
    int chunkBeginning[1024*1024];
    int chunkEnd[1024*1024];
};

layout(std430, binding = 0) buffer GBufferEx {
    uint frame[3840 * 2160];
    vec3 pos[3840*2160];
    int depth[3840* 2160];
    int voxelId[3840*2160];
};

#define CHUNK_SIZE 32
#define WY         32  
#define SAMPLE_RATE 4


uint pcg(uint v) {
    uint s = v * 747796405u + 2891336453u;
    uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (w >> 22u) ^ w;
}
uint hash2(ivec2 p) { return pcg(uint(p.x) + pcg(uint(p.y))); }
uint hash3(ivec3 p) { return pcg(uint(p.x) + pcg(uint(p.y) + pcg(uint(p.z)))); }

uint SavHash(int x, int z, uint seed) {
    uint h = uint(x) * 374761393u + uint(z) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
float SavRand01(int x, int z, uint seed) {
    return float(SavHash(x, z, seed) & 0xFFFFu) / 65535.0;
}

float fade(float t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }

float grad2(uint h, vec2 d) {
    switch (h & 7u) {
        case 0u: return  d.x + d.y;
        case 1u: return -d.x + d.y;
        case 2u: return  d.x - d.y;
        case 3u: return -d.x - d.y;
        case 4u: return  d.x;
        case 5u: return -d.x;
        case 6u: return  d.y;
        default: return -d.y;
    }
}

float perlin_noise_2d(float x, float y) {
    vec2 p  = vec2(x, y);
    vec2 pf = floor(p);
    ivec2 pi = ivec2(pf);
    vec2 f  = p - pf;
    vec2 u  = vec2(fade(f.x), fade(f.y));

    float n00 = grad2(hash2(pi),               f);
    float n10 = grad2(hash2(pi + ivec2(1, 0)), f - vec2(1, 0));
    float n01 = grad2(hash2(pi + ivec2(0, 1)), f - vec2(0, 1));
    float n11 = grad2(hash2(pi + ivec2(1, 1)), f - vec2(1, 1));

    return mix(mix(n00, n10, u.x), mix(n01, n11, u.x), u.y);
}

float grad3(uint h, vec3 d) {
    uint k = h & 15u;
    float u = k < 8u ? d.x : d.y;
    float v = k < 4u ? d.y : ((k == 12u || k == 14u) ? d.x : d.z);
    return ((k & 1u) == 0u ? u : -u) + ((k & 2u) == 0u ? v : -v);
}

float perlin_noise_3d(float x, float y, float z) {
    vec3 p  = vec3(x, y, z);
    vec3 pf = floor(p);
    ivec3 pi = ivec3(pf);
    vec3 f  = p - pf;
    vec3 u  = vec3(fade(f.x), fade(f.y), fade(f.z));

    float n000 = grad3(hash3(pi + ivec3(0,0,0)), f - vec3(0,0,0));
    float n100 = grad3(hash3(pi + ivec3(1,0,0)), f - vec3(1,0,0));
    float n010 = grad3(hash3(pi + ivec3(0,1,0)), f - vec3(0,1,0));
    float n110 = grad3(hash3(pi + ivec3(1,1,0)), f - vec3(1,1,0));
    float n001 = grad3(hash3(pi + ivec3(0,0,1)), f - vec3(0,0,1));
    float n101 = grad3(hash3(pi + ivec3(1,0,1)), f - vec3(1,0,1));
    float n011 = grad3(hash3(pi + ivec3(0,1,1)), f - vec3(0,1,1));
    float n111 = grad3(hash3(pi + ivec3(1,1,1)), f - vec3(1,1,1));

    return mix(mix(mix(n000, n100, u.x), mix(n010, n110, u.x), u.y),
               mix(mix(n001, n101, u.x), mix(n011, n111, u.x), u.y), u.z);
}

float fbm3(vec3 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) {
        v += a * perlin_noise_3d(p.x, p.y, p.z);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

float mountainNoise(vec2 world) {
    float n = 0.0, amp = 1.0, freq = 0.002;
    for (int i = 0; i < 6; i++) {
        n += perlin_noise_2d(world.x * freq, world.y * freq) * amp;
        amp  *= 0.5;
        freq *= 2.0;
    }
    return n;
}

void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID.xyz);
    
}