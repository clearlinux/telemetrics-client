#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <sys/klog.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <signal.h>

#include "log.h"
#include "oops_parser.h"
#include "klog_scanner.h"

#define SYSLOG_ACTION_READ 2
#define SYSLOG_ACTION_SIZE_BUFFER 10
#define MAX_BUF 8192

int main(void)
{
        int log_size = 0;
        char *bufp = NULL;
        size_t buflen = 0;
        int bytes = 0;
        int loopcount = 0;

        oops_parser_init(write_oops_to_file);

        // Allocate space to write the oops file to
        if (allocate_filespace() < 0) {
                telem_log(LOG_ERR, "%s\n", strerror(errno));
                return 1;
        }

        // Signal Handling to close and unlink empty files
        if (signal (SIGINT, signal_handler) == SIG_ERR) {
                telem_log(LOG_ERR, "Error handling interrupt signal\n");
        }
        if (signal (SIGTERM, signal_handler) == SIG_ERR) {
                telem_log(LOG_ERR, "Error handling terminating signal\n");
        }

        while (1) {
                // Gets the size of the kernel ring buffer
                log_size = klogctl(SYSLOG_ACTION_SIZE_BUFFER, NULL, 0);
                if (log_size < 0) {
                        telem_log(LOG_ERR, "Cannot read size of kernel ring buffer\n");
                        return 1;
                }

                buflen = (size_t)log_size;

                if (loopcount++ > 0 && buflen > MAX_BUF) {
                        buflen = MAX_BUF;
                }

                // Gets the contents of the kernel ring buffer
                bufp = (char *)calloc(buflen, sizeof(char));

                malloc_trim(0);
                bytes = klogctl(SYSLOG_ACTION_READ, bufp, log_size);
                if (bytes < 0) {
                        telem_log(LOG_ERR, "Cannot read contents of kernel ring buffer: %s\n", strerror(errno));
                        return 1;
                }

                // Splits the buffer into separate lines with /0 terminating
                split_buf_by_line(bufp, (size_t)bytes);

                free(bufp);
        }

        return 0;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
