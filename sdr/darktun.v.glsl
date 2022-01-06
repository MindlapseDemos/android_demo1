uniform mat4 matrix_modelview_projection;
uniform mat3 matrix_normal;

attribute vec4 attr_vertex;
attribute vec3 attr_normal;
attribute vec2 attr_texcoord;

varying vec3 normal;
varying vec2 texcoord;

void main()
{
	gl_Position = matrix_modelview_projection * attr_vertex;
	normal = matrix_normal * attr_normal;
	texcoord = attr_texcoord;
}
