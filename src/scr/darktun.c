#include "demo.h"
#include "cmesh.h"
#include "meshgen.h"

#define TILE_SZ	3

static int init(void);
static void destroy(void);
static void draw(void);

static struct demoscreen scr = {"darktun", init, destroy, 0, 0, 0, 0, draw};

enum { TILE_STR, TILE_LTURN, TILE_RTURN, TILE_TEE, NUM_TILES };
static const char *tile_files[NUM_TILES] = { "data/dtun_str.obj",
	"data/dtun_lturn.obj", "data/dtun_rturn.obj", "data/dtun_tee.obj" };
static struct cmesh *tiles[NUM_TILES];

void regscr_darktun(void)
{
	dsys_add_screen(&scr);
}

static int init(void)
{
	int i;

	for(i=0; i<NUM_TILES; i++) {
		tiles[i] = cmesh_alloc();
		if(cmesh_load(tiles[i], tile_files[i]) == -1) {
			fprintf(stderr, "darktun: failed to load tile mesh: %s\n", tile_files[i]);
			return -1;
		}
	}

	return 0;
}

static void destroy(void)
{
	int i;
	for(i=0; i<NUM_TILES; i++) {
		cmesh_free(tiles[i]);
	}
}

static void draw(void)
{
	gl_matrix_mode(GL_PROJECTION);
	gl_load_identity();
	glu_perspective(50.0f, win_aspect, 0.5f, 500.0f);

	gl_matrix_mode(GL_MODELVIEW);
	gl_load_identity();
	gl_rotatef(dsys.tmsec / 10.0f, 1, 0, 0);
	gl_rotatef(dsys.tmsec / 20.0f, 0, 1, 0);
	gl_translatef(0, 0, -8);

	glUseProgram(sdr_dbg);
	gl_apply_xform(sdr_dbg);
	cmesh_draw(tiles[3]);
}
