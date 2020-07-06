/**
 * @file nightVisionF.glsl
 *
 * Copyright (c) 2007-$CurrentYear$, Linden Research, Inc.
 * $License$
 */
 


#ifdef DEFINE_GL_FRAGCOLOR
out vec4 frag_color;
#else
#define frag_color gl_FragColor
#endif

uniform sampler2D tex0;
uniform sampler2D tex1;
uniform float brightMult;
uniform float noiseStrength;

VARYING vec2 vary_texcoord0;
VARYING vec2 vary_texcoord1;

float luminance(vec3 color)
{
	/// CALCULATING LUMINANCE (Using NTSC lum weights)
	/// http://en.wikipedia.org/wiki/Luma_%28video%29
	return dot(color, vec3(0.299, 0.587, 0.114));
}


void main(void) 
{
	/// Get scene color
	vec3 color = vec3(texture2D(tex0, vary_texcoord0));
	
	/// Extract luminance and scale up by night vision brightness
	float lum = luminance(color) * brightMult;

	/// Convert into night vision color space
	/// Newer NVG colors (crisper and more saturated)
	vec3 outColor = (lum * vec3(0.91, 1.21, 0.9)) + vec3(-0.07, 0.1, -0.12); 

	/// Add noise
	float noiseValue = texture2D(tex1, vary_texcoord1).r;
	noiseValue = (noiseValue - 0.5) * noiseStrength;

	/// Older NVG colors (more muted)
	// vec3 outColor = (lum * vec3(0.82, 0.75, 0.83)) + vec3(0.05, 0.32, -0.11); 
	
	outColor += noiseValue;

	frag_color = vec4(outColor, 1.0);
}
