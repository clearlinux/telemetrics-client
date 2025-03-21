#include <configuration.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

void test_configuration(const uint8_t *data, size_t size) {

    char content[16384];
    char config_file[] = "/tmp/config_fuzzerXXXXXX";
    static struct configuration config = { { 0 }, { 0 }, { 0 }, false, NULL };

    // Get a null ended string
    strncpy(content, (const char *)data, size);

    save_data(config_file, content);

    read_config_from_file(config_file, &config);
    for (int i = 0; i < 6; i++) {
        if (config.strValues[i] == NULL) {
            continue;
        }
    }
    unlink(config_file);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {

    test_configuration(data, size);

    return 0;
}
