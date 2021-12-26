uniform sampler2D tex;

varying vec2 texcoord;

void main()
{
	vec4 texel = texture2D(tex, texcoord);
	gl_FragColor = texel;
}
