#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include "assfile.h"
#include "cmesh.h"

#include "darray.h"
#include "rbtree.h"
#include "util.h"


struct vertex_pos {
	float x, y, z;
};

struct facevertex {
	int vidx, tidx, nidx;
};

static struct cmesh_material *load_mtllib(const char *fname, const char *dirname, int *num_mtl);
static char *clean_line(char *s);
static char *parse_face_vert(char *ptr, struct facevertex *fv, int numv, int numt, int numn);
static int cmp_facevert(const void *ap, const void *bp);
static void free_rbnode_key(struct rbnode *n, void *cls);

/* merge of different indices per attribute happens during face processing.
 *
 * A triplet of (vertex index/texcoord index/normal index) is used as the key
 * to search in a balanced binary search tree for vertex buffer index assigned
 * to the same triplet if it has been encountered before. That index is
 * appended to the index buffer.
 *
 * If a particular triplet has not been encountered before, a new vertex is
 * appended to the vertex buffer. The index of this new vertex is appended to
 * the index buffer, and also inserted into the tree for future searches.
 */
int cmesh_load(struct cmesh *mesh, const char *fname)
{
	int i, line_num = 0, result = -1;
	int found_quad = 0;
	ass_file *fp = 0;
	char buf[256], *dirname, *endp;
	struct vertex_pos *varr = 0;
	cgm_vec3 *narr = 0;
	cgm_vec2 *tarr = 0;
	struct rbtree *rbtree = 0;
	char *subname = 0;
	int substart = 0, subcount = 0;
	struct cmesh_material *mtllib;
	int num_mtl;

	if(!(fp = ass_fopen(fname, "rb"))) {
		fprintf(stderr, "load_mesh: failed to open file: %s\n", fname);
		goto err;
	}

	if(!(rbtree = rb_create(cmp_facevert))) {
		fprintf(stderr, "load_mesh: failed to create facevertex binary search tree\n");
		goto err;
	}
	rb_set_delete_func(rbtree, free_rbnode_key, 0);

	varr = darr_alloc(0, sizeof *varr);
	narr = darr_alloc(0, sizeof *narr);
	tarr = darr_alloc(0, sizeof *tarr);

	dirname = alloca(strlen(fname) + 1);
	strcpy(dirname, fname);
	if((endp = strrchr(dirname, '/'))) {
		*endp = 0;
	} else {
		dirname = 0;
	}

	while(ass_fgets(buf, sizeof buf, fp)) {
		char *line = clean_line(buf);
		++line_num;

		if(!*line) continue;

		if(memcmp(line, "mtllib", 6) == 0) {
			mtllib = load_mtllib(line + 7, dirname, &num_mtl);
		}

		switch(line[0]) {
		case 'v':
			if(isspace(line[1])) {
				/* vertex */
				struct vertex_pos v;
				int num;

				num = sscanf(line + 2, "%f %f %f", &v.x, &v.y, &v.z);
				if(num < 3) {
					fprintf(stderr, "%s:%d: invalid vertex definition: \"%s\"\n", fname, line_num, line);
					goto err;
				}
				darr_push(varr, &v);

			} else if(line[1] == 't' && isspace(line[2])) {
				/* texcoord */
				cgm_vec2 tc;
				if(sscanf(line + 3, "%f %f", &tc.x, &tc.y) != 2) {
					fprintf(stderr, "%s:%d: invalid texcoord definition: \"%s\"\n", fname, line_num, line);
					goto err;
				}
				tc.y = 1.0f - tc.y;
				darr_push(tarr, &tc);

			} else if(line[1] == 'n' && isspace(line[2])) {
				/* normal */
				cgm_vec3 norm;
				if(sscanf(line + 3, "%f %f %f", &norm.x, &norm.y, &norm.z) != 3) {
					fprintf(stderr, "%s:%d: invalid normal definition: \"%s\"\n", fname, line_num, line);
					goto err;
				}
				darr_push(narr, &norm);
			}
			break;

		case 'f':
			if(isspace(line[1])) {
				/* face */
				char *ptr = line + 2;
				struct facevertex fv;
				struct rbnode *node;
				int vsz = darr_size(varr);
				int tsz = darr_size(tarr);
				int nsz = darr_size(narr);

				for(i=0; i<4; i++) {
					if(!(ptr = parse_face_vert(ptr, &fv, vsz, tsz, nsz))) {
						if(i < 3 || found_quad) {
							fprintf(stderr, "%s:%d: invalid face definition: \"%s\"\n", fname, line_num, line);
							goto err;
						} else {
							break;
						}
					}

					if((node = rb_find(rbtree, &fv))) {
						unsigned int idx = (uintptr_t)node->data;
						assert(idx < cmesh_attrib_count(mesh, CMESH_ATTR_VERTEX));
						cmesh_push_index(mesh, idx);
						subcount++;	/* inc number of submesh indices, in case we have submeshes */
					} else {
						unsigned int newidx = cmesh_attrib_count(mesh, CMESH_ATTR_VERTEX);
						struct facevertex *newfv;
						struct vertex_pos *vptr = varr + fv.vidx;

						cmesh_push_attrib3f(mesh, CMESH_ATTR_VERTEX, vptr->x, vptr->y, vptr->z);
						if(fv.nidx >= 0) {
							float nx = narr[fv.nidx].x;
							float ny = narr[fv.nidx].y;
							float nz = narr[fv.nidx].z;
							cmesh_push_attrib3f(mesh, CMESH_ATTR_NORMAL, nx, ny, nz);
						}
						if(fv.tidx >= 0) {
							float tu = tarr[fv.tidx].x;
							float tv = tarr[fv.tidx].y;
							cmesh_push_attrib2f(mesh, CMESH_ATTR_TEXCOORD, tu, tv);
						}

						cmesh_push_index(mesh, newidx);
						subcount++;	/* inc number of submesh indices, in case we have submeshes */

						newfv = malloc_nf(sizeof *newfv);
						*newfv = fv;
						if(rb_insert(rbtree, newfv, (void*)(uintptr_t)newidx) == -1) {
							fprintf(stderr, "load_mesh: failed to insert facevertex to the binary search tree\n");
							goto err;
						}
					}
				}
				if(i > 3) found_quad = 1;
			}
			break;

		case 'o':
			if(subcount > 0) {
				printf("adding submesh: %s\n", subname);
				cmesh_submesh(mesh, subname, substart / 3, subcount / 3);
			}
			free(subname);
			if((subname = malloc(strlen(line)))) {
				strcpy(subname, clean_line(line + 2));
			}
			substart += subcount;
			subcount = 0;
			break;

		default:
			break;
		}
	}

	if(subcount > 0) {
		/* don't add the final submesh if we never found another. an obj file with a
		 * single 'o' for the whole list of faces, is a single mesh without submeshes
		 */
		if(cmesh_submesh_count(mesh) > 0) {
			printf("adding submesh: %s\n", subname);
			cmesh_submesh(mesh, subname, substart / 3, subcount / 3);
		} else {
			/* ... but use the 'o' name as the name of the mesh instead of the filename */
			if(subname && *subname) {
				cmesh_set_name(mesh, subname);
			}
		}
	}

	result = 0;	/* success */

	printf("loaded %s mesh: %s (%d submeshes): %d vertices, %d faces\n",
			found_quad ? "quad" : "triangle", fname, cmesh_submesh_count(mesh),
			cmesh_attrib_count(mesh, CMESH_ATTR_VERTEX), cmesh_poly_count(mesh));

err:
	if(fp) fclose(fp);
	darr_free(varr);
	darr_free(narr);
	darr_free(tarr);
	rb_free(rbtree);
	free(mtllib);
	free(subname);
	return result;
}

