#version 460
#extension GL_ARB_bindless_texture : require
layout (local_size_x = 8, local_size_y = 8, local_size_z = 8) in;




layout(std430, binding = 0) buffer GBufferEx {
    uint frame[3840 * 2160];
    vec3 pos[3840*2160];
    int depth[3840* 2160];
    int voxelId[3840*2160];
};

uniform uvec2 fluidHandle;
uniform uvec2 fluidHandleBottom;
uniform uvec2 fluidHandleXP, fluidHandleXN, fluidHandleZP, fluidHandleZN;

// Helper to check and move fluid to a target image/coordinate
bool TryMove(uimage3D targetMap, ivec3 targetPos, uimage3D sourceMap, ivec3 sourcePos) {
    if (imageLoad(targetMap, targetPos)[0] == 0) {
        imageStore(targetMap, targetPos, uvec4(1));
        imageStore(sourceMap, sourcePos, uvec4(0));
        return true;
    }
    return false;
}

void main() {
    uimage3D fluidMap = uimage3D(fluidHandle);
    ivec3 p = ivec3(gl_GlobalInvocationID.xyz);
    
    if (any(greaterThanEqual(p, ivec3(64))) || any(lessThan(p, ivec3(0)))) return;
    
    uint fluidVal = imageLoad(fluidMap, p)[0];
    if (fluidVal == 0 || fluidVal == 255) return;

    if (p.y > 0) {
        if (TryMove(fluidMap, p + ivec3(0, -1, 0), fluidMap, p)) return;
    } else {
        // 2. Try to move DOWN into neighbor
        if (TryMove(uimage3D(fluidHandleBottom), ivec3(p.x, 63, p.z), fluidMap, p)) return;
    }
    if (p.y == 0) {
        if (p.x == 63) {
            if (TryMove(uimage3D(fluidHandleXP), ivec3(0, 63, p.z), fluidMap, p)) return;
        }
        if (p.x == 0) {
            if (TryMove(uimage3D(fluidHandleXN), ivec3(63, 63, p.z), fluidMap, p)) return;
        }
        if (p.z == 63) {
            if (TryMove(uimage3D(fluidHandleZP), ivec3(p.x, 63, 0), fluidMap, p)) return;
        }
        if (p.z == 0) {
            if (TryMove(uimage3D(fluidHandleZN), ivec3(p.x, 63, 63), fluidMap, p)) return;
        }
    }
}