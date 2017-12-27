## TXAA

### Halton Sequence: //https://en.wikipedia.org/wiki/Halton_sequence

### Why not MSAA or FXAA?
	
	FXAA isnt good enough to deal with high variance boundaries (corners and specular surfaces or in this case volume that is sampled very few times)
	MSAA does not affect shading aliasing
	Unreal Engine talk on more of this: https://de45xmedrsdbp.cloudfront.net/Resources/files/TemporalAA_small-59732822.pdf

### Resources

https://de45xmedrsdbp.cloudfront.net/Resources/files/TemporalAA_small-59732822.pdf
https://www.gdcvault.com/play/1022970/Temporal-Reprojection-Anti-Aliasing-in
https://www.youtube.com/watch?v=FMfC47xsImU&index=2&list=LLgt_lAI0-x_RhlGyv6DEBAw&t=1106s