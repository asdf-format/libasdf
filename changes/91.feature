Support for the ``stsci.edu/asdf/time/time`` schema via a new
``asdf/core/time.h`` API.  Time values can be read with ``asdf_get_time`` /
``asdf_value_as_time`` and written with ``asdf_set_time``, exposing an
``asdf_time_t`` with the original ``value`` string, the ``format`` and optional
``base_format``, the ``scale`` and ``location``, and a computed timestamp
(``struct timespec`` and ``struct tm``) for the supported formats.

Files written with any of the ``time-1.0.0`` through ``time-1.4.0`` tags are
read; newly written time values use ``time-1.4.0``.
