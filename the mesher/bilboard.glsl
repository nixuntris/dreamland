#version 460
layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(std430, binding = 0) buffer GBufferEx {
    uint frame[3840 * 2160];
    vec3 pos[3840*2160];
    int depth[3840* 2160];
    int voxelId[3840*2160];
};

uniform int renderWidth;
uniform int renderHeight;
uniform vec3 camPos;
uniform vec3 camForward;
uniform mat4 matView;
uniform mat4 matProj;
uniform int frameIndex;
uniform float billboardSize;
uniform int billboardColor;
uniform vec3 billboards[1024];

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

void DrawQuadSoftwareCS(
    vec2 v0, vec2 v1, vec2 v2, vec2 v3,
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

            if (depth[idx] > quadDepth) {
                depth[idx] = quadDepth;
                frame[idx] = color;
                pos[idx] = quadPos;
            }
        }
    }
}

void main()
{
    int billboardId = int(gl_GlobalInvocationID.x);
    if (billboardId >= 512) return;
    vec3 bbPos = billboards[billboardId];

    vec3 toBB = bbPos - camPos;
    float distAlongForward = dot(toBB, camForward);
    if (distAlongForward <= 0.0) return;

    vec3 camRight = normalize(vec3(matView[0][0], matView[1][0], matView[2][0]));
    vec3 camUp    = normalize(vec3(matView[0][1], matView[1][1], matView[2][1]));

    float s = billboardSize;
    vec3 p0 = bbPos - camRight * s - camUp * s;
    vec3 p1 = bbPos + camRight * s - camUp * s;
    vec3 p2 = bbPos + camRight * s + camUp * s;
    vec3 p3 = bbPos - camRight * s + camUp * s;

    vec2 s0, s1, s2, s3;
    if (!GetWorldToScreenClipped(p0, s0) ||
        !GetWorldToScreenClipped(p1, s1) ||
        !GetWorldToScreenClipped(p2, s2) ||
        !GetWorldToScreenClipped(p3, s3))
        return;

    vec2 SCREEN_MIN = vec2(0.0, 0.0);
    vec2 SCREEN_MAX = vec2(float(renderWidth), float(renderHeight));

    if ((s0.x < SCREEN_MIN.x && s1.x < SCREEN_MIN.x && s2.x < SCREEN_MIN.x && s3.x < SCREEN_MIN.x) ||
        (s0.x >= SCREEN_MAX.x && s1.x >= SCREEN_MAX.x && s2.x >= SCREEN_MAX.x && s3.x >= SCREEN_MAX.x) ||
        (s0.y < SCREEN_MIN.y && s1.y < SCREEN_MIN.y && s2.y < SCREEN_MIN.y && s3.y < SCREEN_MIN.y) ||
        (s0.y >= SCREEN_MAX.y && s1.y >= SCREEN_MAX.y && s2.y >= SCREEN_MAX.y && s3.y >= SCREEN_MAX.y))
        return;

    int depthVal = int(distAlongForward);

    DrawQuadSoftwareCS(s0, s1, s2, s3, uint(billboardColor), depthVal, bbPos, billboardId);
}