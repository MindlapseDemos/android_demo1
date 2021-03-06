#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include <assert.h>
#include "opengl.h"
#include "sdr.h"
#include "cmesh.h"
#include "assman.h"
#include "util.h"


struct cmesh_vattrib {
	int nelem;	/* num elements per attribute [1, 4] */
	float *data;
	unsigned int count;
	unsigned int vbo;
	int vbo_valid, data_valid;
};

/* istart,icount are valid only when the mesh is indexed, otherwise icount is 0.
 * vstart,vcount are define the submesh for non-indexed meshes.
 * For indexed meshes, vstart,vcount denote the range of vertices used by each
 * submesh.
 */
struct submesh {
	char *name;
	int nfaces;	/* derived from either icount or vcount */
	int istart, icount;
	int vstart, vcount;
	struct cmesh_material mtl;
	struct submesh *next;
};

struct cmesh {
	char *name;
	unsigned int nverts, nfaces;

	struct submesh *sublist, *subtail;
	int subcount;

	struct cmesh_material mtl;

	/* current value for each attribute for the immediate mode interface */
	cgm_vec4 cur_val[CMESH_NUM_ATTR];

	unsigned int buffer_objects[CMESH_NUM_ATTR + 1];
	struct cmesh_vattrib vattr[CMESH_NUM_ATTR];

	unsigned int *idata;
	unsigned int icount;
	unsigned int ibo;
	int ibo_valid, idata_valid;

	/* index buffer for wireframe rendering (constructed on demand) */
	unsigned int wire_ibo;
	int wire_ibo_valid;

	/* axis-aligned bounding box */
	cgm_vec3 aabb_min, aabb_max;
	int aabb_valid;
	/* bounding sphere */
	cgm_vec3 bsph_center;
	float bsph_radius;
	int bsph_valid;
};


static int clone(struct cmesh *cmdest, struct cmesh *cmsrc, struct submesh *sub);
static int pre_draw(struct cmesh *cm);
static void post_draw(struct cmesh *cm, int cur_sdr);
static void update_buffers(struct cmesh *cm);
static void update_wire_ibo(struct cmesh *cm);
static void calc_aabb(struct cmesh *cm);
static void calc_bsph_noidx(struct cmesh *cm, int start, int count);
static void calc_bsph_idx(struct cmesh *cm, int start, int count);
static void clear_mtl(struct cmesh_material *mtl);
static void clone_mtl(struct cmesh_material *dest, struct cmesh_material *src);

static int def_nelem[CMESH_NUM_ATTR] = {3, 3, 3, 2, 4, 4, 4, 2};

static int sdr_loc[CMESH_NUM_ATTR] = {0, 1, 2, 3, 4, 5, 6, 7};
static int use_custom_sdr_attr;

static const struct cmesh_material defmtl = {
	0, {1, 1, 1}, {0, 0, 0}, {0, 0, 0}, 1, 1, 1
};


/* global state */
void cmesh_set_attrib_sdrloc(int attr, int loc)
{
	sdr_loc[attr] = loc;
}

int cmesh_get_attrib_sdrloc(int attr)
{
	return sdr_loc[attr];
}

void cmesh_clear_attrib_sdrloc(void)
{
	int i;
	for(i=0; i<CMESH_NUM_ATTR; i++) {
		sdr_loc[i] = -1;
	}
}

void cmesh_bind_sdrloc(unsigned int sdr)
{
	glBindAttribLocation(sdr, CMESH_ATTR_VERTEX, "attr_vertex");
	glBindAttribLocation(sdr, CMESH_ATTR_NORMAL, "attr_normal");
	glBindAttribLocation(sdr, CMESH_ATTR_TANGENT, "attr_tangent");
	glBindAttribLocation(sdr, CMESH_ATTR_TEXCOORD, "attr_texcoord");
	glBindAttribLocation(sdr, CMESH_ATTR_COLOR, "attr_color");
	glBindAttribLocation(sdr, CMESH_ATTR_BONEWEIGHTS, "attr_boneweights");
	glBindAttribLocation(sdr, CMESH_ATTR_BONEIDX, "attr_boneidx");
	glBindAttribLocation(sdr, CMESH_ATTR_TEXCOORD2, "attr_texcoord2");
	link_program(sdr);
}

/* mesh functions */
struct cmesh *cmesh_alloc(void)
{
	struct cmesh *cm;

	cm = malloc_nf(sizeof *cm);
	if(cmesh_init(cm) == -1) {
		free(cm);
		return 0;
	}
	return cm;
}

void cmesh_free(struct cmesh *cm)
{
	cmesh_destroy(cm);
	free(cm);
}

int cmesh_init(struct cmesh *cm)
{
	int i;

	memset(cm, 0, sizeof *cm);
	cgm_wcons(cm->cur_val + CMESH_ATTR_COLOR, 1, 1, 1, 1);

	glGenBuffers(CMESH_NUM_ATTR + 1, cm->buffer_objects);

	for(i=0; i<CMESH_NUM_ATTR; i++) {
		cm->vattr[i].vbo = cm->buffer_objects[i];
	}

	cm->ibo = cm->buffer_objects[CMESH_NUM_ATTR];

	cm->mtl = defmtl;
	return 0;
}

void cmesh_destroy(struct cmesh *cm)
{
	int i;

	free(cm->name);

	for(i=0; i<CMESH_NUM_ATTR; i++) {
		free(cm->vattr[i].data);
	}
	free(cm->idata);

	cmesh_clear_submeshes(cm);

	glDeleteBuffers(CMESH_NUM_ATTR + 1, cm->buffer_objects);
	if(cm->wire_ibo) {
		glDeleteBuffers(1, &cm->wire_ibo);
	}

	clear_mtl(&cm->mtl);
}

void cmesh_clear(struct cmesh *cm)
{
	int i;

	for(i=0; i<CMESH_NUM_ATTR; i++) {
		cm->vattr[i].nelem = 0;
		cm->vattr[i].vbo_valid = 0;
		cm->vattr[i].data_valid = 0;
		free(cm->vattr[i].data);
		cm->vattr[i].data = 0;
		cm->vattr[i].count = 0;
	}
	cm->ibo_valid = cm->idata_valid = 0;
	free(cm->idata);
	cm->idata = 0;
	cm->icount = 0;

	cm->wire_ibo_valid = 0;
	cm->nverts = cm->nfaces = 0;

	cm->bsph_valid = cm->aabb_valid = 0;

	cmesh_clear_submeshes(cm);
	clear_mtl(&cm->mtl);
}

int cmesh_clone(struct cmesh *cmdest, struct cmesh *cmsrc)
{
	return clone(cmdest, cmsrc, 0);
}

