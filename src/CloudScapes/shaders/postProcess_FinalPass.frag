#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform sampler2D preFinalImageSampler;

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
#define EXPOSURE 3.7



//column based
mat3 sobel_x = mat3(vec3(-1.0, -2.0, -1.0), vec3(0.0), vec3(1.0, 2.0, 1.0));
mat3 sobel_y = mat3(vec3(-1.0, 0.0, 1.0), vec3(-2.0, 0.0, 2.0), vec3(-1.0, 0.0, 1.0));



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
   vec3 in_color = texture(preFinalImageSampler, in_uv).rgb;


   // // SOBEL FILTERING
   // float offset = 0.001;

   // // Neighbors
   // vec2 _a = vec2(in_uv.x - offset, in_uv.y - offset);
   // vec2 _b = vec2(in_uv.x, in_uv.y - offset);
   // vec2 _c = vec2(in_uv.x + offset, in_uv.y - offset);

   // vec2 _d = vec2(in_uv.x - offset, in_uv.y);
   // vec2 _e = vec2(in_uv.x, in_uv.y);
   // vec2 _f = vec2(in_uv.x + offset, in_uv.y);

   // vec2 _g = vec2(in_uv.x - offset, in_uv.y + offset);
   // vec2 _h = vec2(in_uv.x, in_uv.y + offset);
   // vec2 _i = vec2(in_uv.x + offset, in_uv.y + offset);


   // vec4 pixel_x = (sobel_x[0][0] * vec4(texture(preFinalImageSampler, _a))) + (sobel_x[0][1] * vec4(texture(preFinalImageSampler, _b))) + (sobel_x[0][2] * vec4(texture(preFinalImageSampler, _c))) +
   //                (sobel_x[1][0] * vec4(texture(preFinalImageSampler, _d))) + (sobel_x[1][1] * vec4(texture(preFinalImageSampler, _e))) + (sobel_x[1][2] * vec4(texture(preFinalImageSampler, _f))) +
   //                (sobel_x[2][0] * vec4(texture(preFinalImageSampler, _g))) + (sobel_x[2][1] * vec4(texture(preFinalImageSampler, _h))) + (sobel_x[2][2] * vec4(texture(preFinalImageSampler, _i)));


   // vec4 pixel_y = (sobel_y[0][0] * vec4(texture(preFinalImageSampler, _a))) + (sobel_y[0][1] * vec4(texture(preFinalImageSampler, _b))) + (sobel_y[0][2] * vec4(texture(preFinalImageSampler, _c))) +
   //                (sobel_y[1][0] * vec4(texture(preFinalImageSampler, _d))) + (sobel_y[1][1] * vec4(texture(preFinalImageSampler, _e))) + (sobel_y[1][2] * vec4(texture(preFinalImageSampler, _f))) +
   //                (sobel_y[2][0] * vec4(texture(preFinalImageSampler, _g))) + (sobel_y[2][1] * vec4(texture(preFinalImageSampler, _h))) + (sobel_y[2][2] * vec4(texture(preFinalImageSampler, _i)));


   // vec4 _result = sqrt((pixel_x * pixel_x) + (pixel_y * pixel_y));

   // float gray = dot(in_color, vec3(0.299, 0.587, 0.114));
   // in_color = vec3(gray) * vec3(_result[0], _result[1], _result[2]);

   float whitepoint = 100.0f; //changes the point at which something becomes pure white --> not a hundred precent 
   //sure how it scales though I think the white point is the value that is mapped to 1.0 in the regular RGB space.
   vec3 toneMapped_color = tonemap(in_color, whitepoint);

   // outColor = vec4(toneMapped_color, 1.0);
   outColor = vec4(in_color, 1.0);
}