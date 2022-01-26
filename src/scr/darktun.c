#include <assert.h>
#include "demo.h"
#include "scene.h"
#include "cmesh.h"
#include "meshgen.h"

#define TILE_SZ	6.0f
#define SEQ_SZ	32
#define INTERV	1500
#define DRAW_TILES	3

static int init(void);
static void destroy(void);
static void draw(void);
static void draw_tile(int tid);
static void add_lights(struct scene *scn, struct cmesh *mesh);
static void keyb(int key, int pressed);
static void mouse(int bn, int st, int x, int y);
static void motion(int x, int y);

static struct demoscreen scr = {
	"darktun", init, destroy, 0, 0, 0, 0, draw, keyb, mouse, motion};

enum { TILE_STR, TILE_LTURN, TILE_RTURN, TILE_TEE_LEFT, TILE_TEE_RIGHT, NUM_TILES };
static const char *tile_files[NUM_TILES] = { "data/dtun_str.obj",
	"data/dtun_lturn.obj", "data/dtun_rturn.obj", "data/dtun_tee.obj", 0 };
static struct scene *tiles[NUM_TILES];
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
static int uloc_mtl_color;
static int uloc_light[8];

static float cam_theta, cam_phi, cam_dist;


void regscr_darktun(void)
{
	dsys_add_screen(&scr);
}

static int init(void)
{
	int i;
	float xform[16];
	struct cmesh *mesh;

	if(!(sdr_tun = get_sdrprog("sdr/darktun.v.glsl", "sdr/darktun.p.glsl"))) {
		return -1;
	}
	cmesh_bind_sdrloc(sdr_tun);
	glUseProgram(sdr_tun);
	uloc_mtl_color = glGetUniformLocation(sdr_tun, "mtl_color");
	for(i=0; i<8; i++) {
		char name[16];
		sprintf(name, "light[%d]", i);
		uloc_light[i] = glGetUniformLocation(sdr_tun, name);
	}

	cgm_mtranslation(xform, 0, 0, -TILE_SZ/2);

	for(i=0; i<NUM_TILES; i++) {
		if(tile_files[i]) {
			tiles[i] = scn_alloc_scene();
			mesh = cmesh_alloc();
			if(cmesh_load(mesh, tile_files[i]) == -1) {
				fprintf(stderr, "darktun: failed to load tile mesh: %s\n", tile_files[i]);
				return -1;
			}
			cmesh_load_textures(mesh);
			cmesh_apply_xform(mesh, xform, 0);
			add_lights(tiles[i], mesh);
			scn_add_object(tiles[i], scn_alloc_object(mesh));
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
			scn_free_scene(tiles[i]);
		}
	}
}

static int cur_seq_pos;

static void draw(void)
{
	int i, j, tid, spos, nlights, num;
	long tm;
	float t, rot;
	float viewmat[16];
	cgm_vec3 p, pend, vpos;
	static const cgm_vec3 zero, cent = {0, 0, TILE_SZ / 2};

	gl_matrix_mode(GL_PROJECTION);
	gl_load_identity();
	glu_perspective(60.0f, win_aspect, 0.5f, 500.0f);

	cgm_midentity(viewmat);
	cgm_mtranslation(viewmat, 0, 0, -cam_dist);
	cgm_mprerotate_x(viewmat, cgm_deg_to_rad(cam_phi));
	cgm_mprerotate_y(viewmat, cgm_deg_to_rad(cam_theta));
	cgm_mpretranslate(viewmat, 0, -0.8, 0);

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
		cgm_mprerotate_y(viewmat, cgm_deg_to_rad(rot));
	}
	cgm_mpretranslate(viewmat, p.x, p.y, p.z);

	gl_matrix_mode(GL_MODELVIEW);
	gl_load_matrixf(viewmat);

	nlights = 0;
	spos = cur_seq_pos;
	for(i=0; i<DRAW_TILES; i++) {
		tid = seq[spos];

		num = scn_num_lights(tiles[tid]);
		for(j=0; j<num; j++) {
			if(nlights >= 8) break;
			scn_get_light_pos(tiles[tid]->lights[j], &vpos);
			cgm_vmul_m4v3(&vpos, viewmat);

			glUniform4f(uloc_light[nlights], vpos.x, vpos.y, vpos.z, 1);
			nlights++;
		}

		cgm_mpremul(viewmat, next_xform[tid]);
		spos = (spos + 1) % SEQ_SZ;
	}

	for(i=nlights; i<8; i++) {
		glUniform4f(uloc_light[i], 0, 0, 0, 0);
	}

	spos = cur_seq_pos;
	for(i=0; i<DRAW_TILES; i++) {
		tid = seq[spos];
		gl_apply_xform(sdr_tun);

		draw_tile(tid);

		gl_mult_matrixf(next_xform[tid]);
		spos = (spos + 1) % SEQ_SZ;
	}
}

static void setup_material(struct cmesh_material *mtl)
{
	if(uloc_mtl_color >= 0) {
		glUniform3f(uloc_mtl_color, mtl->color.x, mtl->color.y, mtl->color.z);
	}
	if(mtl->tex[CMESH_TEX_COLOR].id) {
		glBindTexture(GL_TEXTURE_2D, mtl->tex[CMESH_TEX_COLOR].id);
	} else {
		glBindTexture(GL_TEXTURE_2D, deftex_white);
	}
}

static void draw_tile(int tid)
{
	int i, j, num_obj, num_sub;
	struct cmesh *mesh;
	struct cmesh_material *mtl;

	num_obj = scn_num_objects(tiles[tid]);
	for(i=0; i<num_obj; i++) {
		mesh = tiles[tid]->objects[i]->mesh;
		num_sub = cmesh_submesh_count(mesh);

		if(num_sub) {
			for(j=0; j<num_sub; j++) {
				mtl = cmesh_submesh_material(mesh, j);
				setup_material(mtl);
				cmesh_draw_submesh(mesh, j);
			}
		} else {
			mtl = cmesh_material(mesh);
			setup_material(mtl);
			cmesh_draw(mesh);
		}
	}
}

static void add_lights(struct scene *scn, struct cmesh *mesh)
{
	int i, num_sub;
	const char *name;
	struct scn_light *lt;

	num_sub = cmesh_submesh_count(mesh);
	for(i=0; i<num_sub; i++) {
		name = cmesh_submesh_name(mesh, i);
		if(name && strstr(name, "light")) {
			cgm_vec3 center;
			cmesh_submesh_bsphere(mesh, i, &center, 0);
			lt = scn_alloc_light();
			anm_set_position3f((struct anm_node*)lt, center.x, center.y, center.z, 0);
			scn_add_light(scn, lt);
			printf("add light at %f %f %f\n", center.x, center.y, center.z);
		}
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
