#ifndef BUFFERED_TEXTURE_H
#define BUFFERED_TEXTURE_H

// One for GPU (last frame), one for upload (current frame), one for CPU (next frame)
#define PMB_SIZE 3

typedef struct {
    int WriteIndex;
    int ReadIndex;
    int ElementSize;
    uint8_t* BufferMemory;
    GLuint PBO;
    GLsync Syncs[PMB_SIZE];
    GLuint Textures[PMB_SIZE];
    int TexWidth;
    int TexHeight;
} buffered_texture;

buffered_texture* CreateBufferedTexture(int Width, int Height, int Channels);

void UploadToBufferedTexture(buffered_texture* BufTex, uint8_t* Data);

GLuint GetReadableTexture(buffered_texture* BufTex);

#endif // BUFFERED_TEXTURE_H
