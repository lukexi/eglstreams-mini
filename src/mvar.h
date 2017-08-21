#if !defined(MVAR_H)
#define MVAR_H

#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>

typedef void(*free_function)(void*);

typedef struct {
    pthread_mutex_t Mutex;
    bool DataWasRead;
    void* Data;
    free_function Free;
} mvar;


mvar* CreateMVar(free_function Free);

// If this returns data, you are responsible for freeing it!
void* TryReadMVar(mvar* MVar);

// This takes ownership of the data, so don't free it.
// If the write fails, the data is discarded and freed.
bool TryWriteMVar(mvar* MVar, void* Data);


#endif // MVAR_H
