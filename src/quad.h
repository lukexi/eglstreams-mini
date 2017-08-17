#ifndef QUAD_H
#define QUAD_H

#include <GL/glew.h>

#define TEXTURE_WIDTH 1920
#define TEXTURE_HEIGHT 1080

// Want to allocate a few of these for each quad,
// tiny/small/medium/large/fullscreen depending on screen coverage
typedef struct {
    int Width;
    int Height;
    GLuint FB;
    GLuint FBTex;
} quad_texture;

typedef struct {
    GLuint VAO;
    GLuint VBO;
    GLuint UVBO;
    quad_texture Texture;
    int ProjectorIndex;
} mapped_quad;



mapped_quad CreateMappedQuad();
void SetMappedQuadVertices(mapped_quad Quad, float* Vertices);
void DeleteMappedQuad(mapped_quad Quad);

GLuint CreateFullscreenQuad();

#endif // QUAD_H
