#include "data_logger.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

struct data_logger {
    FILE *fd;
    uint64_t timestamp_freq_hz;
};

struct data_logger * data_logger_open(char const *path) {
    if (path == NULL) return NULL;

    struct data_logger *ret = malloc(sizeof(struct data_logger));
    if (ret == NULL) return NULL;

    FILE *fd = fopen(path, "w");
    if (fd == NULL) {
        free(ret);
        return NULL;
    }

    fprintf(fd, "Timestamp,Phase Error\n");

    ret->timestamp_freq_hz = 1;
    ret->fd = fd;
    return ret;
}

void data_logger_set_timestamp_freq(struct data_logger *logger, uint64_t freq_hz) {
    logger->timestamp_freq_hz = freq_hz;
}

void data_logger_close(struct data_logger *logger) {
    fclose(logger->fd);
}

void data_logger_event(
    struct data_logger *logger,
    uint64_t timestamp,
    int32_t phase_error
) {
    fprintf(logger->fd, "%f,%"PRId32"\n",
        (double)timestamp/(double)logger->timestamp_freq_hz,
        phase_error);
}