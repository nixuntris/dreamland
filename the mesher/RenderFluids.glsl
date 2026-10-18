#version 460
#extension GL_ARB_bindless_texture : require
const int groupSize = 8;
layout (local_size_x = groupSize , local_size_y = groupSize , local_size_z = groupSize ) in;

uniform int wx;
uniform int wy;
uniform int wz;

layout(std430, binding = 0) buffer GBuffer {
    uint frame[3840 * 2160];
    vec3 pos[3840*2160];
    int depth[3840* 2160];
};
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
uniform int renderWidth;
uniform int renderHeight;
float halfW = float(renderWidth) / 2.0;
float halfH = float(renderHeight) / 2.0;
float reprocOfhalfW = 1.0 / halfW;
float reprocOfhalfH = 1.0 / halfH;

uniform vec3 camPos;
uniform vec3 camForward;
uniform mat4 matView;
uniform mat4 matProj;
uniform int frameIndex;

bool GetWorldToScreenClipped(vec3 position, out vec2 outScreen)
{
    vec4 clipPos = matProj * matView * vec4(position, 1.0);

    if (clipPos.w <= 0.0)
        return false;

    vec3 ndc = clipPos.xyz / clipPos.w;

    outScreen = (ndc.xy * vec2(1.0, -1.0) + 1.0)
              * vec2(halfW, halfH);

    return true;
}
float edge(vec2 a, vec2 b, vec2 p) {
    return (p.x - a.x) * (b.y - a.y) -
    (p.y - a.y) * (b.x - a.x);
}
bool isTopLeft(vec2 a, vec2 b) {
    vec2 e = b - a;
    return (e.y > 0.0) || (e.y == 0.0 && e.x < 0.0);
}

void DrawQuadSoftwareCS(
    vec2 v0,
    vec2 v1,
    vec2 v2,
    vec2 v3,
    uint color,
    int quadDepth,
    vec3 quadPos
) {
    float minXf = floor(min(min(v0.x, v1.x), min(v2.x, v3.x)));
    float maxXf = ceil (max(max(v0.x, v1.x), max(v2.x, v3.x)));
    float minYf = floor(min(min(v0.y, v1.y), min(v2.y, v3.y)));
    float maxYf = ceil (max(max(v0.y, v1.y), max(v2.y, v3.y)));

    int minX = clamp(int(minXf), 0, renderWidth  - 1);
    int maxX = clamp(int(maxXf), 0, renderWidth  - 1);
    int minY = clamp(int(minYf), 0, renderHeight - 1);
    int maxY = clamp(int(maxYf), 0, renderHeight - 1);
    float area = edge(v0, v1, v2) + edge(v0, v2, v3);
    bool ccw = area > 0.0;

    bool tl0 = isTopLeft(v1, v0);
    bool tl1 = isTopLeft(v2, v1);
    bool tl2 = isTopLeft(v3, v2);
    bool tl3 = isTopLeft(v0, v3);

    for (int y = minY; y <= maxY; y++) {
        if ((y + frameIndex) % 2 == 0) continue;
        for (int x = minX; x <= maxX; x++) {
            vec2 p = vec2(float(x) + 0.5, float(y) + 0.5);

            float w0 = edge(v0, v1, p);
            float w1 = edge(v1, v2, p);
            float w2 = edge(v2, v3, p);
            float w3 = edge(v3, v0, p);

            if (ccw) {
                if (w0 < 0.0 || (w0 == 0.0 && !tl0)) continue;
                if (w1 < 0.0 || (w1 == 0.0 && !tl1)) continue;
                if (w2 < 0.0 || (w2 == 0.0 && !tl2)) continue;
                if (w3 < 0.0 || (w3 == 0.0 && !tl3)) continue;
            } else {
                if (w0 > 0.0 || (w0 == 0.0 && !tl0)) continue;
                if (w1 > 0.0 || (w1 == 0.0 && !tl1)) continue;
                if (w2 > 0.0 || (w2 == 0.0 && !tl2)) continue;
                if (w3 > 0.0 || (w3 == 0.0 && !tl3)) continue;
            }

            int idx = y * 3840 + x;

            uint prevDepth = depth[idx]; 
            
            if (uint(quadDepth) >= prevDepth) continue;
            uint actualOldDepth = atomicMin(depth[idx], int(quadDepth));
            if (actualOldDepth > uint(quadDepth)) {
                frame[idx] = color;
                pos[idx]   = quadPos;
            }
        }
    }
}

