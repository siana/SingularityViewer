/**
 * @file colorFilterF.glsl
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
uniform float brightness;
uniform float contrast;
uniform vec3  contrastBase;
uniform float saturation;
uniform vec3  lumWeights;

uniform float gamma;

VARYING vec2 vary_texcoord0;

float luminance(vec3 color)
{
	/// CALCULATING LUMINANCE (Using NTSC lum weights)
	/// http://en.wikipedia.org/wiki/Luma_%28video%29
	return dot(color, vec3(0.299, 0.587, 0.114));
}

void main(void) 
{
	vec3 color = vec3(texture2D(tex0, vary_texcoord0.st));

	/// Apply gamma
	color = pow(color, vec3(1.0/gamma));
	
	/// Modulate brightness
	color *= brightness;

	/// Modulate contrast
	color = mix(contrastBase, color, contrast);

	/// Modulate saturation
	color = mix(vec3(luminance(color)), color, saturation);

	frag_color = vec4(color, 1.0);
}
