#include <telemetry.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int test_tm_set_payload(char *payload) {
    int ret = 0;
    static uint32_t severity = 1;
    static uint32_t payload_version = 1;
    static const char *opt_class = "clearlinux/debug/fuzzing";
    struct telem_ref *t_ref = NULL;

    if ((ret = tm_create_record(&t_ref, (uint32_t)severity,
                                (char *)opt_class, payload_version)) < 0) {
        return ret;
    }
    ret = tm_set_payload(t_ref, payload);
    tm_free_record(t_ref);
    return ret;
}

void test_tm_set_config_file(char *path) {
    /*
    No need to check returning value because
    the important part is not it fails, but
    if this crashes.
    */
    tm_set_config_file(path);
}

int test_tm_set_event_id(char *full_id) {
    int ret = 0;
    char *id = NULL;
    static uint32_t severity = 1;
    static uint32_t payload_version = 1;
    static const char *opt_class = "clearlinux/debug/fuzzing";
    struct telem_ref *t_ref = NULL;

    if ((ret = tm_create_record(&t_ref, (uint32_t)severity,
                                (char *)opt_class, payload_version)) < 0) {
        return ret;
    }

    id = strndup(full_id, 32);
    ret = tm_set_event_id(t_ref, id);
    free(id);
    tm_free_record(t_ref);
    return ret;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    int ret = 0;
    char *payload = NULL;

    /*
    tm_set_payload expects a null terminated string, this
    step duplicates data into a null terminated string.
     */
    payload = strndup((char *)data, size);
    if (payload == NULL) {
        goto end;
    }

    test_tm_set_payload(payload);
    test_tm_set_config_file(payload);
    test_tm_set_event_id(payload);

    free(payload);
end:

    return ret;
}
