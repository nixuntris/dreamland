#version 460
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_shader_ballot : require
#extension GL_ARB_shader_group_vote : require
layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

const int chunkSize = 32;

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

uniform int wx;
uniform int wy;
uniform int wz;
uniform int renderWidth;
uniform int renderHeight;
uniform vec3 camPos;
uniform mat4 matView;
uniform mat4 matProj;

uint readVoxelData(ivec3 pos) {
    if (any(lessThan(pos, ivec3(0))) ||
        pos.x >= wx * chunkSize ||
        pos.y >= wy * chunkSize ||
        pos.z >= wz * chunkSize)
        return 0u;   
    ivec3 chunk = pos / chunkSize;
    int index = chunk.y * (wx * wz) + chunk.z * wx + chunk.x;
    int regionLod = handles[index].lod;
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

void main() {
    ivec2 px = ivec2(gl_GlobalInvocationID.xy);
    if (px.x >= renderWidth || px.y >= renderHeight) return;

    vec2 ndc = vec2((px.x + 0.5) / renderWidth * 2.0 - 1.0,
                    1.0 - (px.y + 0.5) / renderHeight * 2.0);
    vec4 far = inverse(matProj * matView) * vec4(ndc, 1.0, 1.0);
    vec3 rd = normalize(far.xyz / far.w - camPos);
    rd = mix(rd, vec3(1e-6), lessThan(abs(rd), vec3(1e-6)));

    ivec3 cell = ivec3(floor(camPos));
    ivec3 stepDir = ivec3(sign(rd));
    vec3 tDelta = abs(1.0 / rd);
    vec3 tMax = (vec3(cell) + vec3(max(stepDir, ivec3(0))) - camPos) / rd;

    uint color = 0xFFF2BF87u; 
    int axis = 0;

    for (int i = 0; i < 1024; i++) {
        uint v = readVoxelData(cell);
        if (v != 0u) {
            uint g = uint(clamp(float(v) * 40.0, 0.0, 255.0));
            color = g | (g << 8) | (g << 16) | (255u << 24);
            break;
        }
        if (tMax.x < tMax.y && tMax.x < tMax.z) { cell.x += stepDir.x; tMax.x += tDelta.x; axis = 0; }
        else if (tMax.y < tMax.z)                { cell.y += stepDir.y; tMax.y += tDelta.y; axis = 1; }
        else                                     { cell.z += stepDir.z; tMax.z += tDelta.z; axis = 2; }
    }

    frame[px.y * 3840 + px.x] = color;
}
