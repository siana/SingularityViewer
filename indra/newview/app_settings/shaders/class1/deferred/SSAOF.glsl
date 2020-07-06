/** 
 * @file sunLightF.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2007, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */
 


#ifdef DEFINE_GL_FRAGCOLOR
out vec4 frag_color;
#else
#define frag_color gl_FragColor
#endif

VARYING vec2 vary_fragcoord;

uniform sampler2D depthMapDownsampled;
uniform sampler2D normalMap;
uniform sampler2D noiseMap;

uniform mat4 inv_proj;

uniform float ssao_radius;
uniform float ssao_max_radius;
uniform float ssao_factor;
uniform vec2 kern_scale;
uniform vec2 noise_scale;

vec3 decode_normal(vec2 enc);
vec4 getPosition(vec2 pos_screen);

vec2 getKern(int i)
{
	vec2 kern[8];
	// exponentially (^2) distant occlusion samples spread around origin
	kern[0] = vec2(-1.0, 0.0) * (0.125*0.125*.9+.1);
	kern[1] = vec2(1.0, 0.0) * (0.250*0.250*.9+.1);
	kern[2] = vec2(0.0, 1.0) * (0.375*0.375*.9+.1);
	kern[3] = vec2(0.0, -1.0) * (0.500*0.500*.9+.1);
	kern[4] = vec2(0.7071, 0.7071) * (0.625*0.625*.9+.1);
	kern[5] = vec2(-0.7071, -0.7071) * (0.750*0.750*.9+.1);
	kern[6] = vec2(-0.7071, 0.7071) * (0.875*0.875*.9+.1);
	kern[7] = vec2(0.7071, -0.7071) * (1.000*1.000*.9+.1);
	
	return kern[i];
}

//calculate decreases in ambient lighting when crowded out (SSAO)
float calcAmbientOcclusion(vec4 pos, vec3 norm)
{
	vec2 pos_screen = vary_fragcoord.xy;
	vec2 noise_reflect = texture2D(noiseMap, vary_fragcoord.xy * noise_scale).xy;
	
	 // We treat the first sample as the origin, which definitely doesn't obscure itself thanks to being visible for sampling in the first place.
	float points = 1.0;
	float angle_hidden = 0.0;
		
	// use a kernel scale that diminishes with distance.
	// a scale of less than 32 is just wasting good samples, though.
	vec2 scale = max(32.0, min(ssao_radius / -pos.z, ssao_max_radius)) * kern_scale;
	
	// it was found that keeping # of samples a constant was the fastest, probably due to compiler optimizations (unrolling?)
	for (int i = 0; i < 8; i++)
	{
		vec2 samppos_screen = pos_screen + scale * reflect(getKern(i), noise_reflect);
		vec3 samppos_world = getPosition(samppos_screen).xyz;

		vec3 diff = samppos_world - pos.xyz;

		if (diff.z < ssao_factor && diff.z != 0.0)
		{
			float dist = length(diff);
			float angrel = max(0.0, dot(norm.xyz, diff/dist));
			float distrel = 1.0/(1.0+dist*dist);
			float samplehidden = min(angrel, distrel);
	
			angle_hidden += (samplehidden);
			points += 1.0;
		}
	}

	angle_hidden /= points;

	float rtn = (1.0 - angle_hidden);
	return (rtn * rtn) * (rtn * rtn); //Pow2 to increase darkness to match previous behavior.
}

void main() 
{
	vec2 pos_screen = vary_fragcoord.xy;

	//try doing an unproject here
	
	vec4 pos = getPosition(pos_screen);
	
	vec3 norm = texture2D(normalMap, pos_screen).xyz;
	norm = decode_normal(norm.xy);

	frag_color = vec4(calcAmbientOcclusion(pos,norm),0,0,0);
}
