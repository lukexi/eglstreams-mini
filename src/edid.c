#include "edid.h"

void drm_edid_destroy(drm_edid *edid) {
   free(edid->MonitorName);
   free(edid->SerialNumber);
   free(edid->EISAID);
   free(edid->PNPID);
   free(edid);
}

static char* edid_parse_string(const uint8_t *data) {
   char *text;
   int i;
   int replaced = 0;

   /* this is always 12 bytes, but we can't guarantee it's null
    * terminated or not junk. */
   text = strndup ((const char *) data, 12);

   /* remove insane chars */
   for (i = 0; text[i] != '\0'; i++) {
       if (text[i] == '\n' ||
           text[i] == '\r')
           text[i] = '\0';
   }

   /* nothing left? */
   if (text[0] == '\0') {
       free (text);
       text = NULL;
       goto out;
   }

   /* ensure string is printable */
   for (i = 0; text[i] != '\0'; i++) {
       if (!isprint (text[i])) {
           text[i] = '-';
           replaced++;
       }
   }

   /* if the string is random junk, ignore the string */
   if (replaced > 4) {
       free (text);
       text = NULL;
       goto out;
   }
out:
   return text;
}

#define EDID_DESCRIPTOR_ALPHANUMERIC_DATA_STRING   0xfe
#define EDID_DESCRIPTOR_DISPLAY_PRODUCT_NAME       0xfc
#define EDID_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER  0xff
#define EDID_OFFSET_DATA_BLOCKS                0x36
#define EDID_OFFSET_LAST_BLOCK             0x6c
#define EDID_OFFSET_PNPID              0x08
#define EDID_OFFSET_SERIAL             0x0c

int edid_parse(drm_edid* edid, const uint8_t* data, size_t length) {
   char* tmp;
   int i;
   int rc = 0;
   uint32_t serial;

   /* check header */
   if (length < 128) {
       rc = -1;
       goto out;
   }
   if (data[0] != 0x00 || data[1] != 0xff) {
       rc = -1;
       goto out;
   }

   /* decode the PNP ID from three 5 bit words packed into 2 bytes
    * /--08--\/--09--\
    * 7654321076543210
    * |\---/\---/\---/
    * R  C1   C2   C3 */
   edid->PNPID = malloc(4);
   edid->PNPID[0] = 'A' + ((data[EDID_OFFSET_PNPID+0] & 0x7c) / 4) - 1;
   edid->PNPID[1] = 'A' + ((data[EDID_OFFSET_PNPID+0] & 0x3) * 8) + ((data[EDID_OFFSET_PNPID+1] & 0xe0) / 32) - 1;
   edid->PNPID[2] = 'A' + (data[EDID_OFFSET_PNPID+1] & 0x1f) - 1;
   edid->PNPID[3] = '\0';;

   /* maybe there isn't a ASCII serial number descriptor, so use this instead */
   serial = (int32_t) data[EDID_OFFSET_SERIAL+0];
   serial += (int32_t) data[EDID_OFFSET_SERIAL+1] * 0x100;
   serial += (int32_t) data[EDID_OFFSET_SERIAL+2] * 0x10000;
   serial += (int32_t) data[EDID_OFFSET_SERIAL+3] * 0x1000000;
   if (serial > 0) {
       edid->SerialNumber = malloc(9);
       sprintf (edid->SerialNumber, "%li", (long int) serial);
   }

   /* parse EDID data */
   for (i = EDID_OFFSET_DATA_BLOCKS;
        i <= EDID_OFFSET_LAST_BLOCK;
        i += 18) {
       /* ignore pixel clock data */
       if (data[i] != 0)
           continue;
       if (data[i+2] != 0)
           continue;

       /* any useful blocks? */
       if (data[i+3] == EDID_DESCRIPTOR_DISPLAY_PRODUCT_NAME) {
           tmp = edid_parse_string (&data[i+5]);
           if (tmp != NULL) {
               free (edid->MonitorName);
               edid->MonitorName = tmp;
           }
       } else if (data[i+3] == EDID_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER) {
           tmp = edid_parse_string (&data[i+5]);
           if (tmp != NULL) {
               free (edid->SerialNumber);
               edid->SerialNumber = tmp;
           }
       } else if (data[i+3] == EDID_DESCRIPTOR_ALPHANUMERIC_DATA_STRING) {
           tmp = edid_parse_string (&data[i+5]);
           if (tmp != NULL) {
               free (edid->EISAID);
               edid->EISAID = tmp;
           }
       }
   }
out:
   return rc;
}