static struct cmesh_material *load_mtllib(const char *fname, const char *dirname, int *num_mtl)
{
	int line_num = 0;
	ass_file *fp;
	char buf[256], *pathbuf;
	struct cmesh_material *mtllib;

	if(dirname) {
		pathbuf = alloca(strlen(fname) + strlen(dirname) + 2);
		sprintf(pathbuf, "%s/%s", dirname, fname);
		fname = pathbuf;
	}

	if(!(fp = ass_fopen(fname, "rb"))) {
		fprintf(stderr, "failed to open material file: %s\n", fname);
		return 0;
	}

	mtllib = darr_alloc(0, sizeof *mtllib);

	while(ass_fgets(buf, sizeof buf, fp)) {
		char *line = clean_line(buf);
		++line_num;

		if(!line || !*line) continue;

		/* TODO */
	}

	if(darr_empty(mtllib)) {
		darr_free(mtllib);
		return 0;
	}

	*num_mtl = darr_size(mtllib);
	darr_finalize(mtllib);
	return mtllib;
}

static char *clean_line(char *s)
{
	char *end;

	while(*s && isspace(*s)) ++s;
	if(!*s) return 0;

	end = s;
	while(*end && *end != '#') ++end;
	*end-- = 0;

	while(end > s && isspace(*end)) {
		*end-- = 0;
	}

	return s;
}

static char *parse_idx(char *ptr, int *idx, int arrsz)
{
	char *endp;
	int val = strtol(ptr, &endp, 10);
	if(endp == ptr) return 0;

	if(val < 0) {	/* convert negative indices */
		*idx = arrsz + val;
	} else {
		*idx = val - 1;	/* indices in obj are 1-based */
	}
	return endp;
}

/* possible face-vertex definitions:
 * 1. vertex
 * 2. vertex/texcoord
 * 3. vertex//normal
 * 4. vertex/texcoord/normal
 */
static char *parse_face_vert(char *ptr, struct facevertex *fv, int numv, int numt, int numn)
{
	if(!(ptr = parse_idx(ptr, &fv->vidx, numv)))
		return 0;
	if(*ptr != '/') return (!*ptr || isspace(*ptr)) ? ptr : 0;

	if(*++ptr == '/') {	/* no texcoord */
		fv->tidx = -1;
		++ptr;
	} else {
		if(!(ptr = parse_idx(ptr, &fv->tidx, numt)))
			return 0;
		if(*ptr != '/') return (!*ptr || isspace(*ptr)) ? ptr : 0;
		++ptr;
	}

	if(!(ptr = parse_idx(ptr, &fv->nidx, numn)))
		return 0;
	return (!*ptr || isspace(*ptr)) ? ptr : 0;
}

static int cmp_facevert(const void *ap, const void *bp)
{
	const struct facevertex *a = ap;
	const struct facevertex *b = bp;

	if(a->vidx == b->vidx) {
		if(a->tidx == b->tidx) {
			return a->nidx - b->nidx;
		}
		return a->tidx - b->tidx;
	}
	return a->vidx - b->vidx;
}

static void free_rbnode_key(struct rbnode *n, void *cls)
{
	free(n->key);
}