const int faceVertices[6][4] = int[6][4](
    int[4](0, 4, 6, 2), // z-
    int[4](5, 1, 3, 7), // z+
    int[4](1, 0, 2, 3), // x-
    int[4](4, 5, 7, 6), // x+
    int[4](2, 6, 7, 3), // y+
    int[4](0, 1, 5, 4)  // y-
);

const int tris[6][6] = int[6][6](
    int[6](2, 6, 4, 2, 4, 0),
    int[6](7, 3, 1, 7, 1, 5),
    int[6](3, 2, 1, 1, 2, 0),
    int[6](6, 7, 5, 6, 5, 4),
    int[6](3, 7, 6, 3, 6, 2),
    int[6](4, 5, 1, 4, 1, 0)
);
const ivec3 faceOffsets[6] = ivec3[6](
    ivec3( 0,  0, -1), //fine
    ivec3( 0,  0,  1), //fine
    ivec3(-1,  0,  0),
    ivec3( 1,  0,  0),
    ivec3( 0,  1,  0),
    ivec3( 0, -1,  0)
);

const vec4 colorr[10] = vec4[10](
    vec4(0.0, 1.0, 0.0, 1.0),               // GREEN
    vec4(0.5, 0.5, 0.5, 1.0),               // GRAY
    vec4(0.6, 0.3, 0.0, 1.0),               // BROWN
    vec4(0.36, 0.25, 0.20, 1.0),            // DARKBROWN
    vec4(0.0/255.0, 117.0/255.0, 44.0/255.0, 255.0/255.0), // DARKGREEN
    vec4(1.0, 1.0, 1.0, 1.0),               // WHITE
    vec4(0.0, 0.0, 0.5, 1.0),               // DARKBLUE
    vec4(85.0/255.0, 158.0/255.0, 47.0/255.0, 1.0), // {85,158,47,255}
    vec4(238.0/255.0, 214.0/255.0, 175.0/255.0, 1.0), // DESERT SAND
    vec4(0.0, 0.7, 0.85, 1.0)               // WATERY BLUE
);

struct Region {
    uvec2 handle;
    int lod;
    int lightIds[16];
};
layout(std430, binding = 3) buffer BitHandles {
    Region handles[];
};
uniform vec3 batch[512];
uint readVoxelData(ivec3 pos) {
    ivec3 chunk = pos/64;
    
    if (any(lessThan(pos, ivec3(0))) ||
        pos.x >= wx * 64 ||
        pos.y >= wy * 64 ||
        pos.z >= wz * 64)
        return 0;
    uvec2 handle = handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].handle;
    if (handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].lod==0) return 1;
    if (handle != uvec2(0))
    {
        uimage3D fluidMap = uimage3D(handle);
        ivec3 lp = ivec3(mod(pos, 64.0));
                    
        lp/=handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].lod;
        return imageLoad(fluidMap,lp).x;
    }
        
    return 0;

}
const vec3 verticesr[8] = vec3[8](
    vec3(0.0, 0.0, 0.0), // 0
    vec3(0.0, 0.0, 1.0), // 1
    vec3(0.0, 1.0, 0.0), // 2
    vec3(0.0, 1.0, 1.0), // 3
    vec3(1.0, 0.0, 0.0), // 4
    vec3(1.0, 0.0, 1.0), // 5
    vec3(1.0, 1.0, 0.0), // 6
    vec3(1.0, 1.0, 1.0)  // 7
);
//#define FACE_FRONT  (1 << 0)
//#define FACE_BACK   (1 << 1)
//#define FACE_LEFT   (1 << 2)
//#define FACE_RIGHT  (1 << 3)
//#define FACE_TOP    (1 << 4)
//#define FACE_BOTTOM (1 << 5)
uint PackMetaData(uint mask, uint lod, uint type)
{
    return (uint(mask) & 0x3Fu)
         | ((uint(lod)  & 0x3Fu) << 6)
         | ((uint(type) & 0xFFFFu) << 12);
}

