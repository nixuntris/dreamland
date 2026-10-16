#version 460
#extension GL_ARB_bindless_texture : require
const int voxelAmountPerThreads = 8;
layout (local_size_x = 64/voxelAmountPerThreads, local_size_y = 64/voxelAmountPerThreads, local_size_z = 64/voxelAmountPerThreads) in;

uniform int wx;
uniform int wy;
uniform int wz;

layout(std430, binding = 0) buffer GBufferEx {
    uint frame[3840 * 2160];
    vec3 pos[3840*2160];
    int depth[3840* 2160];
    int voxelId[3840*2160];
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
    vec4 clipPos;

    clipPos.x = (matView[0][0] * position.x + matView[2][0] * position.z + matView[3][0]) * matProj[0][0];
    clipPos.y = (matView[0][1] * position.x + matView[1][1] * position.y + matView[2][1] * position.z + matView[3][1]) * matProj[1][1];
    clipPos.z = (matView[0][2] * position.x + matView[1][2] * position.y + matView[2][2] * position.z + matView[3][2]);
    clipPos.w = matProj[2][3] * clipPos.z;
    clipPos.z = matProj[2][2] * clipPos.z + matProj[3][2] * matView[3][3];

    if (clipPos.w <= 0.0)
        return false;
    clipPos.y *= -1;
    vec3 ndc = clipPos.xyz / clipPos.w;
    outScreen = ndc.xy+1.0;
    outScreen *= vec2(halfW,halfH);
    

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
void DrawLineSoftwareCS(
    vec2 a,
    vec2 b,
    uint color,
    float triDepth,
    vec3 trianglePos)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;

    int steps = int(max(abs(dx), abs(dy)));

    if (steps <= 0)
        return;

    for (int i = 0; i <= steps; i++)
    {
        float t = float(i) / float(steps);

        int x = int(a.x + dx * t);
        int y = int(a.y + dy * t);

        if (x < 0 || x >= renderWidth ||
            y < 0 || y >= renderHeight)
            continue;

        int idx = y * renderWidth + x;

        if (depth[idx] > int(triDepth))
        {
            depth[idx] = int(triDepth);
            frame[idx] = color;
            pos[idx] = trianglePos;
        }
    }
}

