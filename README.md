# Vulkan_Cloudscape_Rendering
Real-time Cloudscape Rendering in Vulkan


## Graphics Pipeline
![](/images/SimplifiedPipeline.png)

## Instructions
### compile GLSL shaders into SPIR-V bytecode:
#### WINDOWS ONLY!
Create a compile.bat file with the following contents:

C:/VulkanSDK/1.0.17.0/Bin32/glslangValidator.exe -V shader.vert
C:/VulkanSDK/1.0.17.0/Bin32/glslangValidator.exe -V shader.frag
pause
Replace the path to glslangValidator.exe with the path to where you installed the Vulkan SDK. Double click the file to run it.


## Notes

- We did not add checks to make sure some features are supported by the GPU before using them, such as anisotropic filtering.


##Resources

- Image Loading Library: https://github.com/nothings/stb
- Obj Loading Library: https://github.com/syoyo/tinyobjloader

- Setting Up Compute Shader that writes to a texture that is sampled by the fragment shader: https://github.com/SaschaWillems/Vulkan/tree/master/examples/raytracing

- Why to include stb in .cpp file: https://stackoverflow.com/questions/43348798/double-inclusion-and-headers-only-library-stbi-image 