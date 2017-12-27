#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform sampler2D prevFrameImage;
layout (set = 0, binding = 1, rgba8) uniform writeonly image2D currentFrameResultImage;

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 outColor;

void main()
{
	outColor = vec4(1.0,1.0,1.0, 1.0) * in_uv.x;
}