void main()
{
    ivec3 chunk = ivec3(batch[int(gl_WorkGroupID.x)]);
    int chunkIndex = chunk.y * (wx * wz) + chunk.z * wx + chunk.x;

    int lod = handles[chunkIndex].lod;
    if (lod == 0) return;

    int voxelCount = 0;
    int offsetx = int(gl_LocalInvocationID.x)*(64/groupSize);
    int offsety = int(gl_LocalInvocationID.y)*(64/groupSize);
    int offsetz = int(gl_LocalInvocationID.z)*(64/groupSize);
    for (int x = offsetx; x < (64/groupSize)+offsetx; x++) {
        for (int y = offsety; y < (64/groupSize)+offsety; y++) {
            for (int z = offsetz; z < (64/groupSize)+offsetz; z++) {
                
                ivec3 p = chunk * 64 + ivec3(x, y, z);

                uint value = readVoxelData(p);
                if (value == 0) continue;

                bool exposed = false;

                for (int f = 0; f < 6; f++) {
                    if (readVoxelData(p + faceOffsets[f]) == 0) {
                        exposed = true;
                        break;
                    }
                }

                if (exposed) voxelCount++;
            }
        }
    }

    if (voxelCount == 0) return;

    int meshletCount = (voxelCount + 63) / 64;
    int beginningId = atomicAdd(meshletCounter, meshletCount);

    uint batchMeta[64];
    ivec3 batchPos[64];

    int count = 0;
    int meshletIndex = 0;
    
    for (int x = offsetx; x < (64/groupSize)+offsetx; x++) {
        for (int y = offsety; y < (64/groupSize)+offsety; y++) {
            for (int z = offsetz; z < (64/groupSize)+offsetz; z++) {
                ivec3 p = chunk * 64 + ivec3(x, y, z);

                uint value = readVoxelData(p);
                if (value == 0) continue;

                uint mask = 0u;

                for (int f = 0; f < 6; f++) {
                    if (readVoxelData(p + faceOffsets[f]) == 0)
                        mask |= 1u << f;
                }

                if (mask == 0u) continue;

                batchMeta[count] =
                    PackMetaData(mask, uint(lod), value);

                batchPos[count] = p;
                count++;

                if (count == 64 || voxelCount == meshletIndex * 64 + count) {
                    ivec3 minXYZ = batchPos[0];
                    ivec3 maxXYZ = batchPos[0];

                    for (int i = 1; i < count; i++) {
                        minXYZ = min(minXYZ, batchPos[i]);
                        maxXYZ = max(maxXYZ, batchPos[i]);
                    }

                    int outputMeshlet = beginningId + meshletIndex;
                    int aabbBase = outputMeshlet * 6;

                    AABBCorners[aabbBase]     = float(minXYZ.x);
                    AABBCorners[aabbBase + 1] = float(minXYZ.y);
                    AABBCorners[aabbBase + 2] = float(minXYZ.z);
                    AABBCorners[aabbBase + 3] = float(maxXYZ.x);
                    AABBCorners[aabbBase + 4] = float(maxXYZ.y);
                    AABBCorners[aabbBase + 5] = float(maxXYZ.z);

                    for (int i = 0; i < count; i++) {
                        ivec3 localPos = batchPos[i] - minXYZ;

                        uint packedVertex =
                            (uint(localPos.x) & 0x3FFu) |
                            ((uint(localPos.y) & 0x3FFu) << 10) |
                            ((uint(localPos.z) & 0x3FFu) << 20);

                        int outputIndex = outputMeshlet * 64 + i;

                        colors[outputIndex] = batchMeta[i];
                        vertices[outputIndex] = packedVertex;
                    }

                    meshletIndex++;
                    count = 0;
                }
            }
        }
    }
}
