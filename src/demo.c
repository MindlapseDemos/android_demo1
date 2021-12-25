#include "demo.h"
#include "opengl.h"

int demo_init(void)
{
	glClearColor(1, 0, 0, 1);
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
