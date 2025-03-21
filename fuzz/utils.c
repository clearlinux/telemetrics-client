#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int save_data(char *filename, const char *data) {
    FILE *temp_file = NULL;
    int fd;

    fd = mkstemp(filename);
    if (fd == -1) {
        return 1;
    }

    temp_file = fdopen(fd, "w");
    if (temp_file == NULL) {
        close(fd);
        return 1;
    }

    fprintf(temp_file, "%s", data);
    fclose(temp_file);

    return 0;
}
