/*
 * pmGL.h --
 *
 *     Poor man's prototypes for OpenGL functions until we do somethimg
 *     more intelligent.
 */

#ifndef _PMGL_H_202005260638
#define _PMGL_H_202005260638

#include "SDL_opengl.h"

extern void glBindBuffer(GLenum  target,  GLuint  buffer);
extern void * glMapBuffer(GLenum  target,  GLenum  access);
extern GLboolean glUnmapBuffer(GLenum  target);
extern void glVertexAttribPointer(GLuint  index,  GLint  size,  GLenum  type,
                                  GLboolean  normalized,  GLsizei  stride,
                                  const GLvoid *  pointer);
extern void glEnableVertexAttribArray(GLuint  index);
extern void glDisableVertexAttribArray(GLuint  index);
extern void glUseProgram(GLuint  program);
extern GLint glGetAttribLocation(GLuint  program,  const GLchar * name);
extern GLint glGetUniformLocation(GLuint  program,  const GLchar * name);
extern void glUniform1i(GLint  location,  GLint  v0);
extern void glUniformMatrix4fv(GLint  location,  GLsizei  count,
                               GLboolean  transpose,  const GLfloat * value);
extern GLboolean glIsShader(GLuint  shader);

extern void glGetShaderiv(GLuint  shader,  GLenum  pname,  GLint * params);

extern void glGetProgramInfoLog(GLuint  program,  GLsizei  maxLength,
                                GLsizei * length,  GLchar * infoLog);
extern GLuint glCreateShader(GLenum  shaderType);

extern void glGetShaderInfoLog(GLuint  shader,  GLsizei  maxLength,
		               GLsizei * length,  GLchar * infoLog);

extern void glGenBuffers(GLsizei  n,  GLuint *  buffers);

extern void glBufferData(GLenum  target,  GLsizeiptr  size,
                         const GLvoid *  data,  GLenum  usage);

extern GLuint glCreateProgram(void);
extern void glDeleteProgram(GLuint  program);

extern void glShaderSource(GLuint  shader,  GLsizei  count,  
                           const GLchar ** string,  const GLint * length);

extern void glAttachShader(GLuint  program,  GLuint  shader);

extern void glGetProgramiv(GLuint program,  GLenum pname,  GLint *params);

extern void glLinkProgram(GLuint  program);

extern void glDeleteShader(GLuint  shader);

extern void glCompileShader(GLuint  shader);

#endif // _PMGL_H_202005260638