static int clone(struct cmesh *cmdest, struct cmesh *cmsrc, struct submesh *sub)
{
	int i, nelem, vstart, vcount, istart, icount;
	char *srcname, *name = 0;
	float *varr[CMESH_NUM_ATTR] = {0};
	float *vptr;
	unsigned int *iptr, *iarr = 0;
	struct cmesh_material mtl;

	srcname = sub ? sub->name : cmsrc->name;
	if(srcname) {
		name = malloc_nf(strlen(srcname) + 1);
		strcpy(name, srcname);
	}

	if(sub) {
		vstart = sub->vstart;
		vcount = sub->vcount;
		istart = sub->istart;
		icount = sub->icount;
	} else {
		vstart = istart = 0;
		vcount = cmsrc->nverts;
		icount = cmsrc->icount;
	}

	if(cmesh_indexed(cmsrc)) {
		iarr = malloc_nf(icount * sizeof *iarr);
	}

	clone_mtl(&mtl, sub ? &sub->mtl : &cmsrc->mtl);

	for(i=0; i<CMESH_NUM_ATTR; i++) {
		if(cmesh_has_attrib(cmsrc, i)) {
			nelem = cmsrc->vattr[i].nelem;
			varr[i] = malloc_nf(vcount * nelem * sizeof(float));
		}
	}

	/* from this point forward nothing can fail */
	cmesh_clear(cmdest);

	for(i=0; i<CMESH_NUM_ATTR; i++) {
		free(cmdest->vattr[i].data);

		if(cmesh_has_attrib(cmsrc, i)) {
			cmesh_attrib(cmsrc, i);	/* force validation of the actual data on the source mesh */

			nelem = cmsrc->vattr[i].nelem;
			cmdest->vattr[i].nelem = nelem;
			cmdest->vattr[i].data = varr[i];
			cmdest->vattr[i].count = vcount;
			vptr = cmsrc->vattr[i].data + vstart * nelem;
			memcpy(cmdest->vattr[i].data, vptr, vcount * nelem * sizeof(float));
			cmdest->vattr[i].data_valid = 1;
			cmdest->vattr[i].vbo_valid = 0;
		} else {
			memset(cmdest->vattr + i, 0, sizeof cmdest->vattr[i]);
		}
	}

	if(cmesh_indexed(cmsrc)) {
		cmesh_index(cmsrc);	/* force validation .... */

		cmdest->idata = iarr;
		cmdest->icount = icount;
		if(sub) {
			/* need to offset all vertex indices by -vstart */
			iptr = cmsrc->idata + istart;
			for(i=0; i<icount; i++) {
				cmdest->idata[i] = *iptr++ - vstart;
			}
		} else {
			memcpy(cmdest->idata, cmsrc->idata + istart, icount * sizeof *cmdest->idata);
		}
		cmdest->idata_valid = 1;
	} else {
		cmdest->idata = 0;
		cmdest->idata_valid = cmdest->ibo_valid = 0;
	}

	free(cmdest->name);
	cmdest->name = name;

	cmdest->nverts = cmsrc->nverts;
	cmdest->nfaces = sub ? sub->nfaces : cmsrc->nfaces;

	memcpy(cmdest->cur_val, cmsrc->cur_val, sizeof cmdest->cur_val);

	cmdest->aabb_min = cmsrc->aabb_min;
	cmdest->aabb_max = cmsrc->aabb_max;
	cmdest->aabb_valid = cmsrc->aabb_valid;
	cmdest->bsph_center = cmsrc->bsph_center;
	cmdest->bsph_radius = cmsrc->bsph_radius;
	cmdest->bsph_valid = cmsrc->bsph_valid;

	/* copy sublist only if we're not cloning a submesh */
	if(!sub) {
		struct submesh *sm, *n, *head = 0, *tail = 0;

		sm = cmsrc->sublist;
		while(sm) {
			n = malloc_nf(sizeof *n);
			name = strdup_nf(sm->name);
			*n = *sm;
			n->name = name;
			n->next = 0;

			if(head) {
				tail->next = n;
				tail = n;
			} else {
				head = tail = n;
			}

			sm = sm->next;
		}

		cmdest->sublist = head;
		cmdest->subtail = tail;
		cmdest->subcount = cmsrc->subcount;
	}

	return 0;
}

int cmesh_set_name(struct cmesh *cm, const char *name)
{
	free(cm->name);
	cm->name = strdup_nf(name);
	return 0;
}

const char *cmesh_name(struct cmesh *cm)
{
	return cm->name;
}

struct cmesh_material *cmesh_material(struct cmesh *cm)
{
	return &cm->mtl;
}

struct cmesh_material *cmesh_submesh_material(struct cmesh *cm, int subidx)
{
	struct submesh *sm = cm->sublist;

	while(sm && subidx-- > 0) {
		sm = sm->next;
	}
	return sm ? &sm->mtl : 0;
}

static int load_textures(struct cmesh_material *mtl)
{
	int i, res = 0;
	for(i=0; i<CMESH_NUM_TEX; i++) {
		if(mtl->tex[i].name && !(mtl->tex[i].id = get_tex2d(mtl->tex[i].name))) {
			res = -1;
		}
	}
	return res;
}

int cmesh_load_textures(struct cmesh *cm)
{
	int res = 0;
	struct submesh *sm;

	if(load_textures(&cm->mtl) == -1) {
		res = -1;
	}

	sm = cm->sublist;
	while(sm) {
		if(load_textures(&sm->mtl) == -1) {
			res = -1;
		}
		sm = sm->next;
	}
	return res;
}

int cmesh_has_attrib(struct cmesh *cm, int attr)
{
	if(attr < 0 || attr >= CMESH_NUM_ATTR) {
		return 0;
	}
	return cm->vattr[attr].vbo_valid | cm->vattr[attr].data_valid;
}

int cmesh_indexed(struct cmesh *cm)
{
	return cm->ibo_valid | cm->idata_valid;
}

/* vdata can be 0, in which case only memory is allocated
 * returns pointer to the attribute array
 */
float *cmesh_set_attrib(struct cmesh *cm, int attr, int nelem, unsigned int num,
		const float *vdata)
{
	float *newarr;

	if(attr < 0 || attr >= CMESH_NUM_ATTR) {
		return 0;
	}
	if(cm->nverts && num != cm->nverts) {
		return 0;
	}

	newarr = malloc_nf(num * nelem * sizeof *newarr);
	if(vdata) {
		memcpy(newarr, vdata, num * nelem * sizeof *newarr);
	}

	cm->nverts = num;

	free(cm->vattr[attr].data);
	cm->vattr[attr].data = newarr;
	cm->vattr[attr].count = num * nelem;
	cm->vattr[attr].nelem = nelem;
	cm->vattr[attr].data_valid = 1;
	cm->vattr[attr].vbo_valid = 0;
	return newarr;
}

float *cmesh_attrib(struct cmesh *cm, int attr)
{
	if(attr < 0 || attr >= CMESH_NUM_ATTR) {
		return 0;
	}
	cm->vattr[attr].vbo_valid = 0;
	return (float*)cmesh_attrib_ro(cm, attr);
}

