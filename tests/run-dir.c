/**
 * Print the temporary directory for the current test run.
 *
 * The run directory is selected by a constructor in util.c and shared by
 * every process in the same process group (see the comment there), so the
 * path printed here is the same one the unit test binaries of the current
 * `make check` are using.  This lets the shell tests write their output
 * alongside the unit tests' temp files.
 */

#include <stdio.h>

#include "util.h"


int main(void) {
    printf("%s\n", get_run_dir());
    return 0;
}
