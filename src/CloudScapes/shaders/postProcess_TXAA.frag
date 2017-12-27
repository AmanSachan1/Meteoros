#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform sampler2D prevFrameImage;
layout (set = 0, binding = 1, rgba8) uniform image2D currentFrameResultImage;

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 outColor;

void main()
{
	ivec2 dim = imageSize(currentFrameResultImage);
	ivec2 pixelPos = clamp(ivec2(round(float(dim.x) * in_uv.x), round(float(dim.y) * in_uv.y)), ivec2(0.0), ivec2(dim.x - 1, dim.y - 1));

	vec4 prevColor = texture( prevFrameImage, in_uv );
	vec4 currColor = imageLoad( currentFrameResultImage, pixelPos );

	vec4 color_TXAA = mix(prevColor, currColor, 0.1);

	imageStore( currentFrameResultImage, pixelPos, color_TXAA );
	outColor = currColor;
}