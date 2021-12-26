#include <stdio.h>
#include <stdlib.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "demo.h"
#include "android_native_app_glue.h"
#include "logger.h"

static void handle_command(struct android_app *app, int32_t cmd);
static int handle_input(struct android_app *app, AInputEvent *ev);
static int handle_touch_input(struct android_app *app, AInputEvent *ev);
static int init_gl(void);
static void destroy_gl(void);

struct android_app *app;

static EGLDisplay dpy;
static EGLSurface surf;
static EGLContext ctx;
static int init_done, paused;

static int width, height;


void android_main(struct android_app *app_ptr)
{
	app = app_ptr;

	app->onAppCmd = handle_command;
	app->onInputEvent = handle_input;

	start_logger();

	for(;;) {
		int num_events;
		struct android_poll_source *pollsrc;

		while(ALooper_pollAll(0, 0, &num_events, (void**)&pollsrc) >= 0) {
			if(pollsrc) {
				pollsrc->process(app, pollsrc);
			}
		}

		if(app->destroyRequested) {
			return;
		}
		if(init_done && !paused) {
			demo_display();
			eglSwapBuffers(dpy, surf);
		}
	}
}

static void handle_command(struct android_app *app, int32_t cmd)
{
	int xsz, ysz;

	switch(cmd) {
	case APP_CMD_PAUSE:
		paused = 1;	/* TODO: handle timers */
		break;
	case APP_CMD_RESUME:
		paused = 0;
		break;

	case APP_CMD_INIT_WINDOW:
		if(init_gl() == -1) {
			exit(1);
		}
		if(demo_init() == -1) {
			exit(1);
		}
		demo_reshape(width, height);
		init_done = 1;
		break;

	case APP_CMD_TERM_WINDOW:
		init_done = 0;
		demo_cleanup();
		destroy_gl();
		break;

	case APP_CMD_WINDOW_RESIZED:
	case APP_CMD_CONFIG_CHANGED:
		xsz = ANativeWindow_getWidth(app->window);
		ysz = ANativeWindow_getHeight(app->window);
		if(xsz != width || ysz != height) {
			printf("reshape(%d, %d)\n", xsz, ysz);
			demo_reshape(xsz, ysz);
			width = xsz;
			height = ysz;
		}
		break;

	/*
	case APP_CMD_SAVE_STATE:
	case APP_CMD_GAINED_FOCUS:
	case APP_CMD_LOST_FOCUS:
	*/
	default:
		break;
	}
}

static int handle_input(struct android_app *app, AInputEvent *ev)
{
	int evtype = AInputEvent_getType(ev);

	switch(evtype) {
	case AINPUT_EVENT_TYPE_MOTION:
		return handle_touch_input(app, ev);

	default:
		break;
	}
	return 0;
}

static int handle_touch_input(struct android_app *app, AInputEvent *ev)
{
	int i, pcount, x, y, idx;
	unsigned int action;
	static int prev_pos[2];

	action = AMotionEvent_getAction(ev);

	idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
		AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

	x = AMotionEvent_getX(ev, idx);
	y = AMotionEvent_getY(ev, idx);

	switch(action & AMOTION_EVENT_ACTION_MASK) {
	case AMOTION_EVENT_ACTION_DOWN:
	case AMOTION_EVENT_ACTION_POINTER_DOWN:
		demo_mouse(0, 1, x, y);

		prev_pos[0] = x;
		prev_pos[1] = y;
		break;

	case AMOTION_EVENT_ACTION_UP:
	case AMOTION_EVENT_ACTION_POINTER_UP:
		demo_mouse(0, 0, x, y);

		prev_pos[0] = x;
		prev_pos[1] = y;
		break;

	case AMOTION_EVENT_ACTION_MOVE:
		pcount = AMotionEvent_getPointerCount(ev);
		for(i=0; i<pcount; i++) {
			int id = AMotionEvent_getPointerId(ev, i);
			if(id == 0) {
				demo_motion(x, y);
				prev_pos[0] = x;
				prev_pos[1] = y;
				break;
			}
		}
		break;

	default:
		break;
	}

	return 1;
}

static int init_gl(void)
{
	static const int eglattr[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_RED_SIZE, 5,
		EGL_GREEN_SIZE, 5,
		EGL_BLUE_SIZE, 5,
		EGL_DEPTH_SIZE, 16,
		EGL_NONE
	};
	static const int ctxattr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

	EGLConfig eglcfg;
	int count, vis;

	dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if(!dpy || !eglInitialize(dpy, 0, 0)) {
		fprintf(stderr, "failed to initialize EGL\n");
		destroy_gl();
		return -1;
	}

	if(!eglChooseConfig(dpy, eglattr, &eglcfg, 1, &count)) {
		fprintf(stderr, "no matching EGL config found\n");
		destroy_gl();
		return -1;
	}

	/* configure the native window visual according to the chosen EGL config */
	eglGetConfigAttrib(dpy, &eglcfg, EGL_NATIVE_VISUAL_ID, &vis);
	ANativeWindow_setBuffersGeometry(app->window, 0, 0, vis);

	if(!(surf = eglCreateWindowSurface(dpy, eglcfg, app->window, 0))) {
		fprintf(stderr, "failed to create window\n");
		destroy_gl();
		return -1;
	}

	if(!(ctx = eglCreateContext(dpy, eglcfg, EGL_NO_CONTEXT, ctxattr))) {
		fprintf(stderr, "failed to create OpenGL ES context\n");
		destroy_gl();
		return -1;
	}
	eglMakeCurrent(dpy, surf, surf, ctx);

	eglQuerySurface(dpy, surf, EGL_WIDTH, &width);
	eglQuerySurface(dpy, surf, EGL_HEIGHT, &height);
	return 0;
}

static void destroy_gl(void)
{
	if(!dpy) return;

	eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

	if(ctx) {
		eglDestroyContext(dpy, ctx);
		ctx = 0;
	}
	if(surf) {
		eglDestroySurface(dpy, surf);
		surf = 0;
	}

	eglTerminate(dpy);
	dpy = 0;
}
