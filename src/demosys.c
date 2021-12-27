#include "demosys.h"

static struct demoscreen *act_tail;

void regscr_testa(void);
void regscr_testb(void);

int dsys_init(const char *fname)
{
	int i;

	regscr_testa();
	regscr_testb();

	for(i=0; i<dsys_num_screens; i++) {
		if(dsys_screens[i]->init() == -1) {
			fprintf(stderr, "failed to initialize demo screen: %s\n", dsys_screens[i]->name);
			return -1;
		}
	}

	return 0;
}

void dsys_destroy(void)
{
	int i;

	for(i=0; i<dsys_num_screens; i++) {
		if(dsys_screens[i]->destroy) {
			dsys_screens[i]->destroy();
		}
	}
	dsys_num_screens = 0;
}

struct demoscreen *dsys_find_screen(const char *name)
{
	int i;

	for(i=0; i<dsys_num_screens; i++) {
		if(strcmp(dsys_screens[i]->name, name) == 0) {
			return dsys_screens[i];
		}
	}
	return 0;
}

void dsys_run_screen(struct demoscreen *scr)
{
	struct demoscreen *act;

	if(!scr) return;
	if(dsys_act_scr == scr && act_tail == scr) return;

	act = dsys_act_scr;
	while(act) {
		if(act->stop) act->stop();
		act = act->next;
	}
	dsys_act_scr = act_tail = scr;
	if(scr->start) scr->start();
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

int dsys_add_screen(struct demoscreen *scr)
{
	if(!scr->name || !scr->init || !scr->draw) {
		fprintf(stderr, "dsys_add_screen: invalid screen\n");
		return -1;
	}
	dsys_screens[dsys_num_screens++] = scr;
	return 0;
}
