#version 430 core

in vec2 textureCoordinates;

layout(location = 0) out vec4 fragmentColor;

uniform sampler2D outputTexture;

void main()
{
    fragmentColor =
        texture(outputTexture, textureCoordinates);
}