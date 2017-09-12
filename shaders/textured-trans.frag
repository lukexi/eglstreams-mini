#version 330 core

uniform sampler2D uTexture;

in vec2 vUV;

out vec4 fragColor;

void main() {
    vec2 vUVInv = vUV;
    vUVInv.x = (1 - vUV.x);
    vec4 texColor = texture(uTexture, vUVInv);

    texColor.a = 0.3;

    fragColor = texColor;
}
