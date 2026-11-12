#version 460
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_shader_ballot : require
#extension GL_ARB_shader_group_vote : require

layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;
const int chunkSize =32;
uniform int wx;
uniform int wy;
uniform int wz;

struct Region {
    uvec2 handle;
    uvec2 probeHandle;
    int lod;
    int filled;
};
layout(std430, binding = 3) buffer BitHandles {
    Region handles[];
};
void setVoxelData(ivec3 pos, uint value) {
    if (any(lessThan(pos, ivec3(0))) ||
        pos.x >= wx * chunkSize ||
        pos.y >= wy * chunkSize ||
        pos.z >= wz * chunkSize)
        return;

    ivec3 chunk = pos / chunkSize;
    int index = chunk.y * (wx * wz) + chunk.z * wx + chunk.x;

    int regionLod = handles[index].lod;

    if (regionLod == 32 || regionLod == 3000)
        return;

    uvec2 handle = handles[index].handle;

    if (handle != uvec2(0)) {
        ivec3 lp = (pos & (chunkSize - 1)) / regionLod;

        bool pending = true;

        while (anyInvocationARB(pending)) {
            if (pending) {
                uvec2 cur = uvec2(
                    readFirstInvocationARB(handle.x),
                    readFirstInvocationARB(handle.y)
                );

                if (handle == cur) {
                    imageStore(uimage3D(cur), lp, uvec4(value));
                    pending = false;
                }
            }
        }
    }
}
uniform uvec2 handle;

uniform uint packedColors[1024];
uint hashVec3(vec3 v) {
    uint x = uint(v.x);
    uint y = uint(v.y);
    uint z = uint(v.z);
    uint h = x * 374761393u + y * 668265263u + z * 1274126177u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

uniform vec3 renderPosition;
void main()
{
    
    ivec3 voxelId = ivec3(gl_GlobalInvocationID.xyz);
    uimage3D fluidMap = uimage3D(handle);
   
    uint r = 0;
    if (handle != uvec2(0)) {
        uimage3D fluidMap = uimage3D(handle);
        bool pending = true;
        while (anyInvocationARB(pending)) {
            if (pending) {
                uvec2 cur = uvec2(readFirstInvocationARB(handle.x), readFirstInvocationARB(handle.y));
                if (handle == cur) {
                    r = imageLoad(uimage3D(cur), voxelId).x;
                    pending = false;
                }
            }
        }
    }  
    vec3 baseP = renderPosition+voxelId;
    if (r==255) {
        setVoxelData(ivec3(baseP),0);
    }
}
