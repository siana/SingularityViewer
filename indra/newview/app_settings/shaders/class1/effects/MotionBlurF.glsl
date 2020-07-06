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
uniform sampler2D tex1;
uniform mat4 inv_proj;
uniform mat4 prev_proj;
uniform int blur_strength;

VARYING vec2 vary_texcoord0;

#define SAMPLE_COUNT 10

vec4 getPosition(vec2 pos_screen, out vec4 ndc)
{
	float depth = texture2D(tex1, pos_screen.xy).r;
	vec2 sc = pos_screen.xy*2.0;
	sc -= vec2(1.0,1.0);
	ndc = vec4(sc.x, sc.y, 2.0*depth-1.0, 1.0);
	vec4 pos = inv_proj * ndc;
	pos /= pos.w;
	pos.w = 1.0;
	return pos;
}

void main(void) 
{
	vec4 ndc;
	vec4 pos = getPosition(vary_texcoord0,ndc);
	vec4 prev_pos = prev_proj * pos;
	prev_pos/=prev_pos.w;
	prev_pos.w = 1.0;
	vec2 vel = ((ndc.xy-prev_pos.xy) * .5) * .01 * blur_strength * 1.0/SAMPLE_COUNT;
	float len = length(vel);
	vel = normalize(vel) * min(len, 50);
	vec3 color = texture2D(tex0, vary_texcoord0.st).rgb; 
	vec2 texcoord = vary_texcoord0 + vel;
	for(int i = 1; i < SAMPLE_COUNT; ++i, texcoord += vel)  
	{  
		color += texture2D(tex0, texcoord.st).rgb; 
	}
	frag_color = vec4(color / SAMPLE_COUNT, 1.0);
}
