#ifndef OPENGL_H_
#define OPENGL_H_

#ifdef HAVE_CONFIG_H_
#include "config.h"
#endif

#if defined(IPHONE) || defined(__IPHONE__)
#include <OpenGLES/ES2/gl.h>

#define glClearDepth	glClearDepthf

#elif defined(ANDROID) || defined(__ANDROID__)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#else

#include <GL/glew.h>

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif	/* __APPLE__ */

#endif	/* IPHONE */

#endif	/* OPENGL_H_ */