const float *cmesh_attrib_ro(struct cmesh *cm, int attr)
{
	if(attr < 0 || attr >= CMESH_NUM_ATTR) {
		return 0;
	}

	if(!cm->vattr[attr].data_valid) {
#if GL_ES_VERSION_2_0
		return 0;
#else
		void *tmp;
		int nelem;
		if(!cm->vattr[attr].vbo_valid) {
			return 0;
		}

		/* local data copy unavailable, grab the data from the vbo */
		nelem = cm->vattr[attr].nelem;
		cm->vattr[attr].data = malloc_nf(cm->nverts * nelem * sizeof(float));
		cm->vattr[attr].count = cm->nverts * nelem;

		glBindBuffer(GL_ARRAY_BUFFER, cm->vattr[attr].vbo);
		tmp = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
		memcpy(cm->vattr[attr].data, tmp, cm->nverts * nelem * sizeof(float));
		glUnmapBuffer(GL_ARRAY_BUFFER);

		cm->vattr[attr].data_valid = 1;
#endif
	}
	return cm->vattr[attr].data;
}

float *cmesh_attrib_at(struct cmesh *cm, int attr, int idx)
{
	float *vptr = cmesh_attrib(cm, attr);
	return vptr ? vptr + idx * cm->vattr[attr].nelem : 0;
}

const float *cmesh_attrib_at_ro(struct cmesh *cm, int attr, int idx)
{
	const float *vptr = cmesh_attrib_ro(cm, attr);
	return vptr ? vptr + idx * cm->vattr[attr].nelem : 0;
}

int cmesh_attrib_count(struct cmesh *cm, int attr)
{
	return cmesh_has_attrib(cm, attr) ? cm->nverts : 0;
}

int cmesh_push_attrib(struct cmesh *cm, int attr, float *v)
{
	float *vptr;
	int i, cursz, newsz;

	if(!cm->vattr[attr].nelem) {
		cm->vattr[attr].nelem = def_nelem[attr];
	}

	cursz = cm->vattr[attr].count;
	newsz = cursz + cm->vattr[attr].nelem;
	vptr = realloc_nf(cm->vattr[attr].data, newsz * sizeof(float));
	cm->vattr[attr].data = vptr;
	cm->vattr[attr].count = newsz;
	vptr += cursz;

	for(i=0; i<cm->vattr[attr].nelem; i++) {
		*vptr++ = *v++;
	}
	cm->vattr[attr].data_valid = 1;
	cm->vattr[attr].vbo_valid = 0;

	if(attr == CMESH_ATTR_VERTEX) {
		cm->nverts = newsz / cm->vattr[attr].nelem;
	}
	return 0;
}

int cmesh_push_attrib1f(struct cmesh *cm, int attr, float x)
{
	float v[4];
	v[0] = x;
	v[1] = v[2] = 0.0f;
	v[3] = 1.0f;
	return cmesh_push_attrib(cm, attr, v);
}

int cmesh_push_attrib2f(struct cmesh *cm, int attr, float x, float y)
{
	float v[4];
	v[0] = x;
	v[1] = y;
	v[2] = 0.0f;
	v[3] = 1.0f;
	return cmesh_push_attrib(cm, attr, v);
}

int cmesh_push_attrib3f(struct cmesh *cm, int attr, float x, float y, float z)
{
	float v[4];
	v[0] = x;
	v[1] = y;
	v[2] = z;
	v[3] = 1.0f;
	return cmesh_push_attrib(cm, attr, v);
}

int cmesh_push_attrib4f(struct cmesh *cm, int attr, float x, float y, float z, float w)
{
	float v[4];
	v[0] = x;
	v[1] = y;
	v[2] = z;
	v[3] = w;
	return cmesh_push_attrib(cm, attr, v);
}

/* indices can be 0, in which case only memory is allocated
 * returns pointer to the index array
 */
unsigned int *cmesh_set_index(struct cmesh *cm, int num, const unsigned int *indices)
{
	unsigned int *tmp;
	int nidx = cm->nfaces * 3;

	if(nidx && num != nidx) {
		return 0;
	}

	tmp = malloc_nf(num * sizeof *tmp);
	if(indices) {
		memcpy(tmp, indices, num * sizeof *tmp);
	}

	free(cm->idata);
	cm->idata = tmp;
	cm->icount = num;
	cm->nfaces = num / 3;
	cm->idata_valid = 1;
	cm->ibo_valid = 0;
	return tmp;
}

unsigned int *cmesh_index(struct cmesh *cm)
{
	cm->ibo_valid = 0;
	return (unsigned int*)cmesh_index_ro(cm);
}

const unsigned int *cmesh_index_ro(struct cmesh *cm)
{
	if(!cm->idata_valid) {
#if GL_ES_VERSION_2_0
		return 0;
#else
		int nidx;
		unsigned int *tmp;

		if(!cm->ibo_valid) {
			return 0;
		}

		/* local copy is unavailable, grab the data from the ibo */
		nidx = cm->nfaces * 3;
		tmp = malloc_nf(nidx * sizeof *cm->idata);
		free(cm->idata);
		cm->idata = tmp;
		cm->icount = nidx;

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cm->ibo);
		tmp = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_READ_ONLY);
		memcpy(cm->idata, tmp, nidx * sizeof *cm->idata);
		glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

		cm->idata_valid = 1;
#endif
	}
	return cm->idata;
}

int cmesh_index_count(struct cmesh *cm)
{
	return cm->nfaces * 3;
}

int cmesh_push_index(struct cmesh *cm, unsigned int idx)
{
	unsigned int *iptr;
	unsigned int cur_sz = cm->icount;
	iptr = realloc_nf(cm->idata, (cur_sz + 1) * sizeof *iptr);
	iptr[cur_sz] = idx;
	cm->idata = iptr;
	cm->icount = cur_sz + 1;
	cm->idata_valid = 1;
	cm->ibo_valid = 0;

	cm->nfaces = cm->icount / 3;
	return 0;
}

int cmesh_poly_count(struct cmesh *cm)
{
	if(cm->nfaces) {
		return cm->nfaces;
	}
	if(cm->nverts) {
		return cm->nverts / 3;
	}
	return 0;
}

/* attr can be -1 to invalidate all attributes */
void cmesh_invalidate_vbo(struct cmesh *cm, int attr)
{
	int i;

	if(attr >= CMESH_NUM_ATTR) {
		return;
	}

	if(attr < 0) {
		for(i=0; i<CMESH_NUM_ATTR; i++) {
			cm->vattr[i].vbo_valid = 0;
		}
	} else {
		cm->vattr[attr].vbo_valid = 0;
	}
}

void cmesh_invalidate_index(struct cmesh *cm)
{
	cm->ibo_valid = 0;
}

