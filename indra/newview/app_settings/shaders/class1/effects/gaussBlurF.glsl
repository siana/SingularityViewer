

#ifdef DEFINE_GL_FRAGCOLOR
out vec4 frag_color;
#else
#define frag_color gl_FragColor
#endif

uniform sampler2D tex0;
uniform int horizontalPass;

VARYING vec2 vary_texcoord0;

uniform vec4 kern[2];
vec3 weight = vec3( 0.2270270270, 0.3162162162, 0.0702702703 );

void main(void)
{
	vec3 color = vec3(texture2D(tex0, vary_texcoord0))*weight.x;
	
	color += weight.y * vec3(texture2D(tex0, vec2(vary_texcoord0.s+kern[horizontalPass].s,vary_texcoord0.t+kern[horizontalPass].s)));
	color += weight.y * vec3(texture2D(tex0, vec2(vary_texcoord0.s-kern[horizontalPass].s,vary_texcoord0.t-kern[horizontalPass].s)));
	color += weight.z * vec3(texture2D(tex0, vec2(vary_texcoord0.s+kern[horizontalPass].t,vary_texcoord0.t+kern[horizontalPass].t)));
	color += weight.z * vec3(texture2D(tex0, vec2(vary_texcoord0.s-kern[horizontalPass].t,vary_texcoord0.t-kern[horizontalPass].t)));

	frag_color = vec4(color.xyz,1.0);
}
