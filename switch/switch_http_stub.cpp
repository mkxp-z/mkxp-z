#ifdef __SWITCH__
#include "net/net.h"

namespace mkxp_net {

// HTTPResponse implementation
int HTTPResponse::status() { return 0; }
std::string &HTTPResponse::body() { return _body; }
StringMap &HTTPResponse::headers() { return _headers; }
HTTPResponse::~HTTPResponse() {}
HTTPResponse::HTTPResponse() : _status(0) {}

// HTTPRequest implementation
HTTPRequest::HTTPRequest(const char *dest, bool follow_redirects) {
    (void)dest; (void)follow_redirects;
}
HTTPRequest::~HTTPRequest() {}

StringMap &HTTPRequest::headers() { return _headers; }

HTTPResponse HTTPRequest::get() {
    return HTTPResponse();
}

HTTPResponse HTTPRequest::post(StringMap &postData) {
    (void)postData;
    return HTTPResponse();
}

HTTPResponse HTTPRequest::post(const char *body, const char *content_type) {
    (void)body; (void)content_type;
    return HTTPResponse();
}

} // namespace mkxp_net
#endif
