#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <argp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "event.h"
#include "file.h"
#include "info.h"
#include "parser.h"

const char *argp_program_version = PACKAGE_STRING;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;


static char doc[] = "asdf — Commandline utilities for managing ASDF files.\n\n";

static char args_doc[] = "COMMAND [ARGS...]";

static struct argp_option global_options[] = {{0}};


// clang-format off
// Need AllowShortEnumsOnASingleLine: false
// but my system's clang-format doesn't have this setting yet.
typedef enum {
    ASDF_SUBCMD_NONE = 0,
    ASDF_SUBCMD_INFO,
    ASDF_SUBCMD_EVENTS,
    ASDF_SUBCMD_VERIFY_CHECKSUMS
} asdf_subcmd_t;
// clang-format on


struct global_args {
    asdf_subcmd_t subcmd;
    char **subcmd_argv;
    int subcmd_argc;
};


static error_t parse_global_opt(int key, char *arg, struct argp_state *state) {
    struct global_args *args = state->input;

    switch (key) {
    case ARGP_KEY_ARG:
        if (!args->subcmd) {
            if (strcmp(arg, "info") == 0) {
                args->subcmd = ASDF_SUBCMD_INFO;
            } else if (strcmp(arg, "events") == 0) {
                args->subcmd = ASDF_SUBCMD_EVENTS;
            } else if (strcmp(arg, "verify-checksums") == 0) {
                args->subcmd = ASDF_SUBCMD_VERIFY_CHECKSUMS;
            } else {
                argp_state_help(state, stdout, ARGP_HELP_STD_ERR);
            }
            args->subcmd_argc = state->argc - state->next + 1;
            args->subcmd_argv = &state->argv[state->next - 1];
            state->next = state->argc;
        } else {
            argp_error(state, "Only one subcommand is allowed.");
        }
        break;

    case ARGP_KEY_NO_ARGS:
        argp_state_help(state, stdout, ARGP_HELP_STD_USAGE);
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}


// info subcommand
static char info_doc[] = "Print a rendering of an ASDF tree.";
static char info_args_doc[] = "FILENAME";


#define INFO_OPT_NO_TREE_KEY 0x100
#define INFO_OPT_VERIFY_CHECKSUMS 0x101


static struct argp_option info_options[] = {
    {"no-tree", INFO_OPT_NO_TREE_KEY, 0, 0, "Do not show the tree", 0},
    {"blocks", 'b', 0, 0, "Show information about blocks", 0},
    {"verify-checksums", INFO_OPT_VERIFY_CHECKSUMS, 0, 0, "Verify block checksums (implies -b)", 0},
    {0}};


struct info_args {
    const char *filename;
    bool print_tree;
    bool print_blocks;
    bool verify_checksums;
};


// NOLINTNEXTLINE(readability-non-const-parameter)
static error_t parse_info_opt(int key, char *arg, struct argp_state *state) {
    struct info_args *args = state->input;

    switch (key) {
    case INFO_OPT_NO_TREE_KEY:
        args->print_tree = false;
        break;
    case 'b':
        args->print_blocks = true;
        break;
    case INFO_OPT_VERIFY_CHECKSUMS:
        args->verify_checksums = true;
        break;
    case ARGP_KEY_ARG:
        if (!args->filename)
            args->filename = arg;
        else
            argp_error(state, "Too many arguments for 'info'. Only FILENAME is expected.");
        break;

    case ARGP_KEY_NO_ARGS:
        argp_error(state, "'info' requires a FILENAME argument.");
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}


static struct argp info_argp = {info_options, parse_info_opt, info_args_doc, info_doc, 0, 0, 0};


static int info_main(struct info_args args) {
    asdf_file_t *file = asdf_open(args.filename, "r");

    if (!file) {
        perror("error");
        return EXIT_FAILURE;
    }

    asdf_info_cfg_t cfg = {
        .filename = args.filename,
        .print_tree = args.print_tree,
        .print_blocks = args.print_blocks || args.verify_checksums,
        .verify_checksums = args.verify_checksums};
    int status = asdf_info(file, stdout, &cfg);
    asdf_close(file);
    return status ? EXIT_FAILURE : EXIT_SUCCESS;
}
// end info


// events subcommand
static char events_doc[] = "Print event stream from ASDF parser (for debugging)";
static char events_args_doc[] = "FILENAME";


#define EVENTS_OPT_NO_YAML_KEY 0x100
#define EVENTS_OPT_CAP_TREE_KEY 0x101


static struct argp_option events_options[] = {
    {"verbose", 'v', 0, 0, "Show extra information about each event", 0},
    {"no-yaml", EVENTS_OPT_NO_YAML_KEY, 0, 0, "Do not produce YAML stream events", 0},
    {"cap-tree",
     EVENTS_OPT_CAP_TREE_KEY,
     0,
     0,
     "Capture the YAML tree and print it (for debugging)",
     0},
    {0}};


struct events_args {
    const char *filename;
    bool verbose;
    bool no_yaml;
    bool cap_tree;
};


// NOLINTNEXTLINE(readability-non-const-parameter)
static error_t parse_events_opt(int key, char *arg, struct argp_state *state) {
    struct events_args *args = state->input;

    switch (key) {
    case EVENTS_OPT_NO_YAML_KEY:
        args->no_yaml = true;
        break;
    case EVENTS_OPT_CAP_TREE_KEY:
        args->cap_tree = true;
        break;
    case 'v':
        args->verbose = true;
        break;
    case ARGP_KEY_ARG:
        if (!args->filename)
            args->filename = arg;
        else
            argp_error(state, "Too many arguments for 'events'. Only FILENAME is expected.");
        break;

    case ARGP_KEY_NO_ARGS:
        argp_error(state, "'events' requires a FILENAME argument.");
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}


static struct argp events_argp = {
    events_options, parse_events_opt, events_args_doc, events_doc, 0, 0, 0};


int events_main(struct events_args args) {
    // Current implementation always outputs YAML events, so this is needed; later update
    // to allow an option to skip YAML events (useful for testing)
    asdf_parser_optflags_t flags = args.no_yaml ? 0 : ASDF_PARSER_OPT_EMIT_YAML_EVENTS;

    if (args.cap_tree)
        flags |= ASDF_PARSER_OPT_BUFFER_TREE;

    asdf_parser_cfg_t parser_cfg = {.flags = flags};

    asdf_parser_t *parser = asdf_parser_create(&parser_cfg);

    if (!parser) {
        fprintf(stderr, "error: could not allocate ASDF parser (OOM?)\n");
        return EXIT_FAILURE;
    }

    if (asdf_parser_set_input_file(parser, args.filename)) {
        fprintf(stderr, "error: %s\n", asdf_parser_get_error(parser));
        asdf_parser_destroy(parser);
        return EXIT_FAILURE;
    }

    asdf_event_t *event = NULL;

    while ((event = asdf_event_iterate(parser))) {
        asdf_event_print(event, stdout, args.verbose);
    }

    int ret = EXIT_SUCCESS;

    if (asdf_parser_has_error(parser)) {
        fprintf(stderr, "error: %s\n", asdf_parser_get_error(parser));
        ret = EXIT_FAILURE;
    }

    asdf_parser_destroy(parser);
    return ret;
}
// end events


// verify-checksums sub-command
static char verify_checksums_doc[] = "Verify binary block MD5 checksums";
static char verify_checksums_args_doc[] = "FILENAME";


static struct argp_option verify_checksums_options[] = {
    {"verbose",
     'v',
     0,
     0,
     "Output checksums of all blocks with or without errors; otherwise output is quiet on success",
     0},
    {0}};


struct verify_checksums_args {
    const char *filename;
    bool verbose;
};


// NOLINTNEXTLINE(readability-non-const-parameter)
static error_t parse_verify_checksums_opt(int key, char *arg, struct argp_state *state) {
    struct events_args *args = state->input;

    switch (key) {
    case 'v':
        args->verbose = true;
        break;
    case ARGP_KEY_ARG:
        if (!args->filename)
            args->filename = arg;
        else
            argp_error(
                state, "Too many arguments for 'verify-checksums'. Only FILENAME is expected.");
        break;

    case ARGP_KEY_NO_ARGS:
        argp_error(state, "'verify-checksums' requires a FILENAME argument.");
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}


static struct argp verify_checksums_argp = {
    verify_checksums_options,
    parse_verify_checksums_opt,
    verify_checksums_args_doc,
    verify_checksums_doc,
    0,
    0,
    0};


int verify_checksums_main(struct verify_checksums_args args) {
    asdf_file_t *file = asdf_open(args.filename, "r");

    if (!file) {
        fprintf(stderr, "error: %s\n", asdf_error(NULL));
        return EXIT_FAILURE;
    }

    int ret = EXIT_SUCCESS;
    FILE *out = args.verbose ? stdout : stderr;

    for (size_t idx = 0; idx < asdf_block_count(file); idx++) {
        asdf_block_t *block = asdf_block_open(file, idx);

        if (!block) {
            fprintf(stderr, "fatal error: %s", asdf_error(file));
            ret = EXIT_FAILURE;
            goto cleanup;
        }

        asdf_block_header_t *header = &block->info.header;
        unsigned char computed_digest[ASDF_BLOCK_CHECKSUM_DIGEST_SIZE] = {0};
        char computed[(ASDF_BLOCK_CHECKSUM_DIGEST_SIZE * 2) + 1] = {0};
        char expected[(ASDF_BLOCK_CHECKSUM_DIGEST_SIZE * 2) + 1] = {0};
        bool valid = asdf_block_checksum_verify(block, computed_digest);

        char *p = expected;

        for (int jdx = 0; jdx < ASDF_BLOCK_CHECKSUM_DIGEST_SIZE; jdx++) {
            p += sprintf(p, "%02x", header->checksum[jdx]);
        }

        if (!valid) {
            p = computed;

            for (int jdx = 0; jdx < ASDF_BLOCK_CHECKSUM_DIGEST_SIZE; jdx++) {
                p += sprintf(p, "%02x", computed_digest[jdx]);
            }

            ret = EXIT_FAILURE;
            fprintf(
                out,
                "Block %zu: checksum mismatch\n  expected: %s\n  computed: %s\n",
                idx,
                expected,
                computed);
        } else if (args.verbose) {
            fprintf(out, "Block %zu: OK\n  checksum: %s\n", idx, expected);
        }

        asdf_block_close(block);
    }

cleanup:
    asdf_close(file);
    return ret;
}
// end verify-checksums


int main(int argc, char *argv[]) {
    struct global_args global_args = {0};
    struct argp global_argp = {global_options, parse_global_opt, args_doc, doc, 0, 0, 0};

    argp_parse(&global_argp, argc, argv, ARGP_IN_ORDER, NULL, &global_args);

    switch (global_args.subcmd) {
    case ASDF_SUBCMD_INFO: {
        struct info_args info_args = {.print_tree = true};
        argp_parse(
            &info_argp, global_args.subcmd_argc, global_args.subcmd_argv, 0, NULL, &info_args);

        return info_main(info_args);
    }
    case ASDF_SUBCMD_EVENTS: {
        struct events_args events_args = {0};
        argp_parse(
            &events_argp, global_args.subcmd_argc, global_args.subcmd_argv, 0, NULL, &events_args);

        return events_main(events_args);
    }
    case ASDF_SUBCMD_VERIFY_CHECKSUMS: {
        struct verify_checksums_args verify_checksums_args = {0};
        argp_parse(
            &verify_checksums_argp,
            global_args.subcmd_argc,
            global_args.subcmd_argv,
            0,
            NULL,
            &verify_checksums_args);

        return verify_checksums_main(verify_checksums_args);
    }
    case ASDF_SUBCMD_NONE:
        break;
    }
    return EXIT_FAILURE;
}
