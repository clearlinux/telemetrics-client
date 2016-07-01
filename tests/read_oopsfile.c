#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "log.h"
#include "read_oopsfile.h"

unsigned long buflen = 0;

char *readfile(char *filepath)
{
        char *bufp = NULL;
        long ret = 0;

        buflen = 0;
        FILE *fp = fopen(filepath, "r");
        if (fp != NULL) {
                if (fseek(fp, 0L, SEEK_END)) {
                        telem_log(LOG_ERR, "error finding end of file\n");
                } else {
                        // Get buffer size
                        ret = ftell(fp);
                        if (ret == -1) {
                                telem_log(LOG_ERR, "Error reading buffer size\n");
                        } else {
                                buflen = (unsigned long)ret;
                        }
                        // Read entire file to Buffer
                        if (fseek(fp, 0L, SEEK_SET)) {
                                telem_log(LOG_ERR, "Error going back to beginning of file\n");
                        }
                        bufp = (char *)malloc(sizeof(char) * (buflen + 1));
                        if (!bufp) {
                                telem_log(LOG_ERR, "Memory Error\n");
                        }
                        if (fread(bufp, sizeof(char), buflen, fp) != buflen) {
                                telem_log(LOG_ERR, "Error reading file\n");
                        } else {
                                bufp[++buflen] = '\0';
                        }
                }
                if (fclose(fp) != 0) {
                        printf("Error closing file: %s\n", strerror(errno));
                }
                //free(bufp);
        } else {
                telem_log(LOG_ERR, "Wrong file pathname: %s", filepath);
        }

        return bufp;
}
/*
   char *getbuf()
   {

   return bufp;
   }
 */

unsigned long getbuflen()
{
        return buflen;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
