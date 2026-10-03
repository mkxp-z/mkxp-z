#include <cstring>
#include <iconv.h>
#include "system-iconv.h"

static_assert(sizeof(mkxp_system_iconv_t) == sizeof(iconv_t), "mkxp_system_iconv_t is the wrong size; please update it in system-iconv.h");

mkxp_system_iconv_t mkxp_system_iconv_open(const char *tocode, const char *fromcode) noexcept {
    mkxp_system_iconv_t converted_cd;
    iconv_t cd = iconv_open(tocode, fromcode);
    std::memcpy(&converted_cd, &cd, sizeof cd);
    return converted_cd;
}

size_t mkxp_system_iconv(mkxp_system_iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft) noexcept {
    iconv_t converted_cd;
    std::memcpy(&converted_cd, &cd, sizeof cd);
    return iconv(converted_cd, inbuf, inbytesleft, outbuf, outbytesleft);
}

int mkxp_system_iconv_close(mkxp_system_iconv_t cd) noexcept {
    iconv_t converted_cd;
    std::memcpy(&converted_cd, &cd, sizeof cd);
    return iconv_close(converted_cd);
}
