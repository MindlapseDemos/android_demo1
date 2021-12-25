#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "miniglut.h"
#include "demo.h"

static void display(void);
static void keypress(unsigned char key, int x, int y);
static void mouse(int bn, int st, int x, int y);

static long start_time;


int main(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutInitWindowSize(1280, 800);
	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
	glutCreateWindow("Mindlapse");

	glutDisplayFunc(display);
	glutIdleFunc(glutPostRedisplay);
	glutReshapeFunc(demo_reshape);
	glutKeyboardFunc(keypress);
	glutMouseFunc(mouse);
	glutMotionFunc(demo_motion);

	if(demo_init() == -1) {
		return 1;
	}
	atexit(demo_cleanup);

	start_time = glutGet(GLUT_ELAPSED_TIME);
	glutMainLoop();
	return 0;
}

static void display(void)
{
	demo_time_msec = glutGet(GLUT_ELAPSED_TIME) - start_time;

	demo_display();

	glutSwapBuffers();
	assert(glGetError() == GL_NO_ERROR);
}

static void keypress(unsigned char key, int x, int y)
{
	if(key == 27) exit(0);

	demo_keyboard(key, 1);
}

static void mouse(int bn, int st, int x, int y)
{
	int bidx = bn - GLUT_LEFT_BUTTON;
	int press = st == GLUT_DOWN;

	demo_mouse(bidx, press, x, y);
}
