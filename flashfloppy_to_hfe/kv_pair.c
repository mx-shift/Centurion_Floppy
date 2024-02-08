#include "kv_pair.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int kv_pair_from_string(char *str, struct kv_pair *dst) {
    if (str == NULL || dst == NULL) {
        return -1;
    }

    char *saveptr = NULL;
    dst->key = strtok_r(str, "=", &saveptr);
    if (dst->key == NULL) {
        return -1;
    }

    dst->value = strtok_r(NULL, "", &saveptr);
    if (dst->value == NULL) {
        dst->value = "";
    }

    return 0;
}

struct kv_pair * kv_pair_list_from_string(char *str) {
    struct kv_pair *kv_list = NULL;
    int kv_count = 0;

    char *saveptr = NULL;
    char *kv_str = strtok_r(str, ",", &saveptr);
    while (kv_str != NULL) {
        kv_list = realloc(kv_list, (kv_count + 1) * sizeof(struct kv_pair));
        if (kv_list == NULL) {
            return NULL;
        }

        int ret = kv_pair_from_string(kv_str, &kv_list[kv_count]);
        if (ret < 0) {
            return NULL;
        }

        ++kv_count;
        kv_str = strtok_r(NULL, ",", &saveptr);
    }

    kv_list = realloc(kv_list, (kv_count + 1) * sizeof(struct kv_pair));
    kv_list[kv_count].key = NULL;
    kv_list[kv_count].value = NULL;

    return kv_list;
}