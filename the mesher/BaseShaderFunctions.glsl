
const int chunkSize = 32;




layout(std430, binding = 0) buffer GBufferEx {
    uint frame[3840 * 2160];
    vec3 pos[3840*2160];
    int depth[3840* 2160];
    int voxelId[3840*2160];
    float lightVal[3840*2160];
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

layout(std430, binding = 10) buffer GPUBatch {
    ivec3 gpuBatch[1024*1024];
};
layout(std430, binding = 2) buffer VerticesBuffer {
    float vertices[];
};

layout(std430, binding = 5) buffer BoolBuffer {
    int triangleAtomic;
    int triangleActive[];
};
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
    float lightVal[3840*2160];
};

struct Region {
    uvec2 handle;
    uvec2 probeHandle;
    int lod;
    int filled;
};

layout(std430, binding = 3) buffer BitHandles {
    Region handles[];
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
uniform int wx;
uniform int wy;
uniform int wz;
uint readVoxelData(ivec3 pos, inout int lod) {
    if (any(lessThan(pos, ivec3(0))) ||
        pos.x >= wx * chunkSize ||
        pos.y >= wy * chunkSize ||
        pos.z >= wz * chunkSize)
        return 0u;   
    ivec3 chunk = pos / chunkSize;
    int index = chunk.y * (wx * wz) + chunk.z * wx + chunk.x;
    int regionLod = handles[index].lod;
    lod = regionLod;
    if (regionLod==32) return handles[index].filled;
    if (regionLod == 3000) return 1u;
    uvec2 handle = handles[index].handle;
    if (handle != uvec2(0)) {
        uimage3D fluidMap = uimage3D(handle);
        ivec3 lp = (pos & (chunkSize - 1)) / regionLod;
        uint r = 0u;
        bool pending = true;
        while (anyInvocationARB(pending)) {
            if (pending) {
                uvec2 cur = uvec2(readFirstInvocationARB(handle.x), readFirstInvocationARB(handle.y));
                if (handle == cur) {
                    r = imageLoad(uimage3D(cur), lp).x;
                    pending = false;
                }
            }
        }
        return r;
    }
    return 0u;
}

bool inBounds(ivec3 p) {
    
    
    return (p.x < 0 || p.x >= wx *chunkSize||
        p.y < 0 || p.y >= wy*chunkSize ||
        p.z < 0 || p.z >= wz*chunkSize);

}
