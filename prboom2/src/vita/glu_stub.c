/* No-op GLU tessellator for the software-only PS Vita build (see glu_stub.h). */

#include <stddef.h>
#include "glu_stub.h"

GLUtesselator *gluNewTess(void) { return NULL; }
void gluDeleteTess(GLUtesselator *tess) { (void) tess; }
void gluTessCallback(GLUtesselator *tess, GLenum which, _GLUfuncptr fn) { (void) tess; (void) which; (void) fn; }
void gluTessBeginPolygon(GLUtesselator *tess, GLvoid *data) { (void) tess; (void) data; }
void gluTessBeginContour(GLUtesselator *tess) { (void) tess; }
void gluTessVertex(GLUtesselator *tess, GLdouble *location, GLvoid *data) { (void) tess; (void) location; (void) data; }
void gluTessEndContour(GLUtesselator *tess) { (void) tess; }
void gluTessEndPolygon(GLUtesselator *tess) { (void) tess; }
void gluNextContour(GLUtesselator *tess, GLenum type) { (void) tess; (void) type; }
const GLubyte *gluErrorString(GLenum error) { (void) error; return (const GLubyte *) "OpenGL is not available on PS Vita"; }
