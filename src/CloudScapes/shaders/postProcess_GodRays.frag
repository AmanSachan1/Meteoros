#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0, rgba8) uniform image2D currentFrameResultImage;
layout (set = 0, binding = 1) uniform sampler2D godRayCreationDataSampler;

layout (set = 1, binding = 0) uniform CameraUBO
{
	mat4 view;
	mat4 proj;
	vec4 eye;
	vec2 tanFovBy2;
} camera;

layout (set = 2, binding = 0) uniform SunAndSkyUBO
{
	vec4 sunLocation;
	vec4 sunDirection;
	vec4 lightColor;
	float sunIntensity;
};

layout(location = 0) in vec2 fragTexCoord;

#define NUM_SAMPLES 100
#define WEIGHT_FOR_SAMPLE 0.02
#define DECAY 1.0
#define EXPOSURE 2.0

//Reference: https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch13.html
void main() 
{	
	ivec2 dim = imageSize(currentFrameResultImage); //texture(currentFrameResultImage, fragTexCoord).rgb;
	ivec2 pixelPos = ivec2(floor(dim.x * fragTexCoord.x), floor(dim.y * fragTexCoord.y)); 
	
	vec4 inverseLightIntensity = vec4(texture(godRayCreationDataSampler, fragTexCoord).rgb, 1.0);

	vec2 radialSize = vec2(1.0 / dim.s, 1.0 / dim.t); 
	vec2 uv = fragTexCoord;

	// vec3 cam_to_sun = normalize(sunLocation.xyz - eye.xyz);
	// vec3 camForward =  -normalize(vec3(camera.view[0][2], camera.view[1][2], camera.view[2][2] ));

	// float sunAngle = abs(tan( (cam_to_sun.y-camForward.y) )); //would divide by z but z = 1
	// if(sunAngle < tanFovBy2)
	// {
	// 	//sun is inside camera frustum
	// 	//calculate screen space pos and proceed normally
	// }
	// else
	// {
	// 	//sun is outside camera frustum
	// 	//figure out closest pixel inside the camera frustum and do radial blur based on that
	// }

	// Sun Pos in screen space
	vec4 sunPos_ = camera.proj * camera.view * sunLocation;
	vec2 sunPos_ss = (sunPos_.xy+1.0f)/2.0f;

	// Calculate vector from pixel to light source in screen space.
	vec2 deltaTexCoord = (uv - sunPos_ss);

	// Divide by number of samples and scale by control factor.
	deltaTexCoord *= 1.0f / float(NUM_SAMPLES) * (1.0f);

	// Store initial sample.
	vec3 color = inverseLightIntensity.xyz;

	// Set up illumination decay factor.
	float illuminationDecay = 1.0f;

	// Evaluate summation from Equation 3 NUM_SAMPLES iterations.
	for (int i = 0; i < NUM_SAMPLES; i++)
	{
		// Step sample location along ray.
		uv -= deltaTexCoord;
		// Retrieve sample at new location.
		vec3 sampleColor = texture(godRayCreationDataSampler, uv).rgb;
		// Apply sample attenuation scale/decay factors.
		sampleColor *= illuminationDecay * WEIGHT_FOR_SAMPLE;
		// Accumulate combined color.
		color += sampleColor;
		// Update exponential decay factor.
		illuminationDecay *= DECAY;
	}

	vec4 final_color = vec4(color*EXPOSURE, 1.0);

	//imageStore( currentFrameResultImage, pixelPos, final_color );
}