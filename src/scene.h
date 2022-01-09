#ifndef SCENE_H_
#define SCENE_H_

#include "cmesh.h"
#include "anim/anim.h"

struct scn_object {
	struct anm_node node;

	struct cmesh *mesh;
};

struct scn_light {
	struct anm_node node;

	cgm_vec3 color;
	float att;
};

struct scene {
	/* dynamic arrays */
	struct scn_object **objects;
	struct scn_light **lights;
};

int scn_init_scene(struct scene *scn);
void scn_destroy_scene(struct scene *scn);
struct scene *scn_alloc_scene(void);
void scn_free_scene(struct scene *scn);

int scn_init_object(struct scn_object *obj, struct cmesh *mesh);
void scn_destroy_object(struct scn_object *obj);
struct scn_object *scn_alloc_object(struct cmesh *mesh);
void scn_free_object(struct scn_object *obj);

int scn_init_light(struct scn_light *lt);
void scn_destroy_light(struct scn_light *lt);
struct scn_light *scn_alloc_light(void);
void scn_free_light(struct scn_light *lt);

void scn_add_object(struct scene *scn, struct scn_object *obj);
void scn_add_light(struct scene *scn, struct scn_light *lt);

int scn_num_objects(struct scene *scn);
int scn_num_lights(struct scene *scn);

void scn_get_light_pos(struct scn_light *lt, cgm_vec3 *pos);

#endif	/* SCENE_H_ */
