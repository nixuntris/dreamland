#version 460
out vec4 FragColor;
uniform vec4 color;




layout(std430, binding = 0) buffer GBufferEx {
    uint frame[3840 * 2160];
    vec3 pos[3840*2160];
    int depth[3840* 2160];
    int voxelId[3840*2160];
    float lightVal[3840*2160];
};
struct Region {
    uvec2 handle;
    uvec2 probeHandle;
    int lod;
};
layout(std430, binding = 3) buffer BitHandles {
    Region handles[];
};

layout(std430, binding = 4) buffer LightingHash {
    float lightData[];
};
layout(std430, binding = 5) buffer BoolBuffer {
    int triangleAtomic;
    int chunkAtomic;
    int triangleActive[];
};
layout(std430, binding = 2) buffer VerticesBuffer {
    float vertices[];
};
layout(std430, binding = 8) buffer VMeshletBuffer {
    float meshletVertices[];
};

layout(std430, binding = 20) buffer PointMap {
    float particlePoint[];
};
layout(std430, binding = 7) buffer AABBBuffer {
    float AABBCorners[];
};

layout(std430, binding = 9) buffer MeshletCounter {
    int meshletCounter;
    int chunkBeginning[1024*1024];
    int chunkEnd[1024*1024];
};

layout(std430, binding = 10) buffer GPUBatch {
    ivec3 gpuBatch[1024*1024];
};
uint packRGBA8(vec4 color)
{
   vec4 c = clamp(color, 0.0, 1.0);
   uvec4 bytes = uvec4(c * 255.0 + 0.5);

   return
        ( bytes.r        ) |
        ( bytes.g <<  8 ) |
        ( bytes.b << 16 ) |
        ( bytes.a << 24 );
}


struct Faces {
	uint mask; 
	uint type;
	uint x;
	uint y;
	uint z;
	uint width;
	uint length;
	uint height;
	uint ao;
}; 
vec4 unpackRGBA8(uint packedr)
{
    return vec4(
        float( packedr        & 0xFFu),
        float((packedr >>  8) & 0xFFu),
        float((packedr >> 16) & 0xFFu),
        float((packedr >> 24) & 0xFFu)
    ) * (1.0 / 255.0);
}
uniform float renderPercentage;
uniform int frameIndex;
uniform int resY;
vec3 fetchFrame(ivec2 p) {
    p = clamp(p, ivec2(0), ivec2(3839, 2159));
    return unpackRGBA8(frame[p.y * 3840 + p.x]).rgb;
}

float fetchLightVal(ivec2 p) {
    p = clamp(p, ivec2(0), ivec2(3839, 2159));
    return lightVal[p.y * 3840 + p.x];
}

void main() {
    vec2 src = vec2(gl_FragCoord.x, float(resY) - gl_FragCoord.y + 0.5) * renderPercentage - 0.5;

    ivec2 base = ivec2(floor(src));
    vec2  f    = fract(src);

    vec3 c00 = fetchFrame(base);
    vec3 c10 = fetchFrame(base + ivec2(1, 0));
    vec3 c01 = fetchFrame(base + ivec2(0, 1));
    vec3 c11 = fetchFrame(base + ivec2(1, 1));

    vec3 color = mix(mix(c00, c10, f.x), mix(c01, c11, f.x), f.y);
    

    FragColor = vec4(color, 1.0);
}