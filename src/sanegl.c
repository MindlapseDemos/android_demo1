#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "cgmath/cgmath.h"

#include "opengl.h"

#ifdef GLDEF
#undef GLDEF
#endif
#include "sanegl.h"

#define MMODE_IDX(x)	((x) - GL_MODELVIEW)
#define MAT_STACK_SIZE	32
#define MAT_IDENT	{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}

#define MAX_VERTS	512

static void gl_draw_immediate(void);

typedef struct { float x, y; } vec2_t;
typedef struct { float x, y, z; } vec3_t;
typedef struct { float x, y, z, w; } vec4_t;

static int mm_idx = 0;
static float mat_stack[3][MAT_STACK_SIZE][16] = {{MAT_IDENT}, {MAT_IDENT}, {MAT_IDENT}};
static int stack_top[3];
static float mat_mvp[16];
static int mvp_valid;
static int prim = -1;

static vec3_t cur_normal = {0, 0, 1};
static vec4_t cur_color = {1, 1, 1, 1}, cur_attrib;
static vec2_t cur_texcoord;

static vec4_t *vert_arr, *col_arr, *attr_arr;
static vec3_t *norm_arr;
static vec2_t *texc_arr;
/*static unsigned int vbuf, cbuf, nbuf, tbuf, abuf;*/
static int vloc, nloc, cloc, tloc, aloc = -1;

static int num_verts, vert_calls;
static int cur_prog;


void gl_matrix_mode(int mm)
{
	mm_idx = MMODE_IDX(mm);
}

void gl_push_matrix(void)
{
	int top = stack_top[mm_idx];

	memcpy(mat_stack[mm_idx][top + 1], mat_stack[mm_idx][top], 16 * sizeof(float));
	stack_top[mm_idx]++;
	mvp_valid = 0;
}

void gl_pop_matrix(void)
{
	stack_top[mm_idx]--;
	mvp_valid = 0;
}

void gl_load_identity(void)
{
	static const float idmat[] = MAT_IDENT;
	int top = stack_top[mm_idx];
	float *mat = mat_stack[mm_idx][top];

	memcpy(mat, idmat, sizeof idmat);
	mvp_valid = 0;
}

void gl_load_matrixf(const float *m)
{
	int top = stack_top[mm_idx];
	float *mat = mat_stack[mm_idx][top];

	memcpy(mat, m, 16 * sizeof *mat);
	mvp_valid = 0;
}

#define M(i, j)	((i << 2) + j)

void gl_mult_matrixf(const float *m2)
{
	int top = stack_top[mm_idx];
	float *m1 = mat_stack[mm_idx][top];

	cgm_mpremul(m1, m2);
	mvp_valid = 0;
}

void gl_translatef(float x, float y, float z)
{
	float mat[16];
	cgm_mtranslation(mat, x, y, z);
	gl_mult_matrixf(mat);
}

void gl_rotatef(float angle, float x, float y, float z)
{
	float mat[16];
	cgm_mrotation(mat, cgm_deg_to_rad(angle), x, y, z);
	gl_mult_matrixf(mat);
}

void gl_scalef(float x, float y, float z)
{
	float mat[16];
	cgm_mscaling(mat, x, y, z);
	gl_mult_matrixf(mat);
}

void gl_ortho(float left, float right, float bottom, float top, float znear, float zfar)
{
	float mat[16];
	cgm_mortho(mat, left, right, bottom, top, znear, zfar);
	gl_mult_matrixf(mat);
}

void gl_frustum(float left, float right, float bottom, float top, float znear, float zfar)
{
	float mat[16];
	cgm_mfrustum(mat, left, right, bottom, top, znear, zfar);
	gl_mult_matrixf(mat);
}

void glu_perspective(float vfov, float aspect, float znear, float zfar)
{
	float mat[16];
	cgm_mperspective(mat, cgm_deg_to_rad(vfov), aspect, znear, zfar);
	gl_mult_matrixf(mat);
}

