#include "core/gl.h"
#include <cstdio>

#define GLX_DEFINE(ret, name, args) PFN_##name p_##name = nullptr;
GLX_FUNCTIONS(GLX_DEFINE)
#undef GLX_DEFINE
PFN_wglSwapIntervalEXT p_wglSwapIntervalEXT = nullptr;

bool loadGL() {
    bool ok = true;
#define GLX_LOAD(ret, name, args) p_##name = (PFN_##name)wglGetProcAddress(#name); if (!p_##name) ok = false;
    GLX_FUNCTIONS(GLX_LOAD)
#undef GLX_LOAD
    p_wglSwapIntervalEXT = (PFN_wglSwapIntervalEXT)wglGetProcAddress("wglSwapIntervalEXT");
    return ok;
}

const char* g_errorLog = nullptr;

[[noreturn]] void fatal(const char* title, const std::string& msg) {
    if (g_errorLog) {
        if (FILE* f = std::fopen(g_errorLog, "w")) { std::fprintf(f, "%s\n%s\n", title, msg.c_str()); std::fclose(f); }
        ExitProcess(1);
    }
    MessageBoxA(nullptr, msg.c_str(), title, MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

GLuint compileShader(GLenum type, std::vector<const char*> src, const char* name) {
    GLuint s = p_glCreateShader(type);
    p_glShaderSource(s, (GLsizei)src.size(), src.data(), nullptr);
    p_glCompileShader(s);
    GLint ok = 0; p_glGetShaderiv(s, K_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[8192]; p_glGetShaderInfoLog(s, sizeof log, nullptr, log);
        fatal("Shader compile error", std::string(name) + ":\n" + log);
    }
    return s;
}

GLuint makeProgram(const char* vs, std::vector<const char*> fs, const char* name) {
    GLuint v = compileShader(K_VERTEX_SHADER, {vs}, "vertex");
    GLuint f = compileShader(K_FRAGMENT_SHADER, fs, name);
    GLuint p = p_glCreateProgram();
    p_glAttachShader(p, v); p_glAttachShader(p, f);
    p_glLinkProgram(p);
    GLint ok = 0; p_glGetProgramiv(p, K_LINK_STATUS, &ok);
    if (!ok) {
        char log[8192]; p_glGetProgramInfoLog(p, sizeof log, nullptr, log);
        fatal("Shader link error", std::string(name) + ":\n" + log);
    }
    p_glDeleteShader(v); p_glDeleteShader(f);
    return p;
}

Target makeTarget(int w, int h, bool hdr) {
    Target t; t.w = w; t.h = h;
    glGenTextures(1, &t.tex);
    glBindTexture(GL_TEXTURE_2D, t.tex);
    if (hdr) glTexImage2D(GL_TEXTURE_2D, 0, K_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    else     glTexImage2D(GL_TEXTURE_2D, 0, K_RGBA8,   w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, K_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, K_CLAMP_TO_EDGE);
    p_glGenFramebuffers(1, &t.fbo);
    p_glBindFramebuffer(K_FRAMEBUFFER, t.fbo);
    p_glFramebufferTexture2D(K_FRAMEBUFFER, K_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0);
    if (p_glCheckFramebufferStatus(K_FRAMEBUFFER) != K_FRAMEBUFFER_COMPLETE)
        fatal("OpenGL", "Framebuffer incomplete (out of GPU memory or RGBA16F unsupported).");
    p_glBindFramebuffer(K_FRAMEBUFFER, 0);
    return t;
}
void freeTarget(Target& t) {
    if (t.fbo) p_glDeleteFramebuffers(1, &t.fbo);
    if (t.tex) glDeleteTextures(1, &t.tex);
    t = Target{};
}
void buildMips(std::vector<Target>& mips, int w, int h) {
    for (auto& m : mips) freeTarget(m);
    mips.clear();
    w /= 2; h /= 2;
    for (int i = 0; i < 7 && w >= 4 && h >= 4; i++) { mips.push_back(makeTarget(w, h)); w /= 2; h /= 2; }
}

void bindTex(int unit, GLuint tex) { p_glActiveTexture(K_TEXTURE0 + unit); glBindTexture(GL_TEXTURE_2D, tex); }
void u1f(GLuint p, const char* n, float x)                   { p_glUniform1f(p_glGetUniformLocation(p, n), x); }
void u2f(GLuint p, const char* n, float x, float y)          { p_glUniform2f(p_glGetUniformLocation(p, n), x, y); }
void u3f(GLuint p, const char* n, float x, float y, float z) { p_glUniform3f(p_glGetUniformLocation(p, n), x, y, z); }
void u1i(GLuint p, const char* n, int x)                     { p_glUniform1i(p_glGetUniformLocation(p, n), x); }
void u4fv(GLuint p, const char* n, int count, const float* v) { p_glUniform4fv(p_glGetUniformLocation(p, n), count, v); }
