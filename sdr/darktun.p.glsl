uniform vec3 mtl_color;
uniform sampler2D tex;

varying vec3 normal;
varying vec2 texcoord;

void main()
{
	vec4 texel = texture2D(tex, texcoord);

	vec3 norm = normalize(normal);
	vec3 ldir = normalize(vec3(-0.2, 1, 0.8));

	float ndotl = max(dot(norm, ldir), 0.0);

	vec3 col = mtl_color * texel.xyz * ndotl;

	gl_FragColor.xyz = col;
	gl_FragColor.w = 1.0;
}
