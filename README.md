# Meteoros
This project is a real-time cloudscape renderer in Vulkan that was made as the final project for the University of Pennsylvania course, CIS 565: GPU Programming and Architecture. It is based on the theory and implementation as described in the following SIGGRAPH 2015 and 2017 presentations: 

** 2015 ** The Real-time Volumetric Cloudscapes of Horizon Zero Dawn
** 2017 ** Nubis: Authoring Realtime Volumetric Cloudscapes with the Decima Engine 

Contributors:
1. Meghana Seshadri - M.S.E. Computer Graphics and Game Technology, UPenn
2. Aman Sachan - M.S.E. Computer Graphics and Game Technology, UPenn

## Project Description 

This goal of this project was to implement the 

Please see a more detailed description of the algorithms and set up below in the [Detailed Description](https://github.com/Aman-Sachan-asach/Meteoros/edit/master/README.md#detailed-description)


## Instructions
* Compile GLSL shaders into SPIR-V bytecode:
* ** Windows ONLY ** Create a compile.bat file with the following contents:

```
C:/VulkanSDK/1.0.17.0/Bin32/glslangValidator.exe -V shader.vert
C:/VulkanSDK/1.0.17.0/Bin32/glslangValidator.exe -V shader.frag
pause
```
Note: Replace the path to glslangValidator.exe with the path to where you installed the Vulkan SDK. Double click the file to run it.


## Detailed Description


### Graphics Pipeline
![](/images/SimplifiedPipeline.png)



## Implementation Notes

- We did not add checks to make sure some features are supported by the GPU before using them, such as anisotropic filtering.


## Performance Analysis 

Performance analysis conducted on: Windows 10, i7-7700HQ @ 2.8GHz 32GB, GTX 1070(laptop GPU) 8074MB (Personal Machine: Customized MSI GT62VR 7RE)


## Resources
- Curl Noise Textures: http://bitsquid.blogspot.com/2016/07/volumetric-clouds.html
- Image Loading Library: https://github.com/nothings/stb
- Obj Loading Library: https://github.com/syoyo/tinyobjloader

- Setting Up Compute Shader that writes to a texture that is sampled by the fragment shader: https://github.com/SaschaWillems/Vulkan/tree/master/examples/raytracing

- Why to include stb in .cpp file: https://stackoverflow.com/questions/43348798/double-inclusion-and-headers-only-library-stbi-image 
