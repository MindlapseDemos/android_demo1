#include <stdio.h>
#include <string.h>
#include "demo.h"
#include "demosys.h"
#include "treestore.h"
#include "assfile.h"
#include "rbtree.h"
#include "darray.h"
#include "music.h"

void regscr_testa(void);
void regscr_testb(void);

static void proc_screen_script(struct demoscreen *scr, struct ts_node *node);
static void proc_track(struct ts_node *node, struct demoscreen *pscr);
static long io_read(void *buf, size_t bytes, void *uptr);

static int upd_pending;


int dsys_init(const char *fname)
{
	int i;
	struct ts_io io = {0};
	struct ts_node *ts, *tsnode;
	struct demoscreen *scr;

	start_time = stop_time = time_msec = 0;
	upd_pending = 0;

	memset(&dsys, 0, sizeof dsys);
	if(!(dsys.trackmap = rb_create(RB_KEY_STRING))) {
		return -1;
	}

	dsys.ev = darr_alloc(0, sizeof *dsys.ev);

	regscr_testa();
	regscr_testb();

	for(i=0; i<dsys.num_screens; i++) {
		if(dsys.screens[i]->init() == -1) {
			fprintf(stderr, "failed to initialize demo screen: %s\n", dsys.screens[i]->name);
			return -1;
		}
	}

	if(!fname || !(io.data = ass_fopen(fname, "rb"))) {
		dsys_run_screen(dsys.screens[0]);
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

		} else if(strcmp(tsnode->name, "track") == 0) {
			proc_track(tsnode, 0);
		}
		tsnode = tsnode->next;
	}

	dsys.tend = ts_lookup_num(ts, "demo.end", 0);
	ts_free_tree(ts);
	ass_fclose(io.data);
	return 0;
}

static void proc_screen_script(struct demoscreen *scr, struct ts_node *node)
{
	struct ts_node *sub;
	struct ts_attr *attr;
	long tm;

	attr = node->attr_list;
	while(attr) {
		if(sscanf(attr->name, "key_%ld", &tm) == 1 && attr->val.type == TS_NUMBER) {
			anm_set_value(&scr->track, tm, attr->val.fnum);
		}
		attr = attr->next;
	}

	sub = node->child_list;
	while(sub) {
		if(strcmp(sub->name, "track") == 0) {
			proc_track(sub, scr);
		}
		sub = sub->next;
	}
}