void DrawTriangleWireframeCS(
    vec2 v0,
    vec2 v1,
    vec2 v2,
    uint color,
    float triDepth,
    vec3 trianglePos)
{
    DrawLineSoftwareCS(v0, v1, color, triDepth, trianglePos);
    DrawLineSoftwareCS(v1, v2, color, triDepth, trianglePos);
    DrawLineSoftwareCS(v2, v0, color, triDepth, trianglePos);
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
        
            //if ((x + frameIndex) % 2 == 0) continue;

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

vec3 unpack_vertex(uint packedr) {
    vec3 result;
    result.x = float(packedr & 0x3FFu);
    result.y = float((packedr >> 10u) & 0x3FFu);
    result.z = float((packedr >> 20u) & 0x3FFu);
    return result;
}
vec4 debugColors[12] = vec4[](
    vec4(1.0, 0.0, 0.0, 1.0), // Red
    vec4(0.0, 1.0, 0.0, 1.0), // Green
    vec4(0.0, 0.0, 1.0, 1.0), // Blue
    vec4(1.0, 1.0, 0.0, 1.0), // Yellow
    vec4(1.0, 0.0, 1.0, 1.0), // Magenta
    vec4(0.0, 1.0, 1.0, 1.0), // Cyan
    vec4(1.0, 0.5, 0.0, 1.0), // Orange
    vec4(0.5, 0.0, 1.0, 1.0), // Purple
    vec4(0.5, 0.5, 0.5, 1.0), // Grey
    vec4(0.0, 0.5, 0.0, 1.0), // Dark Green
    vec4(0.5, 0.0, 0.0, 1.0), // Dark Red
    vec4(0.0, 0.0, 0.5, 1.0)  // Dark Blue
);
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

// Triangle indices
const int tris[6][6] = int[6][6](
    int[6](2, 6, 4, 2, 4, 0),
    int[6](7, 3, 1, 7, 1, 5),
    int[6](3, 2, 1, 1, 2, 0),
    int[6](6, 7, 5, 6, 5, 4),
    int[6](3, 7, 6, 3, 6, 2),
    int[6](4, 5, 1, 4, 1, 0)
);
const ivec3 faceOffsets[6] = ivec3[6](
    ivec3( 0,  0,  -1), // FRONT
    ivec3( 0,  0, 1), // BACK
    ivec3(-1,  0,  0), // LEFT
    ivec3( 1,  0,  0), // RIGHT
    ivec3( 0,  1,  0), // TOP
    ivec3( 0, -1,  0)  // BOTTOM
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
struct VoxelMeta {
    int mask;  // 6 bits
    int lod;   // 6 bits
    int width; // 6 bits
    int depth; // 6 bits
    int type;  // 8 bits
};
VoxelMeta UnpackMetaData(uint m)
{
    VoxelMeta meta;
    meta.mask  = int(m         & 0x3Fu);
    meta.lod   = int((m >> 6)  & 0x3Fu);
    meta.width = int((m >> 12) & 0x3Fu);
    meta.depth = int((m >> 18) & 0x3Fu);
    meta.type  = int((m >> 24) & 0xFFu);
    return meta;
}

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

uint readVoxelData(ivec3 pos) {
    ivec3 chunk = pos/64;
    
    uvec2 handle = handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].handle;

    if (handle != uvec2(0))
    {
        uimage3D fluidMap = uimage3D(handle);
        ivec3 lp = ivec3(mod(pos, 64.0));
                    
        lp/=handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].lod;
        return imageLoad(fluidMap,lp).x;
    }
    return 0;

}
void main()
{

    float m15reproc = 1.0 / matView[3][3];
    for (int x = 0; x < voxelAmountPerThreads; x++) {
        for (int y =0 ; y < voxelAmountPerThreads; y++) {
            for (int z = 0; z < voxelAmountPerThreads; z++) {
                
                ivec3 pos = ivec3(gl_GlobalInvocationID.xyz)*voxelAmountPerThreads + ivec3(x, y, z);
                uint value = readVoxelData(ivec3(pos));
                if (value==0) continue;
                
                for (int i = 0; i < 6; i++) {
                    ivec3 neighbor = ivec3(pos) + faceOffsets[i];

                    vec3 v1 = pos+verticesr[tris[i][0]];
                    vec3 v2 = pos+verticesr[tris[i][1]];
                    vec3 v3 = pos+verticesr[tris[i][2]];
                    vec3 v4 = pos+verticesr[tris[i][3]];
                    vec3 v5 = pos+verticesr[tris[i][4]];
                    vec3 v6 = pos+verticesr[tris[i][5]];
    
                    vec3 c = vec3(
                        (v1+v2+v3)/3.0f
                    );
                    vec3 edge1 = v2 - v1;
                    vec3 edge2 = v3 - v1;
                    vec3 faceNormal = normalize(cross(edge1, edge2));

                    if (dot(faceNormal, camPos - c) <= 0.0 && c.y<camPos.y)
                        continue;
                    vec3 toC = c - camPos;
                    float distance = dot(toC, camForward);


                    vec3 toV1 = v1 - camPos;
                    vec3 toV2 = v2 - camPos;
                    vec3 toV3 = v3 - camPos;

                    float z1 = dot(toV1, camForward);
                    float z2 = dot(toV2, camForward);
                    float z3 = dot(toV3, camForward);

                    if (z1 <= 0 && z2 <= 0 && z3 <= 0)
                        continue;

                    vec2 p1, p2, p3,p6;
                    if (!GetWorldToScreenClipped(v1, p1) ||
                        !GetWorldToScreenClipped(v2, p2) ||
                        !GetWorldToScreenClipped(v3, p3) ||
                        !GetWorldToScreenClipped(v6, p6))
                        continue;

                    vec2 SCREEN_MIN = vec2(0.0, 0.0);
                    vec2 SCREEN_MAX = vec2(float(renderWidth), float(renderHeight));
    
                    if ((p1.x < SCREEN_MIN.x && p2.x < SCREEN_MIN.x && p3.x < SCREEN_MIN.x) || 
                        (p1.x >= SCREEN_MAX.x && p2.x >= SCREEN_MAX.x && p3.x >= SCREEN_MAX.x) || 
                        (p1.y < SCREEN_MIN.y && p2.y < SCREEN_MIN.y && p3.y < SCREEN_MIN.y) ||
                        (p1.y >= SCREEN_MAX.y && p2.y >= SCREEN_MAX.y && p3.y >= SCREEN_MAX.y)) continue;
                    DrawQuadSoftwareCS(p1, p2, p6, p3,packRGBA8(colorr[9]), int(distance), c);
                }
            }
        }
    }
    
}
