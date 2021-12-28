#ifndef DEMOSYS_H_
#define DEMOSYS_H_

#include "anim/track.h"

struct demoscreen {
	char *name;

	int (*init)(void);
	void (*destroy)(void);
	void (*reshape)(int x, int y);

	void (*start)(void);
	void (*stop)(void);

	void (*update)(long tmsec);
	void (*draw)(void);

	void (*keyboard)(int key, int pressed);
	void (*mouse)(int bn, int pressed, int x, int y);
	void (*motion)(int x, int y);

	struct anm_track track;
	int active, prio;
	float vis;
};

/* global demo state */
int dsys_running;	/* run/stop state */
int dsys_eof;		/* end of demo flag, seek back to reset */
long dsys_time;		/* demo time in milliseconds */

#define MAX_DSYS_SCREENS	64
struct demoscreen *dsys_screens[MAX_DSYS_SCREENS];
int dsys_num_screens;
struct demoscreen *dsys_act[MAX_DSYS_SCREENS];
int dsys_num_act;


int dsys_init(const char *fname);
void dsys_destroy(void);

void dsys_update(void);
void dsys_draw(void);

void dsys_run(void);
void dsys_stop(void);
void dsys_seek_abs(long tm);
void dsys_seek_rel(long dt);
void dsys_seek_norm(float t);

/* overrides the demo sequence, and runs a single screen */
struct demoscreen *dsys_find_screen(const char *name);
void dsys_run_screen(struct demoscreen *scr);

int dsys_add_screen(struct demoscreen *scr);

#endif	/* DEMOSYS_H_ */
