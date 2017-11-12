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