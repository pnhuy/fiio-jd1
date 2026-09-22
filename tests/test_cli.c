#include "cli.h"

#include <stdio.h>
#include <stdlib.h>

struct test_case {
    int argc;
    char *argv[5];
    int result;
    enum action action;
    uint32_t address;
    double gain;
};

int main(void) {
    const struct test_case cases[] = {
        {2, {"jd1ctl", "probe"}, 0, ACTION_PROBE, 0, 0},
        {2, {"jd1ctl", "gain"}, 0, ACTION_GAIN, 0, 0},
        {3, {"jd1ctl", "read", "0x66"}, 0, ACTION_READ, 102, 0},
        {3, {"jd1ctl", "read", "010"}, 0, ACTION_READ, 10, 0},
        {3, {"jd1ctl", "read", "0xffffffff"}, 0, ACTION_READ, UINT32_MAX, 0},
        {3, {"jd1ctl", "save", "--yes"}, 0, ACTION_SAVE, 0, 0},
        {4, {"jd1ctl", "set-gain", "-60", "--yes"}, 0, ACTION_SET_GAIN, 0, -60},
        {4, {"jd1ctl", "set-gain", "0", "--yes"}, 0, ACTION_SET_GAIN, 0, 0},
        {3, {"jd1ctl", "watch", "-20.5"}, 0, ACTION_WATCH, 0, -20.5},
        {2, {"jd1ctl", "--help"}, 1, 0, 0, 0},
        {1, {"jd1ctl"}, 2, 0, 0, 0},
        {2, {"jd1ctl", "unknown"}, 2, 0, 0, 0},
        {3, {"jd1ctl", "probe", "extra"}, 2, 0, 0, 0},
        {3, {"jd1ctl", "gain", "extra"}, 2, 0, 0, 0},
        {2, {"jd1ctl", "read"}, 2, 0, 0, 0},
        {3, {"jd1ctl", "read", ""}, 2, 0, 0, 0},
        {3, {"jd1ctl", "read", "-1"}, 2, 0, 0, 0},
        {3, {"jd1ctl", "read", "0x"}, 2, 0, 0, 0},
        {3, {"jd1ctl", "read", "12junk"}, 2, 0, 0, 0},
        {3, {"jd1ctl", "read", "4294967296"}, 2, 0, 0, 0},
        {3, {"jd1ctl", "read", "999999999999999999999999999"}, 2, 0, 0, 0},
        {2, {"jd1ctl", "save"}, 2, 0, 0, 0},
        {3, {"jd1ctl", "save", "yes"}, 2, 0, 0, 0},
        {3, {"jd1ctl", "set-gain", "-20"}, 2, 0, 0, 0},
        {4, {"jd1ctl", "set-gain", "-20", "yes"}, 2, 0, 0, 0},
        {3, {"jd1ctl", "watch", "nan"}, 2, 0, 0, 0},
        {3, {"jd1ctl", "watch", "inf"}, 2, 0, 0, 0},
        {3, {"jd1ctl", "watch", "-inf"}, 2, 0, 0, 0},
        {3, {"jd1ctl", "watch", "1e999"}, 2, 0, 0, 0},
        {3, {"jd1ctl", "watch", ""}, 2, 0, 0, 0},
        {3, {"jd1ctl", "watch", "-60.5"}, 2, 0, 0, 0},
        {3, {"jd1ctl", "watch", "0.5"}, 2, 0, 0, 0},
        {3, {"jd1ctl", "watch", "-0.25"}, 2, 0, 0, 0},
        {3, {"jd1ctl", "watch", "-20junk"}, 2, 0, 0, 0},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        struct cli_options options;
        struct test_case t = cases[i];
        int result = parse_cli(t.argc, t.argv, &options);
        if (result != t.result || (result == 0 &&
            (options.action != t.action || options.address != t.address
             || options.gain_db != t.gain))) {
            fprintf(stderr, "FAIL: CLI case %zu\n", i);
            return EXIT_FAILURE;
        }
    }
    puts("All 34 CLI cases passed (no hardware accessed).");
    return EXIT_SUCCESS;
}
