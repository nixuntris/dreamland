#version 460
layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;




layout(std430, binding = 0) buffer GBufferEx {
    uint frame[3840 * 2160];
    vec3 pos[3840*2160];
    int depth[3840* 2160];
    int voxelId[3840*2160];
};

layout(std430, binding = 5) buffer BoolBuffer {
    int triangleAtomic;
    int triangleActive[];
};

layout(std430, binding = 6) buffer Colors {
    uint colors[];
};

layout(std430, binding = 2) buffer VerticesBuffer {
    uint vertices[];
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

bool GetWorldToScreenClipped(vec3 position, out vec2 outScreen) {
    vec4 clipPos = matProj * matView * vec4(position, 1.0);
    
    if (clipPos.w <= 0.0)
        return false;
    
    vec3 ndc = clipPos.xyz / clipPos.w;
    outScreen = vec2(
        (ndc.x + 1.0) * 0.5 * float(renderWidth),
        (1.0 - ndc.y) * 0.5 * float(renderHeight)
    );
    
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

vec4 unpackRGBA8(uint packedr)
{
    return vec4(
        float( packedr        & 0xFFu),
        float((packedr >>  8) & 0xFFu),
        float((packedr >> 16) & 0xFFu),
        float((packedr >> 24) & 0xFFu)
    ) * (1.0 / 255.0);
}
vec3 unpack_vertex(uint packedr) {
    vec3 result;
    result.x = float(packedr & 0x3FFu);
    result.y = float((packedr >> 10u) & 0x3FFu);
    result.z = float((packedr >> 20u) & 0x3FFu);
    return result;
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

// Re-ordered triangle indices to perfectly normalize quad layout across all faces
const int tris[6][6] = int[6][6](
    int[6](2, 6, 0, 0, 6, 4), // Face 0
    int[6](7, 3, 5, 5, 3, 1), // Face 1
    int[6](3, 2, 1, 1, 2, 0), // Face 2 (Fixed layout)
    int[6](6, 7, 4, 4, 7, 5), // Face 3
    int[6](3, 7, 2, 2, 7, 6), // Face 4
    int[6](4, 5, 0, 0, 5, 1)  // Face 5
);

uniform uint packedColors[1024];
uint hashVec3(vec3 v) {
    uint x = uint(v.x);
    uint y = uint(v.y);
    uint z = uint(v.z);
    uint h = x * 374761393u + y * 668265263u + z * 1274126177u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
struct VoxelMeta {
    int mask;  // 6 bits (Bits 0-5)
    int lod;   // 6 bits (Bits 6-11)
    int type;  // 16 bits (Bits 12-27)
};

VoxelMeta UnpackMetaData(uint m)
{
    VoxelMeta meta;
    meta.mask = int(m         & 0x3Fu);
    meta.lod  = int((m >> 6)  & 0x3Fu);
    meta.type = int((m >> 12) & 0xFFFFu);
    return meta;
}

layout(std430, binding = 7) buffer AABBBuffer {
    float AABBCorners[];
};
uniform int offset;

void DrawQuadSoftwareCS(
    vec2 v0,
    vec2 v1,
    vec2 v2,
    vec2 v3,
    uint color,
    int quadDepth,
    vec3 quadPos,
    int id
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
                voxelId[idx] = id;
            }
        }
    }
}
void main()
{

        vec2 SCREEN_MIN = vec2(0.0, 0.0);
        vec2 SCREEN_MAX = vec2(float(renderWidth), float(renderHeight));
    
    float m15reproc = 1.0 / matView[3][3];

    int triangleId = int(gl_GlobalInvocationID.x)+offset;
    if (triangleActive[triangleId/64]==-1 || triangleId/64>=triangleAtomic) return;
    int meshletId = triangleActive[triangleId/64];
    triangleId = meshletId *64+int(gl_LocalInvocationID.x);
    int base = triangleId ;
    vec3 meshletPosition = vec3(AABBCorners[meshletId*6],AABBCorners[meshletId*6+1],AABBCorners[meshletId*6+2]);
    VoxelMeta meta = UnpackMetaData(colors[triangleId]);
    vec3 baseP = vec3(unpack_vertex(vertices[base]))  +meshletPosition;
    int distance = int(dot(baseP-camPos, camForward));

    if (meta.type==0) return;
    uint hash = hashVec3(baseP);
    uint color = packedColors[meta.type];
    if (hash%2==0) {
        vec4 hr = unpackRGBA8(packedColors[meta.type]);
        hr.rgb *= 0.95;
        color = packRGBA8(hr);
    }
    else if (hash%3==0) {
        vec4 hr = unpackRGBA8(packedColors[meta.type]);
        hr.rgb *= 1.05;
        color = packRGBA8(hr);
    }
    for (int i = 0; i < 6; i++) {
        if ((meta.mask & (1u << i)) == 0u) continue;
        vec3 v1 = baseP+verticesr[tris[i][0]]*meta.lod;
        vec3 v2 = baseP+verticesr[tris[i][1]]*meta.lod;
        vec3 v3 = baseP+verticesr[tris[i][2]]*meta.lod;
        
        vec3 c = vec3(
            (v1+v2+v3)/3.0f
        );
        vec3 edge1 = v2 - v1;
        vec3 edge2 = v3 - v1;
        vec3 faceNormal = normalize(cross(edge1, edge2));

       if (dot(faceNormal, camPos - c) <= 0.0)
            continue;
        vec3 toC = c - camPos;
        vec3 v6 = baseP +verticesr[tris[i][5]]*meta.lod;
    

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

        if ((p1.x < SCREEN_MIN.x && p2.x < SCREEN_MIN.x && p3.x < SCREEN_MIN.x) || 
            (p1.x >= SCREEN_MAX.x && p2.x >= SCREEN_MAX.x && p3.x >= SCREEN_MAX.x) || 
            (p1.y < SCREEN_MIN.y && p2.y < SCREEN_MIN.y && p3.y < SCREEN_MIN.y) ||
            (p1.y >= SCREEN_MAX.y && p2.y >= SCREEN_MAX.y && p3.y >= SCREEN_MAX.y)) continue;
        
        DrawQuadSoftwareCS(p1, p2, p6, p3, color, int(distance), c,triangleId);
    }
}
