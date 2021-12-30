#include <stdio.h>
#include "demo.h"
#include "opengl.h"
#include "sanegl.h"
#include "assman.h"
#include "demosys.h"

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
	glBindTexture(GL_TEXTURE_2D, tex_logo);
	glUseProgram(sdr_foo);
	gl_begin(GL_QUADS);
	gl_texcoord2f(0, 1);
	gl_vertex2f(-1, -1);
	gl_texcoord2f(1, 1);
	gl_vertex2f(1, -1);
	gl_texcoord2f(1, 0);
	gl_vertex2f(1, 1);
	gl_texcoord2f(0, 0);
	gl_vertex2f(-1, 1);
	gl_end();
	swap_buffers();

	if(dsys_init("data/demoscript") == -1) {
		return -1;
	}

	return 0;
}

void demo_cleanup(void)
{
	dsys_destroy();
}

void demo_display(void)
{
	dsys_update();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindTexture(GL_TEXTURE_2D, tex_logo);
	glUseProgram(sdr_foo);
	gl_begin(GL_QUADS);
	gl_color4f(1, 1, 1, dsys_value("flashlogo"));
	gl_texcoord2f(0, 1);
	gl_vertex2f(-1, -1);
	gl_texcoord2f(1, 1);
	gl_vertex2f(1, -1);
	gl_texcoord2f(1, 0);
	gl_vertex2f(1, 1);
	gl_texcoord2f(0, 0);
	gl_vertex2f(-1, 1);
	gl_end();
	glDisable(GL_BLEND);

	dsys_draw();
}

void demo_reshape(int x, int y)
{
	int i;

	glViewport(0, 0, x, y);

	for(i=0; i<dsys.num_screens; i++) {
		if(dsys.screens[i]->reshape) {
			dsys.screens[i]->reshape(x, y);
		}
	}
}

void demo_keyboard(int key, int pressed)
{
	if(!pressed) return;

	switch(key) {
	case ' ':
		if(dsys.running) {
			dsys_stop();
		} else {
			dsys_run();
		}
		break;

	case '\b':
		dsys_seek_abs(0);
		break;

	default:
		if(key >= '0' && key <= '9') {
			dsys_seek_rel((float)(key - '0') / 9.0f);

		} else if(key >= KEY_F1 && key <= KEY_F12) {
			int idx = key - KEY_F1;
			if(idx < dsys.num_screens) {
				dsys_run_screen(dsys.screens[idx]);
			}

		} else {
			int i;
			for(i=0; i<dsys.num_act; i++) {
				struct demoscreen *scr = dsys.act[i];
				if(scr->keyboard) scr->keyboard(key, pressed);
			}
		}
	}
}

void demo_mouse(int bn, int pressed, int x, int y)
{
	int i;
	for(i=0; i<dsys.num_act; i++) {
		struct demoscreen *scr = dsys.act[i];
		if(scr->mouse) scr->mouse(bn, pressed, x, y);
	}
}

void demo_motion(int x, int y)
{
	int i;
	for(i=0; i<dsys.num_act; i++) {
		struct demoscreen *scr = dsys.act[i];
		if(scr->motion) scr->motion(x, y);
	}
}
