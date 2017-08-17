#include "texture.h"
#include "utils.h"

int BGRAChannelsToGL(int channels) {
    switch(channels) {
        case 4: return GL_BGRA;
        case 3: return GL_BGR;
        case 2: return GL_RG;
        case 1: return GL_RED;
        default: return -1;
    }
}

int RGBAChannelsToGL(int channels) {
    switch(channels) {
        case 4: return GL_RGBA;
        case 3: return GL_RGB;
        case 2: return GL_RG;
        case 1: return GL_RED;
        default: return -1;
    }
}

static const int RGBASwizzleMask[] = {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA};
static const int GrayscaleSwizzleMask[] = {GL_RED, GL_RED, GL_RED, GL_ONE};

int CreateTexture(int width, int height, int channels) {
    GLuint Tex;
    glGenTextures(1, &Tex);
    glBindTexture(GL_TEXTURE_2D, Tex);

    // Switching to using DSA API
    glTextureParameteri(Tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(Tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // glTextureParameterf(Tex, GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16);

    glPixelStorei(GL_UNPACK_LSB_FIRST, 0);
    glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTextureStorage2D(
        Tex,
        1,
        GL_RGBA8,
        width,
        height);


    const int* SwizzleMask = channels == 1 ? GrayscaleSwizzleMask : RGBASwizzleMask;
    glTextureParameteriv(Tex, GL_TEXTURE_SWIZZLE_RGBA, SwizzleMask);

    return Tex;
}

void UpdateTexture(GLuint Tex, int Width, int Height, GLenum Format, const void* Data) {
    const GLenum ImageType = GL_UNSIGNED_BYTE;

    // Use RGB(a) or copy single-channel images to all channels for grayscale
    const int* SwizzleMask = Format == GL_RED ? GrayscaleSwizzleMask : RGBASwizzleMask;
    glTextureParameteriv(Tex, GL_TEXTURE_SWIZZLE_RGBA, SwizzleMask);

    glTextureSubImage2D(Tex,
        0,
        0, 0,
        Width, Height,
        Format,
        ImageType,
        Data);

    glGenerateTextureMipmap(Tex);
}
