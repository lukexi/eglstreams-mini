#version 330 core

in vec2 vUV;

out vec4 fragColor;

void main() {
    fragColor = vec4(vUV.x,vUV.y,1,1);
}
