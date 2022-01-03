#include "demo.h"
#include "cmesh.h"
#include "meshgen.h"

static int init(void);
static void destroy(void);
static void draw(void);

static struct demoscreen scr = {"darktun", init, destroy, 0, 0, 0, 0, draw};
static struct cmesh *mesh;

void regscr_darktun(void)
{
	dsys_add_screen(&scr);
}

static int init(void)
{
	mesh = cmesh_alloc();
	gen_torus(mesh, 3.0f, 1.0f, 20, 8, 1.0f, 1.0f);
	return 0;
}

static void destroy(void)
{
	cmesh_free(mesh);
}

static void draw(void)
{
	gl_matrix_mode(GL_PROJECTION);
	gl_load_identity();
	glu_perspective(50.0f, win_aspect, 0.5f, 500.0f);

	gl_matrix_mode(GL_MODELVIEW);
	gl_load_identity();
	gl_rotatef(dsys.tmsec / 10.0f, 1, 0, 0);
	gl_rotatef(dsys.tmsec / 20.0f, 0, 1, 0);
	gl_translatef(0, 0, -8);

	glUseProgram(sdr_dbg);
	gl_apply_xform(sdr_dbg);
	cmesh_draw(mesh);
}
