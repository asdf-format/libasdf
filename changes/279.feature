Structured logging handler: `asdf_log_cfg_t` gained new fields for supporting
custom log handlers.  Applications can now pass their own log handler to
libasdf to redirect the library's log messages to their own logging systems.

For the default (original) stream-based logger we also bracket its outputs with
``flockfile``/``funlockfile`` to ensure log outputs are serialized.

This required a small ABI breakage as noted above.  Accepted on a practical
basis since no known software is using the logging config interface yet.
