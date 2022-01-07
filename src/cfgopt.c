#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "assfile.h"
#include "cfgopt.h"

#ifdef NDEBUG
/* release default options */
#define DEFOPT_FULLSCR	1
#define DEFOPT_VSYNC	1
#define DEFOPT_MUSIC	1
#else
/* debug default options */
#define DEFOPT_FULLSCR	0
#define DEFOPT_VSYNC	1
#define DEFOPT_MUSIC	1
#endif

struct options opt = {
	0,	/* screen name */
	DEFOPT_FULLSCR,
	DEFOPT_VSYNC,
	DEFOPT_MUSIC
};

static const char *usagefmt = "Usage: %s [options]\n"
	"Options:\n"
	"  -music/-nomusic      toggle music playback\n"
	"  -scr,-screen <name>  ignore demoscript, run specific screen\n"
	"  -fs/-win             run fullscreen/windowed\n"
	"  -vsync/-novsync      toggle vsync\n"
	"  -h,-help             print usage and exit\n";

int parse_args(int argc, char **argv)
{
	int i;
	char *scrname = 0;

	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-') {
			if(strcmp(argv[i], "-music") == 0) {
				opt.music = 1;
			} else if(strcmp(argv[i], "-nomusic") == 0) {
				opt.music = 0;
			} else if(strcmp(argv[i], "-scr") == 0 || strcmp(argv[i], "-screen") == 0) {
				scrname = argv[++i];
			} else if(strcmp(argv[i], "-vsync") == 0) {
				opt.vsync = 1;
			} else if(strcmp(argv[i], "-novsync") == 0) {
				opt.vsync = 0;
			} else if(strcmp(argv[i], "-fs") == 0) {
				opt.fullscreen = 1;
			} else if(strcmp(argv[i], "-win") == 0) {
				opt.fullscreen = 0;
			} else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0) {
				printf(usagefmt, argv[0]);
				exit(0);
			} else {
				fprintf(stderr, "invalid option: %s\n", argv[i]);
				fprintf(stderr, usagefmt, argv[0]);
				return -1;
			}
		} else {
			if(scrname) {
				fprintf(stderr, "unexpected option: %s\n", argv[i]);
				fprintf(stderr, usagefmt, argv[0]);
				return -1;
			}
			scrname = argv[i];
		}
	}

	if(scrname) {
		opt.scrname = scrname;
	}
	return 0;
}


static char *strip_space(char *s)
{
	int len;
	char *end;

	while(*s && isspace(*s)) ++s;
	if(!*s) return 0;

	if((end = strrchr(s, '#'))) {
		--end;
	} else {
		len = strlen(s);
		end = s + len - 1;
	}

	while(end > s && isspace(*end)) *end-- = 0;
	return end > s ? s : 0;
}

static int bool_value(char *s)
{
	char *ptr = s;
	while(*ptr) {
		*ptr = tolower(*ptr);
		++ptr;
	}

	return strcmp(s, "true") == 0 || strcmp(s, "yes") == 0 || strcmp(s, "1") == 0;
}

int load_config(const char *fname)
{
	FILE *fp;
	char buf[256];
	int nline = 0;

	if(!(fp = ass_fopen(fname, "rb"))) {
		return 0;	/* just ignore missing config files */
	}

	while(ass_fgets(buf, sizeof buf, fp)) {
		char *line, *key, *value;

		++nline;
		if(!(line = strip_space(buf))) {
			continue;
		}

		if(!(value = strchr(line, '='))) {
			fprintf(stderr, "%s:%d invalid key/value pair\n", fname, nline);
			ass_fclose(fp);
			return -1;
		}
		*value++ = 0;

		if(!(key = strip_space(line)) || !(value = strip_space(value))) {
			fprintf(stderr, "%s:%d invalid key/value pair\n", fname, nline);
			ass_fclose(fp);
			return -1;
		}

		if(strcmp(line, "music") == 0) {
			opt.music = bool_value(value);
		} else if(strcmp(line, "screen") == 0) {
			opt.scrname = strdup(value);
		} else if(strcmp(line, "vsync") == 0) {
			opt.vsync = bool_value(value);
		} else if(strcmp(line, "fullscreen") == 0) {
			opt.fullscreen = bool_value(value);
		} else {
			fprintf(stderr, "%s:%d invalid option: %s\n", fname, nline, line);
			ass_fclose(fp);
			return -1;
		}
	}

	ass_fclose(fp);
	return 0;
}
