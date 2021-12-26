#include "opengl.h"

int init_opengl(void)
{
#ifdef __glew_h__
	glewInit();
#endif
	return 0;
}
