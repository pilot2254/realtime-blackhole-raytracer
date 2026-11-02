// Full-screen vertex shader, bloom, final composite and copy.
#include "render/shaders.h"

const char* VS_FULLSCREEN = R"GLSL(
#version 330 core
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

const char* FS_DOWN = R"GLSL(
#version 330 core
out vec4 o;
uniform sampler2D uSrc;
uniform vec2 uSrcTexel;
uniform vec2 uDstRes;
vec3 S(vec2 uv) { return min(texture(uSrc, uv).rgb, vec3(60.0)); }
void main() {
    vec2 uv = gl_FragCoord.xy / uDstRes;
    vec2 t  = uSrcTexel;
    vec3 a = S(uv + t * vec2(-2,  2)), b = S(uv + t * vec2(0,  2)), c = S(uv + t * vec2(2,  2));
    vec3 d = S(uv + t * vec2(-2,  0)), e = S(uv),                    f = S(uv + t * vec2(2,  0));
    vec3 g = S(uv + t * vec2(-2, -2)), h = S(uv + t * vec2(0, -2)), i = S(uv + t * vec2(2, -2));
    vec3 j = S(uv + t * vec2(-1,  1)), k = S(uv + t * vec2(1,  1));
    vec3 l = S(uv + t * vec2(-1, -1)), m = S(uv + t * vec2(1, -1));
    vec3 r = e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
    o = vec4(max(r, vec3(0.0)), 1.0);
}
)GLSL";

const char* FS_UP = R"GLSL(
#version 330 core
out vec4 o;
uniform sampler2D uSrc;
uniform vec2 uSrcTexel;
uniform vec2 uDstRes;
void main() {
    vec2 uv = gl_FragCoord.xy / uDstRes;
    vec2 t  = uSrcTexel;
    vec3 r = texture(uSrc, uv).rgb * 4.0;
    r += (texture(uSrc, uv + t * vec2(-1, 0)).rgb + texture(uSrc, uv + t * vec2(1, 0)).rgb +
          texture(uSrc, uv + t * vec2(0, -1)).rgb + texture(uSrc, uv + t * vec2(0, 1)).rgb) * 2.0;
    r += (texture(uSrc, uv + t * vec2(-1, -1)).rgb + texture(uSrc, uv + t * vec2(1, -1)).rgb +
          texture(uSrc, uv + t * vec2(-1, 1)).rgb  + texture(uSrc, uv + t * vec2(1, 1)).rgb);
    o = vec4(r / 16.0, 1.0);
}
)GLSL";

const char* FS_COMPOSITE = R"GLSL(
#version 330 core
out vec4 o;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform vec2  uRes;
uniform float uTime;
uniform float uBloomStr;
uniform float uExposure;
uniform float uVignette;
uniform float uCA;
uniform float uGrain;
uniform float uFade;
uniform sampler2D uOverlay;   // titles / captions / HUD, brightness = coverage, rows top-down
uniform float uOverlayOn;
uniform float uBar;           // letterbox bar height in pixels
uniform float uEHT;           // 0..1 blend to the Event Horizon Telescope view
uniform sampler2D uBlur;      // deep level of the bloom pyramid: a telescope-sized blur
uniform vec2  uBlurTexel;
uniform float uShake;         // 0..1 horizon-crossing camera shake

// the "afmhot" colour map of the EHT images
vec3 afmhot(float x) { return clamp(vec3(2.0 * x, 2.0 * x - 0.5, 2.0 * x - 1.0), 0.0, 1.0); }

vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
float hash(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }

void main() {
    vec2 uv  = gl_FragCoord.xy / uRes;
    vec2 cc  = uv - 0.5;
    if (uShake > 0.0) {
        // a handheld jolt: a few incommensurate wobbles, zoomed in a touch so the edges never show
        float a = 0.007 * uShake, tt = uTime;
        vec2 j = vec2(sin(tt * 47.0) + 0.6 * sin(tt * 83.0 + 1.3) + 0.4 * sin(tt * 131.0 + 2.1),
                      sin(tt * 53.0 + 0.7) + 0.6 * sin(tt * 97.0 + 2.9) + 0.4 * sin(tt * 149.0 + 0.4)) / 2.0;
        uv = 0.5 + cc * (1.0 - 2.0 * a) + j * a * vec2(uRes.y / uRes.x, 1.0);
    }
    vec2 off = cc * dot(cc, cc) * uCA;
    vec3 hdr = vec3(texture(uScene, uv + off).r, texture(uScene, uv).g, texture(uScene, uv - off).b);
    vec3 blm = texture(uBloom, uv).rgb;
    vec3 c   = mix(hdr, blm, uBloomStr) + blm * uBloomStr * 0.25;
    c = aces(c * uExposure);
    c *= 1.0 - uVignette * pow(length(cc * vec2(1.0, 1.15)) * 1.25, 2.6);
    c *= uFade;
    c = pow(c, vec3(1.0 / 2.2));
    if (uEHT > 0.0) {
        // a radio telescope the size of the Earth still only resolves a blur of the photon ring
        vec3 b = vec3(0.0);
        for (int j = -1; j <= 1; j++)
            for (int i = -1; i <= 1; i++)
                b += texture(uBlur, uv + vec2(i, j) * uBlurTexel * 1.5).rgb * ((i == 0 ? 2.0 : 1.0) * (j == 0 ? 2.0 : 1.0));
        float lum = dot(b / 16.0, vec3(0.3, 0.55, 0.15)) * uExposure;
        // the radio sky behind is dark: only the bright ring survives
        c = mix(c, afmhot(1.0 - exp(-max(lum - 0.18, 0.0) * 1.5)), uEHT);
    }
    c += (hash(gl_FragCoord.xy + fract(uTime) * 917.0) - 0.5) * (uGrain / 255.0);
    if (gl_FragCoord.y < uBar || gl_FragCoord.y > uRes.y - uBar) c = vec3(0.0);
    if (uOverlayOn > 0.5) {
        vec2 ov = vec2(uv.x, 1.0 - uv.y);
        vec2 px = vec2(uRes.y / 1080.0) / uRes;
        float sh = 0.0;
        for (int i = 1; i <= 4; i++) sh += texture(uOverlay, ov + px * vec2(-1.2, 1.2) * float(i)).g;
        c *= 1.0 - 0.6 * clamp(sh * 0.35, 0.0, 1.0);
        c = mix(c, vec3(0.97, 0.94, 0.88), texture(uOverlay, ov).g);
    }
    o = vec4(c, 1.0);
}
)GLSL";

// copy / scale / letterbox blit
const char* FS_COPY = R"GLSL(
#version 330 core
out vec4 o;
uniform sampler2D uSrc;
uniform vec2  uOffset;
uniform vec2  uRes;
uniform float uScale;
void main() {
    vec2 uv = (gl_FragCoord.xy - uOffset) / uRes;
    o = vec4(texture(uSrc, uv).rgb * uScale, 1.0);
}
)GLSL";
