//
//  encoding.h
//  mkxp-z
//
//  Created by ゾロアーク on 6/22/21.
//

#ifndef encoding_h
#define encoding_h

#include <string>
#include <string.h>

#include "util/encoding.h"
#include <iconv.h>
#include <uchardet.h>
#include <errno.h>

namespace Encoding {

static std::string getCharset(const std::string &str) {
    uchardet_t ud = uchardet_new();
    uchardet_handle_data(ud, str.c_str(), str.length());
    uchardet_data_end(ud);
    
    std::string ret(uchardet_get_charset(ud));
    uchardet_delete(ud);
    
    if (ret.empty())
        throw Exception(Exception::MKXPError, "Could not detect string encoding", str.c_str());
    return ret;
}

static std::string convertString(const std::string &str, const char *charset, const char *destCharset = "UTF-8") {
    // Conversion doesn't need to happen if it's already UTF-8
    if (!strcmp(destCharset, "UTF-8") && (!strcmp(charset, "UTF-8") || !strcmp(charset, "ASCII"))) {
        return std::string(str);
    }
    
    iconv_t cd = iconv_open(destCharset, charset);
    
    size_t inLen = str.size();
    size_t outLen = inLen * 4;
    std::string buf(outLen, '\0');
    char *inPtr = const_cast<char*>(str.c_str());
    char *outPtr = const_cast<char*>(buf.c_str());
    
    errno = 0;
    size_t result = iconv(cd, &inPtr, &inLen, &outPtr, &outLen);
    
    iconv_close(cd);
    
    if (result != (size_t)-1 && errno == 0)
    {
        buf.resize(buf.size()-outLen);
    }
    else {
        throw Exception(Exception::MKXPError, "Unable to convert string (Guessed encoding: %s)", charset);
    }
    
    return buf;
}

static std::string convertString(const std::string &str) {
    return convertString(str, getCharset(str).c_str());
}
}

#endif /* encoding_h */
