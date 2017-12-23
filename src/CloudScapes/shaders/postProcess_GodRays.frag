#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0, rgba32f) uniform image2D currentFrameResultImage;
layout (set = 1, binding = 0) uniform sampler2D godRayCreationDataSampler;

layout (set = 2, binding = 0) uniform CameraUBO
{
	mat4 view;
	mat4 proj;
	vec4 eye;
	vec2 tanFovBy2;
} camera;

layout (set = 3, binding = 0) uniform SunAndSkyUBO
{
	vec4 sunLocation;
	vec4 sunDirection;
	vec4 lightColor;
	float sunIntensity;
} sunAndSky;

layout(location = 0) in vec2 in_uv;

#define MAX_SAMPLES 100
#define ATMOSPHERE_DENSITY 1.0f
#define WEIGHT_FOR_SAMPLE 0.001
#define DECAY 1.0
#define EXPOSURE 1.0

#define BLACK vec3(0,0,0)
#define WHITE vec3(1,1,1)

#define PI 3.14159265

const vec4 bitEnc = vec4(1.,255.,65025.,16581375.);
const vec4 bitDec = 1./bitEnc;

float extract32fFromRGBA8f(vec2 uv)
{
	vec4 x = texture(godRayCreationDataSampler, uv);
	return dot(x, bitDec);
}

bool uv_OutsideCamFrustum(in vec2 test_uv)
{
	if( test_uv.x < 0.0 || test_uv.x > 1.0 ||
		test_uv.y < 0.0 || test_uv.y > 1.0 )
	{
		return true;
	}

	return false;
}

void godRayLoop(in vec2 sampleUV, inout vec4 sampleValue, inout float illuminationDecay, inout vec3 accumColor)
{	
	vec3 sampleColor = sampleValue.rgb * sampleValue.a;
	sampleColor *= illuminationDecay * WEIGHT_FOR_SAMPLE; // Apply sample attenuation scale/decay factors.
	illuminationDecay *= DECAY; // Update exponential decay factor.
	accumColor += sampleColor; //add to accumulated godray color
}

//References: - https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch13.html
//			  -	https://github.com/Erkaman/glsl-godrays
void main()
{
	ivec2 dim = imageSize(currentFrameResultImage); //texture(currentFrameResultImage, fragTexCoord).rgb;
	ivec2 pixelPos = clamp(ivec2(round(float(dim.x) * in_uv.x), round(float(dim.y) * in_uv.y)), ivec2(0.0), ivec2(dim.x - 1, dim.y - 1));
	vec2 uv = in_uv;

	int numSamples = MAX_SAMPLES;
	// vec3 sunLocation = vec3(0.0, 1.0, 0.0)*max(0.2, (1.0-sunAndSky.sunLocation.y)); //move with sun location god rays
	vec3 sunLocation = vec3(0.0, 1.0, 0.0); //static god rays

	vec3 cam_to_sun = normalize(sunLocation.xyz - camera.eye.xyz);
	vec3 camForward =  -normalize(vec3(camera.view[0][2], camera.view[1][2], camera.view[2][2] ));
	//Blend godRay Intensity by blend factor to prevernt hard cut offs when sun goes behind camera
	float blendFactor = dot(cam_to_sun, camForward);

	if(blendFactor < 0.0)
	{
		//if dot product is -ve the sun is in the hemisphere behind the camera
		//sun is behind the camera you cant really do this correctly without actual volumetric calculations or creating a light map (inverse shadow map)
		//All of that is expensive and so we're not going to bother changing that
		return;
	}

	// Sun position in ndc space does not necessarily mean that it is inside the camera frustum;
	vec4 sunPos_ndc = camera.proj * camera.view * vec4(sunLocation,1.0); 	// Sun Pos in NDC space
	vec2 sunPos_ss = clamp((sunPos_ndc.xy+1.0f)/2.0f, 0.0, 1.0); //Sun Pos in Screen Space

	bool flag_sunOutsideCamFrustum = uv_OutsideCamFrustum(sunPos_ss);

	vec3 accumColor = vec3(0.0f, 0.0f, 0.0f);
	float illuminationDecay = 1.0f;
	// Calculate the radial change in uv coords in a direction towards the light source, i.e the sun
	vec2 delta_uv = ((uv - sunPos_ss)/float(numSamples)) * ATMOSPHERE_DENSITY;

	// Take samples along the vector from the pixel to the light source;
	//Use these samples to calculate an accumulated color
	vec4 sampleValue = sunAndSky.lightColor;
	vec2 forwardInScreenSpace = vec2(0.0,1.0);

	if(flag_sunOutsideCamFrustum)
	{
		//This case causes more divergence so we want to handle it separately from the normal logic
		for (int i = 0; i < numSamples; i++)
		{
			if(uv_OutsideCamFrustum(uv))
			{
				//If sun is outside Camera Frustum; create a gradient that will be used as the color of the pixels outside the camera Frustum --> This assumes the 
				//godRay is completely unoccluded. ---> may want to scale down intensity in this
				float t = dot( uv-sunPos_ss, forwardInScreenSpace);
				vec4 fakePixel = vec4(mix(BLACK, sampleValue.xyz, t), 1.0);
				godRayLoop(uv, fakePixel, illuminationDecay, accumColor);
				uv -= delta_uv; // Step sample location along ray.
			}
			else
			{
				sampleValue.a = extract32fFromRGBA8f(uv); // Retrieve sample at new location.
				godRayLoop(uv, sampleValue, illuminationDecay, accumColor);
				uv -= delta_uv; // Step sample location along ray.
			}
		}
		accumColor = vec3(0.0f, 0.0f, 0.0f);
	}
	else
	{
		//Do things normally if sun is in cam Frustum
		for (int i = 0; i < numSamples; i++)
		{
			sampleValue.a = extract32fFromRGBA8f(uv); // Retrieve sample at new location.
			godRayLoop(uv, sampleValue, illuminationDecay, accumColor);
			uv -= delta_uv; // Step sample location along ray.
		}
	}

	vec4 god_ray_color =  vec4( accumColor * EXPOSURE, 1.0 ) * blendFactor; //texture(godRayCreationDataSampler, uv);//
	vec4 originalpixelColor = imageLoad( currentFrameResultImage, pixelPos );

	// if(length(god_ray_color) < 0.001) {
	// 	god_ray_color = vec4(1, 0, 0, 1);
	// }
	// vec4 final_color = vec4(originalpixelColor.aaa, 1);///*originalpixelColor + */god_ray_color;

	vec4 final_color = originalpixelColor + god_ray_color;

	imageStore( currentFrameResultImage, pixelPos, originalpixelColor );
}