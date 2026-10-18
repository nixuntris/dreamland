#version 460
#extension GL_ARB_bindless_texture : require

const int groupSize = 8;
const int chunkSize = 32;
const int perThread = chunkSize / groupSize;
const int threadCount = groupSize * groupSize * groupSize;

layout (local_size_x = groupSize, local_size_y = groupSize, local_size_z = groupSize) in;

uniform int wx;
uniform int wy;
uniform int wz;

layout(std430, binding = 6) buffer Colors {
    uint colors[];
};

layout(std430, binding = 2) buffer VerticesBuffer {
    uint vertices[];
};

layout(std430, binding = 9) buffer MeshletCounter {
    int meshletCounter;
};

layout(std430, binding = 7) buffer AABBBuffer {
    float AABBCorners[];
};

struct Region {
    uvec2 handle;
    int lod;
    int lightIds[16];
};

layout(std430, binding = 3) buffer BitHandles {
    Region handles[];
};

const ivec3 faceOffsets[6] = ivec3[6](
    ivec3( 0,  0, -1),
    ivec3( 0,  0,  1),
    ivec3(-1,  0,  0),
    ivec3( 1,  0,  0),
    ivec3( 0,  1,  0),
    ivec3( 0, -1,  0)
);

shared uint sScan[threadCount];
shared int sMin[threadCount * 3];
shared int sMax[threadCount * 3];
shared int sBegin;

uint readVoxelData(ivec3 pos) {
    if (any(lessThan(pos, ivec3(0))) ||
        pos.x >= wx * chunkSize ||
        pos.y >= wy * chunkSize ||
        pos.z >= wz * chunkSize)
        return 0u;

    ivec3 chunk = pos / chunkSize;
    int index = chunk.y * (wx * wz) + chunk.z * wx + chunk.x;
    int regionLod = handles[index].lod;

    if (regionLod == 0) return 1u;

    uvec2 handle = handles[index].handle;
    if (handle != uvec2(0)) {
        uimage3D fluidMap = uimage3D(handle);
        ivec3 lp = (pos & (chunkSize - 1)) / regionLod;
        return imageLoad(fluidMap, lp).x;
    }

    return 0u;
}

uint PackMetaData(uint mask, uint lod, uint type)
{
    return (mask & 0x3Fu)
         | ((lod  & 0x3Fu) << 6)
         | ((type & 0xFFFFu) << 12);
}

layout(std430, binding = 10) buffer GPUBatch {
    int gpuBatch[1024*1024* 3];
};
void main()
{
    uint tid = gl_LocalInvocationIndex;

    ivec3 chunk = ivec3(
        gpuBatch[int(gl_WorkGroupID.x) * 3 + 0],
        gpuBatch[int(gl_WorkGroupID.x) * 3 + 1],
        gpuBatch[int(gl_WorkGroupID.x) * 3 + 2]
    );
    int chunkIndex = chunk.y * (wx * wz) + chunk.z * wx + chunk.x;
    int lod = handles[chunkIndex].lod;
    if (lod == 0) return;

    for (int c = 0; c < 3; c++) {
        sMin[tid * 3u + uint(c)] = 1 << 30;
        sMax[tid * 3u + uint(c)] = -1;
    }

    ivec3 base = ivec3(gl_LocalInvocationID) * perThread;
    ivec3 chunkOrigin = chunk * chunkSize;

    uint cachePos[perThread * perThread * perThread];
    uint cacheVal[perThread * perThread * perThread];
    uint myCount = 0u;

    for (int x = 0; x < perThread; x++) {
        for (int y = 0; y < perThread; y++) {
            for (int z = 0; z < perThread; z++) {
                ivec3 lp = base + ivec3(x, y, z);
                ivec3 p = chunkOrigin + lp;

                uint value = readVoxelData(p);
                if (value == 0u) continue;

                uint mask = 0u;
                for (int f = 0; f < 6; f++) {
                    if (readVoxelData(p + faceOffsets[f]) == 0u)
                        mask |= 1u << f;
                }
                if (mask == 0u) continue;

                cachePos[myCount] = uint(lp.x) | (uint(lp.y) << 5) | (uint(lp.z) << 10) | (mask << 15);
                cacheVal[myCount] = value-1;
                myCount++;
            }
        }
    }

    sScan[tid] = myCount;
    barrier();

    for (uint offset = 1u; offset < uint(threadCount); offset <<= 1) {
        uint v = sScan[tid];
        if (tid >= offset) v += sScan[tid - offset];
        barrier();
        sScan[tid] = v;
        barrier();
    }

    uint total = sScan[threadCount - 1];
    if (total == 0u) return;

    uint exclusive = sScan[tid] - myCount;
    uint meshletCount = (total + 63u) / 64u;

    if (tid == 0u) {
        sBegin = atomicAdd(meshletCounter, int(meshletCount));
    }

    for (uint i = 0u; i < myCount; i++) {
        uint packed = cachePos[i];
        ivec3 lp = ivec3(int(packed & 31u), int((packed >> 5) & 31u), int((packed >> 10) & 31u));
        uint m = (exclusive + i) >> 6;
        atomicMin(sMin[m * 3u],      lp.x);
        atomicMin(sMin[m * 3u + 1u], lp.y);
        atomicMin(sMin[m * 3u + 2u], lp.z);
        atomicMax(sMax[m * 3u],      lp.x);
        atomicMax(sMax[m * 3u + 1u], lp.y);
        atomicMax(sMax[m * 3u + 2u], lp.z);
    }
    barrier();

    int beginningId = sBegin;

    if (tid < meshletCount) {
        int aabbBase = (beginningId + int(tid)) * 6;
        AABBCorners[aabbBase]     = float(sMin[tid * 3u]      + chunkOrigin.x);
        AABBCorners[aabbBase + 1] = float(sMin[tid * 3u + 1u] + chunkOrigin.y);
        AABBCorners[aabbBase + 2] = float(sMin[tid * 3u + 2u] + chunkOrigin.z);
        AABBCorners[aabbBase + 3] = float(sMax[tid * 3u]      + chunkOrigin.x);
        AABBCorners[aabbBase + 4] = float(sMax[tid * 3u + 1u] + chunkOrigin.y);
        AABBCorners[aabbBase + 5] = float(sMax[tid * 3u + 2u] + chunkOrigin.z);
    }

    uint outBase = uint(beginningId) * 64u;

    for (uint i = 0u; i < myCount; i++) {
        uint packed = cachePos[i];
        ivec3 lp = ivec3(int(packed & 31u), int((packed >> 5) & 31u), int((packed >> 10) & 31u));
        uint mask = (packed >> 15) & 0x3Fu;

        uint idx = exclusive + i;
        uint m = idx >> 6;
        ivec3 minXYZ = ivec3(sMin[m * 3u], sMin[m * 3u + 1u], sMin[m * 3u + 2u]);
        ivec3 local = lp - minXYZ;

        uint packedVertex =
            (uint(local.x) & 0x3FFu) |
            ((uint(local.y) & 0x3FFu) << 10) |
            ((uint(local.z) & 0x3FFu) << 20);

        uint outIndex = outBase + idx;
        colors[outIndex] = PackMetaData(mask, uint(lod), cacheVal[i]);
        vertices[outIndex] = packedVertex;
    }
}