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
    
    if (!strncmp(ret.c_str(), "IBM", 3)) {
        ret = "CP" + ret.substr(3);
    } else if (!strncmp(ret.c_str(), "MAC-", 4)) {
        ret = "MAC" + ret.substr(4);
    } else if (!strncmp(ret.c_str(), "WINDOWS-", 8) || !strncmp(ret.c_str(), "Windows-", 8)) {
        ret = "CP" + ret.substr(8);
    }
    return ret;
}

static std::string convertString(const std::string &str, const char *charset) {
    // Conversion doesn't need to happen if it's already UTF-8
    if (!strcmp(charset, "UTF-8") || !strcmp(charset, "ASCII")) {
        return std::string(str);
    }
    
    iconv_t cd = iconv_open("UTF-8", charset);
    
    size_t inLen = str.size();
    size_t outLen = inLen * 4;
    std::string buf(outLen, '\0');
    const char *inPtr = str.c_str();
    char *outPtr = &buf[0];
    
    errno = 0;
    size_t result = iconv(cd, const_cast<char **>(&inPtr), &inLen, &outPtr, &outLen);
    
    iconv_close(cd);
    
    if (result != (size_t)-1 && errno == 0)
    {
        buf.resize(buf.size()-outLen);
    }
    else {
        buf.clear();
    }
    
    return buf;
}

static std::string convertStringToUtf32(const std::string &str) {
    
    std::string charset = getCharset(str);
    
    iconv_t cd = iconv_open("UCS-4-INTERNAL", charset.c_str());
    
    size_t inLen = str.size();
    size_t outLen = inLen * 4;
    std::string buf(outLen, '\0');
    const char *inPtr = str.c_str();
    char *outPtr = &buf[0];
    
    errno = 0;
    size_t result = iconv(cd, const_cast<char **>(&inPtr), &inLen, &outPtr, &outLen);
    
    iconv_close(cd);
    
    if (result != (size_t)-1 && errno == 0)
    {
        buf.resize(buf.size()-outLen);
    }
    else {
        buf.clear();
    }
    
    return buf;
}

static std::string convertString(const std::string &str) {
    return convertString(str, getCharset(str).c_str());
}
}

#endif /* encoding_h */