int cmesh_append(struct cmesh *cmdest, struct cmesh *cmsrc)
{
	int i, nelem, newsz, origsz, srcsz;
	float *vptr;
	unsigned int *iptr;
	unsigned int idxoffs;

	if(!cmdest->nverts) {
		return cmesh_clone(cmdest, cmsrc);
	}

	for(i=0; i<CMESH_NUM_ATTR; i++) {
		if(cmesh_has_attrib(cmdest, i) && cmesh_has_attrib(cmsrc, i)) {
			/* force validation of the data arrays */
			cmesh_attrib(cmdest, i);
			cmesh_attrib_ro(cmsrc, i);

			assert(cmdest->vattr[i].nelem == cmsrc->vattr[i].nelem);
			nelem = cmdest->vattr[i].nelem;
			origsz = cmdest->nverts * nelem;
			newsz = cmdest->nverts + cmsrc->nverts * nelem;

			vptr = realloc_nf(cmdest->vattr[i].data, newsz * sizeof *vptr);
			memcpy(vptr + origsz, cmsrc->vattr[i].data, cmsrc->nverts * nelem * sizeof(float));
			cmdest->vattr[i].data = vptr;
			cmdest->vattr[i].count = newsz;
		}
	}

	if(cmesh_indexed(cmdest)) {
		assert(cmesh_indexed(cmsrc));
		/* force validation ... */
		cmesh_index(cmdest);
		cmesh_index_ro(cmsrc);

		idxoffs = cmdest->nverts;
		origsz = cmdest->icount;
		srcsz = cmsrc->icount;
		newsz = origsz + srcsz;

		iptr = realloc_nf(cmdest->idata, newsz * sizeof *iptr);
		cmdest->idata = iptr;
		cmdest->icount = newsz;

		/* copy and fixup all the new indices */
		iptr += origsz;
		for(i=0; i<srcsz; i++) {
			*iptr++ = cmsrc->idata[i] + idxoffs;
		}
	}

	cmdest->wire_ibo_valid = 0;
	cmdest->aabb_valid = 0;
	cmdest->bsph_valid = 0;
	return 0;
}

void cmesh_clear_submeshes(struct cmesh *cm)
{
	struct submesh *sm;

	while(cm->sublist) {
		sm = cm->sublist;
		cm->sublist = cm->sublist->next;
		free(sm->name);
		clear_mtl(&sm->mtl);
		free(sm);
	}
	cm->subtail = 0;
	cm->subcount = 0;
}

int cmesh_submesh(struct cmesh *cm, const char *name, int fstart, int fcount)
{
	int i;
	unsigned int minv = UINT_MAX, maxv = 0;
	unsigned int *iptr;
	struct submesh *sm;

	if(fstart < 0 || fcount < 1 || fstart + fcount > cm->nfaces) {
		return -1;
	}

	sm = malloc_nf(sizeof *sm);
	sm->name = strdup_nf(name);
	sm->nfaces = fcount;
	clone_mtl(&sm->mtl, &cm->mtl);

	if(cmesh_indexed(cm)) {
		sm->istart = fstart * 3;
		sm->icount = fcount * 3;

		/* find out which vertices are used by this submesh */
		iptr = cm->idata + sm->istart;
		for(i=0; i<sm->icount; i++) {
			unsigned int vidx = *iptr++;
			if(vidx < minv) minv = vidx;
			if(vidx > maxv) maxv = vidx;
		}
		sm->vstart = minv;
		sm->vcount = maxv - minv + 1;
	} else {
		sm->istart = sm->icount = 0;
		sm->vstart = fstart * 3;
		sm->vcount = fcount * 3;
	}

	sm->next = 0;
	if(cm->sublist) {
		cm->subtail->next = sm;
		cm->subtail = sm;
	} else {
		cm->sublist = cm->subtail = sm;
	}
	cm->subcount++;
	return 0;
}

int cmesh_remove_submesh(struct cmesh *cm, int idx)
{
	struct submesh dummy;
	struct submesh *prev, *sm;

	if(idx >= cm->subcount) {
		return -1;
	}

	dummy.next = cm->sublist;
	prev = &dummy;

	while(prev->next && idx-- > 0) {
		prev = prev->next;
	}

	if(!(sm = prev->next)) return -1;

	prev->next = sm->next;
	free(sm->name);
	clear_mtl(&sm->mtl);
	free(sm);

	cm->subcount--;
	assert(cm->subcount >= 0);

	cm->sublist = dummy.next;
	if(!cm->sublist) {
		cm->sublist = cm->subtail = 0;
	} else {
		if(cm->subtail == sm) cm->subtail = prev;
	}
	return 0;
}

int cmesh_find_submesh(struct cmesh *cm, const char *name)
{
	int idx = 0;
	struct submesh *sm = cm->sublist;
	while(sm) {
		if(strcmp(sm->name, name) == 0) {
			assert(idx <= cm->subcount);
			return idx;
		}
		idx++;
		sm = sm->next;
	}
	return -1;
}

int cmesh_submesh_count(struct cmesh *cm)
{
	return cm->subcount;
}

static struct submesh *get_submesh(struct cmesh *m, int idx)
{
	struct submesh *sm = m->sublist;
	while(sm && --idx >= 0) {
		sm = sm->next;
	}
	return sm;
}

int cmesh_clone_submesh(struct cmesh *cmdest, struct cmesh *cm, int subidx)
{
	struct submesh *sub;

	if(!(sub = get_submesh(cm, subidx))) {
		return -1;
	}
	return clone(cmdest, cm, sub);
}

const char *cmesh_submesh_name(struct cmesh *cm, int idx)
{
	struct submesh *sub;

	if(!(sub = get_submesh(cm, idx))) {
		return 0;
	}
	return sub->name;
}



/* assemble a complete vertex by adding all the useful attributes */
int cmesh_vertex(struct cmesh *cm, float x, float y, float z)
{
	int i, j;

	cgm_wcons(cm->cur_val + CMESH_ATTR_VERTEX, x, y, z, 1.0f);
	cm->vattr[CMESH_ATTR_VERTEX].data_valid = 1;
	cm->vattr[CMESH_ATTR_VERTEX].nelem = 3;

	for(i=0; i<CMESH_NUM_ATTR; i++) {
		if(cm->vattr[i].data_valid) {
			int newsz = cm->vattr[i].count + cm->vattr[i].nelem;
			float *tmp = realloc_nf(cm->vattr[i].data, newsz * sizeof *tmp);
			tmp += cm->vattr[i].count;

			cm->vattr[i].data = tmp;
			cm->vattr[i].count = newsz;

			for(j=0; j<cm->vattr[i].nelem; j++) {
				*tmp++ = *(&cm->cur_val[i].x + j);
			}
		}
		cm->vattr[i].vbo_valid = 0;
		cm->vattr[i].data_valid = 1;
	}

	if(cm->idata_valid) {
		free(cm->idata);
		cm->idata = 0;
		cm->icount = 0;
	}
	cm->ibo_valid = cm->idata_valid = 0;
	return 0;
}

