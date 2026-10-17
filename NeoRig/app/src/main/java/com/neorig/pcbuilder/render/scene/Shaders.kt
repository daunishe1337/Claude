package com.neorig.pcbuilder.render.scene

/**
 * Шейдеры OpenGL ES 3.0 (GLSL ES 3.00).
 * Лежат в общем (не Android) коде, чтобы их можно было проверить и вне устройства.
 */
object Shaders {

    /** Основной шейдер: освещение Блинна–Фонга, полусферический амбиент, RGB-точки, контровой свет. */
    const val LIT_VERTEX = """#version 300 es
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;
uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat3 uNormalMatrix;
out vec3 vWorldPos;
out vec3 vNormal;
out vec4 vColor;
void main() {
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorldPos = world.xyz;
    vNormal = uNormalMatrix * aNormal;
    vColor = aColor;
    gl_Position = uViewProj * world;
}
"""

    const val LIT_FRAGMENT = """#version 300 es
precision highp float;
in vec3 vWorldPos;
in vec3 vNormal;
in vec4 vColor;
uniform vec4 uBaseColor;
uniform vec3 uEmissive;
uniform float uMetallic;
uniform float uRoughness;
uniform float uUnlit;
uniform float uGlass;
uniform vec3 uCameraPos;
uniform vec3 uKeyDir;
uniform vec3 uKeyColor;
uniform vec3 uFillDir;
uniform vec3 uFillColor;
uniform vec3 uRimColor;
uniform vec3 uSkyColor;
uniform vec3 uGroundColor;
uniform int uPointCount;
uniform vec3 uPointPos[4];
uniform vec3 uPointColor[4];
out vec4 fragColor;

vec3 toneMap(vec3 c) {
    c = max(c, vec3(0.0));
    c = (c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14);
    return pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.2));
}

float blinn(vec3 n, vec3 l, vec3 v, float shininess) {
    vec3 h = normalize(l + v);
    return pow(max(dot(n, h), 0.0), shininess) * (shininess + 8.0) / 64.0;
}

void main() {
    vec4 base = uBaseColor * vColor;
    if (uUnlit > 0.5) {
        fragColor = vec4(toneMap(base.rgb + uEmissive), base.a);
        return;
    }
    vec3 n = normalize(vNormal);
    if (!gl_FrontFacing) n = -n;
    vec3 v = normalize(uCameraPos - vWorldPos);
    float ndv = max(dot(n, v), 0.0);
    vec3 lk = normalize(uKeyDir);

    if (uGlass > 0.5) {
        // Стекло: почти без рассеянного света, только отражения и блики, по краям плотнее (френель)
        float fresnel = pow(1.0 - ndv, 3.0);
        vec3 reflection = mix(uGroundColor, uSkyColor * 2.2, n.y * 0.5 + 0.5);
        vec3 glass = base.rgb * 0.2 + reflection * (0.15 + 0.6 * fresnel)
            + vec3(0.8, 0.86, 1.0) * blinn(n, lk, v, 320.0) * 0.7;
        float glassAlpha = clamp(base.a + 0.4 * fresnel, 0.0, 0.7);
        fragColor = vec4(toneMap(glass + uEmissive), glassAlpha);
        return;
    }

    float shininess = mix(160.0, 6.0, uRoughness);
    vec3 diffuse = base.rgb * (1.0 - 0.75 * uMetallic);
    vec3 specular = mix(vec3(0.04), base.rgb, uMetallic) * (1.0 - 0.6 * uRoughness);

    vec3 color = diffuse * mix(uGroundColor, uSkyColor, n.y * 0.5 + 0.5);
    color += specular * mix(uGroundColor, uSkyColor, 0.7) * uMetallic * 1.5;

    color += (diffuse * max(dot(n, lk), 0.0) + specular * blinn(n, lk, v, shininess)) * uKeyColor;
    vec3 lf = normalize(uFillDir);
    color += (diffuse * max(dot(n, lf), 0.0) + specular * blinn(n, lf, v, shininess)) * uFillColor;

    for (int i = 0; i < 4; i++) {
        if (i >= uPointCount) break;
        vec3 d = uPointPos[i] - vWorldPos;
        float dist2 = max(dot(d, d), 1.0);
        vec3 lp = d * inversesqrt(dist2);
        float att = 1.0 / (1.0 + dist2 * 0.0035);
        color += (diffuse * max(dot(n, lp), 0.0) + specular * blinn(n, lp, v, shininess)) * uPointColor[i] * att;
    }

    float rim = pow(1.0 - ndv, 3.0);
    color += uRimColor * rim * (0.25 + 0.5 * uMetallic);
    color += uEmissive;
    fragColor = vec4(toneMap(color), base.a);
}
"""

    /** Ореол RGB-подсветки: квадрат, повёрнутый к камере, с радиальным затуханием. */
    const val GLOW_VERTEX = """#version 300 es
layout(location = 0) in vec3 aPosition;
uniform mat4 uViewProj;
uniform vec3 uCenter;
uniform vec3 uRight;
uniform vec3 uUp;
out vec2 vUv;
void main() {
    vUv = aPosition.xy * 2.0;
    vec3 world = uCenter + uRight * aPosition.x + uUp * aPosition.y;
    gl_Position = uViewProj * vec4(world, 1.0);
}
"""

    const val GLOW_FRAGMENT = """#version 300 es
precision mediump float;
in vec2 vUv;
uniform vec3 uColor;
uniform float uIntensity;
out vec4 fragColor;
void main() {
    float d2 = dot(vUv, vUv);
    float a = exp(-d2 * 3.5) * (1.0 - smoothstep(0.55, 1.0, d2));
    fragColor = vec4(uColor * a * uIntensity, 1.0);
}
"""

    /** Фон: вертикальный градиент и мягкое неоновое пятно за сценой. */
    const val BACKGROUND_VERTEX = """#version 300 es
layout(location = 0) in vec3 aPosition;
out vec2 vUv;
void main() {
    vUv = aPosition.xy * 0.5 + 0.5;
    gl_Position = vec4(aPosition.xy, 0.9999, 1.0);
}
"""

    const val BACKGROUND_FRAGMENT = """#version 300 es
precision mediump float;
in vec2 vUv;
uniform vec3 uTop;
uniform vec3 uBottom;
uniform vec3 uGlowA;
uniform vec3 uGlowB;
uniform float uAspect;
out vec4 fragColor;
void main() {
    vec3 c = mix(uBottom, uTop, smoothstep(0.0, 1.0, vUv.y));
    vec2 p = vUv - vec2(0.38, 0.62);
    p.x *= uAspect;
    c += uGlowA * exp(-dot(p, p) * 5.0);
    vec2 q = vUv - vec2(0.75, 0.35);
    q.x *= uAspect;
    c += uGlowB * exp(-dot(q, q) * 6.0);
    fragColor = vec4(c, 1.0);
}
"""
}
