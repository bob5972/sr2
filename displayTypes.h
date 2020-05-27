/*
 * displayTypes.h --
 */

#ifndef _DISPLAY_TYPES_H
#define _DISPLAY_TYPES_H

typedef struct vec4 {
  union {
    GLfloat x;
    GLfloat r;
    GLfloat s;
  };
  union {
    GLfloat y;
    GLfloat g;
    GLfloat t;
  };
  union {
    GLfloat z;
    GLfloat b;
    GLfloat p;
  };
  union {
    GLfloat w;
    GLfloat a;
    GLfloat q;
  };
} vec4;

typedef struct vec3 {
  union {
    GLfloat x;
    GLfloat r;
    GLfloat s;
  };
  union {
    GLfloat y;
    GLfloat g;
    GLfloat t;
  };
  union {
    GLfloat z;
    GLfloat b;
    GLfloat p;
  };
} vec3;

typedef struct vec2 {
  union {
    GLfloat x;
    GLfloat r;
    GLfloat s;
  };
  union {
    GLfloat y;
    GLfloat g;
    GLfloat t;
  };
} vec2;

#endif
