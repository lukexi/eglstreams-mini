#include <memory.h>
#include <stdlib.h>
#include "shader-buffer.h"
#include "utils.h"

shader_buffer* CreateShaderBuffer(size_t ElementSize, GLuint ShaderBindingPoint) {

    shader_buffer* SSB = calloc(1, sizeof(shader_buffer));

    glCreateBuffers(1, &SSB->SSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSB->SSBO);

    // When binding the buffer with glBindBufferRange, the memory offsets must be aligned to
    // GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT (usually 256).
    GLint SSBOAlignment;
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &SSBOAlignment);

    size_t AlignedElementSize =
        (ElementSize / SSBOAlignment) * SSBOAlignment + SSBOAlignment;

    // Reserve and clear SSBO memory
    size_t MemorySize = AlignedElementSize * PMB_SIZE;
    int Flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    glNamedBufferStorage(SSB->SSBO, MemorySize, NULL, Flags);
    void* BufferMemory = glMapNamedBufferRange(SSB->SSBO, 0, MemorySize, Flags);
    memset(BufferMemory, 0, MemorySize);

    SSB->SSBOMemory         = BufferMemory;
    SSB->ElementSize        = ElementSize;
    SSB->AlignedElementSize = AlignedElementSize;
    SSB->ShaderBindingPoint = ShaderBindingPoint;

    GLCheck("CreateShaderBufferSSB");

    return SSB;
}

void DeleteShaderBuffer(shader_buffer* SSB) {
    glUnmapNamedBuffer(SSB->SSBO);
    glDeleteBuffers(1, &SSB->SSBO);
    free(SSB);
}

void ShaderBufferBeginWrite(shader_buffer* SSB) {
    const size_t WritableMemoryOffset = SSB->AlignedElementSize * SSB->WriteIndex;
    const void*  WritableMemory       = SSB->SSBOMemory         + WritableMemoryOffset;

    WaitSync(SSB->Syncs[SSB->WriteIndex]);

    memset((void*)WritableMemory, 0, SSB->ElementSize);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSB->SSBO);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER,
        SSB->ShaderBindingPoint,
        SSB->SSBO,
        WritableMemoryOffset,
        SSB->ElementSize);
}

void ShaderBufferEndWrite(shader_buffer* SSB) {
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
    LockSync(&SSB->Syncs[SSB->WriteIndex]);

    const int WrittenIndex = SSB->WriteIndex;
    SSB->WriteIndex = (WrittenIndex + 1) % PMB_SIZE;
    SSB->ReadIndex  = (WrittenIndex + 2) % PMB_SIZE;
}

const void* ShaderBufferGetReadableMemory(shader_buffer* SSB) {
    const size_t ReadableMemoryOffset = SSB->AlignedElementSize * SSB->ReadIndex;
    const void*  ReadableMemory       = SSB->SSBOMemory + ReadableMemoryOffset;
    return ReadableMemory;
}
