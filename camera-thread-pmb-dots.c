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
#include "dotdetector.h"
#include "dotframe.h"
#include "texture.h"
#include "mvar.h"

static const int CameraChannels = 3;
static const int CameraWidth = 1920;
static const int CameraHeight = 1080;
static const int CameraFPS = 30;

#define NUM_TEXTURES 3
GLuint CameraTexIDs[NUM_TEXTURES];

int DrawIndex = 0;

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


    for (int i = 0; i < NUM_TEXTURES; i++) {
        CameraTexIDs[i] = CreateTexture(CameraWidth, CameraHeight, CameraChannels);
    }

    const int CameraBufferSize = CameraWidth * CameraHeight * CameraChannels;
    const int TripleBufferSize = CameraBufferSize * NUM_TEXTURES; // Triple buffering

    GLuint PBO;
    glGenBuffers(1, &PBO);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
    int Flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    glNamedBufferStorage(PBO, TripleBufferSize, NULL, Flags);
    uint8_t* CameraPBOBuffer = (uint8_t*)glMapNamedBufferRange(PBO, 0, TripleBufferSize, Flags);

    // Camera setup
    mvar* CameraMVar = CreateMVar(free);
    pthread_t CameraThread;
    int ResultCode = pthread_create(&CameraThread, NULL, CameraThreadMain, CameraMVar);
    assert(!ResultCode);

    dotdetector_state* DotDetector = InitializeDotDetector();
    dot_t* Dots = malloc(MAX_DOTS * sizeof(dot_t));

    int BufferIndex = 0;
    fps FPS = MakeFPS("Display Thread");
    while (1) {

        // Update camera
        uint8_t* CameraBuffer = (uint8_t*)TryReadMVar(CameraMVar);
        if (CameraBuffer) {
            NEWTIME(UpdateTexture);
            const size_t BufferOffset = CameraBufferSize * BufferIndex;
            const uint8_t* CurrentPBOBuffer = CameraPBOBuffer + BufferOffset;
            memcpy((void*)CurrentPBOBuffer, CameraBuffer, CameraBufferSize);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, PBO);
            glTextureSubImage2D(
                CameraTexIDs[BufferIndex],
                0,
                0, 0,
                CameraWidth, CameraHeight,
                GL_RGB,
                GL_UNSIGNED_BYTE,
                (void*)BufferOffset);
            GRAPHTIME(UpdateTexture, "*");

            BufferIndex = (BufferIndex + 1) % NUM_TEXTURES;
            DrawIndex = (BufferIndex + 1) % NUM_TEXTURES;
        }

        NEWTIME(Detect);
        int NumDots = DetectDots(DotDetector,
                4, 12,
                CameraTexIDs[DrawIndex], GL_RGBA8,
                Dots);
        GRAPHTIME(Detect, ".");
        // printf("Got dots: %i\n", NumDots);
        // printf(">%i %i>\n", BufferIndex, DrawIndex);

        // Draw

        NEWTIME(Draw);
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(FullscreenQuadProgram);
        glBindVertexArray(FullscreenQuadVAO);
        glBindTexture(GL_TEXTURE_2D, CameraTexIDs[DrawIndex]);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindTexture(GL_TEXTURE_2D, 0);
        GRAPHTIME(Draw, "/");

        // NEWTIME(Swap);
        eglSwapBuffers(
            Display->DisplayDevice,
            Display->Surface);
        // GRAPHTIME(Swap, "+");
        GLCheck("Display Thread");

        // TickFPS(&FPS);
    }

    return 0;
}
