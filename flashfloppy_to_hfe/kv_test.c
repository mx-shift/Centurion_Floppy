#include "kv_pair.h"

#include <libgen.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <kv-pair>\n", basename(argv[0]));
        return -1;
    }

    struct kv_pair *kv_list = kv_pair_list_from_string(argv[1]);
    if (kv_list == NULL) {
        fprintf(stderr, "ERROR: kv_pair_list_from_string returned NULL\n");
        return -1;
    }

    for (struct kv_pair *pair = kv_list; pair != NULL && pair->key != NULL; ++pair) {
        printf("key: \"%s\"\n", pair->key);
        printf("value: \"%s\"\n", pair->value);
    }

    return 0;
}