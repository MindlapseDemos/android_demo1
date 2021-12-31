#include <stdio.h>
#include "opengl.h"

int init_opengl(void)
{
#ifdef __glew_h__
	glewInit();
#endif

	printf("GL vendor: %s\n", glGetString(GL_VENDOR));
	printf("GL renderer: %s\n", glGetString(GL_RENDERER));
	printf("GL version: %s\n", glGetString(GL_VERSION));

	return 0;
}
