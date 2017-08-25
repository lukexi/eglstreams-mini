#version 330 core

uniform float uTime;
uniform sampler2D uTexture;

in vec2 vUV;

out vec4 fragColor;

void main() {
    vec2 vUVInv = vUV;
    vUVInv.x = (1 - vUV.x);
    fragColor = texture(uTexture, vUVInv);
}
