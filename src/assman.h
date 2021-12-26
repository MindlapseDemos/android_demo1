#ifndef ASSMAN_H_
#define ASSMAN_H_

/* use these functions to load assets or return already loaded assets */
unsigned int get_tex2d(const char *fname);
unsigned int get_texcube(const char *fname);
unsigned int get_vsdr(const char *fname);
unsigned int get_psdr(const char *fname);
unsigned int get_sdrprog(const char *vfname, const char *pfname);

int add_asset(const char *name, unsigned int id);
unsigned int lookup_asset(const char *name);

#endif	/* ASSMAN_H_ */
