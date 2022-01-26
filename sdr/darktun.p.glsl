uniform vec3 mtl_color;
uniform sampler2D tex;

varying vec3 pos;
varying vec3 normal;
varying vec2 texcoord;
varying vec4 vlight[8];

void main()
{
	vec4 texel = texture2D(tex, texcoord);

	vec3 norm = normalize(normal);

	vec3 color = vec3(0.05, 0.05, 0.05) * texel.xyz;
	for(int i=0; i<8; i++) {
		vec3 ldir = vlight[i].xyz - pos;
		float dsq = dot(ldir, ldir);
		float energy = min(vlight[i].w / (dsq * 0.1), 1.0);
		float ndotl = max(dot(norm, normalize(ldir)), 0.0);
		color += mtl_color * texel.xyz * ndotl * energy;
	}

	gl_FragColor.xyz = color;
	gl_FragColor.w = 1.0;
}
