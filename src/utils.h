
#if !defined(UTILS_H)
#define UTILS_H


#define ARRAY_LEN(_arr) ((int)sizeof(_arr) / (int)sizeof(*_arr))
#define UNUSED(x) (void)(x)
#define RANDFLOAT ((float)rand()/(float)(RAND_MAX))
#define RANDRANGE(low,hi) (RANDFLOAT*(hi-low) + low)

#define MAX(a,b) (a > b ? a : b)
#define MIN(a,b) (a < b ? a : b)
#define CLAMP(l,h,a) (MAX(l, MIN(h, a)))

void Fatal(const char *format, ...);
float GetTime(void);

void GLCheck(const char* name);

void EnableGLDebug();

int NextPowerOfTwo(int x);

#define NEWTIME(name) float __##name##Before = GetTime();
#define ENDTIME(name) printf("Took: %.2fms\n", (GetTime() - __##name##Before) * 1000);
#define GRAPHTIME(name, sym) printf("%20s", #name); Graph(sym, (GetTime() - __##name##Before) * 1000);

void Graph(char* sym, int N);


typedef struct {
    int Frames;
    int CurrentSecond;
    char* Name;
} fps;

fps MakeFPS(char* Name);
void TickFPS(fps* FPS);

#endif /* UTILS_H */
