#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "demo.h"
#include "android_native_app_glue.h"

static struct android_app *app;
static EGLDisplay dpy;
static EGLSurface surf;
static EGLContext ctx;

void android_main(struct android_app *app_ptr)
{
	app_dummy();
	app = app_ptr;

	app->onAppCmd = handle_command;
	app->onInputEvent = handle_input;

	for(;;) {
		int num_events;
		struct android_poll_source *pollsrc;

		while(ALooper_pollAll(0, 0, &num_events, (void**)&pollsrc) >= 0) {
			if(pollsrc) {
				pollsrc->process(ap, pollsrc);
			}
		}

		if(app->destroyRequested) {
			return;
		}
		if(!paused) {
			demo_display();
			eglSwapBuffers(dpy, surf);
		}
	}
}
