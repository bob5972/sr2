/*
 * displayUtil.h --
 */

#include "SDL_opengl.h"

#ifndef _DISPLAY_UTIL_H
#define _DISPLAY_UTIL_H

GLuint DisplayUtil_CreateVBO(GLenum target, uint32 size, GLenum usage);
GLuint DisplayUtil_CreateProgram(GLchar *vertex, GLchar *fragment);
void DisplayUtil_DestroyProgram(GLuint program);

#endif
