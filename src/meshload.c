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
static char *fullpath(const char *fname, const char *dirname);
static char *parse_face_vert(char *ptr, struct facevertex *fv, int numv, int numt, int numn);
static int cmp_facevert(const void *ap, const void *bp);
static void free_rbnode_key(struct rbnode *n, void *cls);
static void clear_mtl(struct cmesh_material *mtl);
static void clone_mtl(struct cmesh_material *dest, struct cmesh_material *src);


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
	int i, subidx, line_num = 0, result = -1;
	int found_quad = 0;
	ass_file *fp = 0;
	char buf[256], *dirname, *endp;
	struct vertex_pos *varr = 0;
	cgm_vec3 *narr = 0;
	cgm_vec2 *tarr = 0;
	struct rbtree *rbtree = 0;
	char *subname = 0;
	int substart = 0, subcount = 0;
	struct cmesh_material *mtllib = 0, *curmtl, defmtl;
	int num_mtl;

	curmtl = &defmtl;
	defmtl = *cmesh_material(mesh);

	if(!(fp = ass_fopen(fname, "rb"))) {
		fprintf(stderr, "load_mesh: failed to open file: %s\n", fname);
		goto err;
	}

	if(!(rbtree = rb_create(cmp_facevert))) {
		fprintf(stderr, "load_mesh: failed to create facevertex binary search tree\n");
		goto err;
	}
	rb_set_delete_func(rbtree, free_rbnode_key, 0);

	cmesh_set_name(mesh, fname);

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
				printf("adding submesh: %s (mtl: %s)\n", subname, curmtl->name);
				subidx = cmesh_submesh_count(mesh);
				cmesh_submesh(mesh, subname, substart / 3, subcount / 3);
				clone_mtl(cmesh_submesh_material(mesh, subidx), curmtl);
			}
			free(subname);
			if((subname = malloc(strlen(line)))) {
				strcpy(subname, clean_line(line + 2));
			}
			substart += subcount;
			subcount = 0;
			break;

		case 'm':
			if(memcmp(line, "mtllib", 6) == 0 && (line = clean_line(line + 6))) {
				int num;
				void *tmp = load_mtllib(line, dirname, &num);
				if(tmp) {
					free(mtllib);
					mtllib = tmp;
					num_mtl = num;
				}
			}
			break;

		case 'u':
			if(memcmp(line, "usemtl", 6) == 0 && mtllib && (line = clean_line(line + 6))) {
				for(i=0; i<num_mtl; i++) {
					if(strcmp(mtllib[i].name, line) == 0) {
						curmtl = mtllib + i;
						break;
					}
				}
			}
			break;

		default:
			break;
		}
	}

	clone_mtl(cmesh_material(mesh), curmtl);

	if(subcount > 0) {
		/* don't add the final submesh if we never found another. an obj file with a
		 * single 'o' for the whole list of faces, is a single mesh without submeshes
		 */
		if(cmesh_submesh_count(mesh) > 0) {
			printf("adding submesh: %s (mtl: %s)\n", subname, curmtl->name);
			subidx = cmesh_submesh_count(mesh);
			cmesh_submesh(mesh, subname, substart / 3, subcount / 3);
			clone_mtl(cmesh_submesh_material(mesh, subidx), curmtl);
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
	if(fp) ass_fclose(fp);
	darr_free(varr);
	darr_free(narr);
	darr_free(tarr);
	rb_free(rbtree);
	if(mtllib) {
		for(i=0; i<num_mtl; i++) {
			clear_mtl(mtllib + i);
		}
		free(mtllib);
	}
	free(subname);
	return result;
}

static struct cmesh_material *load_mtllib(const char *fname, const char *dirname, int *num_mtl)
{
	int line_num = 0;
	ass_file *fp;
	char buf[256], *pathbuf;
	struct cmesh_material *mtllib, *mtl = 0;

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

		if(memcmp(line, "newmtl", 6) == 0 && (line = clean_line(line + 6))) {
			darr_push(mtllib, 0);
			mtl = mtllib + darr_size(mtllib) - 1;
			memset(mtl, 0, sizeof *mtl);
			mtl->name = strdup_nf(line);

		} else if(memcmp(line, "Kd", 2) == 0) {
			if(mtl) sscanf(line + 3, "%f %f %f", &mtl->color.x, &mtl->color.y, &mtl->color.z);
		} else if(memcmp(line, "Ks", 2) == 0) {
			if(mtl) sscanf(line + 3, "%f %f %f", &mtl->specular.x, &mtl->specular.y, &mtl->specular.z);
		} else if(memcmp(line, "Ke", 2) == 0) {
			if(mtl) sscanf(line + 3, "%f %f %f", &mtl->emissive.x, &mtl->emissive.y, &mtl->emissive.z);
		} else if(memcmp(line, "Ns", 2) == 0) {
			if(mtl) mtl->roughness = atof(line + 2);
		} else if(memcmp(line, "Ni", 2) == 0) {
			if(mtl) mtl->ior = atof(line + 3);
		} else if(line[0] == 'd' && isspace(line[1])) {
			if(mtl) mtl->alpha = atof(line + 2);
		} else if(memcmp(line, "map_Kd", 6) == 0) {
			if(mtl && (line = clean_line(line + 6))) {
				mtl->tex[CMESH_TEX_COLOR].name = fullpath(line, dirname);
			}
		} else if(memcmp(line, "map_Ke", 6) == 0) {
			if(mtl && (line = clean_line(line + 6))) {
				mtl->tex[CMESH_TEX_LIGHT].name = fullpath(line, dirname);
			}
		} else if(memcmp(line, "map_bump", 8) == 0 || memcmp(line, "bump", 4) == 0) {
			while(*line && !isspace(*line)) line++;
			if(mtl && (line = clean_line(line))) {
				mtl->tex[CMESH_TEX_BUMP].name = fullpath(line, dirname);
			}
		} else if(memcmp(line, "refl", 4) == 0) {
			if(mtl && (line = clean_line(line))) {
				/* TODO: support -type */
				mtl->tex[CMESH_TEX_REFLECT].name = fullpath(line, dirname);
			}
		}
	}

	ass_fclose(fp);

	if(darr_empty(mtllib)) {
		darr_free(mtllib);
		return 0;
	}

	*num_mtl = darr_size(mtllib);
	mtllib = darr_finalize(mtllib);
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

static char *fullpath(const char *fname, const char *dirname)
{
	char *path;
	if(dirname) {
		path = malloc_nf(strlen(fname) + strlen(dirname) + 2);
		sprintf(path, "%s/%s", dirname, fname);
	} else {
		path = strdup_nf(fname);
	}
	return path;
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


static void clear_mtl(struct cmesh_material *mtl)
{
	int i;
	if(!mtl) return;
	free(mtl->name);
	for(i=0; i<CMESH_NUM_TEX; i++) {
		free(mtl->tex[i].name);
	}
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
