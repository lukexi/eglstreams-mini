#include <GL/glew.h>
#include "utils.h"
#include "dotframe.h"
#include "global-state.h"
#include "shader.h"
#include <math.h>

#include "dotdetector.h"





// define dot_x_tim_sort
#define SORT_NAME dot_x
#define SORT_TYPE dot_t
#define SORT_CMP(Dot1, Dot2) ((Dot1).Position.x - (Dot2).Position.x)
#include "sort.h"

static dot_t ConsolidatedDotAtIndex (dot_t* Dots, int DotsCnt, int i, int MaxRadius) {
    dot_t Nope = { 0 };

    dot_t* Dot1 = &Dots[i];
    if (Dot1->Consolidated) { return Nope; }  // already got this one

    Dot1->Consolidated = 1;
    vec2 PositionSum = Dot1->Position;
    vec2 RadiusSum = Dot1->Radius;
    float LSum = Dot1->L;
    float aSum = Dot1->a;
    float bSum = Dot1->b;
    int Count = 1;
    double MaxX = Dot1->Position.x + Dot1->Radius.x;
    double MaxY = Dot1->Position.y + Dot1->Radius.y;

    for (int j = i + 1; j < DotsCnt; j++) {
        dot_t* Dot2 = &Dots[j];
        if (Dot2->Position.x > Dot1->Position.x + Dot1->Radius.x) { break; }     // no overlap in x means we're done
        if (Dot2->Position.y > Dot1->Position.y + Dot1->Radius.y || Dot2->Position.y < Dot1->Position.y - Dot1->Radius.y) { continue; }  // no overlap in y means ignore
        if (Dot2->Consolidated) { continue; }  // already got this one

        if (Dot2->Position.x + Dot2->Radius.x > MaxX) { MaxX = Dot2->Position.x + Dot2->Radius.x; }
        if (Dot2->Position.y + Dot2->Radius.y > MaxY) { MaxY = Dot2->Position.y + Dot2->Radius.y; }

        Dot2->Consolidated = 1;
        PositionSum = vec2_plus(PositionSum, Dot2->Position);
        RadiusSum = vec2_plus(RadiusSum, Dot2->Radius);
        LSum += Dot2->L;
        aSum += Dot2->a;
        bSum += Dot2->b;
        Count++;
    }

    // if the whole run is too big, throw it out
    if (MaxX - (Dot1->Position.x - Dot1->Radius.x) > MaxRadius * 2) { return Nope; }
    if (MaxY - (Dot1->Position.y - Dot1->Radius.y) > MaxRadius * 2) { return Nope; }

    dot_t Consolidated = {
        .Valid = 1,
        .Position = vec2_times(1 / (float)Count, PositionSum),
        .Radius = vec2_times(1 / (float)Count, RadiusSum),
        .L = LSum / Count,
        .a = aSum / Count,
        .b = bSum / Count,
        .Consolidated = 1
    };
    return Consolidated;
}



dotdetector_state* InitializeDotDetector () {
    dotdetector_state* Detector = calloc(1, sizeof(dotdetector_state));

    Detector->Program = CreateComputeProgramFromPath("shaders/dotdetect.comp");

    const int ShaderBindingPoint = 1;
    Detector->ShaderBuffer = CreateShaderBuffer(sizeof(found_dots), ShaderBindingPoint);

    glGenQueries(1, &Detector->Query);

    return Detector;
}

