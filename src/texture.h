#ifndef TEXTURE_H
#define TEXTURE_H

#include <GL/glew.h>

int CreateTexture(int width, int height, int channels);

void UpdateTexture(GLuint tex, int width, int height, GLenum Format, const void* data);


#endif // TEXTURE_H
