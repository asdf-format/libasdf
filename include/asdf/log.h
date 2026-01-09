#ifndef ASDF_LOG_H
#define ASDF_LOG_H

#include <stdbool.h>
#include <stdio.h>

typedef enum {
    ASDF_LOG_NONE = 0,
    ASDF_LOG_TRACE,
    ASDF_LOG_DEBUG,
    ASDF_LOG_INFO,
    ASDF_LOG_WARN,
    ASDF_LOG_ERROR,
    ASDF_LOG_FATAL,
} asdf_log_level_t;

#define ASDF_LOG_NUM_LEVELS (ASDF_LOG_FATAL + 1)


typedef struct {
    FILE *stream; /* Currently just defaults to stderr */
    asdf_log_level_t level;
    bool no_color;
} asdf_log_cfg_t;


#endif /* ASDF_LOG_H */
