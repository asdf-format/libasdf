/* Internal definitions for numeric types */

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <float.h>

#ifdef HAVE_FLOAT16
// Typedef _Float16 internally as 'half' for consistency with 'float' and
// 'double'
typedef _Float16 half;

#ifndef FLT16_MAX
/* FLT16_MAX may not be defined by float.h unless it was included early
 * via __STCD_WANT_IEC-60559_TYPES_EXT__, so just define it via the
 * GCC/clang predefine
 */
#define FLT16_MAX __FLT16_MAX__
#endif
#endif
