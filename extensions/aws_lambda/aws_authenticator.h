#pragma once
#include <set>
#include <string>
#include <unordered_map>

#include "hmac_sha2.h"
#include "sha2.h"

#include "proxy_wasm_intrinsics.h"

using SystemTime = uint64_t;

class TimeSource {
public:
  // TimeSource
  SystemTime systemTime() { return getCurrentTimeNanoseconds(); }
};

using HeaderMap = std::vector<std::pair<std::string, std::string>>;


typedef bool (*LowerCaseStringCompareFunc)(const std::string &,
                                           const std::string &);

typedef std::set<std::string, LowerCaseStringCompareFunc> HeaderList;

class AwsAuthenticator {
public:
  AwsAuthenticator();

  ~AwsAuthenticator();

  void init(const std::string *access_key, const std::string *secret_key);

  void updatePayloadHash(const std::string_view data);

  void sign(const HeaderMap &request_headers,
            const HeaderList &headers_to_sign,
            const std::string &region);

  /**
   * This creates a a list of headers to sign to be used by sign.
   */
  static HeaderList
  createHeaderToSign(std::initializer_list<std::string> headers);

private:

  std::string signWithTime(const HeaderMap& request_headers,
                           const HeaderList &headers_to_sign,
                           const std::string &region, SystemTime now);

  std::string addDate(SystemTime now);

  std::pair<std::string, std::string>
  prepareHeaders(const HeaderList &headers_to_sign);

  std::string getBodyHexSha();
  void fetchUrl();
  std::string computeCanonicalRequestHash(std::string_view request_method,
                                          const std::string &canonical_Headers,
                                          const std::string &signed_headers,
                                          const std::string &hexpayload);
  std::string getCredntialScopeDate(SystemTime now);
  std::string getCredntialScope(const std::string &region,
                                const std::string &datenow);

  std::string computeSignature(const std::string &region,
                               const std::string &credential_scope_date,
                               const std::string &credential_scope,
                               const std::string &request_date_time,
                               const std::string &hashed_canonical_request);

  static bool lowercasecompare(const std::string &i,
                               const std::string &j);

  class Sha256 {
  public:
    static constexpr int LENGTH = SHA256_DIGEST_SIZE;
    Sha256();
    void update(const std::string &data);
    void update(const std::string_view data);

    void update(char c);
    void update(const uint8_t *bytes, size_t size);
    void update(const char *chars, size_t size);
    void finalize(uint8_t *out);

  private:
    sha256_ctx context_;
  };

  class HMACSha256 {
  public:
    HMACSha256();
    ~HMACSha256();
    size_t length() const;
    void init(const std::string &data);
    void init(const uint8_t *bytes, size_t size);
    void update(const std::string &data);
    void update(std::string_view data);
    void update(std::initializer_list<const std::string *> strings);
    void update(const uint8_t *bytes, size_t size);
    void finalize(uint8_t *out, unsigned int *out_len);

  private:
    hmac_sha256_ctx context_;
    bool firstinit{true};
  };

  template <typename T>
  static void recusiveHmacHelper(HMACSha256 &hmac, uint8_t *out,
                                 unsigned int &out_len, const T &what) {
    hmac.init(out, out_len);
    hmac.update(what);
    hmac.finalize(out, &out_len);
  }

  Sha256 body_sha_;

  TimeSource time_source_;
  const std::string *access_key_{};
  std::string first_key_;
  std::string_view service_{};
  std::string_view method_{};
  std::string_view query_string_{};
  std::string_view url_base_{};

  const HeaderMap* request_headers_;
};