uniform vec3 mtl_color;
uniform sampler2D tex;

varying vec3 normal;
varying vec2 texcoord;

void main()
{

	gl_FragColor.xyz = mtl_color;
	gl_FragColor.w = 1.0;
}
