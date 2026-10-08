/* Minimal GLU declarations for the software-only PS Vita build.
 *
 * Only the tessellator API used by gl_preprocess.c is declared. The Vita
 * build never enters OpenGL mode, so these functions are never called at
 * run time; they only have to exist so the GL renderer code links. */

#ifndef DSDA_VITA_GLU_STUB_H
#define DSDA_VITA_GLU_STUB_H

#include <SDL_opengl.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GLU_TESS_BEGIN   100100
#define GLU_TESS_VERTEX  100101
#define GLU_TESS_END     100102
#define GLU_TESS_ERROR   100103
#define GLU_TESS_COMBINE 100105
#define GLU_CW           100120

typedef struct GLUtesselator GLUtesselator;
typedef void (*_GLUfuncptr)(void);

GLUtesselator *gluNewTess(void);
void gluDeleteTess(GLUtesselator *tess);
void gluTessCallback(GLUtesselator *tess, GLenum which, _GLUfuncptr fn);
void gluTessBeginPolygon(GLUtesselator *tess, GLvoid *data);
void gluTessBeginContour(GLUtesselator *tess);
void gluTessVertex(GLUtesselator *tess, GLdouble *location, GLvoid *data);
void gluTessEndContour(GLUtesselator *tess);
void gluTessEndPolygon(GLUtesselator *tess);
void gluNextContour(GLUtesselator *tess, GLenum type);
const GLubyte *gluErrorString(GLenum error);

#ifdef __cplusplus
}
#endif

#endif
