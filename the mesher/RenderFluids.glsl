#version 460
#extension GL_ARB_bindless_texture : require
const int voxelAmountPerThreads = 8;
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

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

uint readVoxelData(ivec3 pos) {
    ivec3 chunk = pos/64;
    
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

    float m15reproc = 1.0 / matView[3][3];
    ivec3 chunk = ivec3(gl_GlobalInvocationID.xyz);
    
    if (handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].lod == 0) return;
    uint voxels[4096];
    ivec3 poses[4096];
    int vCount = 0;
    for (int x = 0; x < 64; x++) {
        for (int y =0 ; y < 64; y++) {
            for (int z = 0; z < 64; z++) {
                
                ivec3 pos = ivec3(gl_GlobalInvocationID.xyz)*64+ivec3(x,y,z);
                uint value = readVoxelData(ivec3(pos));
                if (value==0) continue;
                uint mask = 0;

                for (int i = 0; i < 6; i++) {
                    ivec3 neighbor = ivec3(pos) + faceOffsets[i];
                    uint type = readVoxelData(neighbor);
                    if (type == 0) {
                        mask|= (1u << i);
                    }
                }
                uint voxelPoint = PackMetaData(mask,handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].lod, value);
                voxels[vCount] = voxelPoint;
                poses[vCount] = pos;
                if (mask!=0 && value!=0) vCount+=1;

            }
        }
    }
    int meshletCount = int(ceil(float(vCount) / 64.0f));
    int beginningId = atomicAdd(meshletCounter, meshletCount);

    for (int i = 0; i < meshletCount; i++) {

        ivec3 minXYZ = ivec3(100000000); //AABB corners
        ivec3 maxXYZ = ivec3(-100000000);
        for (int id = i*64; id < i*64+64 && id<vCount; id++) {
            if (id>vCount) break;
            ivec3 pos = poses[id];
            if (pos.x>maxXYZ.x) maxXYZ.x = pos.x;
            if (pos.y>maxXYZ.y) maxXYZ.y = pos.y;
            if (pos.z>maxXYZ.z) maxXYZ.z = pos.z;
            if (pos.x<minXYZ.x) minXYZ.x = pos.x;
            if (pos.y<minXYZ.y) minXYZ.y = pos.y;
            if (pos.z<minXYZ.z) minXYZ.z = pos.z;
        }
        int aabbBase = (beginningId + i) * 6;
        if (minXYZ.x>maxXYZ.x || minXYZ.y>maxXYZ.y || minXYZ.z>maxXYZ.z) {
            minXYZ = ivec3(0);
            maxXYZ = ivec3(1000);
        }
        AABBCorners[aabbBase] = float(minXYZ.x);
        AABBCorners[aabbBase+1] = float(minXYZ.y);
        AABBCorners[aabbBase+2] = float(minXYZ.z);
        AABBCorners[aabbBase+3] = float(maxXYZ.x);
        AABBCorners[aabbBase+4] = float(maxXYZ.y);
        AABBCorners[aabbBase+5] = float(maxXYZ.z);
        for (int id = i*64; id < i*64+64 && id<vCount; id++) {
            if (id>vCount) break;
            poses[id] -= minXYZ;
        }
        for (int id = i*64; id < i*64+64 && id<vCount; id++) {
            if (id>vCount) break;
            poses[id] = clamp(poses[id],ivec3(0),ivec3(1024));
            
		    uint pp1 = uint(poses[id].x) & 0x3FF;
		    uint pp2 = uint(poses[id].y) & 0x3FF;
		    uint pp3 = uint(poses[id].z) & 0x3FF;
            uint packedVertex = pp1 | (pp2<<10) | (pp3<<20);
            int voxelId = (beginningId+i)*64+int(mod(id,64));
            colors[voxelId] = voxels[id];
            vertices[voxelId] = packedVertex;
        }
    }
}
