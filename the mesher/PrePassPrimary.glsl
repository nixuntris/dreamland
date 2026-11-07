#version 460
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_shader_ballot : require
#extension GL_ARB_shader_group_vote : require
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

const int chunkSize = 32;

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
uniform vec3 camForward;
uniform int wx;
uniform int wy;
uniform int wz;
uniform int renderWidth;
uniform int renderHeight;
uniform vec3 camPos;
uniform mat4 matView;
uniform mat4 matProj;
uniform int frameIndex;
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
    if (regionLod==32) return handles[index].filled; //the problem is here
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

    vec3 cell = ivec3(floor(camPos));

    uint color = 0xFFF2BF87u; 
    int axis = 0;
    bool hitAnything = false;
    float distanceTravelled = 0;
    for (int i = 0; i < 4096; i++) {
        int lod;
        uint v = readVoxelData(ivec3(cell),lod);

        float distanceTravelled = distance(floor(camPos),cell);
        if (any(lessThan(cell, ivec3(0))) ||
            cell.x >= wx * chunkSize ||
            cell.y >= wy * chunkSize ||
            cell.z >= wz * chunkSize || 
            distanceTravelled>4096)
        {
            
            depth[px.y * 3840 + px.x] = 4096;
        }
            

        ivec3 chunk = ivec3(cell) / chunkSize;
        int index = chunk.y * (wx * wz) + chunk.z * wx + chunk.x;

        if (handles[index].lod == 0) { 
            cell += rd*float(lod);
            continue;
        }
        
        if (v != 0u) {
            depth[px.y * 3840 + px.x] = int(distanceTravelled);
            break;
        }

        cell += rd*float(lod);
    }
}
