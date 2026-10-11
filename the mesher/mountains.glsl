#version 460
#extension GL_ARB_bindless_texture : require
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
uniform uvec2 terrainHandle;
void main() {

    ivec3 p = ivec3(gl_GlobalInvocationID.xyz);
	uimage3D terrain = uimage3D(terrainHandle);
	for (int i = 0; i < 300; i++) {
		imageStore(terrain,ivec3(p.x,i,p.y),uvec4(1));
	}
}