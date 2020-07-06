/** 
 * @file blurLightF.glsl
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

uniform sampler2D depthMap;
uniform sampler2D normalMap;
uniform sampler2D lightMap;

uniform float dist_factor;
uniform float blur_size;
uniform vec2 delta;
uniform vec2 kern_scale;

VARYING vec2 vary_fragcoord;

uniform mat4 inv_proj;

vec3 decode_normal(vec2 enc);
vec4 getPosition(vec2 pos_screen);

vec2 getKern(int i)
{
	vec2 kern[4];
	
	kern[0] = vec2(0.3989422804,0.1994711402);
	kern[1] = vec2(0.2419707245,0.1760326634);
	kern[2] = vec2(0.0539909665,0.1209853623);
	kern[3] = vec2(0.0044318484,0.0647587978);
	return kern[i];
}

void main() 
{
	vec2 tc = vary_fragcoord.xy;
	vec3 norm = texture2D(normalMap, tc).xyz;
	norm = decode_normal(norm.xy); // unpack norm

	vec3 pos = getPosition(tc).xyz;
	vec4 ccol = texture2D(lightMap, tc).rgba;

	vec2 dlt = delta / (vec2(1.0)+norm.xy*norm.xy);
	dlt /= max(-pos.z*dist_factor, 1.0);
	
	vec2 defined_weight = getKern(0).xy; // special case the first (centre) sample's weight in the blur; we have to sample it anyway so we get it for 'free'
	vec4 col = defined_weight.xyxx * ccol;

	// relax tolerance according to distance to avoid speckling artifacts, as angles and distances are a lot more abrupt within a small screen area at larger distances
	float pointplanedist_tolerance_pow2 = pos.z*-0.001;

	// perturb sampling origin slightly in screen-space to hide edge-ghosting artifacts where smoothing radius is quite large
	vec2 tc_v = fract(0.5 * tc.xy / kern_scale); // we now have floor(mod(tc,2.0))*0.5
	float tc_mod = 2.0 * abs(tc_v.x - tc_v.y); // diff of x,y makes checkerboard
	tc += ( (tc_mod - 0.5) * dlt * 0.5 ) * kern_scale;

	for (int i = 1; i < 4; i++)
	{
		vec2 samptc = (tc + i * dlt);
		vec3 samppos = getPosition(samptc).xyz; 
		float d = dot(norm.xyz, samppos.xyz-pos.xyz);// dist from plane

		if (d*d <= pointplanedist_tolerance_pow2)
		{
			vec4 weight = getKern(i).xyxx;
			col += texture2D(lightMap, samptc)*weight;
			defined_weight += weight.xy;
		}
	}
	for (int i = 1; i < 4; i++)
	{
		vec2 samptc = (tc - i * dlt);
	    vec3 samppos = getPosition(samptc).xyz; 
		float d = dot(norm.xyz, samppos.xyz-pos.xyz);// dist from plane

		if (d*d <= pointplanedist_tolerance_pow2)
		{
			vec4 weight = getKern(i).xyxx;
			col += texture2D(lightMap, samptc)*weight;
			defined_weight += weight.xy;
		}
	}

	col /= defined_weight.xyxx;
	//col.y *= col.y; // delinearize SSAO effect post-blur    // Singu note: Performed pre-blur as to remove blur requirement
	
	frag_color = col;
}

