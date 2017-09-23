/*
Single-threaded camera->persistent mapped pixel buffer->texture.
Renders to one display only to avoid double vsync.
This is very low latency and lovely, and serves as our target experience.
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
#include "global-state.h"

#define NUM_TEXTURES 3
GLuint CameraTexIDs[NUM_TEXTURES];

int DrawIndex = 0;


int main() {
    GetTime();

    egl_state* EGL = SetupEGL();

    egl_display* Display = &EGL->Displays[0];
    eglMakeCurrent(Display->DisplayDevice,
        Display->Surface, Display->Surface,
        Display->Context);
    // eglSwapInterval(Display->DisplayDevice, 0);
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
    uint8_t* CameraBuffer = (uint8_t*)glMapNamedBufferRange(PBO, 0, TripleBufferSize, Flags);

    // Camera setup
    int IsAsync = 1;
    camera_state* CameraState = camera_open("Video0", "/dev/video0", CameraWidth, CameraHeight, CameraFPS, IsAsync);
    if (CameraState == NULL) {
        Fatal("Couldn't find a camera : (\n");
    }

    GLCheck("Display ThreadX");


    int BufferIndex = 0;
    while (1) {
        // Update camera

        size_t BufferOffset = CameraBufferSize * BufferIndex;

        uint8_t* CurrentBuffer = CameraBuffer + BufferOffset;
        NEWTIME(Capture);
        camera_capture(CameraState, CurrentBuffer);
        GRAPHTIME(Capture, "-");

        NEWTIME(UpdateTexture);
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
        printf(">%i %i>\n", BufferIndex, DrawIndex);

        // Draw

        // printf("Drawing %s\n", DisplayName);
        // Draw a texture to the display framebuffer

        NEWTIME(Draw);
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(FullscreenQuadProgram);
        glBindVertexArray(FullscreenQuadVAO);
        glBindTexture(GL_TEXTURE_2D, CameraTexIDs[DrawIndex]);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindTexture(GL_TEXTURE_2D, 0);
        GRAPHTIME(Draw, "/");

        NEWTIME(Swap);
        eglSwapBuffers(
            Display->DisplayDevice,
            Display->Surface);
        GRAPHTIME(Swap, "+");
        GLCheck("Display Thread");
    }

    return 0;
}
