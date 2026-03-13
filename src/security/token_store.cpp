#include "security/token_store.h"

#include "util/fs.h"

#include <vector>

#ifdef _WIN32
#  include <windows.h>
#  include <wincrypt.h>
#endif

namespace skillctl::security {

static StoreResult ok() {
  StoreResult r;
  r.ok = true;
  return r;
}

static StoreResult fail(std::string msg) {
  StoreResult r;
  r.ok = false;
  r.error = std::move(msg);
  return r;
}

static LoadTokenResult load_fail(std::string msg) {
  LoadTokenResult r;
  r.error = std::move(msg);
  return r;
}

StoreResult store_token_secure(const std::string& profile, const std::string& token) {
#ifdef _WIN32
  DATA_BLOB in;
  in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(token.data()));
  in.cbData = static_cast<DWORD>(token.size());

  std::wstring entropy_w(profile.begin(), profile.end());
  DATA_BLOB entropy;
  entropy.pbData = reinterpret_cast<BYTE*>(entropy_w.data());
  entropy.cbData = static_cast<DWORD>(entropy_w.size() * sizeof(wchar_t));

  DATA_BLOB out{};
  if (!CryptProtectData(&in, L"skillctl token", &entropy, nullptr, nullptr, 0, &out)) {
    return fail("DPAPI CryptProtectData failed");
  }
  std::vector<unsigned char> buf(out.pbData, out.pbData + out.cbData);
  LocalFree(out.pbData);

  std::string bytes(reinterpret_cast<const char*>(buf.data()), buf.size());
  std::string err;
  auto path = util::token_path_for_profile(profile);
  if (!util::write_file_atomic(path, bytes, &err)) return fail("write token failed: " + err);
  return ok();
#else
  (void)profile;
  (void)token;
  return fail("secure token store not available on this platform");
#endif
}

LoadTokenResult load_token_secure(const std::string& profile) {
#ifdef _WIN32
  auto path = util::token_path_for_profile(profile);
  if (!util::file_exists(path)) {
    LoadTokenResult r;
    return r;
  }
  std::string bytes;
  std::string err;
  if (!util::read_file(path, &bytes, &err)) return load_fail("read token failed: " + err);

  DATA_BLOB in;
  in.pbData = reinterpret_cast<BYTE*>(bytes.data());
  in.cbData = static_cast<DWORD>(bytes.size());

  std::wstring entropy_w(profile.begin(), profile.end());
  DATA_BLOB entropy;
  entropy.pbData = reinterpret_cast<BYTE*>(entropy_w.data());
  entropy.cbData = static_cast<DWORD>(entropy_w.size() * sizeof(wchar_t));

  DATA_BLOB out{};
  if (!CryptUnprotectData(&in, nullptr, &entropy, nullptr, nullptr, 0, &out)) {
    return load_fail("DPAPI CryptUnprotectData failed");
  }
  std::string token(reinterpret_cast<const char*>(out.pbData), out.cbData);
  LocalFree(out.pbData);

  LoadTokenResult r;
  r.token = std::move(token);
  return r;
#else
  (void)profile;
  LoadTokenResult r;
  r.error = "secure token store not available on this platform";
  return r;
#endif
}

StoreResult delete_token_secure(const std::string& profile) {
#ifdef _WIN32
  auto path = util::token_path_for_profile(profile);
  try {
    if (util::file_exists(path)) std::filesystem::remove(path);
    return ok();
  } catch (const std::exception& e) {
    return fail(std::string("delete token failed: ") + e.what());
  }
#else
  (void)profile;
  return fail("secure token store not available on this platform");
#endif
}

}  // namespace skillctl::security

