/* tiny c compiler compatibility */

#ifdef __TINYC__
#include <stddef.h>
#else
#include_next <stdint.h>
#endif
