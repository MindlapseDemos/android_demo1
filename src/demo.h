#ifndef DEMO_H_
#define DEMO_H_

#include "opengl.h"
#include "sanegl.h"
#include "demosys.h"
#include "assman.h"
#include "util.h"
#include "cfgopt.h"

enum {
	KEY_F1 = 128,
	KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
	KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
	KEY_PGUP, KEY_PGDOWN
};

long time_msec;

int demo_init(void);
void demo_cleanup(void);

void demo_display(void);
void demo_reshape(int x, int y);
void demo_keyboard(int key, int pressed);
void demo_mouse(int bn, int pressed, int x, int y);
void demo_motion(int x, int y);

void swap_buffers(void);

#endif	/* DEMO_H_ */
