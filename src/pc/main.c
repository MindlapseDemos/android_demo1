#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "opengl.h"
#include "miniglut.h"
#include "demo.h"
#include "cfgopt.h"

static void display(void);
static void keypress(unsigned char key, int x, int y);
static void skeypress(int key, int x, int y);
static void mouse(int bn, int st, int x, int y);
static int translate_key(int key);

static int prev_xsz, prev_ysz;
static long start_time;


int main(int argc, char **argv)
{
	glutInit(&argc, argv);

	load_config("demo.cfg");
	if(parse_args(argc, argv) == -1) {
		return 1;
	}

	glutInitWindowSize(1280, 800);
	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
	glutCreateWindow("Mindlapse");

	glutDisplayFunc(display);
	glutIdleFunc(glutPostRedisplay);
	glutReshapeFunc(demo_reshape);
	glutKeyboardFunc(keypress);
	glutSpecialFunc(skeypress);
	glutMouseFunc(mouse);
	glutMotionFunc(demo_motion);

	if(opt.fullscreen) {
		prev_xsz = glutGet(GLUT_WINDOW_WIDTH);
		prev_ysz = glutGet(GLUT_WINDOW_HEIGHT);
		glutFullScreen();
	}

	if(demo_init() == -1) {
		return 1;
	}
	atexit(demo_cleanup);

	start_time = glutGet(GLUT_ELAPSED_TIME);
	glutMainLoop();
	return 0;
}

void swap_buffers(void)
{
	glutSwapBuffers();
}

static void display(void)
{
	time_msec = glutGet(GLUT_ELAPSED_TIME) - start_time;

	demo_display();

	glutSwapBuffers();
	assert(glGetError() == GL_NO_ERROR);
}

static void keypress(unsigned char key, int x, int y)
{
	switch(key) {
	case 27:
		glutExit();
		break;

	case 'f':
	case 'F':
		opt.fullscreen ^= 1;
		if(opt.fullscreen) {
			prev_xsz = glutGet(GLUT_WINDOW_WIDTH);
			prev_ysz = glutGet(GLUT_WINDOW_HEIGHT);
			glutFullScreen();
		} else {
			glutReshapeWindow(prev_xsz, prev_ysz);
		}
		break;

	default:
		demo_keyboard(key, 1);
	}
}

static void skeypress(int key, int x, int y)
{
	if((key = translate_key(key))) {
		demo_keyboard(key, 1);
	}
}

static void mouse(int bn, int st, int x, int y)
{
	int bidx = bn - GLUT_LEFT_BUTTON;
	int press = st == GLUT_DOWN;

	demo_mouse(bidx, press, x, y);
}

static int translate_key(int key)
{
	if(key >= GLUT_KEY_F1 && key <= GLUT_KEY_F12) {
		return key - GLUT_KEY_F1 + KEY_F1;
	}
	switch(key) {
	case GLUT_KEY_LEFT:
		return KEY_LEFT;
	case GLUT_KEY_RIGHT:
		return KEY_RIGHT;
	case GLUT_KEY_UP:
		return KEY_UP;
	case GLUT_KEY_DOWN:
		return KEY_DOWN;
	case GLUT_KEY_PAGE_UP:
		return KEY_PGUP;
	case GLUT_KEY_PAGE_DOWN:
		return KEY_PGDOWN;
	default:
		break;
	}
	return 0;
}
