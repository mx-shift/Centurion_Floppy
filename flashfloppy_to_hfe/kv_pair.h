#ifndef KV_PAIR_H_
#define KV_PAIR_H_

struct kv_pair
{
    const char *key;
    const char *value;
};

int kv_pair_from_string(char *str, struct kv_pair *dst);
struct kv_pair * kv_pair_list_from_string(char *str);

#endif