void gl_apply_xform(unsigned int prog)
{
	int loc, mvidx, pidx, tidx, mvtop, ptop, ttop;

	mvidx = MMODE_IDX(GL_MODELVIEW);
	pidx = MMODE_IDX(GL_PROJECTION);
	tidx = MMODE_IDX(GL_TEXTURE);

	mvtop = stack_top[mvidx];
	ptop = stack_top[pidx];
	ttop = stack_top[tidx];

	assert(prog);

	if((loc = glGetUniformLocation(prog, "matrix_modelview")) != -1) {
		glUniformMatrix4fv(loc, 1, 0, mat_stack[mvidx][mvtop]);
	}

	if((loc = glGetUniformLocation(prog, "matrix_projection")) != -1) {
		glUniformMatrix4fv(loc, 1, 0, mat_stack[pidx][ptop]);
	}

	if((loc = glGetUniformLocation(prog, "matrix_texture")) != -1) {
		glUniformMatrix4fv(loc, 1, 0, mat_stack[tidx][ttop]);
	}

	if((loc = glGetUniformLocation(prog, "matrix_normal")) != -1) {
		float nmat[9];

		nmat[0] = mat_stack[mvidx][mvtop][0];
		nmat[1] = mat_stack[mvidx][mvtop][1];
		nmat[2] = mat_stack[mvidx][mvtop][2];
		nmat[3] = mat_stack[mvidx][mvtop][4];
		nmat[4] = mat_stack[mvidx][mvtop][5];
		nmat[5] = mat_stack[mvidx][mvtop][6];
		nmat[6] = mat_stack[mvidx][mvtop][8];
		nmat[7] = mat_stack[mvidx][mvtop][9];
		nmat[8] = mat_stack[mvidx][mvtop][10];
		glUniformMatrix3fv(loc, 1, 0, nmat);
	}

	if((loc = glGetUniformLocation(prog, "matrix_modelview_projection")) != -1) {
		if(!mvp_valid) {
			cgm_mcopy(mat_mvp, mat_stack[mvidx][mvtop]);
			cgm_mmul(mat_mvp, mat_stack[pidx][ptop]);
		}
		glUniformMatrix4fv(loc, 1, 0, mat_mvp);
	}
}


/* immediate mode rendering */
void gl_begin(int p)
{
	if(!vert_arr) {
		vert_arr = malloc(MAX_VERTS * sizeof *vert_arr);
		norm_arr = malloc(MAX_VERTS * sizeof *norm_arr);
		texc_arr = malloc(MAX_VERTS * sizeof *texc_arr);
		col_arr = malloc(MAX_VERTS * sizeof *col_arr);
		attr_arr = malloc(MAX_VERTS * sizeof *attr_arr);
		assert(vert_arr && norm_arr && texc_arr && col_arr && attr_arr);
	}

	prim = p;
	num_verts = vert_calls = 0;

	glGetIntegerv(GL_CURRENT_PROGRAM, &cur_prog);
	assert(cur_prog);

	gl_apply_xform(cur_prog);

	vloc = glGetAttribLocation(cur_prog, "attr_vertex");
	nloc = glGetAttribLocation(cur_prog, "attr_normal");
	cloc = glGetAttribLocation(cur_prog, "attr_color");
	tloc = glGetAttribLocation(cur_prog, "attr_texcoord");
}

void gl_end(void)
{
	if(num_verts > 0) {
		gl_draw_immediate();
	}
	aloc = -1;
}

static void gl_draw_immediate(void)
{
	int glprim;

	if(vloc == -1) {
		fprintf(stderr, "gl_draw_immediate call with vloc == -1\n");
		return;
	}

	glprim = prim == GL_QUADS ? GL_TRIANGLES : prim;

	glVertexAttribPointer(vloc, 4, GL_FLOAT, 0, 0, vert_arr);
	glEnableVertexAttribArray(vloc);

	if(nloc != -1) {
		glVertexAttribPointer(nloc, 3, GL_FLOAT, 0, 0, norm_arr);
		glEnableVertexAttribArray(nloc);
	}

	if(cloc != -1) {
		glVertexAttribPointer(cloc, 4, GL_FLOAT, 1, 0, col_arr);
		glEnableVertexAttribArray(cloc);
	}

	if(tloc != -1) {
		glVertexAttribPointer(tloc, 2, GL_FLOAT, 0, 0, texc_arr);
		glEnableVertexAttribArray(tloc);
	}

	if(aloc != -1) {
		glVertexAttribPointer(aloc, 4, GL_FLOAT, 0, 0, attr_arr);
		glEnableVertexAttribArray(aloc);
	}

	glDrawArrays(glprim, 0, num_verts);

	glDisableVertexAttribArray(vloc);
	if(nloc != -1) {
		glDisableVertexAttribArray(nloc);
	}
	if(cloc != -1) {
		glDisableVertexAttribArray(cloc);
	}
	if(tloc != -1) {
		glDisableVertexAttribArray(tloc);
	}
	if(aloc != -1) {
		glDisableVertexAttribArray(aloc);
	}
}


