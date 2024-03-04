#include <endian.h>
#include <errno.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BC_BUF_SIZE_BYTES (2 * 1024 * 1024)

#include "algorithm.h"
#include "kv_pair.h"

#include "algorithm_bitcell_width_pi_v1.h"
#include "algorithm_bitcell_width_pi_v2.h"
#include "algorithm_fdc9216.h"
#include "algorithm_flashfloppy_v341.h"
#include "algorithm_flashfloppy_master.h"
#include "algorithm_greaseweazle_default_pll.h"
#include "algorithm_greaseweazle_fallback_pll.h"

static struct algorithm *ALGS[] = {
    &algorithm_bitcell_width_pi_v1,
    &algorithm_bitcell_width_pi_v2,
    &algorithm_fdc9216,
    &algorithm_flashfloppy_v341,
    &algorithm_flashfloppy_master,
    &algorithm_greaseweazle_default_pll,
    &algorithm_greaseweazle_fallback_pll,
    NULL
};

void usage(const char *const progname)
{
    fprintf(stderr, "Usage: %s <ff_samples> <out-dir> <hfe-bit-rate-kbps> <algorithm>\n", progname);
    fprintf(stderr, "\n");
    fprintf(stderr, "Algorithms:\n");

    for (struct algorithm **alg = ALGS; *alg != NULL; ++alg)
    {
        fprintf(stderr, "\t* %s\n", (*alg)->name);
        for (const struct parameter* param = (*alg)->params; param != NULL && param->name != NULL; param++) {
            fprintf(stderr, "\t\t%-20s    %8s    %s\n",
                param->name, (param->required != 0 ? "required" : ""), param->description);
        }
    }

    exit(1);
}

