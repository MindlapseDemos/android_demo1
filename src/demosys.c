#include <stdio.h>
#include <string.h>
#include "demo.h"
#include "demosys.h"
#include "treestore.h"
#include "assfile.h"

void regscr_testa(void);
void regscr_testb(void);

static void proc_screen_script(struct demoscreen *scr, struct ts_node *node);
static long io_read(void *buf, size_t bytes, void *uptr);


int dsys_init(const char *fname)
{
	int i;
	struct ts_io io = {0};
	struct ts_node *ts, *tsnode;
	struct demoscreen *scr;

	regscr_testa();
	regscr_testb();

	for(i=0; i<dsys_num_screens; i++) {
		if(dsys_screens[i]->init() == -1) {
			fprintf(stderr, "failed to initialize demo screen: %s\n", dsys_screens[i]->name);
			return -1;
		}
	}

	if(!fname || !(io.data = ass_fopen(fname, "rb"))) {
		dsys_run_screen(dsys_screens[0]);
		return 0;
	}
	io.read = io_read;

	if(!(ts = ts_load_io(&io)) || strcmp(ts->name, "demo") != 0) {
		ass_fclose(io.data);
		fprintf(stderr, "failed to read demoscript\n");
		return -1;
	}

	tsnode = ts->child_list;
	while(tsnode) {
		if(strcmp(tsnode->name, "screen") == 0 &&
				(scr = dsys_find_screen(ts_get_attr_str(tsnode, "name", 0)))) {
			proc_screen_script(scr, tsnode);
		}
		tsnode = tsnode->next;
	}

	ass_fclose(io.data);
	return 0;
}

static void proc_screen_script(struct demoscreen *scr, struct ts_node *node)
{
	struct ts_attr *attr;
	long tm;

	attr = node->attr_list;
	while(attr) {
		if(sscanf(attr->name, "key_%ld", &tm) == 1 && attr->val.type == TS_NUMBER) {
			anm_set_value(&scr->track, tm, attr->val.fnum);
		}
		attr = attr->next;
	}
}

static long io_read(void *buf, size_t bytes, void *uptr)
{
	return ass_fread(buf, 1, bytes, uptr);
}


void dsys_destroy(void)
{
	int i;

	for(i=0; i<dsys_num_screens; i++) {
		anm_destroy_track(&dsys_screens[i]->track);
		if(dsys_screens[i]->destroy) {
			dsys_screens[i]->destroy();
		}
	}
	dsys_num_screens = 0;
}

void dsys_update(void)
{
	int i, j, sort_needed = 0;
	struct demoscreen *scr;

	dsys_time = time_msec;

	dsys_num_act = 0;
	for(i=0; i<dsys_num_screens; i++) {
		scr = dsys_screens[i];
		scr->vis = anm_get_value(&scr->track, dsys_time);

		if(scr->vis > 0.0f) {
			if(!scr->active) {
				if(scr->start) scr->start();
				scr->active = 1;
			}
			if(scr->update) scr->update(dsys_time);

			if(dsys_num_act && scr->prio != dsys_act[dsys_num_act - 1]->prio) {
				sort_needed = 1;
			}
			dsys_act[dsys_num_act++] = scr;
		} else {
			if(scr->active) {
				if(scr->stop) scr->stop();
				scr->active = 0;
			}
		}
	}

	if(sort_needed) {
		for(i=0; i<dsys_num_act; i++) {
			for(j=i+1; j<dsys_num_act; j++) {
				if(dsys_act[j]->prio > dsys_act[j - 1]->prio) {
					void *tmp = dsys_act[j];
					dsys_act[j] = dsys_act[j - 1];
					dsys_act[j - 1] = tmp;
				}
			}
		}
	}
}

/* TODO: do something about draw ordering of the active screens */
void dsys_draw(void)
{
	int i;
	for(i=0; i<dsys_num_act; i++) {
		dsys_act[i]->draw();
	}
}

void dsys_run(void)
{
}

void dsys_stop(void)
{
}

void dsys_seek_abs(long tm)
{
}

void dsys_seek_rel(long dt)
{
}

void dsys_seek_norm(float t)
{
}


struct demoscreen *dsys_find_screen(const char *name)
{
	int i;

	if(!name) return 0;

	for(i=0; i<dsys_num_screens; i++) {
		if(strcmp(dsys_screens[i]->name, name) == 0) {
			return dsys_screens[i];
		}
	}
	return 0;
}

void dsys_run_screen(struct demoscreen *scr)
{
	int i;

	if(!scr) return;
	if(dsys_num_act == 1 && dsys_act[0] == scr) return;

	for(i=0; i<dsys_num_act; i++) {
		if(dsys_act[i]->stop) dsys_act[i]->stop();
		dsys_act[i]->active = 0;
	}

	dsys_act[0] = scr;
	dsys_num_act = 1;

	if(scr->start) scr->start();
	scr->active = 1;
}


int dsys_add_screen(struct demoscreen *scr)
{
	if(!scr->name || !scr->init || !scr->draw) {
		fprintf(stderr, "dsys_add_screen: invalid screen\n");
		return -1;
	}
	if(anm_init_track(&scr->track) == -1) {
		fprintf(stderr, "dsys_add_screen: failed to initialize keyframe track\n");
		return -1;
	}
	anm_set_track_interpolator(&scr->track, ANM_INTERP_LINEAR);
	anm_set_track_extrapolator(&scr->track, ANM_EXTRAP_EXTEND);
	anm_set_track_default(&scr->track, 0);

	dsys_screens[dsys_num_screens++] = scr;
	return 0;
}
