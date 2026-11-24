#ifndef SYSTEM_ICONV_H
#define SYSTEM_ICONV_H

#include <cstddef>

typedef void *mkxp_system_iconv_t;

mkxp_system_iconv_t mkxp_system_iconv_open(const char *tocode, const char *fromcode) noexcept;
size_t mkxp_system_iconv(mkxp_system_iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft) noexcept;
int mkxp_system_iconv_close(mkxp_system_iconv_t cd) noexcept;

#endif // SYSTEM_ICONV_H
