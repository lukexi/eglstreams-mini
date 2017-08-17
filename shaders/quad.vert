#version 330 core

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aUV;

out vec2 vUV;

void main(void) {
    gl_Position = vec4(aPosition, 0.0, 1.0);
    vUV = aUV;
}