void cmesh_normal(struct cmesh *cm, float nx, float ny, float nz)
{
	cgm_wcons(cm->cur_val + CMESH_ATTR_NORMAL, nx, ny, nz, 1.0f);
	cm->vattr[CMESH_ATTR_NORMAL].nelem = 3;
}

void cmesh_tangent(struct cmesh *cm, float tx, float ty, float tz)
{
	cgm_wcons(cm->cur_val + CMESH_ATTR_TANGENT, tx, ty, tz, 1.0f);
	cm->vattr[CMESH_ATTR_TANGENT].nelem = 3;
}

void cmesh_texcoord(struct cmesh *cm, float u, float v, float w)
{
	cgm_wcons(cm->cur_val + CMESH_ATTR_TEXCOORD, u, v, w, 1.0f);
	cm->vattr[CMESH_ATTR_TEXCOORD].nelem = 3;
}

void cmesh_boneweights(struct cmesh *cm, float w1, float w2, float w3, float w4)
{
	cgm_wcons(cm->cur_val + CMESH_ATTR_BONEWEIGHTS, w1, w2, w3, w4);
	cm->vattr[CMESH_ATTR_BONEWEIGHTS].nelem = 4;
}

void cmesh_boneidx(struct cmesh *cm, int idx1, int idx2, int idx3, int idx4)
{
	cgm_wcons(cm->cur_val + CMESH_ATTR_BONEIDX, idx1, idx2, idx3, idx4);
	cm->vattr[CMESH_ATTR_BONEIDX].nelem = 4;
}

static float *get_vec4(struct cmesh *cm, int attr, int idx, cgm_vec4 *res)
{
	int i;
	float *sptr, *dptr;
	cgm_wcons(res, 0, 0, 0, 1);
	if(!(sptr = cmesh_attrib_at(cm, attr, idx))) {
		return 0;
	}
	dptr = &res->x;

	for(i=0; i<cm->vattr[attr].nelem; i++) {
		*dptr++ = sptr[i];
	}
	return sptr;
}

static float *get_vec3(struct cmesh *cm, int attr, int idx, cgm_vec3 *res)
{
	int i;
	float *sptr, *dptr;
	cgm_vcons(res, 0, 0, 0);
	if(!(sptr = cmesh_attrib_at(cm, attr, idx))) {
		return 0;
	}
	dptr = &res->x;

	for(i=0; i<cm->vattr[attr].nelem; i++) {
		*dptr++ = sptr[i];
	}
	return sptr;
}

void cmesh_apply_xform(struct cmesh *cm, float *xform, float *dir_xform)
{
	unsigned int i;
	int j;
	cgm_vec4 v;
	cgm_vec3 n, t;
	float *vptr;

	if(!dir_xform) dir_xform = xform;

	for(i=0; i<cm->nverts; i++) {
		if(!(vptr = get_vec4(cm, CMESH_ATTR_VERTEX, i, &v))) {
			return;
		}
		cgm_wmul_m4v4(&v, xform);
		for(j=0; j<cm->vattr[CMESH_ATTR_VERTEX].nelem; j++) {
			*vptr++ = (&v.x)[j];
		}

		if(cmesh_has_attrib(cm, CMESH_ATTR_NORMAL)) {
			if((vptr = get_vec3(cm, CMESH_ATTR_NORMAL, i, &n))) {
				cgm_vmul_m3v3(&n, dir_xform);
				for(j=0; j<cm->vattr[CMESH_ATTR_NORMAL].nelem; j++) {
					*vptr++ = (&n.x)[j];
				}
			}
		}
		if(cmesh_has_attrib(cm, CMESH_ATTR_TANGENT)) {
			if((vptr = get_vec3(cm, CMESH_ATTR_TANGENT, i, &t))) {
				cgm_vmul_m3v3(&t, dir_xform);
				for(j=0; j<cm->vattr[CMESH_ATTR_TANGENT].nelem; j++) {
					*vptr++ = (&t.x)[j];
				}
			}
		}
	}
}

void cmesh_flip(struct cmesh *cm)
{
	cmesh_flip_faces(cm);
	cmesh_flip_normals(cm);
}

void cmesh_flip_faces(struct cmesh *cm)
{
	int i, j, idxnum, vnum, nelem;
	unsigned int *indices;
	float *verts, *vptr;

	if(cmesh_indexed(cm)) {
		if(!(indices = cmesh_index(cm))) {
			return;
		}
		idxnum = cmesh_index_count(cm);
		for(i=0; i<idxnum; i+=3) {
			unsigned int tmp = indices[i + 2];
			indices[i + 2] = indices[i + 1];
			indices[i + 1] = tmp;
		}
	} else {
		if(!(verts = cmesh_attrib(cm, CMESH_ATTR_VERTEX))) {
			return;
		}
		vnum = cmesh_attrib_count(cm, CMESH_ATTR_VERTEX);
		nelem = cm->vattr[CMESH_ATTR_VERTEX].nelem;
		for(i=0; i<vnum; i+=3) {
			for(j=0; j<nelem; j++) {
				vptr = verts + (i + 1) * nelem + j;
				float tmp = vptr[nelem];
				vptr[nelem] = vptr[0];
				vptr[0] = tmp;
			}
		}
	}
}
void cmesh_flip_normals(struct cmesh *cm)
{
	int i, num;
	float *nptr = cmesh_attrib(cm, CMESH_ATTR_NORMAL);
	if(!nptr) return;

	num = cm->nverts * cm->vattr[CMESH_ATTR_NORMAL].nelem;
	for(i=0; i<num; i++) {
		*nptr = -*nptr;
		nptr++;
	}
}

int cmesh_explode(struct cmesh *cm)
{
	int i, j, k, idxnum, nnverts;
	unsigned int *indices;

	if(!cmesh_indexed(cm)) return 0;

	indices = cmesh_index(cm);
	assert(indices);

	idxnum = cmesh_index_count(cm);
	nnverts = idxnum;

	for(i=0; i<CMESH_NUM_ATTR; i++) {
		const float *srcbuf;
		float *tmpbuf, *dstptr;

		if(!cmesh_has_attrib(cm, i)) continue;

		srcbuf = cmesh_attrib(cm, i);
		tmpbuf = malloc_nf(nnverts * cm->vattr[i].nelem * sizeof(float));
		dstptr = tmpbuf;

		for(j=0; j<idxnum; j++) {
			unsigned int idx = indices[j];
			const float *srcptr = srcbuf + idx * cm->vattr[i].nelem;

			for(k=0; k<cm->vattr[i].nelem; k++) {
				*dstptr++ = *srcptr++;
			}
		}

		free(cm->vattr[i].data);
		cm->vattr[i].data = tmpbuf;
		cm->vattr[i].count = nnverts * cm->vattr[i].nelem;
		cm->vattr[i].data_valid = 1;
	}

	cm->ibo_valid = 0;
	cm->idata_valid = 0;
	free(cm->idata);
	cm->idata = 0;
	cm->icount = 0;

	cm->nverts = nnverts;
	cm->nfaces = idxnum / 3;
	return 0;
}

