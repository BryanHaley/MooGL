/*
 * Copyright (C) The MooGL Project
 *
 * legacy_gl.h
 * MGL
 *
 * The compatibility profile calls the tests use. glcorearb.h has none of
 * them; libmoogl exports them all.
 */

#ifndef legacy_gl_h
#define legacy_gl_h

#include "mgl_legacy_enums.h"

void glBegin(GLenum mode);
void glEnd(void);
void glVertex2f(GLfloat x, GLfloat y);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void glVertex2i(GLint x, GLint y);
void glColor3f(GLfloat r, GLfloat g, GLfloat b);
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a);
void glNormal3f(GLfloat x, GLfloat y, GLfloat z);
void glTexCoord2f(GLfloat s, GLfloat t);
void glMultiTexCoord2f(GLenum unit, GLfloat s, GLfloat t);
void glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
void glMatrixMode(GLenum mode);
void glLoadIdentity(void);
void glLoadMatrixf(const GLfloat *m);
void glMultMatrixf(const GLfloat *m);
void glPushMatrix(void);
void glPopMatrix(void);
void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void glScalef(GLfloat x, GLfloat y, GLfloat z);
void glRotatef(GLfloat a, GLfloat x, GLfloat y, GLfloat z);
void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);
void glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);
void glEnableClientState(GLenum array);
void glDisableClientState(GLenum array);
void glVertexPointer(GLint size, GLenum type, GLsizei stride, const void *ptr);
void glColorPointer(GLint size, GLenum type, GLsizei stride, const void *ptr);
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *ptr);
void glNormalPointer(GLenum type, GLsizei stride, const void *ptr);
void glClientActiveTexture(GLenum unit);
void glInterleavedArrays(GLenum format, GLsizei stride, const void *pointer);
void glArrayElement(GLint i);
void glLightfv(GLenum light, GLenum pname, const GLfloat *params);
void glLightf(GLenum light, GLenum pname, GLfloat param);
void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params);
void glMaterialf(GLenum face, GLenum pname, GLfloat param);
void glLightModelfv(GLenum pname, const GLfloat *params);
void glLightModeli(GLenum pname, GLint param);
void glColorMaterial(GLenum face, GLenum mode);
void glShadeModel(GLenum mode);
void glTexEnvi(GLenum target, GLenum pname, GLint param);
void glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params);
void glTexGeni(GLenum coord, GLenum pname, GLint param);
void glTexGenfv(GLenum coord, GLenum pname, const GLfloat *params);
void glFogi(GLenum pname, GLint param);
void glFogf(GLenum pname, GLfloat param);
void glFogfv(GLenum pname, const GLfloat *params);
void glPushAttrib(GLbitfield mask);
void glPopAttrib(void);
void glPushClientAttrib(GLbitfield mask);
void glPopClientAttrib(void);
void glEdgeFlag(GLboolean flag);
void glEdgeFlagPointer(GLsizei stride, const void *ptr);
GLuint glGenLists(GLsizei range);
void glNewList(GLuint list, GLenum mode);
void glEndList(void);
void glCallList(GLuint list);
void glCallLists(GLsizei n, GLenum type, const void *lists);
void glListBase(GLuint base);
void glDeleteLists(GLuint list, GLsizei range);
GLboolean glIsList(GLuint list);
GLint glRenderMode(GLenum mode);
void glSelectBuffer(GLsizei size, GLuint *buffer);
void glInitNames(void);
void glPushName(GLuint name);
void glPopName(void);
void glLoadName(GLuint name);
void glRasterPos2i(GLint x, GLint y);
void glRasterPos2f(GLfloat x, GLfloat y);
void glRasterPos3f(GLfloat x, GLfloat y, GLfloat z);
void glWindowPos2i(GLint x, GLint y);
void glDrawPixels(GLsizei w, GLsizei h, GLenum format, GLenum type, const void *pixels);
void glBitmap(GLsizei w, GLsizei h, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap);
void glCopyPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum type);
void glPixelZoom(GLfloat x, GLfloat y);
void glAlphaFunc(GLenum func, GLfloat ref);
void glClipPlane(GLenum plane, const GLdouble *equation);

void glTextureRangeAPPLE(GLenum target, GLsizei length, const void *pointer);
void glGetTexParameterPointervAPPLE(GLenum target, GLenum pname, void **params);

void glMap1f(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points);
void glMap2f(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2,
             GLint vstride, GLint vorder, const GLfloat *points);
void glMapGrid2f(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2);
void glEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2);
void glEvalCoord2f(GLfloat u, GLfloat v);
void glGetMapiv(GLenum target, GLenum query, GLint *v);
void glGetMapfv(GLenum target, GLenum query, GLfloat *v);

typedef void *GLhandleARB;
GLhandleARB glCreateShaderObjectARB(GLenum type);
void glShaderSourceARB(GLhandleARB obj, GLsizei count, const GLchar **string, const GLint *length);
void glCompileShaderARB(GLhandleARB obj);
GLhandleARB glCreateProgramObjectARB(void);
void glAttachObjectARB(GLhandleARB container, GLhandleARB obj);
void glLinkProgramARB(GLhandleARB obj);
void glUseProgramObjectARB(GLhandleARB obj);
void glGetObjectParameterivARB(GLhandleARB obj, GLenum pname, GLint *params);
GLhandleARB glGetHandleARB(GLenum pname);
GLint glGetUniformLocationARB(GLhandleARB obj, const GLchar *name);
void glUniform4fARB(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void glDeleteObjectARB(GLhandleARB obj);
void glBindBufferARB(GLenum target, GLuint buffer);
void glGenBuffersARB(GLsizei n, GLuint *buffers);

#endif /* legacy_gl_h */
