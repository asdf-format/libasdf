#ifndef ASDF_LOG_H
#define ASDF_LOG_H

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

#endif /* ASDF_LOG_H */
