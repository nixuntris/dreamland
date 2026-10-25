#version 460
out vec4 FragColor;
uniform vec4 color;




layout(std430, binding = 0) buffer GBufferEx {
    uint frame[3840 * 2160];
    vec3 pos[3840*2160];
    int depth[3840* 2160];
    int voxelId[3840*2160];
};
struct Region {
    uvec2 handle;
    int lod;
    int lightIds[16];
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
uniform int frameIndex;
uniform int resY;
void main() {
	ivec2 pixelCoords = ivec2(vec2(gl_FragCoord.xy));
    FragColor = vec4(unpackRGBA8(frame[(resY-pixelCoords.y) * 3840 + pixelCoords.x]).rgb,1);
    
    
}