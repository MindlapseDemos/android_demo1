uniform mat4 matrix_modelview_projection;
uniform mat3 matrix_normal;
uniform vec4 light[8];

attribute vec4 attr_vertex;
attribute vec3 attr_normal;
attribute vec2 attr_texcoord;

varying vec3 normal;
varying vec2 texcoord;

varying vec4 vlight[8];

void main()
{
	gl_Position = matrix_modelview_projection * attr_vertex;
	normal = matrix_normal * attr_normal;
	texcoord = attr_texcoord;

	for(int i=0; i<8; i++) {
		vlight[i].xyz = light[i].xyz;
		vlight[i].w = light[i].w;	// TODO: attenuation
	}
}
