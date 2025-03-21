#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int save_config_file(char *config_template, const char *data) {
    FILE *tempFile = NULL;
    int fd;

    fd = mkstemp(config_template);
    if (fd == -1) {
        return 1;
    }

    tempFile = fdopen(fd, "w");
    if (tempFile == NULL) {
        close(fd);
        return 1;
    }

    fprintf(tempFile, "%s", data);
    fclose(tempFile);

    return 0;
}
