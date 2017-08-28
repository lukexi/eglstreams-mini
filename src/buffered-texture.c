#include <GL/glew.h>
#include <stdio.h>
#include <memory.h>
#include "buffered-texture.h"
#include "texture.h"

void WaitSync(GLsync Sync) {
    if (!Sync) {
        return;
    }
    while (1) {
        GLenum WaitResult = glClientWaitSync(Sync, GL_SYNC_FLUSH_COMMANDS_BIT, 1);
        if (WaitResult == GL_ALREADY_SIGNALED ||
            WaitResult == GL_CONDITION_SATISFIED) {
            return;
        }
        printf("***WAITING ON A BUFFERED TEXTURE SYNC OBJECT (tell Luke if you see this)\n");
    }
}

void LockSync(GLsync* Sync) {
    glDeleteSync(*Sync);
    *Sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}


// e.g.
buffered_texture* CreateBufferedTexture(int Width, int Height, int Channels) {
    const int TextureSizeInBytes = Width * Height * Channels;

    const int BufferCount = PMB_SIZE;
    // Total buffer size
    const int MemorySize = BufferCount * TextureSizeInBytes;

    buffered_texture* BufTex = malloc(sizeof(buffered_texture));

    GLuint PBO; // Pixel Buffer Object
    glCreateBuffers(1, &PBO);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
    int Flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    glNamedBufferStorage(PBO, MemorySize, NULL, Flags);
    void* BufferMemory = glMapNamedBufferRange(PBO, 0, MemorySize, Flags);

    BufTex->BufferMemory = (uint8_t*)BufferMemory;
    BufTex->PBO          = PBO;

    for (int Index = 0; Index < BufferCount; Index++) {
        BufTex->Textures[Index] = CreateTexture(Width, Height, Channels);
        BufTex->Syncs[Index] = NULL;
    }

    BufTex->WriteIndex = 0;
    BufTex->ReadIndex  = 0;
    BufTex->ElementSize = TextureSizeInBytes;
    BufTex->TexWidth  = Width;
    BufTex->TexHeight = Height;

    return BufTex;
}



void UploadToBufferedTexture(buffered_texture* BufTex, uint8_t* Data) {

    // Wait for the sync associated with this buffer
    WaitSync(BufTex->Syncs[BufTex->WriteIndex]);

    const size_t   MemoryOffset   = BufTex->ElementSize * BufTex->WriteIndex;

    const uint8_t* WritableMemory = BufTex->BufferMemory + MemoryOffset;

    // If we want to avoid this memcpy, we can split this function
    // here and give back a pointer to WriteableMemory to write into directly.

    // NOTE(lxi): this may in fact be worth a try; the only GL communication
    // needed from the camera thread is the WaitSync call.

    memcpy((void*)WritableMemory,
        Data,
        BufTex->ElementSize);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, BufTex->PBO);
    glTextureSubImage2D(
        BufTex->Textures[BufTex->WriteIndex],
        0,
        0, 0,
        BufTex->TexWidth, BufTex->TexHeight,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        (void*)MemoryOffset);

    // Lock the sync associated with this buffer
    // We'll wait on it next time it rolls around to
    // make sure we're not stomping on in-flight data.
    LockSync(&BufTex->Syncs[BufTex->WriteIndex]);

    // Advance the ring buffer indicies.
    // We want the draw-call (read) index to
    // always be 2 ahead of the upload (write) index,
    // with the one in the middle reserved for the GPU's current rendering.
    const int WrittenIndex = BufTex->WriteIndex;
    BufTex->WriteIndex = (WrittenIndex + 1) % PMB_SIZE;
    BufTex->ReadIndex  = (WrittenIndex + 2) % PMB_SIZE;
}

GLuint GetReadableTexture(buffered_texture* BufTex) {
    return BufTex->Textures[BufTex->ReadIndex];
}