int DetectDots (dotdetector_state* Detector,
                int MinRadius, int MaxRadius,
                GLuint CameraTexID, GLenum CameraTexFormat,
                dot_t* OutDots) {

    glUseProgram(Detector->Program);
    // glUniform1f(glGetUniformLocation(Program, "uTime"), time);

    // Set min and max radius of detectable dots
    glUniform1f(glGetUniformLocation(Detector->Program, "uMinRadius"), MinRadius);
    glUniform1f(glGetUniformLocation(Detector->Program, "uMaxRadius"), MaxRadius);

    // Set the input texture
    const GLuint InTextureUnit = 0;
    glUniform1i(glGetUniformLocation(Detector->Program, "uImageInput"), InTextureUnit);
    glBindImageTexture(InTextureUnit, CameraTexID, 0, GL_FALSE, 0, GL_READ_ONLY, CameraTexFormat);
    // const GLuint OutTextureUnit = 1;
    // glUniform1i(glGetUniformLocation(Program, "uImageOutput"), OutTextureUnit);
    // glBindImageTexture(OutTextureUnit, ComputeOutTexID, 0, GL_FALSE, 0, GL_WRITE_ONLY, ComputeOutTexFormat);

    glBeginQuery(GL_TIME_ELAPSED, Detector->Query);

    // reset the SSBO
    ShaderBufferBeginWrite(Detector->ShaderBuffer);
    const int GroupSize = 16;
    glDispatchComputeGroupSizeARB(
        // Number of groups (x,y,z)
        NextPowerOfTwo(CameraWidth)/GroupSize, NextPowerOfTwo(CameraHeight)/GroupSize, 1,
        // Group size (x,y,z)
        GroupSize,    GroupSize,    1
        );
    // Make sure writing to image has finished before read
    ShaderBufferEndWrite(Detector->ShaderBuffer);

    glEndQuery(GL_TIME_ELAPSED);

    // Extract the results of the last run of the dot detector compute shader
    const found_dots* ReadableMemory  = (found_dots*)ShaderBufferGetReadableMemory(Detector->ShaderBuffer);

    // Assemble dots for sorting
    dot_t Dots[MAX_DOTS];
    for (int i = 0; i < ReadableMemory->NumDots; i++) {
        Dots[i].Valid = 1;
        Dots[i].Position.x = ReadableMemory->Dots[i].x;
        Dots[i].Position.y = ReadableMemory->Dots[i].y;
        Dots[i].Radius.x = ReadableMemory->Dots[i].rx;
        Dots[i].Radius.y = ReadableMemory->Dots[i].ry;
        Dots[i].L = ReadableMemory->Dots[i].L;
        Dots[i].a = ReadableMemory->Dots[i].a;
        Dots[i].b = ReadableMemory->Dots[i].b;
        Dots[i].Consolidated = 0;
    }

    dot_x_tim_sort(Dots, ReadableMemory->NumDots);

    // Consolidate dots
    dot_t ConsolidatedDots[MAX_DOTS];
    int ConsolidatedDotsCount = 0;
    for (int i = 0; i < ReadableMemory->NumDots; i++) {
        dot_t Dot = ConsolidatedDotAtIndex(Dots, ReadableMemory->NumDots, i, MaxRadius);
        if (!Dot.Valid) { continue; }

        int j = ConsolidatedDotsCount;  // ensure the list is ordered by x coordinate
        while (j > 0 && Dot.Position.x < ConsolidatedDots[j - 1].Position.x) {  // while the dot's x is less than the previous dot
            ConsolidatedDots[j] = ConsolidatedDots[j - 1];     // move the previous dot to the next location (happens rarely)
            j--;
        }
        ConsolidatedDots[j] = Dot;  // insert the dot into its ordered position
        ConsolidatedDotsCount++;
    }

    float BinSize = 6 * 17;  // roughly 6in x 6in at 17dpi
    int MaxDotsInBin = 20;
    int BinsWidth = ceil((float)1920 / BinSize);
    int BinsHeight = ceil((float)1080 / BinSize);

    // DotsInBinCount will be set to MaxDotsInBin + 1 to mean "this bin is full and every dot in it knows it!"
    int* DotsInBinCountArr = calloc(BinsWidth * BinsHeight, sizeof(int));  // calloc for zero-initialization
    dot_t** DotsInBinArr = malloc(BinsWidth * BinsHeight * MaxDotsInBin * sizeof(dot_t*));

    for (int i = 0; i < ConsolidatedDotsCount; i++) {
        dot_t* Dot = &ConsolidatedDots[i];
        int BinX = floor(Dot->Position.x / BinSize);
        int BinY = floor(Dot->Position.y / BinSize);
        int BinIndex = BinX + BinY * BinsWidth;
        int DotsInBinCount = DotsInBinCountArr[BinIndex];
        if (DotsInBinCount < MaxDotsInBin) {
            // Just insert it
            DotsInBinArr[BinIndex * MaxDotsInBin + DotsInBinCount] = Dot;
            DotsInBinCountArr[BinIndex]++;
        } else {
            // We certainly want to disqualify this dot
            Dot->Valid = 0;
            // And if this is the first dot to disqualify this bin, it should tell its dots so
            if (DotsInBinCount == MaxDotsInBin) {
                for (int j = 0; j < MaxDotsInBin; j++) {
                    DotsInBinArr[BinIndex * MaxDotsInBin + j]->Valid = 0;
                }
                DotsInBinCountArr[BinIndex] = MaxDotsInBin + 1;
            }
        }
    }
    free(DotsInBinCountArr);
    free(DotsInBinArr);

    int OutDotsCount = 0;
    for (int i = 0; i < ConsolidatedDotsCount; i++) {
        dot_t* Dot = &ConsolidatedDots[i];
        if (!Dot->Valid) { continue; }
        OutDots[OutDotsCount] = *Dot;
        OutDotsCount++;
    }

    return OutDotsCount;
}

void QueryDotDetectorComputeTime(dotdetector_state* Detector) {
    // If you like, print out the timing info
    if (!glIsQuery(Detector->Query)) {
        return;
    }
    int QueryDone;
    glGetQueryObjectiv(Detector->Query,
        GL_QUERY_RESULT_AVAILABLE,
        &QueryDone);
    GLCheck("Foo");
    if (QueryDone) {
        GLuint64 elapsed_time;
        glGetQueryObjectui64v(Detector->Query, GL_QUERY_RESULT, &elapsed_time);
        printf("Dot detection took %.2f ms\n", elapsed_time / 1000000.0);
        GLCheck("Bar");
    }
}

void CleanupDotDetector (dotdetector_state* Detector) {
    glDeleteProgram(Detector->Program);
    glDeleteQueries(1, &Detector->Query);
    DeleteShaderBuffer(Detector->ShaderBuffer);
    free(Detector);
}