int main(int argc, const char *const argv[])
{
    if (argc < 4)
    {
        usage(argv[0]);
    }

    const char *const ff_sample_path = argv[1];
    const char *const out_dir = argv[2];
    char *endptr = NULL;
    unsigned long hfe_bit_rate_kbps = strtoul(argv[3], &endptr, 10);
    char * algorithm = strdup(argv[4]);

    char * file_prefix = basename(strdup(ff_sample_path));
    char * suffix = strrchr(file_prefix, '.');
    if (suffix != NULL && strcmp(suffix, ".ff_samples") == 0) {
        *suffix = '\0';
    }

    char *hfe_path;
    asprintf(&hfe_path, "%s/%s.%ld_%s.hfe", out_dir, file_prefix, hfe_bit_rate_kbps, algorithm);

    char *data_log_path;
    asprintf(&data_log_path, "%s/%s.%ld_%s.csv", out_dir, file_prefix, hfe_bit_rate_kbps, algorithm);

    if (*endptr != '\0') {
        fprintf(stderr, "ERROR: hfe-bit-rate-kbps must be a positive integer\n");
        return 1;
    }

    // Open sample input file
    FILE *ff_sample_fd = fopen(ff_sample_path, "rb");
    if (ff_sample_fd == NULL)
    {
        fprintf(stderr, "ERROR: Unable to open ff samples file: %s\n", ff_sample_path);
        return 1;
    }

    // Figure out how big the sample file is.
    if (fseek(ff_sample_fd, 0, SEEK_END) < 0)
    {
        fprintf(stderr, "ERROR: failed to seek to end of ff sample file: %s\n", strerror(errno));
        return 1;
    }
    long ff_sample_size = ftell(ff_sample_fd);
    if (ff_sample_size < 0)
    {
        fprintf(stderr, "ERROR: unable to read current ff sample file position: %s\n", strerror(errno));
        return 1;
    }
    rewind(ff_sample_fd);

    // Allocate memory for samples
    size_t ff_sample_count = ff_sample_size / sizeof(uint16_t);
    uint16_t *ff_samples = calloc(ff_sample_count, sizeof(uint16_t));
    if (ff_samples == NULL)
    {
        fprintf(stderr, "ERROR: failed to allocate memory for %lu samples\n", ff_sample_count);
        return 1;
    }

    // Read samples into buffer
    {
        int res = fread(ff_samples, sizeof(uint16_t), ff_sample_count, ff_sample_fd);
        if (res < 0)
        {
            fprintf(stderr, "ERROR: error reading samples from file: %s\n", strerror(errno));
            return 1;
        }
        else if (res != ff_sample_count)
        {
            fprintf(stderr, "ERROR: only read %d samples of %lu expected\n", res, ff_sample_count);
            return 1;
        }
        fclose(ff_sample_fd);
    }

    /* Process the flux timings into the raw bitcell buffer. */

    printf("Starting to process flux to bitcells\n");

    uint32_t *bc_buf = malloc(BC_BUF_SIZE_BYTES);
    uint32_t bc_bufmask = (BC_BUF_SIZE_BYTES / 4) - 1;
    uint16_t write_bc_ticks = (500*72) / hfe_bit_rate_kbps;
    uint32_t bc_prod;

    struct kv_pair *algorithm_params = NULL;

    char *param_start = strchr(algorithm, '[');
    char *param_end = strrchr(algorithm, ']');
    if (param_start != NULL && param_end != NULL) {
        *param_start = '\0';
        *param_end = '\0';

        param_start++;

        algorithm_params = kv_pair_list_from_string(param_start);
    }

    struct algorithm **alg = NULL;
    for (alg = ALGS; *alg != NULL; ++alg) {
        if (strcmp(algorithm, (*alg)->name) == 0)
            break;
    }

    if (*alg == NULL)
    {
        fprintf(stderr, "Unknown algorithm: %s\n", algorithm);
        return 1;
    }

    struct data_logger *logger = data_logger_open(data_log_path);
    if (logger == NULL) {
        fprintf(stderr, "Failed to open data log \"%s\"", data_log_path);
        return 1;
    }

    printf("Running %s with write_bc_ticks=%hu\n", (*alg)->name, write_bc_ticks);
    bc_prod = (*alg)->func((uint16_t)write_bc_ticks, ff_samples, ff_sample_count, bc_buf, bc_bufmask, algorithm_params, logger);

    data_logger_close(logger);
    logger = NULL;

    printf("Decoded %u bitcells\n", bc_prod);

    if (bc_prod == 0) {
        return 0;
    }

    if (bc_prod / 4 >= BC_BUF_SIZE_BYTES)
    {
        fprintf(stderr, "ERROR: decoded more bitcells than buffer space\n");
        return 1;
    }

    /* Round up to next byte and word in case last byte is partial */
    size_t bc_bytes = (bc_prod + 7) / 8;
    size_t bc_words = bc_bytes / 4;

    /* Write HFE */
    FILE *hfe_fd = fopen(hfe_path, "w");
    if (hfe_fd == NULL)
    {
        fprintf(stderr, "ERROR: unable to open output HFE file: %s\n", strerror(errno));
        return 1;
    }

    const uint8_t header[] = {
        'H',
        'X',
        'C',
        'P',
        'I',
        'C',
        'F',
        'E',
        /* Revision */ 0x0,
        /* Number of tracks */ 0x1,
        /* Number of sides */ 0x1,
        /* Track encoding */ 0xFF /* Unknown */,
        /* Bitrate (kbps) */ hfe_bit_rate_kbps & 0xFF,
        (hfe_bit_rate_kbps >> 8) & 0xFF,
        /* RPM */ 0x00,
        0x00,
        /* Interface mode */ 0x07 /* GENERIC_SHUGGART_DD_FLOPPYMODE */,
        /* Reserved */ 0,
        /* Track list offset */ 0x01,
        0x00,
    };
    fwrite(header, sizeof(header), 1, hfe_fd);

    // Track list
    size_t track_data_length_bytes = bc_bytes * 2;
    const uint8_t track_list[] = {
        /* Track data offset */ 0x02,
        0x00,
        /* Track data length */ (track_data_length_bytes & 0xFF),
        (track_data_length_bytes >> 8) & 0xFF,
    };
    fseek(hfe_fd, 0x200, SEEK_SET);
    fwrite(track_list, sizeof(track_list), 1, hfe_fd);

    // Track data
    for (int ii = 0; ii < bc_words; ++ii)
    {
        uint32_t bits_left_to_right = be32toh(bc_buf[ii]);
        uint8_t bits_out[4];

        for (int jj = 31; jj >= 0; --jj, bits_left_to_right >>= 1)
        {
            int bit = bits_left_to_right & 1;

            int byte_idx = jj / 8;
            bits_out[byte_idx] = bits_out[byte_idx] << 1 | bit;
        }

        long byte_number = ii * 4;
        long block_number = byte_number / 256;
        long offset = 0x400 + (block_number * 512) + (byte_number % 256);
        //printf("Writing word %d (data: 0x%x, 0x%x 0x%x 0x%x 0x%x) to offset 0x%lx\n", ii, be32toh(bc_buf[ii]), bits_out[0], bits_out[1], bits_out[2], bits_out[3], offset);
        fseek(hfe_fd, offset, SEEK_SET);
        fwrite(bits_out, 4, 1, hfe_fd);
    }

    fclose(hfe_fd);

    return 0;
}
