/*
This runs the camera in a thread and passes buffers over to the main thread
for copying into a persistent mapped buffer.
The memcpy takes about 1ms, but this otherwise runs well.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <GL/glew.h>
#include <math.h>
#include <pthread.h>

#include "egl.h"
#include "utils.h"

#include "camera.h"
#include "shader.h"
#include "quad.h"
#include "texture.h"
#include "mvar.h"

static const int CameraChannels = 3;
static const int CameraWidth = 1920;
static const int CameraHeight = 1080;
static const int CameraFPS = 30;


int WaitCount = 0;

void* CameraThreadMain(void* Args) {
    mvar* CameraMVar = (mvar*)Args;

    // Camera setup
    int IsAsync = 1;
    camera_state* CameraState = camera_open_any(CameraWidth, CameraHeight, CameraFPS, IsAsync);
    if (CameraState == NULL) {
        Fatal("Couldn't find a camera : (\n");
    }

    while (1) {
        uint8_t* CameraBuffer = malloc(CameraWidth * CameraHeight * CameraChannels);
        camera_capture(CameraState, CameraBuffer);
        TryWriteMVar(CameraMVar, CameraBuffer);
    }

    return NULL;
}

void WaitBuffer(GLsync Sync) {
    if (!Sync) {
        return;
    }
    while (1) {
        GLenum WaitResult = glClientWaitSync(Sync, GL_SYNC_FLUSH_COMMANDS_BIT, 1);
        if (WaitResult == GL_ALREADY_SIGNALED ||
            WaitResult == GL_CONDITION_SATISFIED) {
            return;
        }
        WaitCount++;
    }
}

void LockBuffer(GLsync* Sync) {
    glDeleteSync(*Sync);
    *Sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

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

// e.g.
buffered_texture* CreateBufferedTexture(int Width, int Height, int Channels) {
    const int TextureSizeInBytes = Width * Height * Channels;

    const int BufferCount = PMB_SIZE;
    // Total buffer size
    const int MemorySize = BufferCount * TextureSizeInBytes;

    buffered_texture* BufTex = malloc(sizeof(buffered_texture));

    GLuint PBO; // Pixel Buffer Object
    glGenBuffers(1, &PBO);
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

    WaitBuffer(BufTex->Syncs[BufTex->WriteIndex]);

    const size_t   BufferOffset   = BufTex->ElementSize * BufTex->WriteIndex;

    const uint8_t* WritableMemory = BufTex->BufferMemory + BufferOffset;

    // If we want to avoid this memcpy, we can split this function
    // here and give back a pointer to WriteableMemory to write into directly.

    // NOTE(lxi): this may in fact be worth a try; the only GL communication
    // needed from the camera thread is the WaitBuffer call.

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
        (void*)BufferOffset);

    LockBuffer(&BufTex->Syncs[BufTex->WriteIndex]);

    // Advance the ring buffer indicies.
    // We want the draw-call (read) index to
    // always be 2 ahead of the upload (write) index,
    // with the one in the middle reserved for the GPU's current rendering.
    const int WrittenIndex = BufTex->WriteIndex;
    BufTex->WriteIndex = (WrittenIndex + 1) % PMB_SIZE;
    BufTex->ReadIndex  = (WrittenIndex + 3) % PMB_SIZE;
}

GLuint GetReadableTexture(buffered_texture* BufTex) {
    return BufTex->Textures[BufTex->ReadIndex];
}

int main() {
    GetTime();

    egl_state* EGL = SetupEGL();

    egl_display* Display = &EGL->Displays[0];
    eglMakeCurrent(Display->DisplayDevice,
        Display->Surface, Display->Surface,
        Display->Context);
    eglSwapInterval(Display->DisplayDevice, 0);
    glViewport(0, 0, (GLint)Display->Width, (GLint)Display->Height);

    GLuint FullscreenQuadProgram = CreateVertFragProgramFromPath(
        "shaders/basic.vert",
        "shaders/textured.frag"
        );
    GLuint FullscreenQuadVAO = CreateFullscreenQuad();


    buffered_texture* CameraBufTex = CreateBufferedTexture(CameraWidth, CameraHeight, CameraChannels);

    // Camera setup
    mvar* CameraMVar = CreateMVar(free);
    pthread_t CameraThread;
    int ResultCode = pthread_create(&CameraThread, NULL, CameraThreadMain, CameraMVar);
    assert(!ResultCode);

    while (1) {

        // Update camera
        uint8_t* NewCameraBuffer = (uint8_t*)TryReadMVar(CameraMVar);
        if (NewCameraBuffer) {

            NEWTIME(UpdateTexture);

            UploadToBufferedTexture(CameraBufTex, NewCameraBuffer);
            free(NewCameraBuffer);

            GRAPHTIME(UpdateTexture, "*");
            // printf(">%i %i>\n", BufferIndex, DrawIndex);
        }

        // Draw
        NEWTIME(Draw);
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(FullscreenQuadProgram);
        glBindVertexArray(FullscreenQuadVAO);
        glBindTexture(GL_TEXTURE_2D, GetReadableTexture(CameraBufTex));
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindTexture(GL_TEXTURE_2D, 0);
        GRAPHTIME(Draw, "/");

        NEWTIME(Swap);
        eglSwapBuffers(
            Display->DisplayDevice,
            Display->Surface);
        GRAPHTIME(Swap, "+");
        GLCheck("Display Thread");
        if (WaitCount) printf("Have waited %i times\n", WaitCount);
    }

    return 0;
}
