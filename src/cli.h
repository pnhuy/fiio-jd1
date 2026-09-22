#ifndef JD1_CLI_H
#define JD1_CLI_H

#include <stdint.h>

enum action {
    ACTION_PROBE, ACTION_READ, ACTION_GAIN, ACTION_SET_GAIN,
    ACTION_SAVE, ACTION_WATCH
};

struct cli_options {
    enum action action;
    uint32_t address;
    double gain_db;
};

/* Returns 0 for valid options, 1 for help, or 2 for invalid arguments.
 * Parsing never accesses hardware. */
int parse_cli(int argc, char **argv, struct cli_options *options);

#endif
