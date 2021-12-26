attribute vec4 apos;
attribute vec2 atex;

varying vec2 texcoord;

void main()
{
	gl_Position = apos;
	texcoord = atex;
}
