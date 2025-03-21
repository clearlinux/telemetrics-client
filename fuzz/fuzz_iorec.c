#include <configuration.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "utils.h"

void test_read_record(const uint8_t *data, size_t size) {

    char content[16384];
    char record_file[] = "/tmp/record_fuzzerXXXXXX";
    static struct configuration config = { { 0 }, { 0 }, { 0 }, false, NULL };

    // Get a null ended string
    strncpy(content, (const char *)data, size);

    save_data(record_file, content);

    // read_record(char *fullpath, char *headers[], char **body, char **cfg_file)

    unlink(record_file);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    test_read_record(data, size);
    return 0;
}
