uniform vec3 mtl_color;
uniform sampler2D tex;

varying vec3 normal;
varying vec2 texcoord;
varying vec4 vlight[8];

void main()
{
	vec4 texel = texture2D(tex, texcoord);

	vec3 norm = normalize(normal);

	vec3 color = vec3(0.05, 0.05, 0.05) * texel.xyz;
	for(int i=0; i<8; i++) {
		vec3 ldir = normalize(vlight[i].xyz);
		float ndotl = max(dot(norm, ldir), 0.0);
		color += mtl_color * texel.xyz * ndotl * vlight[i].w;
	}

	gl_FragColor.xyz = color;
	gl_FragColor.w = 1.0;
}
