#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 1) in vec3 texcoord;

layout(set = 0, binding = 10) uniform samplerCube environment_cubemap;
layout(set = 0, binding = 11) uniform samplerCube radiance_cubemap;
layout(set = 0, binding = 12) uniform samplerCube irradiance_cubemap;

layout(location = 0) out vec4 out_color;

void main() 
{
    vec4 skybox_color = texture(radiance_cubemap, normalize(texcoord));
	out_color = skybox_color;
}