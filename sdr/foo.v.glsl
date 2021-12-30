attribute vec4 attr_vertex, attr_color;
attribute vec2 attr_texcoord;

varying vec4 color;
varying vec2 texcoord;

void main()
{
	gl_Position = attr_vertex;
	texcoord = attr_texcoord;
	color = attr_color;
}
