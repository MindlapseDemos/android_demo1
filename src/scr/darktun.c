#include "demo.h"

static int init(void);
static void draw(void);

static struct demoscreen scr = {"darktun", init, 0, 0, 0, 0, 0, draw};

void regscr_darktun(void)
{
	dsys_add_screen(&scr);
}

static int init(void)
{
	return 0;
}

static void draw(void)
{
}