void cmesh_calc_face_normals(struct cmesh *cm)
{
	/* TODO */
}

static int pre_draw(struct cmesh *cm)
{
	int i, loc;

	update_buffers(cm);

	if(!cm->vattr[CMESH_ATTR_VERTEX].vbo_valid) {
		return -1;
	}

	if(sdr_loc[CMESH_ATTR_VERTEX] == -1) {
		return -1;
	}

	for(i=0; i<CMESH_NUM_ATTR; i++) {
		loc = sdr_loc[i];
		if(loc >= 0 && cm->vattr[i].vbo_valid) {
			glBindBuffer(GL_ARRAY_BUFFER, cm->vattr[i].vbo);
			glVertexAttribPointer(loc, cm->vattr[i].nelem, GL_FLOAT, GL_FALSE, 0, 0);
			glEnableVertexAttribArray(loc);
		}
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	return 0;
}

void cmesh_draw(struct cmesh *cm)
{
	pre_draw(cm);

	if(cm->ibo_valid) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cm->ibo);
		glDrawElements(GL_TRIANGLES, cm->nfaces * 3, GL_UNSIGNED_INT, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	} else {
		glDrawArrays(GL_TRIANGLES, 0, cm->nverts);
	}

	post_draw(cm, 0);
}

void cmesh_draw_range(struct cmesh *cm, int start, int count)
{
	int cur_sdr;

	if((cur_sdr = pre_draw(cm)) == -1) {
		return;
	}

	if(cm->ibo_valid) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cm->ibo);
		glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, (void*)(uintptr_t)(start * 4));
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	} else {
		glDrawArrays(GL_TRIANGLES, start, count);
	}

	post_draw(cm, cur_sdr);
}

void cmesh_draw_submesh(struct cmesh *cm, int subidx)
{
	struct submesh *sm = cm->sublist;

	while(sm && subidx-- > 0) {
		sm = sm->next;
	}
	if(!sm) return;

	if(sm->icount) {
		cmesh_draw_range(cm, sm->istart, sm->icount);
	} else {
		cmesh_draw_range(cm, sm->vstart, sm->vcount);
	}
}

static void post_draw(struct cmesh *cm, int cur_sdr)
{
	int i;

	for(i=0; i<CMESH_NUM_ATTR; i++) {
		int loc = sdr_loc[i];
		if(loc >= 0 && cm->vattr[i].vbo_valid) {
			glDisableVertexAttribArray(loc);
		}
	}
}

void cmesh_draw_wire(struct cmesh *cm, float linesz)
{
	int cur_sdr, nfaces;

	if((cur_sdr = pre_draw(cm)) == -1) {
		return;
	}
	update_wire_ibo(cm);

	nfaces = cmesh_poly_count(cm);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cm->wire_ibo);
	glDrawElements(GL_LINES, nfaces * 6, GL_UNSIGNED_INT, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	post_draw(cm, cur_sdr);
}

void cmesh_draw_vertices(struct cmesh *cm, float ptsz)
{
	int cur_sdr;
	if((cur_sdr = pre_draw(cm)) == -1) {
		return;
	}

	/* TODO use billboards if needed */
	glDrawArrays(GL_POINTS, 0, cm->nverts);

	post_draw(cm, cur_sdr);
}

void cmesh_draw_normals(struct cmesh *cm, float len)
{
#ifndef GL_ES_VERSION_2_0
	int i, cur_sdr, vert_nelem, norm_nelem;
	int loc = -1;
	const float *varr, *norm;

	varr = cmesh_attrib_ro(cm, CMESH_ATTR_VERTEX);
	norm = cmesh_attrib_ro(cm, CMESH_ATTR_NORMAL);
	if(!varr || !norm) return;

	vert_nelem = cm->vattr[CMESH_ATTR_VERTEX].nelem;
	norm_nelem = cm->vattr[CMESH_ATTR_NORMAL].nelem;

	glGetIntegerv(GL_CURRENT_PROGRAM, &cur_sdr);
	if(cur_sdr && use_custom_sdr_attr) {
		if((loc = sdr_loc[CMESH_ATTR_VERTEX]) < 0) {
			return;
		}
	}

	glBegin(GL_LINES);
	for(i=0; i<cm->nverts; i++) {
		float x, y, z, endx, endy, endz;

		x = varr[i * vert_nelem];
		y = varr[i * vert_nelem + 1];
		z = varr[i * vert_nelem + 2];
		endx = x + norm[i * norm_nelem] * len;
		endy = y + norm[i * norm_nelem + 1] * len;
		endz = z + norm[i * norm_nelem + 2] * len;

		if(loc == -1) {
			glVertex3f(x, y, z);
			glVertex3f(endx, endy, endz);
		} else {
			glVertexAttrib3f(loc, x, y, z);
			glVertexAttrib3f(loc, endx, endy, endz);
		}
	}
	glEnd();
#endif	/* GL_ES_VERSION_2_0 */
}

void cmesh_draw_tangents(struct cmesh *cm, float len)
{
#ifndef GL_ES_VERSION_2_0
	int i, cur_sdr, vert_nelem, tang_nelem;
	int loc = -1;
	const float *varr, *tang;

	varr = cmesh_attrib_ro(cm, CMESH_ATTR_VERTEX);
	tang = cmesh_attrib_ro(cm, CMESH_ATTR_TANGENT);
	if(!varr || !tang) return;

	vert_nelem = cm->vattr[CMESH_ATTR_VERTEX].nelem;
	tang_nelem = cm->vattr[CMESH_ATTR_TANGENT].nelem;

	glGetIntegerv(GL_CURRENT_PROGRAM, &cur_sdr);
	if(cur_sdr && use_custom_sdr_attr) {
		if((loc = sdr_loc[CMESH_ATTR_VERTEX]) < 0) {
			return;
		}
	}

	glBegin(GL_LINES);
	for(i=0; i<cm->nverts; i++) {
		float x, y, z, endx, endy, endz;

		x = varr[i * vert_nelem];
		y = varr[i * vert_nelem + 1];
		z = varr[i * vert_nelem + 2];
		endx = x + tang[i * tang_nelem] * len;
		endy = y + tang[i * tang_nelem + 1] * len;
		endz = z + tang[i * tang_nelem + 2] * len;

		if(loc == -1) {
			glVertex3f(x, y, z);
			glVertex3f(endx, endy, endz);
		} else {
			glVertexAttrib3f(loc, x, y, z);
			glVertexAttrib3f(loc, endx, endy, endz);
		}
	}
	glEnd();
#endif	/* GL_ES_VERSION_2_0 */
}

