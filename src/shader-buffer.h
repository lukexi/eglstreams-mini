#ifndef SHADER_BUFFER_H
#define SHADER_BUFFER_H

#include <GL/glew.h>
#include <texture-buffer.h>

typedef struct {
    GLuint SSBO;
    void* SSBOMemory;
    GLsync Syncs[PMB_SIZE];
    int WriteIndex;
    int ReadIndex;
    size_t ElementSize;
    size_t AlignedElementSize;
} shader_buffer;

shader_buffer* CreateShaderBuffer(size_t ElementSize);

// This memsets the SSBO's next writable storage to 0, but could easily take
// an element to memcpy in.
void ShaderBufferBeginWrite(shader_buffer* SSB);
void ShaderBufferEndWrite(shader_buffer* SSB);
const void* ShaderBufferGetReadableMemory(shader_buffer* SSB);

void DeleteShaderBuffer(shader_buffer* SSB);

#endif // SHADER_BUFFER_H
