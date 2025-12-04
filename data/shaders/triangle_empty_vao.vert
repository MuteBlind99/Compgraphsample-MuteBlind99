#version 300 es
precision mediump float;

out vec3 fragColor;

// Déclaration des tableaux sans initialisation directe
const vec2 positions[3] = vec2[3](
vec2(0.0, 0.5),
vec2(0.5, -0.5),
vec2(-0.5, -0.5)
);

const vec3 colors[3] = vec3[3](
vec3(1.0, 0.0, 0.0),
vec3(0.0, 1.0, 0.0),
vec3(0.0, 0.0, 1.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
    fragColor = colors[gl_VertexID];
}