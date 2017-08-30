#ifndef BUFFERED_TEXTURE_H
#define BUFFERED_TEXTURE_H

// One for GPU (last frame), one for upload (current frame), one for CPU (next frame)
#ifndef PMB_SIZE
#define PMB_SIZE 3
#endif

typedef struct {
    int WriteIndex;
    int ReadIndex;
    size_t ElementSize;
    uint8_t* BufferMemory;
    GLuint PBO;
    GLsync Syncs[PMB_SIZE];
    GLuint Textures[PMB_SIZE];
    int TexWidth;
    int TexHeight;
} texture_buffer;

texture_buffer* CreateBufferedTexture(int Width, int Height, int Channels);

void UploadToBufferedTexture(texture_buffer* BufTex, uint8_t* Data);

GLuint GetReadableTexture(texture_buffer* BufTex);


void WaitSync(GLsync Sync);
void LockSync(GLsync* Sync);

#endif // BUFFERED_TEXTURE_H
