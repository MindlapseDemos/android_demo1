attribute vec4 attr_vertex;
attribute vec3 attr_normal;

uniform mat4 matrix_modelview, matrix_projection;
uniform mat3 matrix_normal;

varying vec3 normal;

void main()
{
	mat4 mvp = matrix_projection * matrix_modelview;
	gl_Position = mvp * attr_vertex;
	normal = matrix_normal * attr_normal;
}
