#ifndef DATA_LOGGER_H_
#define DATA_LOGGER_H_

#include <stdint.h>

struct data_logger;

struct data_logger * data_logger_open(char const *path);
void data_logger_close(struct data_logger *logger);

void data_logger_event(
    struct data_logger *logger,
    uint64_t timestamp,
    int32_t phase_error
);

#endif