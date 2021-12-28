#include "demo.h"
#include "noise.h"
#include "cgmath/cgmath.h"

static int init(void);
static void destroy(void);
static void draw(void);

static unsigned int sdr_foo;

static struct demoscreen scr = { "testb", init, destroy, 0, 0, 0, 0, draw };

void regscr_testb(void)
{
	dsys_add_screen(&scr);
}

static int init(void)
{
	if(!(sdr_foo = get_sdrprog("sdr/foo.v.glsl", "sdr/foo-notex.p.glsl"))) {
		return -1;
	}
	return 0;
}

static void destroy(void)
{
}

#define NX	16
#define NY	10

static void draw(void)
{
	int i, j;
	float x, y, xr, yr, sz;
	float t = dsys_time / 700.0f;

	glUseProgram(sdr_foo);
	gl_begin(GL_QUADS);
	for(i=0; i<NY; i++) {
		y = (i + 0.5f) / (NY/2.0f) - 1.0f;
		for(j=0; j<NX; j++) {
			x = (j + 0.5f) / (NX/2.0f) - 1.0f;
			sz = cgm_lerp(1.0f, noise2(x * 5.0f, t) * noise2(y * 5.0f, t) * 2.5f, scr.vis);
			if(sz < 0.0f) sz = 0.0f;
			if(sz > 1.0f) sz = 1.0f;
			xr = sz / NX;
			yr = sz / NY;

			gl_vertex2f(x - xr, y - yr);
			gl_vertex2f(x + xr, y - yr);
			gl_vertex2f(x + xr, y + yr);
			gl_vertex2f(x - xr, y + yr);
		}
	}
	gl_end();
}
