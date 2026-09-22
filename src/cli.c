#include "cli.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *stream, const char *program) {
    fprintf(stream, "Usage:\n  %s probe\n  %s read ADDRESS\n"
            "  %s gain\n  %s set-gain DB --yes\n"
            "  %s save --yes\n  %s watch DB\n  %s --help\n"
            "ADDRESS accepts decimal or 0x-prefixed hexadecimal.\n"
            "DB must be -60.0..0.0 in exact 0.5 dB steps.\n",
            program, program, program, program, program, program, program);
}

int parse_cli(int argc, char **argv, struct cli_options *options) {
    *options = (struct cli_options){0};
    if (argc == 2 && (strcmp(argv[1], "--help") == 0
                      || strcmp(argv[1], "-h") == 0)) {
        usage(stdout, argv[0]);
        return 1;
    }
    if (argc < 2) goto invalid;

    if (strcmp(argv[1], "probe") == 0) {
        options->action = ACTION_PROBE;
        if (argc != 2) goto invalid;
    } else if (strcmp(argv[1], "gain") == 0) {
        options->action = ACTION_GAIN;
        if (argc != 2) goto invalid;
    } else if (strcmp(argv[1], "read") == 0) {
        options->action = ACTION_READ;
        if (argc != 3 || !isxdigit((unsigned char)argv[2][0])) goto invalid;
        char *end;
        errno = 0;
        int base = argv[2][0] == '0' && (argv[2][1] == 'x' || argv[2][1] == 'X')
            ? 16 : 10;
        unsigned long value = strtoul(argv[2], &end, base);
        if (errno || end == argv[2] || *end || value > UINT32_MAX) goto invalid;
        options->address = (uint32_t)value;
    } else if (strcmp(argv[1], "save") == 0) {
        options->action = ACTION_SAVE;
        if (argc != 3 || strcmp(argv[2], "--yes") != 0) goto invalid;
    } else if (strcmp(argv[1], "set-gain") == 0 || strcmp(argv[1], "watch") == 0) {
        int watch = strcmp(argv[1], "watch") == 0;
        options->action = watch ? ACTION_WATCH : ACTION_SET_GAIN;
        if ((watch && argc != 3)
            || (!watch && (argc != 4 || strcmp(argv[3], "--yes") != 0))) goto invalid;
        char *end;
        errno = 0;
        double value = strtod(argv[2], &end);
        if (errno || end == argv[2] || *end || !isfinite(value)
            || value < -60.0 || value > 0.0) goto invalid;
        /* Cast only after checking finiteness and range. */
        double steps = value * 2.0;
        if ((double)(int)steps != steps) goto invalid;
        options->gain_db = value;
    } else {
        goto invalid;
    }
    return 0;

invalid:
    usage(stderr, argv[0]);
    return 2;
}
