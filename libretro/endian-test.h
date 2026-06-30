#include <boost/predef/other/endian.h>

#if 0
#elif BOOST_ENDIAN_BIG_BYTE
big
#elif BOOST_ENDIAN_LITTLE_BYTE
little
#else
#error "failed to determine endianness"
#endif
