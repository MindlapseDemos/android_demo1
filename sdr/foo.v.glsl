attribute vec4 attr_vertex;
attribute vec2 attr_texcoord;

varying vec2 texcoord;

void main()
{
	gl_Position = attr_vertex;
	texcoord = attr_texcoord;
}