void gl_vertex2f(float x, float y)
{
	gl_vertex4f(x, y, 0.0f, 1.0f);
}

void gl_vertex3f(float x, float y, float z)
{
	gl_vertex4f(x, y, z, 1.0f);
}

void gl_vertex4f(float x, float y, float z, float w)
{
	int i, buffer_full;

	if(prim == GL_QUADS && vert_calls % 4 == 3) {
		for(i=0; i<2; i++) {
			if(aloc != -1) {
				attr_arr[num_verts] = attr_arr[num_verts - 3 + i];
			}
			if(cloc != -1) {
				col_arr[num_verts] = col_arr[num_verts - 3 + i];
			}
			if(tloc != -1) {
				texc_arr[num_verts] = texc_arr[num_verts - 3 + i];
			}
			if(nloc != -1) {
				norm_arr[num_verts] = norm_arr[num_verts - 3 + i];
			}
			vert_arr[num_verts] = vert_arr[num_verts - 3 + i];
			num_verts++;
		}
	}

	vert_arr[num_verts].x = x;
	vert_arr[num_verts].y = y;
	vert_arr[num_verts].z = z;
	vert_arr[num_verts].w = w;

	if(cloc != -1) {
		col_arr[num_verts] = cur_color;
	}
	if(nloc != -1) {
		norm_arr[num_verts] = cur_normal;
	}
	if(tloc != -1) {
		texc_arr[num_verts] = cur_texcoord;
	}
	if(aloc != -1) {
		attr_arr[num_verts] = cur_attrib;
	}

	vert_calls++;
	num_verts++;

	if(prim == GL_QUADS) {
		/* leave space for 6 more worst-case and don't allow flushes mid-quad */
		buffer_full = num_verts >= MAX_VERTS - 6 && vert_calls % 4 == 0;
	} else {
		buffer_full = num_verts >= MAX_VERTS - prim;
	}

	if(buffer_full) {
		gl_draw_immediate();
		gl_begin(prim);	/* reset everything */
	}
}


void gl_normal3f(float x, float y, float z)
{
	cur_normal.x = x;
	cur_normal.y = y;
	cur_normal.z = z;
}


void gl_color3f(float r, float g, float b)
{
	cur_color.x = r;
	cur_color.y = g;
	cur_color.z = b;
	cur_color.w = 1.0f;
}

void gl_color4f(float r, float g, float b, float a)
{
	cur_color.x = r;
	cur_color.y = g;
	cur_color.z = b;
	cur_color.w = a;
}


void gl_texcoord1f(float s)
{
	cur_texcoord.x = s;
	cur_texcoord.y = 0.0f;
}

void gl_texcoord2f(float s, float t)
{
	cur_texcoord.x = s;
	cur_texcoord.y = t;
}

void gl_vertex_attrib2f(int loc, float x, float y)
{
	aloc = loc;
	cur_attrib.x = x;
	cur_attrib.y = y;
	cur_attrib.z = 0.0f;
	cur_attrib.w = 1.0f;
}

void gl_vertex_attrib3f(int loc, float x, float y, float z)
{
	aloc = loc;
	cur_attrib.x = x;
	cur_attrib.y = y;
	cur_attrib.z = z;
	cur_attrib.w = 1.0f;
}

void gl_vertex_attrib4f(int loc, float x, float y, float z, float w)
{
	aloc = loc;
	cur_attrib.x = x;
	cur_attrib.y = y;
	cur_attrib.z = z;
	cur_attrib.w = w;
}
