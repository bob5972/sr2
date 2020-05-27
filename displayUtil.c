#include <stdio.h>

#include "mbtypes.h"
#include "mbassert.h"
#include "pmGL.h"
#include "SDL_opengl.h"
#include "SDL_surface.h"
#include "displayUtil.h"

static void
printLog(GLuint obj)
{
  int infologLength = 0;
  int length;
    
  NOT_TESTED();
  if (glIsShader(obj)) {
    glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length);
  } else {
    glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &length);
  }
    
  char infoLog[length];
    
  if (glIsShader(obj)) {
    glGetShaderInfoLog(obj, length, &infologLength, infoLog);
  } else {
    glGetProgramInfoLog(obj, length, &infologLength, infoLog);
  }
  
  if (infologLength > 0) {
    printf("%s\n",infoLog);
  }
}

GLuint
DisplayUtil_CreateVBO(GLenum target, uint32 size, GLenum usage) {
  GLuint buffer;
  NOT_TESTED();
  glGenBuffers(1, &buffer);
  glBindBuffer(target, buffer);
  glBufferData(target, size, NULL, usage);
  glBindBuffer(target, 0);
  return buffer;
}

GLuint
DisplayUtil_CreateProgram(GLchar *vertex, GLchar *fragment) {
  GLuint program;
  GLuint vertexShader;
  GLuint fragmentShader;
  const GLchar **vertexP = (const GLchar **) &vertex; 
  const GLchar **fragmentP = (const GLchar **) &fragment;

  NOT_TESTED();
  program = glCreateProgram();
  vertexShader = glCreateShader(GL_VERTEX_SHADER);
  fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(vertexShader, 1, vertexP, NULL);
  glCompileShader(vertexShader);
  printLog(vertexShader);
  glShaderSource(fragmentShader, 1, fragmentP, NULL);
  glCompileShader(fragmentShader);
  printLog(fragmentShader);
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glLinkProgram(program);
  printLog(program);
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
  return program;
}

void
DisplayUtil_DestroyProgram(GLuint program) {
  NOT_TESTED();
  glDeleteProgram(program);
}

