#include "demo.h"
#include "noise.h"

static int init(void);
static void destroy(void);
static void draw(void);

static unsigned int sdr_foo;

static struct demoscreen scr = { "testa", init, destroy, 0, 0, 0, 0, draw };

void regscr_testa(void)
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

static void draw(void)
{
	int i;
	float t = dsys_time / 1000.0f;

	glUseProgram(sdr_foo);
	gl_begin(GL_QUADS);
	for(i=0; i<16; i++) {
		float x0 = i / 8.0f - 1.0f + 0.01;
		float x1 = (i + 1) / 8.0f - 1.0f - 0.01;
		float y = noise2((float)i * 1.24f, t);
		gl_vertex2f(x0, -1);
		gl_vertex2f(x1, -1);
		gl_vertex2f(x1, y);
		gl_vertex2f(x0, y);
	}
	gl_end();
}
