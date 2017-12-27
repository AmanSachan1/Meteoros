#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform sampler2D inputImageSampler;
layout (set = 0, binding = 1, rgba8) uniform writeonly image2D currentFrameResultImage;

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 outColor;

//Reference: http://filmicworlds.com/blog/filmic-tonemapping-operators/
//Also https://www.shadertoy.com/view/lslGzl
//Originally from the game Uncharted 2 by Naughty Dog
#define A 0.15
#define B 0.50
#define C 0.10
#define D 0.20
#define E 0.02
#define F 0.30
#define INVGAMMA (1.0 / 2.2)
#define EXPOSURE 2.5

vec3 Uncharted2Tonemap(vec3 x)
{
   return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 tonemap(vec3 x, float whiteBalance) 
{
   vec3 color = Uncharted2Tonemap(EXPOSURE * x);

   vec3 white = vec3(whiteBalance);
   vec3 whitemap = 1.0 / Uncharted2Tonemap(white);

   color *= whitemap;
   return pow(color, vec3(INVGAMMA));
}

void main() 
{
	ivec2 dim = imageSize(currentFrameResultImage);
	ivec2 pixelPos = clamp(ivec2(round(float(dim.x) * in_uv.x), round(float(dim.y) * in_uv.y)), ivec2(0.0), ivec2(dim.x - 1, dim.y - 1));

	vec3 in_color = texture(inputImageSampler, in_uv).rgb;

	float whitepoint = 100.0f; //changes the point at which something becomes pure white --> not a hundred precent 
	//sure how it scales though I think the white point is the value that is mapped to 1.0 in the regular RGB space.
	vec3 toneMapped_color = tonemap(in_color, whitepoint);

	imageStore( currentFrameResultImage, pixelPos, vec4(toneMapped_color, 1.0) );
	// imageStore( currentFrameResultImage, pixelPos, vec4(in_color, 1.0) );
}