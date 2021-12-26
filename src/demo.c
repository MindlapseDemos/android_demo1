#include "demo.h"
#include "opengl.h"
#include "assman.h"

static unsigned int sdr_foo;
static unsigned int tex_logo;

int demo_init(void)
{
	if(init_opengl() == -1) {
		return -1;
	}

	if(!(sdr_foo = get_sdrprog("sdr/foo.v.glsl", "sdr/foo.p.glsl"))) {
		return -1;
	}
	if(!(tex_logo = get_tex2d("data/ml_logo_old.png"))) {
		return -1;
	}
	return 0;
}

void demo_cleanup(void)
{
}

void demo_display(void)
{
	glClear(GL_COLOR_BUFFER_BIT);

}

void demo_reshape(int x, int y)
{
	glViewport(0, 0, x, y);
}

void demo_keyboard(int key, int pressed)
{
}

void demo_mouse(int bn, int pressed, int x, int y)
{
}

void demo_motion(int x, int y)
{
}
