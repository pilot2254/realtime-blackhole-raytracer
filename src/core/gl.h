#pragma once
// Minimal OpenGL 3.3 loader for Win32 (WGL) plus shader, render-target and uniform helpers.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#include <string>
#include <vector>

typedef char GLch;

// every GL entry point beyond OpenGL 1.1 that the app uses
#define GLX_FUNCTIONS(X)                                                                   \
    X(GLuint, glCreateShader, (GLenum))                                                   \
    X(void,   glShaderSource, (GLuint, GLsizei, const GLch* const*, const GLint*))        \
    X(void,   glCompileShader, (GLuint))                                                  \
    X(void,   glGetShaderiv, (GLuint, GLenum, GLint*))                                    \
    X(void,   glGetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLch*))                     \
    X(GLuint, glCreateProgram, (void))                                                    \
    X(void,   glAttachShader, (GLuint, GLuint))                                           \
    X(void,   glLinkProgram, (GLuint))                                                    \
    X(void,   glGetProgramiv, (GLuint, GLenum, GLint*))                                   \
    X(void,   glGetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLch*))                    \
    X(void,   glDeleteShader, (GLuint))                                                   \
    X(void,   glUseProgram, (GLuint))                                                     \
    X(GLint,  glGetUniformLocation, (GLuint, const GLch*))                                \
    X(void,   glUniform1f, (GLint, GLfloat))                                              \
    X(void,   glUniform2f, (GLint, GLfloat, GLfloat))                                     \
    X(void,   glUniform3f, (GLint, GLfloat, GLfloat, GLfloat))                            \
    X(void,   glUniform1i, (GLint, GLint))                                                \
    X(void,   glUniform4fv, (GLint, GLsizei, const GLfloat*))                             \
    X(void,   glGenVertexArrays, (GLsizei, GLuint*))                                      \
    X(void,   glBindVertexArray, (GLuint))                                                \
    X(void,   glGenFramebuffers, (GLsizei, GLuint*))                                      \
    X(void,   glDeleteFramebuffers, (GLsizei, const GLuint*))                             \
    X(void,   glBindFramebuffer, (GLenum, GLuint))                                        \
    X(void,   glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint))            \
    X(GLenum, glCheckFramebufferStatus, (GLenum))                                         \
    X(void,   glActiveTexture, (GLenum))

#define GLX_DECLARE(ret, name, args) typedef ret (WINAPI* PFN_##name) args; extern PFN_##name p_##name;
GLX_FUNCTIONS(GLX_DECLARE)
#undef GLX_DECLARE
typedef BOOL (WINAPI* PFN_wglSwapIntervalEXT)(int);
extern PFN_wglSwapIntervalEXT p_wglSwapIntervalEXT;      // optional

bool loadGL();                                          // needs a current context; false if anything is missing

constexpr GLenum K_FRAGMENT_SHADER = 0x8B30, K_VERTEX_SHADER = 0x8B31, K_COMPILE_STATUS = 0x8B81,
                 K_LINK_STATUS = 0x8B82, K_FRAMEBUFFER = 0x8D40, K_COLOR_ATTACHMENT0 = 0x8CE0,
                 K_FRAMEBUFFER_COMPLETE = 0x8CD5, K_TEXTURE0 = 0x84C0, K_RGBA16F = 0x881A,
                 K_RGBA8 = 0x8058, K_CLAMP_TO_EDGE = 0x812F, K_BGR = 0x80E0, K_BGRA = 0x80E1;

[[noreturn]] void fatal(const char* title, const std::string& msg);

GLuint compileShader(GLenum type, std::vector<const char*> src, const char* name);
GLuint makeProgram(const char* vs, std::vector<const char*> fs, const char* name);

// a texture with its framebuffer
struct Target { GLuint tex = 0, fbo = 0; int w = 0, h = 0; };
Target makeTarget(int w, int h, bool hdr = true);       // RGBA16F, or RGBA8 when !hdr
void freeTarget(Target& t);
void buildMips(std::vector<Target>& mips, int w, int h); // half-resolution chain for bloom

void bindTex(int unit, GLuint tex);
void u1f(GLuint p, const char* n, float x);
void u2f(GLuint p, const char* n, float x, float y);
void u3f(GLuint p, const char* n, float x, float y, float z);
void u1i(GLuint p, const char* n, int x);
void u4fv(GLuint p, const char* n, int count, const float* v);
