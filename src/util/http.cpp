#include "util/http.h"

namespace skillctl::util {

CurlGlobal::CurlGlobal() { curl_global_init(CURL_GLOBAL_DEFAULT); }
CurlGlobal::~CurlGlobal() { curl_global_cleanup(); }

}  // namespace skillctl::util

