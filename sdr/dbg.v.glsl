attribute vec4 attr_vertex;
attribute vec3 attr_normal;

uniform mat4 matrix_modelview_projection;
uniform mat3 matrix_normal;

varying vec3 normal;

void main()
{
	gl_Position = matrix_modelview_projection * attr_vertex;
	normal = matrix_normal * attr_normal;
}
