#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <argp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "event.h"
#include "info.h"
#include "parse.h"

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
    ASDF_SUBCMD_EVENTS
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


static struct argp_option info_options[] = {
    {"no-tree", INFO_OPT_NO_TREE_KEY, 0, 0, "Do not show the tree", 0},
    {"blocks", 'b', 0, 0, "Show information about blocks", 0},
    {0}};

struct info_args {
    const char *filename;
    bool print_tree;
    bool print_blocks;
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


static struct argp info_argp = {info_options, parse_info_opt, info_args_doc, info_doc};


static int info_main(const char *filename, bool print_tree, bool print_blocks) {
    FILE *file = fopen(filename, "r");

    if (!file) {
        perror("error");
        return EXIT_FAILURE;
    }

    asdf_info_cfg_t cfg = {
        .filename = filename,
        .print_tree = print_tree,
        .print_blocks = print_blocks,
    };
    int status = asdf_info(file, stdout, &cfg);
    fclose(file);
    return status ? EXIT_FAILURE : EXIT_SUCCESS;
}
// end info


// events subcommand
static char events_doc[] = "Print event stream from ASDF parser (for debugging)";
static char events_args_doc[] = "FILENAME";


#define EVENTS_OPT_NO_YAML_KEY 0x100


static struct argp_option events_options[] = {
    {"verbose", 'v', 0, 0, "Show extra information about each event", 0},
    {"no-yaml", EVENTS_OPT_NO_YAML_KEY, 0, 0, "Do not produce YAML stream events", 0}, {0}};


struct events_args {
    const char *filename;
    bool verbose;
    bool no_yaml;
};


// NOLINTNEXTLINE(readability-non-const-parameter)
static error_t parse_events_opt(int key, char *arg, struct argp_state *state) {
    struct events_args *args = state->input;

    switch (key) {
    case EVENTS_OPT_NO_YAML_KEY:
        args->no_yaml = true;
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


static struct argp events_argp = {events_options, parse_events_opt, events_args_doc, events_doc};


int events_main(const char *filename, bool verbose, bool no_yaml) {
    FILE *file = fopen(filename, "r");

    if (!file) {
        perror("error");
        return EXIT_FAILURE;
    }

    asdf_parser_t parser;
    // Current implementation always outputs YAML events, so this is needed; later update
    // to allow an option to skip YAML events (useful for testing)
    asdf_parser_cfg_t parser_cfg = {.flags = no_yaml ? 0 : ASDF_PARSER_OPT_EMIT_YAML_EVENTS};

    if (asdf_parser_init(&parser, &parser_cfg)) {
        fprintf(stderr, "error: %s\n", asdf_parser_get_error(&parser));
        fclose(file);
        return EXIT_FAILURE;
    }

    if (asdf_parser_set_input_file(&parser, file, filename)) {
        fprintf(stderr, "error: %s\n", asdf_parser_get_error(&parser));
        fclose(file);
        return EXIT_FAILURE;
    }

    asdf_event_t event = {0};

    while (asdf_event_iterate(&parser, &event) == 0) {
        asdf_event_print(&event, stdout, verbose);
    }

    if (asdf_parser_has_error(&parser)) {
        fprintf(stderr, "error: %s\n", asdf_parser_get_error(&parser));
        asdf_event_destroy(&parser, &event);
        asdf_parser_destroy(&parser);
        fclose(file);
        return EXIT_FAILURE;
    }

    asdf_event_destroy(&parser, &event);
    asdf_parser_destroy(&parser);
    fclose(file);
    return EXIT_SUCCESS;
}
// end events


int main(int argc, char *argv[]) {
    struct global_args global_args = {0};
    struct argp global_argp = {global_options, parse_global_opt, args_doc, doc};

    argp_parse(&global_argp, argc, argv, ARGP_IN_ORDER, NULL, &global_args);

    switch (global_args.subcmd) {
    case ASDF_SUBCMD_INFO: {
        struct info_args info_args = {.print_tree = true};
        argp_parse(
            &info_argp, global_args.subcmd_argc, global_args.subcmd_argv, 0, NULL, &info_args);

        return info_main(info_args.filename, info_args.print_tree, info_args.print_blocks);
    }
    case ASDF_SUBCMD_EVENTS: {
        struct events_args events_args = {0};
        argp_parse(
            &events_argp, global_args.subcmd_argc, global_args.subcmd_argv, 0, NULL, &events_args);

        return events_main(events_args.filename, events_args.verbose, events_args.no_yaml);
    }
    case ASDF_SUBCMD_NONE:
        break;
    }
    return EXIT_FAILURE;
}
