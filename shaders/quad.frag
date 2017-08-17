#version 410 core

uniform sampler2D uTexture;

in vec2 vUV;

out vec4 fragColor;


void main() {
    vec2 UV = vUV;
    UV.y = 1 - UV.y;
    fragColor = texture(uTexture, UV);
}

