The basic idea is to add a small value to every pixel right before it is quantized (i.e. converted from the floating point representation used in the shader to 8 bits per channel in the framebuffer). 

Dithering is used to prevent banding
The idea is that the least significant bits of the color that would ordinarily get thrown out are combined with this added value and cause the pixel to have a chance of rounding differently than nearby pixels.

http://www.anisopteragames.com/how-to-fix-color-banding-with-dithering/

https://en.wikipedia.org/wiki/Dither	

WangHashNoise is a replacement for dithering noise function
Reference: https://youtu.be/4D5uX8wL1V8?t=11m22s
Super fast because of all the binary operations
Quality noise without repeating patterns

float WangHashNoise(uint u, uint v, uint s)