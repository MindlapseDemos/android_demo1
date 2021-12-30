#include <stdio.h>
#include <string.h>
#include "demo.h"
#include "demosys.h"
#include "treestore.h"
#include "assfile.h"
#include "rbtree.h"
#include "darray.h"

void regscr_testa(void);
void regscr_testb(void);

static void proc_screen_script(struct demoscreen *scr, struct ts_node *node);
static void proc_track(struct ts_node *node, const char *pname);
static long io_read(void *buf, size_t bytes, void *uptr);


int dsys_init(const char *fname)
{
	int i;
	struct ts_io io = {0};
	struct ts_node *ts, *tsnode;
	struct demoscreen *scr;

	memset(&dsys, 0, sizeof dsys);
	if(!(dsys.trackmap = rb_create(RB_KEY_STRING))) {
		return -1;
	}
	dsys.track = darr_alloc(0, sizeof *dsys.track);
	dsys.value = darr_alloc(0, sizeof *dsys.value);

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
			proc_track(sub, scr->name);
		}
		sub = sub->next;
	}
}

static void proc_track(struct ts_node *node, const char *pname)
{
	char *name, *buf;
	struct ts_attr *attr;
	long tm;
	int tidx;
	struct anm_track *trk;

	if(!(name = (char*)ts_get_attr_str(node, "name", 0))) {
		return;
	}
	if(pname) {
		buf = alloca(strlen(name) + strlen(pname) + 2);
		sprintf(buf, "%s.%s", pname, name);
		name = buf;
	}

	if((tidx = dsys_add_track(name)) == -1) {
		return;
	}
	trk = dsys.track + tidx;

	attr = node->attr_list;
	while(attr) {
		if(sscanf(attr->name, "key_%ld", &tm) == 1 && attr->val.type == TS_NUMBER) {
			anm_set_value(trk, tm, attr->val.fnum);
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

	darr_free(dsys.track);
	darr_free(dsys.value);
	rb_free(dsys.trackmap);
}

void dsys_update(void)
{
	int i, j, sort_needed = 0;
	struct demoscreen *scr;

	dsys.tmsec = time_msec;

	dsys.num_act = 0;
	for(i=0; i<dsys.num_screens; i++) {
		scr = dsys.screens[i];
		scr->vis = anm_get_value(&scr->track, dsys.tmsec);

		if(scr->vis > 0.0f) {
			if(!scr->active) {
				if(scr->start) scr->start();
				scr->active = 1;
			}
			if(scr->update) scr->update(dsys.tmsec);

			if(dsys.num_act && scr->prio != dsys.act[dsys.num_act - 1]->prio) {
				sort_needed = 1;
			}
			dsys.act[dsys.num_act++] = scr;
		} else {
			if(scr->active) {
				if(scr->stop) scr->stop();
				scr->active = 0;
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

	/* evaluate tracks */
	for(i=0; i<dsys.num_tracks; i++) {
		dsys.value[i] = anm_get_value(dsys.track + i, dsys.tmsec);
	}
}

/* TODO: do something about draw ordering of the active screens */
void dsys_draw(void)
{
	int i;
	for(i=0; i<dsys.num_act; i++) {
		dsys.act[i]->draw();
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

	if(!scr) return;
	if(dsys.num_act == 1 && dsys.act[0] == scr) return;

	for(i=0; i<dsys.num_act; i++) {
		if(dsys.act[i]->stop) dsys.act[i]->stop();
		dsys.act[i]->active = 0;
	}

	dsys.act[0] = scr;
	dsys.num_act = 1;

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
	anm_set_track_extrapolator(&scr->track, ANM_EXTRAP_CLAMP);
	anm_set_track_default(&scr->track, 0);

	dsys.screens[dsys.num_screens++] = scr;
	return 0;
}

int dsys_add_track(const char *name)
{
	struct anm_track trk;
	int idx;

	if(rb_find(dsys.trackmap, (char*)name)) {
		fprintf(stderr, "ignoring duplicate track: %s\n", name);
		return -1;
	}

	idx = darr_size(dsys.track);
	darr_push(dsys.track, &trk);
	darr_pushf(dsys.value, 0);

	anm_init_track(dsys.track + idx);

	if(rb_insert(dsys.trackmap, (char*)name, (void*)(intptr_t)idx) == -1) {
		fprintf(stderr, "failed to insert to track map: %s\n", name);
		abort();
	}
	dsys.num_tracks = idx + 1;
	return idx;
}

int dsys_find_track(const char *name)
{
	struct rbnode *n = rb_find(dsys.trackmap, (char*)name);
	if(!n) return -1;

	return (intptr_t)n->data;
}

float dsys_value(const char *name)
{
	int idx = dsys_find_track(name);
	return idx == -1 ? 0.0f : dsys.value[idx];
}
