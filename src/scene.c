#include <stdio.h>
#include <stdlib.h>
#include "scene.h"
#include "util.h"
#include "darray.h"

int scn_init_scene(struct scene *scn)
{
	scn->objects = darr_alloc(0, sizeof *scn->objects);
	scn->lights = darr_alloc(0, sizeof *scn->lights);
	return 0;
}

void scn_destroy_scene(struct scene *scn)
{
	int i, num;

	num = darr_size(scn->objects);
	for(i=0; i<num; i++) {
		scn_free_object(scn->objects[i]);
	}
	darr_free(scn->objects);

	num = darr_size(scn->lights);
	for(i=0; i<num; i++) {
		scn_free_light(scn->lights[i]);
	}
	darr_free(scn->lights);
}

struct scene *scn_alloc_scene(void)
{
	struct scene *scn = malloc_nf(sizeof *scn);
	scn_init_scene(scn);
	return scn;
}

void scn_free_scene(struct scene *scn)
{
	scn_destroy_scene(scn);
	free(scn);
}

int scn_init_object(struct scn_object *obj, struct cmesh *mesh)
{
	if(anm_init_node(&obj->node) == -1) {
		fprintf(stderr, "failed to initialize node\n");
		abort();
	}
	obj->mesh = mesh;
	return 0;
}

void scn_destroy_object(struct scn_object *obj)
{
	anm_destroy_node(&obj->node);
	cmesh_free(obj->mesh);
}

struct scn_object *scn_alloc_object(struct cmesh *mesh)
{
	struct scn_object *obj = malloc_nf(sizeof *obj);
	scn_init_object(obj, mesh);
	return obj;
}

void scn_free_object(struct scn_object *obj)
{
	scn_destroy_object(obj);
	free(obj);
}

int scn_init_light(struct scn_light *lt)
{
	if(anm_init_node(&lt->node) == -1) {
		fprintf(stderr, "failed to initialize node\n");
		abort();
	}
	cgm_vcons(&lt->color, 1, 1, 1);
	lt->att = 1.0f;
	return 0;
}

void scn_destroy_light(struct scn_light *lt)
{
	anm_destroy_node(&lt->node);
}

struct scn_light *scn_alloc_light(void)
{
	struct scn_light *lt = malloc_nf(sizeof *lt);
	scn_init_light(lt);
	return lt;
}

void scn_free_light(struct scn_light *lt)
{
	scn_destroy_light(lt);
	free(lt);
}

void scn_add_object(struct scene *scn, struct scn_object *obj)
{
	darr_push(scn->objects, &obj);
}

void scn_add_light(struct scene *scn, struct scn_light *lt)
{
	darr_push(scn->lights, &lt);
}

int scn_num_objects(struct scene *scn)
{
	return darr_size(scn->objects);
}

int scn_num_lights(struct scene *scn)
{
	return darr_size(scn->lights);
}

void scn_get_light_pos(struct scn_light *lt, cgm_vec3 *pos)
{
	anm_get_position(&lt->node, &pos->x, 0);
}
