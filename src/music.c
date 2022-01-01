#include <stdio.h>
#include <sys/stat.h>
#include "music.h"
#include "assfile.h"
#include "miniaudio/miniaudio.h"

static ma_result vopen(ma_vfs *vfs, const char *path, ma_uint32 mode, ma_vfs_file *fret);
static ma_result vclose(ma_vfs *vfs, ma_vfs_file fp);
static ma_result vread(ma_vfs *vfs, ma_vfs_file fp, void *dest, size_t sz, size_t *numread);
static ma_result vseek(ma_vfs *vfs, ma_vfs_file fp, ma_int64 offs, ma_seek_origin from);
static ma_result vtell(ma_vfs *vfs, ma_vfs_file fp, ma_int64 *pos);
static ma_result vinfo(ma_vfs *vfs, ma_vfs_file fp, ma_file_info *inf);

static ma_engine engine;
static ma_sound sound;
static ma_resource_manager resman;

static ma_vfs_callbacks vfs = {
	vopen, 0,
	vclose,
	vread, 0,
	vseek,
	vtell,
	vinfo
};

int init_music(void)
{
	unsigned int flags;
	ma_engine_config engcfg;
	ma_resource_manager_config rescfg;

	rescfg = ma_resource_manager_config_init();
	rescfg.pVFS = &vfs;
	if(ma_resource_manager_init(&rescfg, &resman) != 0) {
		fprintf(stderr, "failed to initialize miniaudio resource manager\n");
		return -1;
	}

	engcfg = ma_engine_config_init();
	engcfg.pResourceManager = &resman;

	if(ma_engine_init(&engcfg, &engine) != 0) {
		fprintf(stderr, "failed to initialize miniaudio engine\n");
		return -1;
	}

	flags = MA_SOUND_FLAG_STREAM;
	if(ma_sound_init_from_file(&engine, "data/music.ogg", flags, 0, 0, &sound) != 0) {
		fprintf(stderr, "failed to load music\n");
		return -1;
	}
	return 0;
}

void destroy_music(void)
{
	ma_sound_stop(&sound);
	ma_sound_uninit(&sound);
	ma_engine_uninit(&engine);
}

void play_music(void)
{
	ma_sound_start(&sound);
}

void stop_music(void)
{
	ma_sound_stop(&sound);
}



static ma_result vopen(ma_vfs *vfs, const char *path, ma_uint32 mode, ma_vfs_file *fret)
{
	ass_file *fp;

	if(mode != MA_OPEN_MODE_READ) return -1;

	if(!(fp = ass_fopen(path, "rb"))) {
		return -1;
	}
	*fret = fp;
	return 0;
}

static ma_result vclose(ma_vfs *vfs, ma_vfs_file fp)
{
	ass_fclose(fp);
	return 0;
}

static ma_result vread(ma_vfs *vfs, ma_vfs_file fp, void *dest, size_t sz, size_t *numread)
{
	size_t res;

	res = ass_fread(dest, 1, sz, fp);
	if(numread) *numread = res;

	if(res != sz) {
		if(res == 0) return MA_AT_END;
	}
	return MA_SUCCESS;
}

static ma_result vseek(ma_vfs *vfs, ma_vfs_file fp, ma_int64 offs, ma_seek_origin org)
{
	int from;
	switch(org) {
	case ma_seek_origin_start:
		from = SEEK_SET;
		break;
	case ma_seek_origin_current:
		from = SEEK_CUR;
		break;
	case ma_seek_origin_end:
		from = SEEK_END;
		break;
	}
	return ass_fseek(fp, offs, from) == -1 ? -1 : 0;
}

static ma_result vtell(ma_vfs *vfs, ma_vfs_file fp, ma_int64 *pos)
{
	*pos = ass_ftell(fp);
	return 0;
}

static ma_result vinfo(ma_vfs *vfs, ma_vfs_file fp, ma_file_info *inf)
{
	int fd;
	struct stat st;

#ifdef _MSC_VER
	fd = _fileno(fp);
#else
	fd = fileno(fp);
#endif

	if(fstat(fd, &st) != 0) {
		return MA_ERROR;
	}
	inf->sizeInBytes = st.st_size;
	return MA_SUCCESS;
}
