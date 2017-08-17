#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H
#include <GL/glew.h>

void CreateFramebuffer(GLenum Storage, GLsizei SizeX, GLsizei SizeY, GLuint *TextureIDOut, GLuint *FramebufferIDOut);


#endif // FRAMEBUFFER_H
