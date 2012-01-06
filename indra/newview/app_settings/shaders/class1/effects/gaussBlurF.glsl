#extension GL_ARB_texture_rectangle : enable

#ifdef DEFINE_GL_FRAGCOLOR
out vec4 gl_FragColor;
#endif

uniform sampler2DRect tex0;
uniform int horizontalPass;

VARYING vec2 vary_texcoord0;

vec2 offset = vec2( 1.3846153846, 3.2307692308 );
vec3 weight = vec3( 0.2270270270, 0.3162162162, 0.0702702703 );
 
void main(void)
{
	vec3 color = vec3(texture2DRect(tex0, vary_texcoord0))*weight.x;
	
	if(horizontalPass == 1)
	{
		color += weight.y * vec3(texture2DRect(tex0, vec2(vary_texcoord0.s+offset.s,vary_texcoord0.t)));
		color += weight.y * vec3(texture2DRect(tex0, vec2(vary_texcoord0.s-offset.s,vary_texcoord0.t)));
		color += weight.z * vec3(texture2DRect(tex0, vec2(vary_texcoord0.s+offset.t,vary_texcoord0.t)));
		color += weight.z * vec3(texture2DRect(tex0, vec2(vary_texcoord0.s-offset.t,vary_texcoord0.t)));
		
	}
	else
	{
		color += weight.y * vec3(texture2DRect(tex0, vec2(vary_texcoord0.s,vary_texcoord0.t+offset.s)));
		color += weight.y * vec3(texture2DRect(tex0, vec2(vary_texcoord0.s,vary_texcoord0.t-offset.s)));
		color += weight.z * vec3(texture2DRect(tex0, vec2(vary_texcoord0.s,vary_texcoord0.t+offset.t)));
		color += weight.z * vec3(texture2DRect(tex0, vec2(vary_texcoord0.s,vary_texcoord0.t-offset.t)));
	}
	gl_FragColor = vec4(color.xyz,1.0);
}