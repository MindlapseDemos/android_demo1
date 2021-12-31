#include <string.h>
#include "opengl.h"
#include "assman.h"
#include "sdr.h"
#include "imago2.h"
#include "assfile.h"
#include "rbtree.h"
#include "util.h"

struct asset {
	int type;
	char *name;
	unsigned int id;
};

static struct rbtree *rb;

static size_t io_read(void *buf, size_t bytes, void *uptr);
static long io_seek(long offs, int whence, void *uptr);
static void del_rbnode(struct rbnode *rb, void *cls);

int init_assman(void)
{
	if(!(rb = rb_create(RB_KEY_STRING))) {
		return -1;
	}
	rb_set_delete_func(rb, del_rbnode, 0);
	return 0;
}

void destroy_assman(void)
{
	rb_free(rb);
	rb = 0;
}

unsigned int get_tex2d(const char *fname)
{
	unsigned int id;
	struct img_pixmap pixmap;
	struct img_io io;

	if((id = lookup_asset(fname))) {
		return id;
	}

	if(!(io.uptr = ass_fopen(fname, "rb"))) {
		fprintf(stderr, "failed to open image: %s\n", fname);
		return 0;
	}
	io.read = io_read;
	io.write = 0;
	io.seek = io_seek;

	img_init(&pixmap);
	if(img_read(&pixmap, &io) == -1) {
		fprintf(stderr, "failed to read image file: %s\n", fname);
		ass_fclose(io.uptr);
		return 0;
	}
	ass_fclose(io.uptr);

	if(!(id = img_gltexture(&pixmap))) {
		fprintf(stderr, "failed to create OpenGL texture from: %s\n", fname);
	}
	img_destroy(&pixmap);

	if(id) {
		add_asset(fname, ASS_TEX, id);
	}
	return id;
}

unsigned int get_texcube(const char *fname)
{
	return 0;	/* TODO */
}

unsigned int get_vsdr(const char *fname)
{
	unsigned int sdr;
	long sz;
	char *buf;
	struct assfile *fp;

	if((sdr = lookup_asset(fname))) {
		return sdr;
	}

	if(!(fp = ass_fopen(fname, "rb"))) {
		fprintf(stderr, "failed to load vertex shader: %s\n", fname);
		return 0;
	}
	ass_fseek(fp, 0, SEEK_END);
	sz = ass_ftell(fp);
	ass_fseek(fp, 0, SEEK_SET);

	buf = malloc_nf(sz + 1);
	if(ass_fread(buf, 1, sz, fp) < sz) {
		fprintf(stderr, "failed to read vertex shader: %s\n", fname);
		free(buf);
		ass_fclose(fp);
		return 0;
	}
	buf[sz] = 0;
	ass_fclose(fp);

	printf("vertex shader %s ", fname);
	fflush(stdout);
	sdr = create_vertex_shader(buf);
	free(buf);

	if(sdr) add_asset(fname, ASS_SDR, sdr);
	return sdr;
}

unsigned int get_psdr(const char *fname)
{
	unsigned int sdr;
	long sz;
	char *buf;
	struct assfile *fp;

	if((sdr = lookup_asset(fname))) {
		return sdr;
	}

	if(!(fp = ass_fopen(fname, "rb"))) {
		fprintf(stderr, "failed to load vertex shader: %s\n", fname);
		return 0;
	}
	ass_fseek(fp, 0, SEEK_END);
	sz = ass_ftell(fp);
	ass_fseek(fp, 0, SEEK_SET);

	buf = malloc_nf(sz + 1);
	if(ass_fread(buf, 1, sz, fp) < sz) {
		fprintf(stderr, "failed to read vertex shader: %s\n", fname);
		free(buf);
		ass_fclose(fp);
		return 0;
	}
	buf[sz] = 0;
	ass_fclose(fp);

	printf("pixel shader %s ", fname);
	fflush(stdout);
	sdr = create_pixel_shader(buf);
	free(buf);

	if(sdr) add_asset(fname, ASS_SDR, sdr);
	return sdr;
}

unsigned int get_sdrprog(const char *vfname, const char *pfname)
{
	unsigned int vsdr, psdr, prog;
	char *progname;

	progname = alloca(strlen(vfname) + strlen(pfname) + 2);
	sprintf(progname, "%s,%s", vfname, pfname);

	if((prog = lookup_asset(progname))) {
		return prog;
	}

	if(!(vsdr = get_vsdr(vfname)) || !(psdr = get_psdr(pfname))) {
		return 0;
	}
	if(!(prog = create_program_link(vsdr, psdr, 0))) {
		return 0;
	}
	add_asset(progname, ASS_PROG, prog);
	return prog;
}

int add_asset(const char *name, int type, unsigned int id)
{
	struct asset *ass;

	ass = malloc_nf(sizeof *ass);
	ass->type = type;
	ass->name = strdup_nf(name);
	ass->id = id;

	if(rb_insert(rb, (char*)ass->name, ass) == -1) {
		return -1;
	}
	return 0;
}

unsigned int lookup_asset(const char *name)
{
	struct rbnode *n;

	if(!(n = rb_find(rb, (char*)name))) {
		return 0;
	}
	return ((struct asset*)n->data)->id;
}


static size_t io_read(void *buf, size_t bytes, void *uptr)
{
	return ass_fread(buf, 1, bytes, uptr);
}

static long io_seek(long offs, int whence, void *uptr)
{
	return ass_fseek(uptr, offs, whence);
}

static void del_rbnode(struct rbnode *rb, void *cls)
{
	struct asset *ass = rb->data;

	free(ass->name);

	switch(ass->type) {
	case ASS_TEX:
		glDeleteTextures(1, &ass->id);
		break;

	case ASS_SDR:
		glDeleteShader(ass->id);
		break;

	case ASS_PROG:
		glDeleteProgram(ass->id);
		break;
	}
}
