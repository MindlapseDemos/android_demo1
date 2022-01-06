#include <assert.h>
#include "demo.h"
#include "cmesh.h"
#include "meshgen.h"

#define TILE_SZ	6.0f
#define SEQ_SZ	32
#define INTERV	1500

static int init(void);
static void destroy(void);
static void draw(void);
static void draw_tile(int tid);
static void keyb(int key, int pressed);
static void mouse(int bn, int st, int x, int y);
static void motion(int x, int y);

static struct demoscreen scr = {
	"darktun", init, destroy, 0, 0, 0, 0, draw, keyb, mouse, motion};

enum { TILE_STR, TILE_LTURN, TILE_RTURN, TILE_TEE_LEFT, TILE_TEE_RIGHT, NUM_TILES };
static const char *tile_files[NUM_TILES] = { "data/dtun_str.obj",
	"data/dtun_lturn.obj", "data/dtun_rturn.obj", "data/dtun_tee.obj", 0 };
static struct cmesh *tiles[NUM_TILES];
static const cgm_vec3 nextpos[NUM_TILES] = {
	{0, 0, -TILE_SZ},
	{-TILE_SZ / 2, 0, -TILE_SZ / 2},
	{TILE_SZ / 2, 0, -TILE_SZ / 2},
	{-TILE_SZ / 2, 0, -TILE_SZ / 2},
	{TILE_SZ / 2, 0, -TILE_SZ / 2}
};
static const float nextrot[NUM_TILES] = {0, 90, -90, 90, -90};
static float next_xform[NUM_TILES][16];

static int seq[SEQ_SZ];

static unsigned int sdr_tun;
static int uloc_mtl_color = -1;

static float cam_theta, cam_phi, cam_dist;


void regscr_darktun(void)
{
	dsys_add_screen(&scr);
}

static int init(void)
{
	int i;
	float xform[16];

	if(!(sdr_tun = get_sdrprog("sdr/darktun.v.glsl", "sdr/darktun.p.glsl"))) {
		return -1;
	}
	cmesh_bind_sdrloc(sdr_tun);
	glUseProgram(sdr_tun);
	uloc_mtl_color = glGetUniformLocation(sdr_tun, "mtl_color");

	cgm_mtranslation(xform, 0, 0, -TILE_SZ/2);

	for(i=0; i<NUM_TILES; i++) {
		if(tile_files[i]) {
			tiles[i] = cmesh_alloc();
			if(cmesh_load(tiles[i], tile_files[i]) == -1) {
				fprintf(stderr, "darktun: failed to load tile mesh: %s\n", tile_files[i]);
				return -1;
			}
			cmesh_apply_xform(tiles[i], xform, 0);
		} else {
			assert(tiles[i - 1]);
			tiles[i] = tiles[i - 1];
		}

		cgm_midentity(next_xform[i]);
		cgm_mrotate_y(next_xform[i], cgm_deg_to_rad(nextrot[i]));
		cgm_mtranslate(next_xform[i], nextpos[i].x, nextpos[i].y, nextpos[i].z);
	}

	/* generate a random sequence (TODO hard-code it before release) */
	seq[0] = TILE_STR;	/* always start from straight */
	printf("darktun seq: %d", seq[0]);
	for(i=1; i<SEQ_SZ; i++) {
		seq[i] = rand() % (NUM_TILES + 1);
		if(seq[i] >= NUM_TILES) seq[i] = TILE_STR;
		printf(" %d", seq[i]);
	}
	putchar('\n');

	return 0;
}

static void destroy(void)
{
	int i;
	for(i=0; i<NUM_TILES; i++) {
		if(i < NUM_TILES - 1 && tiles[i + 1] != tiles[i]) {
			cmesh_free(tiles[i]);
		}
	}
}

static int cur_seq_pos;

static void draw(void)
{
	int i, tid, spos;
	long tm;
	float t, rot;
	cgm_vec3 p, pend;
	static const cgm_vec3 zero, cent = {0, 0, TILE_SZ / 2};

	gl_matrix_mode(GL_PROJECTION);
	gl_load_identity();
	glu_perspective(60.0f, win_aspect, 0.5f, 500.0f);

	gl_matrix_mode(GL_MODELVIEW);
	gl_load_identity();
	gl_translatef(0, 0, -cam_dist);
	gl_rotatef(cam_phi, 1, 0, 0);
	gl_rotatef(cam_theta, 0, 1, 0);
	gl_translatef(0, -0.8, 0);

	glUseProgram(sdr_tun);

	tm = dsys.tmsec - scr.start_time;
	cur_seq_pos = tm / INTERV % SEQ_SZ;
	t = (float)(tm % INTERV) / INTERV;

	if(t < 0.5f) {
		cgm_vlerp(&p, &zero, &cent, t * 2.0f);
	} else {
		t = (t - 0.5f) * 2.0f;
		pend = nextpos[seq[cur_seq_pos]];
		cgm_vcons(&pend, -pend.x, -pend.y, -pend.z);
		cgm_vlerp(&p, &cent, &pend, t);

		t *= 3.0f;
		rot = -nextrot[seq[cur_seq_pos]] * (t > 1.0f ? 1.0f : t);
		gl_rotatef(rot, 0, 1, 0);
	}
	gl_translatef(p.x, p.y, p.z);

	spos = cur_seq_pos;
	for(i=0; i<3; i++) {
		tid = seq[spos];
		gl_apply_xform(sdr_tun);

		draw_tile(tid);

		gl_mult_matrixf(next_xform[tid]);
		spos = (spos + 1) % SEQ_SZ;
	}
}

static void draw_tile(int tid)
{
	int i, num_sub = cmesh_submesh_count(tiles[tid]);
	struct cmesh_material *mtl;

	if(num_sub) {
		for(i=0; i<num_sub; i++) {
			mtl = cmesh_submesh_material(tiles[tid], i);

			if(uloc_mtl_color >= 0) {
				glUniform3f(uloc_mtl_color, mtl->color.x, mtl->color.y, mtl->color.z);
			}

			cmesh_draw_submesh(tiles[tid], i);
		}
	} else {
		mtl = cmesh_material(tiles[tid]);

		if(uloc_mtl_color >= 0) {
			glUniform3f(uloc_mtl_color, mtl->color.x, mtl->color.y, mtl->color.z);
		}
		cmesh_draw(tiles[tid]);
	}
}

static void keyb(int key, int pressed)
{
	switch(key) {
	case ']':
		cur_seq_pos = (cur_seq_pos + 1) % SEQ_SZ;
		break;

	case '[':
		cur_seq_pos = (cur_seq_pos + SEQ_SZ - 1) % SEQ_SZ;
		break;

	default:
		break;
	}
}

static int bnstate[8], prev_x, prev_y;

static void mouse(int bn, int st, int x, int y)
{
	bnstate[bn] = st;
	prev_x = x;
	prev_y = y;
}

static void motion(int x, int y)
{
	int dx = x - prev_x;
	int dy = y - prev_y;
	prev_x = x;
	prev_y = y;

	if(!(dx | dy)) return;

	if(bnstate[0]) {
		cam_theta += dx * 0.5f;
		cam_phi += dy * 0.5f;
		if(cam_phi < -90) cam_phi = -90;
		if(cam_phi > 90) cam_phi = 90;
	}
	if(bnstate[2]) {
		cam_dist += dy * 0.1;
		if(cam_dist < 0) cam_dist = 0;
	}
}
