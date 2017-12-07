#version 450
#extension GL_ARB_separate_shader_objects : enable

//Reference: https://github.com/SaschaWillems/Vulkan/blob/master/data/shaders/radialblur/radialblur.vert

out gl_PerVertex 
{
    vec4 gl_Position;
};

layout (location = 0) out vec2 outUV;

void main() 
{
	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
}