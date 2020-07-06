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
uniform int layerCount;

VARYING vec2 vary_texcoord0;

void main(void) 
{
	vec3 color = pow(floor(pow(vec3(texture2D(tex0, vary_texcoord0.st)),vec3(.6)) * layerCount)/layerCount,vec3(1.66666));
	frag_color = vec4(color, 1.0);
}
