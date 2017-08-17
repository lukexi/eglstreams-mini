#include "framebuffer.h"
#include "utils.h"
// Create and configure the texture to use for our framebuffer
GLuint CreateFramebufferTexture(GLenum Storage, GLsizei SizeX, GLsizei SizeY) {
    GLuint TexID;
    glGenTextures(1, &TexID);

    glBindTexture   (GL_TEXTURE_2D, TexID);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexStorage2D  (GL_TEXTURE_2D, 1, Storage, SizeX, SizeY);
    glBindTexture   (GL_TEXTURE_2D, 0);

    return TexID;
}


void AttachDepthStencilBuffer(GLsizei SizeX, GLsizei SizeY) {
    // Generate a render buffer for depth
    GLuint RenderbufferID;
    glGenRenderbuffers(1, &RenderbufferID);

    // Add a depth buffer
    glBindRenderbuffer(GL_RENDERBUFFER, RenderbufferID);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, SizeX, SizeY);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // Attach the render buffer as the depth target
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RenderbufferID);
}

// Create a flat render target with the given size and storage format
void CreateFramebuffer(GLenum Storage, GLsizei SizeX, GLsizei SizeY, GLuint *TextureIDOut, GLuint *FramebufferIDOut) {
    GLuint FBTexID = CreateFramebufferTexture(Storage, SizeX, SizeY);

    GLuint FramebufferID;
    glGenFramebuffers(1, &FramebufferID);
    glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID);

    // Attach the texture as the color buffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FBTexID, 0);

    AttachDepthStencilBuffer(SizeX, SizeY);

    GLenum Result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (Result != GL_FRAMEBUFFER_COMPLETE)  {
        Fatal("createFramebuffer: Framebuffer status incomplete");
        *TextureIDOut = 0;
        *FramebufferIDOut = 0;
        return;
    }

    // Clear the texture
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    *TextureIDOut = FBTexID;
    *FramebufferIDOut = FramebufferID;
}


