#if !defined (EDID_H)
#define EDID_H
#include <stdint.h>
#include <ctype.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>


typedef struct {
   char* MonitorName;
   char* SerialNumber;
   char* EISAID;
   char* PNPID;
} drm_edid;

int edid_parse(drm_edid* edid, const uint8_t* data, size_t length);
void drm_edid_destroy(drm_edid* edid);

#endif