static void proc_track(struct ts_node *node, struct demoscreen *pscr)
{
	char *name, *buf;
	struct ts_attr *attr;
	long tm;
	int idx;
	struct demoevent *ev;

	if(!(name = (char*)ts_get_attr_str(node, "name", 0))) {
		return;
	}
	if(pscr) {
		buf = alloca(strlen(name) + strlen(pscr->name) + 2);
		sprintf(buf, "%s.%s", pscr->name, name);
		name = buf;
	}

	if((idx = dsys_add_event(name)) == -1) {
		return;
	}
	ev = dsys.ev + idx;
	ev->scr = pscr;

	attr = node->attr_list;
	while(attr) {
		if(sscanf(attr->name, "key_%ld", &tm) == 1 && attr->val.type == TS_NUMBER) {
			anm_set_value(&ev->track, tm, attr->val.fnum);
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

	for(i=0; i<dsys.num_screens; i++) {
		anm_destroy_track(&dsys.screens[i]->track);
		if(dsys.screens[i]->destroy) {
			dsys.screens[i]->destroy();
		}
	}
	dsys.num_screens = 0;

	darr_free(dsys.ev);
	rb_free(dsys.trackmap);
}

void dsys_update(void)
{
	int i, j, sort_needed = 0;
	long tm;
	struct demoscreen *scr;

	time_msec = sys_time - start_time;

	if(!dsys.running && !upd_pending) return;
	upd_pending = 0;

	dsys.tmsec = time_msec;
	if(dsys.tend > 0) {
		if(dsys.tmsec >= dsys.tend) {
			dsys.tmsec = dsys.tend;
			dsys.t = 1.0f;
			dsys_stop();
		} else {
			dsys.t = (float)dsys.tmsec / (float)dsys.tend;
		}
	}

	/* evaluate tracks */
	for(i=0; i<dsys.num_ev; i++) {
		tm = dsys.tmsec;
		if((scr = dsys.ev[i].scr) && scr->start_time >= 0) {
			tm -= dsys.ev[i].scr->start_time;
		}
		dsys.ev[i].value = anm_get_value(&dsys.ev[i].track, tm);
	}

	if(dsys.scr_override) {
		scr = dsys.scr_override;
		scr->vis = 1;
		if(scr->update) scr->update(dsys.tmsec);
		return;
	}

	dsys.num_act = 0;
	for(i=0; i<dsys.num_screens; i++) {
		scr = dsys.screens[i];
		scr->vis = anm_get_value(&scr->track, dsys.tmsec);

		if(scr->vis > 0.0f) {
			if(scr->start_time < 0) {
				if(scr->start) scr->start();
				scr->start_time = dsys.tmsec;
			}
			if(scr->update) scr->update(dsys.tmsec);

			if(dsys.num_act && scr->prio != dsys.act[dsys.num_act - 1]->prio) {
				sort_needed = 1;
			}
			dsys.act[dsys.num_act++] = scr;
		} else {
			if(scr->start_time >= 0) {
				if(scr->stop) scr->stop();
				scr->start_time = -1;
			}
		}
	}

	if(sort_needed) {
		for(i=0; i<dsys.num_act; i++) {
			for(j=i+1; j<dsys.num_act; j++) {
				if(dsys.act[j]->prio > dsys.act[j - 1]->prio) {
					void *tmp = dsys.act[j];
					dsys.act[j] = dsys.act[j - 1];
					dsys.act[j - 1] = tmp;
				}
			}
		}
	}
}

void dsys_draw(void)
{
	int i;

	if(dsys.scr_override) {
		dsys.scr_override->draw();
		return;
	}

	for(i=0; i<dsys.num_act; i++) {
		dsys.act[i]->draw();
	}
}

void dsys_run(void)
{
	if(!dsys.running) {
		play_music();
		dsys.running = 1;
		if(stop_time > 0) {
			start_time += time_msec - stop_time;
			stop_time = 0;
		}
	}
}

void dsys_stop(void)
{
	if(dsys.running) {
		stop_music();
		dsys.running = 0;
		stop_time = time_msec;
	}
}

void dsys_seek_abs(long tm)
{
	start_time = sys_time - tm;
	if(!dsys.running) {
		stop_time = 0;
	}
	upd_pending = 1;
	seek_music(tm);
}

void dsys_seek_rel(long dt)
{
	start_time -= dt;
	if(sys_time < start_time) start_time = sys_time;
	if(dsys.tend > 0 && sys_time - start_time > dsys.tend) {
		start_time = sys_time - dsys.tend;
	}
	upd_pending = 1;
	seek_music(sys_time - start_time);
}

void dsys_seek_norm(float t)
{
	if(dsys.tend <= 0) return;
	dsys_seek_abs(dsys.tend * t);
	upd_pending = 1;
}


struct demoscreen *dsys_find_screen(const char *name)
{
	int i;

	if(!name) return 0;

	for(i=0; i<dsys.num_screens; i++) {
		if(strcmp(dsys.screens[i]->name, name) == 0) {
			return dsys.screens[i];
		}
	}
	return 0;
}

void dsys_run_screen(struct demoscreen *scr)
{
	int i;

	if(!scr) {
		if(dsys.scr_override) {
			scr = dsys.scr_override;
			if(scr->stop) scr->stop();
		}
		dsys.scr_override = 0;
		return;
	}

	for(i=0; i<dsys.num_act; i++) {
		if(dsys.act[i]->stop) dsys.act[i]->stop();
		dsys.act[i]->start_time = -1;
	}
	dsys.num_act = 0;

	dsys.scr_override = scr;
	dsys.tend = 0;
	dsys.t = 0;

	if(scr->start) scr->start();
	scr->start_time = dsys.tmsec;
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
	anm_set_track_extrapolator(&scr->track, ANM_EXTRAP_CLAMP);
	anm_set_track_default(&scr->track, 0);

	dsys.screens[dsys.num_screens++] = scr;
	return 0;
}

int dsys_add_event(const char *name)
{
	struct demoevent ev = {0};
	int idx;

	if(rb_find(dsys.trackmap, (char*)name)) {
		fprintf(stderr, "ignoring duplicate track: %s\n", name);
		return -1;
	}

	ev.name = strdup_nf(name);
	anm_init_track(&ev.track);
	anm_set_track_interpolator(&ev.track, ANM_INTERP_LINEAR);
	anm_set_track_extrapolator(&ev.track, ANM_EXTRAP_CLAMP);
	anm_set_track_default(&ev.track, 0);

	idx = darr_size(dsys.ev);
	darr_push(dsys.ev, &ev);


	if(rb_insert(dsys.trackmap, ev.name, (void*)(intptr_t)idx) == -1) {
		fprintf(stderr, "failed to insert to track map: %s\n", name);
		abort();
	}
	dsys.num_ev = idx + 1;
	return idx;
}

int dsys_find_event(const char *name)
{
	struct rbnode *n = rb_find(dsys.trackmap, (char*)name);
	if(!n) return -1;

	return (intptr_t)n->data;
}

float dsys_value(const char *name)
{
	int idx = dsys_find_event(name);
	return idx == -1 ? 0.0f : dsys.ev[idx].value;
}
