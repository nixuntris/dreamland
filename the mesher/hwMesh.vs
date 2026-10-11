#version 460
layout (location=0) in vec3 aPos;
layout (location=1) in vec3 aColor;

uniform mat4 MVP;

flat out vec3 color;

uniform int offsetInt;
layout(std430, binding = 6) buffer Colors {
    uint colors[];
};
uniform float size;

layout(std430, binding = 0) buffer GBufferEx {
    uint frame[3840 * 2160];
    vec3 pos[3840*2160];
    int depth[3840* 2160];
    int voxelId[3840*2160];
};
layout(std430, binding = 5) buffer BoolBuffer {
    int triangleAtomic;
    int chunkAtomic;
    int triangleActive[];
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
int renderWidth = 1920;
int renderHeight = 1080;
float halfW = float(renderWidth) / 2.0;
float halfH = float(renderHeight) / 2.0;
float reprocOfhalfW = 1.0 / halfW;
float reprocOfhalfH = 1.0 / halfH;


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
const vec4 colorr[9] = vec4[9](
    vec4(0.0, 1.0, 0.0, 1.0),               // GREEN
    vec4(0.5, 0.5, 0.5, 1.0),               // GRAY
    vec4(0.6, 0.3, 0.0, 1.0),               // BROWN
    vec4(0.36, 0.25, 0.20, 1.0),            // DARKBROWN
    vec4(0.0/255.0, 117.0/255.0, 44.0/255.0, 255.0/255.0), // DARKGREEN
    vec4(1.0, 1.0, 1.0, 1.0),               // WHITE
    vec4(0.0, 0.0, 0.5, 1.0),               // DARKBLUE
    vec4(85.0/255.0, 158.0/255.0, 47.0/255.0, 1.0), // {85,158,47,255}
    vec4(238.0/255.0, 214.0/255.0, 175.0/255.0, 1.0) // DESERT SAND
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
uniform vec3 camPos;
uniform vec3 camForward;
uniform vec3 objectRotation;
flat out float depthr;
uniform vec3 objectPos;
mat3 EulerToMatrix(vec3 e) {
    float cx = cos(e.x), sx = sin(e.x);
    float cy = cos(e.y), sy = sin(e.y);
    float cz = cos(e.z), sz = sin(e.z);
    mat3 rx = mat3(1.0, 0.0, 0.0, 0.0, cx, -sx, 0.0, sx, cx);
    mat3 ry = mat3(cy, 0.0, sy, 0.0, 1.0, 0.0, -sy, 0.0, cy);
    mat3 rz = mat3(cz, -sz, 0.0, sz, cz, 0.0, 0.0, 0.0, 1.0);
    return rz * ry * rx;
}
void main() {
    mat3 rot = EulerToMatrix(objectRotation);
    vec3 rotated = rot * (aPos * size);
	gl_Position = MVP * vec4(rotated,1.0);
	color = aColor;
    vec3 worldPos = rotated + objectPos;
    vec3 toC = (worldPos) - camPos;
    depthr = dot(toC, camForward);
}