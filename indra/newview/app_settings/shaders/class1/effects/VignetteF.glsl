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
uniform float vignette_strength;				//0 - 1
uniform float vignette_radius;					//0 - 8
uniform float vignette_darkness;				//0 - 1
uniform float vignette_desaturation;			//0 - 10
uniform float vignette_chromatic_aberration;	//0 - .1

VARYING vec2 vary_texcoord0;

float luminance(vec3 color)
{
	/// CALCULATING LUMINANCE (Using NTSC lum weights)
	/// http://en.wikipedia.org/wiki/Luma_%28video%29
	return dot(color, vec3(0.299, 0.587, 0.114));
}

void main(void) 
{
	vec3 color = texture2D(tex0, vary_texcoord0).rgb;
	vec2 offset = vary_texcoord0-vec2(.5);
	float vignette = clamp(pow(1 - dot(offset,offset),vignette_radius) + 1-vignette_strength, 0.0, 1.0);
	float inv_vignette = 1-vignette;
	
	//vignette chromatic aberration (g
	vec2 shift = offset * vignette_chromatic_aberration * inv_vignette;
	float g = texture2D(tex0,vary_texcoord0-shift).g;
	float b = texture2D(tex0,vary_texcoord0-2*shift).b;
	color.gb = vec2(g,b);

	//vignette 'black' overlay.
	color = mix(color * vignette, color, 1-vignette_darkness);
	
	//vignette desaturation
	color = mix(vec3(luminance(color)), color, clamp(1-inv_vignette*vignette_desaturation,0,1));
	
	frag_color = vec4(color,1.0);
}
