varying vec3 normal;

void main()
{
	vec3 n = normalize(normal);
	gl_FragColor.xyz = n * 0.5 + 0.5;
	gl_FragColor.w = 1.0;
}
