#pragma once

#include <curl/curl.h>

namespace skillctl::util {

struct CurlGlobal {
  CurlGlobal();
  ~CurlGlobal();
  CurlGlobal(const CurlGlobal&) = delete;
  CurlGlobal& operator=(const CurlGlobal&) = delete;
};

}  // namespace skillctl::util