static void update_buffers(struct cmesh *cm)
{
	int i;

	for(i=0; i<CMESH_NUM_ATTR; i++) {
		if(cmesh_has_attrib(cm, i) && !cm->vattr[i].vbo_valid) {
			glBindBuffer(GL_ARRAY_BUFFER, cm->vattr[i].vbo);
			glBufferData(GL_ARRAY_BUFFER, cm->nverts * cm->vattr[i].nelem * sizeof(float),
					cm->vattr[i].data, GL_STATIC_DRAW);
			cm->vattr[i].vbo_valid = 1;
		}
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	if(cm->idata_valid && !cm->ibo_valid) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cm->ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, cm->nfaces * 3 * sizeof(unsigned int),
				cm->idata, GL_STATIC_DRAW);
		cm->ibo_valid = 1;
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
}

static void update_wire_ibo(struct cmesh *cm)
{
	int i, num_faces;
	unsigned int *wire_idxarr, *dest;

	update_buffers(cm);

	if(cm->wire_ibo_valid) return;

	if(!cm->wire_ibo) {
		glGenBuffers(1, &cm->wire_ibo);
	}
	num_faces = cmesh_poly_count(cm);

	wire_idxarr = malloc_nf(num_faces * 6 * sizeof *wire_idxarr);
	dest = wire_idxarr;

	if(cm->ibo_valid) {
		/* we're dealing with an indexed mesh */
		const unsigned int *idxarr = cmesh_index_ro(cm);

		for(i=0; i<num_faces; i++) {
			*dest++ = idxarr[0];
			*dest++ = idxarr[1];
			*dest++ = idxarr[1];
			*dest++ = idxarr[2];
			*dest++ = idxarr[2];
			*dest++ = idxarr[0];
			idxarr += 3;
		}
	} else {
		/* not an indexed mesh */
		for(i=0; i<num_faces; i++) {
			int vidx = i * 3;
			*dest++ = vidx;
			*dest++ = vidx + 1;
			*dest++ = vidx + 1;
			*dest++ = vidx + 2;
			*dest++ = vidx + 2;
			*dest++ = vidx;
		}
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cm->wire_ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_faces * 6 * sizeof(unsigned int),
			wire_idxarr, GL_STATIC_DRAW);
	free(wire_idxarr);
	cm->wire_ibo_valid = 1;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

static void calc_aabb(struct cmesh *cm)
{
	int i, j;

	if(!cmesh_attrib_ro(cm, CMESH_ATTR_VERTEX)) {
		return;
	}

	cgm_vcons(&cm->aabb_min, FLT_MAX, FLT_MAX, FLT_MAX);
	cgm_vcons(&cm->aabb_max, -FLT_MAX, -FLT_MAX, -FLT_MAX);

	for(i=0; i<cm->nverts; i++) {
		const float *v = cmesh_attrib_at_ro(cm, CMESH_ATTR_VERTEX, i);
		for(j=0; j<3; j++) {
			if(v[j] < (&cm->aabb_min.x)[j]) {
				(&cm->aabb_min.x)[j] = v[j];
			}
			if(v[j] > (&cm->aabb_max.x)[j]) {
				(&cm->aabb_max.x)[j] = v[j];
			}
		}
	}
	cm->aabb_valid = 1;
}

void cmesh_aabbox(struct cmesh *cm, cgm_vec3 *vmin, cgm_vec3 *vmax)
{
	if(!cm->aabb_valid) {
		calc_aabb(cm);
	}
	*vmin = cm->aabb_min;
	*vmax = cm->aabb_max;
}

static void calc_bsph_noidx(struct cmesh *cm, int start, int count)
{
	int i;
	float s, dist_sq;

	if(!cmesh_attrib_ro(cm, CMESH_ATTR_VERTEX)) {
		return;
	}
	if(count <= 0) count = cm->nverts;

	cgm_vcons(&cm->bsph_center, 0, 0, 0);

	/* first find the center */
	for(i=0; i<count; i++) {
		const float *v = cmesh_attrib_at_ro(cm, CMESH_ATTR_VERTEX, i + start);
		cm->bsph_center.x += v[0];
		cm->bsph_center.y += v[1];
		cm->bsph_center.z += v[2];
	}
	s = 1.0f / (float)count;
	cm->bsph_center.x *= s;
	cm->bsph_center.y *= s;
	cm->bsph_center.z *= s;

	cm->bsph_radius = 0.0f;
	for(i=0; i<count; i++) {
		const cgm_vec3 *v = (const cgm_vec3*)cmesh_attrib_at_ro(cm, CMESH_ATTR_VERTEX, i + start);
		if((dist_sq = cgm_vdist_sq(v, &cm->bsph_center)) > cm->bsph_radius) {
			cm->bsph_radius = dist_sq;
		}
	}
	cm->bsph_radius = sqrt(cm->bsph_radius);
	cm->bsph_valid = 1;
}

static void calc_bsph_idx(struct cmesh *cm, int start, int count)
{
	int i;
	float s, dist_sq;

	if(!cmesh_attrib_ro(cm, CMESH_ATTR_VERTEX) || !cmesh_index_ro(cm)) {
		return;
	}
	if(count <= 0) count = cm->icount;

	cgm_vcons(&cm->bsph_center, 0, 0, 0);

	/* first find the center */
	for(i=0; i<count; i++) {
		int idx = cm->idata[i + start];
		const float *v = cmesh_attrib_at_ro(cm, CMESH_ATTR_VERTEX, idx);
		cm->bsph_center.x += v[0];
		cm->bsph_center.y += v[1];
		cm->bsph_center.z += v[2];
	}
	s = 1.0f / (float)count;
	cm->bsph_center.x *= s;
	cm->bsph_center.y *= s;
	cm->bsph_center.z *= s;

	cm->bsph_radius = 0.0f;
	for(i=0; i<count; i++) {
		int idx = cm->idata[i + start];
		const cgm_vec3 *v = (const cgm_vec3*)cmesh_attrib_at_ro(cm, CMESH_ATTR_VERTEX, idx);
		if((dist_sq = cgm_vdist_sq(v, &cm->bsph_center)) > cm->bsph_radius) {
			cm->bsph_radius = dist_sq;
		}
	}
	cm->bsph_radius = sqrt(cm->bsph_radius);
	cm->bsph_valid = 1;
}


float cmesh_bsphere(struct cmesh *cm, cgm_vec3 *center, float *rad)
{
	if(!cm->bsph_valid) {
		calc_bsph_noidx(cm, 0, 0);
	}
	if(center) *center = cm->bsph_center;
	if(rad) *rad = cm->bsph_radius;
	return cm->bsph_radius;
}

float cmesh_submesh_bsphere(struct cmesh *cm, int subidx, cgm_vec3 *center, float *rad)
{
	struct submesh *sm = get_submesh(cm, subidx);
	if(!sm) {
		cgm_vcons(center, 0, 0, 0);
		*rad = 0;
		return 0;
	}

	if(sm->icount) {
		calc_bsph_idx(cm, sm->istart, sm->icount);
	} else {
		calc_bsph_noidx(cm, sm->vstart, sm->vcount);
	}
	cm->bsph_valid = 0;

	if(center) *center = cm->bsph_center;
	if(rad) *rad = cm->bsph_radius;
	return cm->bsph_radius;
}

/* TODO */
void cmesh_texcoord_apply_xform(struct cmesh *cm, float *xform);
void cmesh_texcoord_gen_plane(struct cmesh *cm, cgm_vec3 *norm, cgm_vec3 *tang);
void cmesh_texcoord_gen_box(struct cmesh *cm);
void cmesh_texcoord_gen_cylinder(struct cmesh *cm);

int cmesh_dump(struct cmesh *cm, const char *fname)
{
	FILE *fp = fopen(fname, "wb");
	if(fp) {
		int res = cmesh_dump_file(cm, fp);
		fclose(fp);
		return res;
	}
	return -1;
}

int cmesh_dump_file(struct cmesh *cm, FILE *fp)
{
	static const char *label[] = { "pos", "nor", "tan", "tex", "col", "bw", "bid", "tex2" };
	static const char *elemfmt[] = { 0, " %s(%g)", " %s(%g, %g)", " %s(%g, %g, %g)", " %s(%g, %g, %g, %g)", 0 };
	int i, j;

	if(!cmesh_has_attrib(cm, CMESH_ATTR_VERTEX)) {
		return -1;
	}

	fprintf(fp, "VERTEX ATTRIBUTES\n");

	for(i=0; i<cm->nverts; i++) {
		fprintf(fp, "%5u:", i);
		for(j=0; j<CMESH_NUM_ATTR; j++) {
			if(cmesh_has_attrib(cm, j)) {
				const float *v = cmesh_attrib_at_ro(cm, j, i);
				int nelem = cm->vattr[j].nelem;
				fprintf(fp, elemfmt[nelem], label[j], v[0], nelem > 1 ? v[1] : 0.0f,
						nelem > 2 ? v[2] : 0.0f, nelem > 3 ? v[3] : 0.0f);
			}
		}
		fputc('\n', fp);
	}

	if(cmesh_indexed(cm)) {
		const unsigned int *idx = cmesh_index_ro(cm);
		int numidx = cmesh_index_count(cm);
		int numtri = numidx / 3;
		assert(numidx % 3 == 0);

		fprintf(fp, "FACES\n");

		for(i=0; i<numtri; i++) {
			fprintf(fp, "%5d: %d %d %d\n", i, idx[0], idx[1], idx[2]);
			idx += 3;
		}
	}
	return 0;
}

int cmesh_dump_obj(struct cmesh *cm, const char *fname)
{
	FILE *fp = fopen(fname, "wb");
	if(fp) {
		int res = cmesh_dump_obj_file(cm, fp, 0);
		fclose(fp);
		return res;
	}
	return -1;
}

#define HAS_VN	1
#define HAS_VT	2

int cmesh_dump_obj_file(struct cmesh *cm, FILE *fp, int voffs)
{
	static const char *fmtstr[] = {" %u", " %u//%u", " %u/%u", " %u/%u/%u"};
	int i, j, num, nelem;
	unsigned int aflags = 0;

	if(!cmesh_has_attrib(cm, CMESH_ATTR_VERTEX)) {
		return -1;
	}


	nelem = cm->vattr[CMESH_ATTR_VERTEX].nelem;
	if((num = cm->vattr[CMESH_ATTR_VERTEX].count) != cm->nverts * nelem) {
		fprintf(stderr, "vertex array size (%d) != nverts (%d)\n", num, cm->nverts);
	}
	for(i=0; i<cm->nverts; i++) {
		const float *v = cmesh_attrib_at_ro(cm, CMESH_ATTR_VERTEX, i);
		fprintf(fp, "v %f %f %f\n", v[0], nelem > 1 ? v[1] : 0.0f, nelem > 2 ? v[2] : 0.0f);
	}

	if(cmesh_has_attrib(cm, CMESH_ATTR_NORMAL)) {
		aflags |= HAS_VN;
		nelem = cm->vattr[CMESH_ATTR_NORMAL].nelem;
		if((num = cm->vattr[CMESH_ATTR_NORMAL].count) != cm->nverts * nelem) {
			fprintf(stderr, "normal array size (%d) != nverts (%d)\n", num, cm->nverts);
		}
		for(i=0; i<cm->nverts; i++) {
			const float *v = cmesh_attrib_at_ro(cm, CMESH_ATTR_NORMAL, i);
			fprintf(fp, "vn %f %f %f\n", v[0], nelem > 1 ? v[1] : 0.0f, nelem > 2 ? v[2] : 0.0f);
		}
	}

	if(cmesh_has_attrib(cm, CMESH_ATTR_TEXCOORD)) {
		aflags |= HAS_VT;
		nelem = cm->vattr[CMESH_ATTR_TEXCOORD].nelem;
		if((num = cm->vattr[CMESH_ATTR_TEXCOORD].count) != cm->nverts * nelem) {
			fprintf(stderr, "texcoord array size (%d) != nverts (%d)\n", num, cm->nverts);
		}
		for(i=0; i<cm->nverts; i++) {
			const float *v = cmesh_attrib_at_ro(cm, CMESH_ATTR_TEXCOORD, i);
			fprintf(fp, "vt %f %f\n", v[0], nelem > 1 ? v[1] : 0.0f);
		}
	}

	if(cmesh_indexed(cm)) {
		const unsigned int *idxptr = cmesh_index_ro(cm);
		int numidx = cmesh_index_count(cm);
		int numtri = numidx / 3;
		assert(numidx % 3 == 0);

		for(i=0; i<numtri; i++) {
			fputc('f', fp);
			for(j=0; j<3; j++) {
				unsigned int idx = *idxptr++ + 1 + voffs;
				fprintf(fp, fmtstr[aflags], idx, idx, idx);
			}
			fputc('\n', fp);
		}
	} else {
		int numtri = cm->nverts / 3;
		unsigned int idx = 1 + voffs;
		for(i=0; i<numtri; i++) {
			fputc('f', fp);
			for(j=0; j<3; j++) {
				fprintf(fp, fmtstr[aflags], idx, idx, idx);
				++idx;
			}
			fputc('\n', fp);
		}
	}
	return 0;
}

static void clear_mtl(struct cmesh_material *mtl)
{
	int i;
	if(!mtl) return;
	free(mtl->name);
	for(i=0; i<CMESH_NUM_TEX; i++) {
		free(mtl->tex[i].name);
	}
	*mtl = defmtl;
}

static void clone_mtl(struct cmesh_material *dest, struct cmesh_material *src)
{
	int i;
	*dest = *src;
	if(src->name) dest->name = strdup_nf(src->name);
	for(i=0; i<CMESH_NUM_TEX; i++) {
		if(src->tex[i].name) {
			dest->tex[i].name = strdup_nf(src->tex[i].name);
		}
	}
}
