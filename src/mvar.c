#include "mvar.h"


mvar* CreateMVar(free_function Free) {
    mvar* MVar = calloc(1,sizeof(mvar));
    pthread_mutex_init(&MVar->Mutex, NULL);
    MVar->Free = Free;
    MVar->Data = NULL;
    MVar->DataWasRead = false;
    return MVar;
}

// If this returns data, you are responsible for freeing it!
void* TryReadMVar(mvar* MVar) {
    int error = pthread_mutex_trylock(&MVar->Mutex);
    if (!error) {
        void* Data = MVar->Data;
        MVar->Data = NULL;
        MVar->DataWasRead = true;
        pthread_mutex_unlock(&MVar->Mutex);
        return Data;
    }
    return NULL;
}

// This takes ownership of the data
bool TryWriteMVar(mvar* MVar, void* Data) {
    int error = pthread_mutex_trylock(&MVar->Mutex);
    if (!error) {
        // If no one ever read the data, free it!
        if (!MVar->DataWasRead && MVar->Data != NULL) {
            MVar->Free(MVar->Data);
        }
        MVar->Data = Data;
        MVar->DataWasRead = false;
        pthread_mutex_unlock(&MVar->Mutex);
        return true;
    }
    // If we can't write to the MVar, just discard the data
    MVar->Free(Data);
    return false;
}

