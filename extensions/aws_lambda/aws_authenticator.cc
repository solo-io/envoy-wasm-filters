#include "aws_authenticator.h"

#include <algorithm>
#include <list>
#include <string>
#include <sstream>
#include <iomanip>

class AwsAuthenticatorValues {
public:
  static constexpr std::string_view Algorithm{"AWS4-HMAC-SHA256"};
  static constexpr std::string_view Service{"lambda"};
  static constexpr std::string_view Newline{"\n"};
  static constexpr std::string_view DateHeader{"x-amz-date"};
  static constexpr std::string_view Post{"POST"};
};


AwsAuthenticator::AwsAuthenticator() {
  // TODO(yuval-k) hardcoded for now
  service_ = AwsAuthenticatorValues::Service;
  method_ = AwsAuthenticatorValues::Post;
}

void AwsAuthenticator::init(const std::string *access_key,
                            const std::string *secret_key) {
  access_key_ = access_key;
  const std::string &secret_key_ref = *secret_key;
  first_key_ = "AWS4" + secret_key_ref;
}

AwsAuthenticator::~AwsAuthenticator() {}

HeaderList AwsAuthenticator::createHeaderToSign(
    std::initializer_list<std::string> headers) {
  // A C++ set is sorted. which is required by AWS signature algorithm.
  HeaderList ret(AwsAuthenticator::lowercasecompare);
  ret.insert(headers);
  ret.insert(std::string(AwsAuthenticatorValues::DateHeader));
  return ret;
}

void AwsAuthenticator::updatePayloadHash(const std::string_view data) {
  body_sha_.update(data);
}

bool AwsAuthenticator::lowercasecompare(const std::string &i,
                                        const std::string &j) {
  return (i < j);
}

std::string AwsAuthenticator::addDate(SystemTime now) {
  // TODO(yuval-k): This can be cached or optimized if needed
  std::time_t timenow = now;
  std::ostringstream request_date_time_stream;
  request_date_time_stream << std::put_time(std::gmtime(&timenow), "%Y%m%dT%H%M%SZ");
  auto request_date_time = request_date_time_stream.str();
  addRequestHeader(AwsAuthenticatorValues::DateHeader, request_date_time);
  return request_date_time;
}

std::pair<std::string, std::string>
AwsAuthenticator::prepareHeaders(const HeaderList &headers_to_sign) {
  std::stringstream canonical_headers_stream;
  std::stringstream signed_headers_stream;

  for (auto header = headers_to_sign.begin(), end = headers_to_sign.end();
       header != end; header++) {
    auto headerData = getRequestHeader(*header);
    const std::string_view headerEntry = headerData->view();

    auto headerName = header;
    canonical_headers_stream << *headerName;
    signed_headers_stream << *headerName;

    canonical_headers_stream << ':';
    if (!headerEntry.empty()) {
      canonical_headers_stream << headerEntry;
      // TODO: add warning if null
    }
    canonical_headers_stream << '\n';
    HeaderList::const_iterator next = header;
    next++;
    if (next != end) {
      signed_headers_stream << ";";
    }
  }
  std::string canonical_headers = canonical_headers_stream.str();
  std::string signed_headers = signed_headers_stream.str();

  std::pair<std::string, std::string> pair =
      std::make_pair(std::move(canonical_headers), std::move(signed_headers));
  return pair;
}

std::string HexEncode(const uint8_t* data, size_t length) {
  static const char* const digits = "0123456789abcdef";

  std::string ret;
  ret.reserve(length * 2);

  for (size_t i = 0; i < length; i++) {
    uint8_t d = data[i];
    ret.push_back(digits[d >> 4]);
    ret.push_back(digits[d & 0xf]);
  }

  return ret;
}

std::string AwsAuthenticator::getBodyHexSha() {

  uint8_t payload_out[Sha256::LENGTH];
  body_sha_.finalize(payload_out);
  std::string hexpayload = HexEncode(payload_out, Sha256::LENGTH);
  return hexpayload;
}

std::string_view  findQueryStringStart(std::string_view path_str) {
  size_t query_offset = path_str.find('?');
  if (query_offset == std::string_view::npos) {
    query_offset = path_str.length();
  }
  path_str.remove_prefix(query_offset);
  return path_str;
}

void AwsAuthenticator::fetchUrl() {
  auto path = getRequestHeader(":path");

  const std::string_view canonical_url = path->view();
  url_base_ = canonical_url;
  query_string_ = findQueryStringStart(canonical_url);
  if (query_string_.length() != 0) {
    url_base_.remove_suffix(query_string_.length());
    // remove the question mark
    query_string_.remove_prefix(1);
  }
}


std::string AwsAuthenticator::computeCanonicalRequestHash(
    std::string_view request_method, const std::string &canonical_headers,
    const std::string &signed_headers, const std::string &hexpayload) {

  // Do iternal classes for sha and hmac.
  Sha256 canonicalRequestHash;

  canonicalRequestHash.update(request_method);
  canonicalRequestHash.update('\n');
  canonicalRequestHash.update(url_base_);
  canonicalRequestHash.update('\n');
  if (query_string_.length() != 0) {
    canonicalRequestHash.update(query_string_);
  }
  canonicalRequestHash.update('\n');
  canonicalRequestHash.update(canonical_headers);
  canonicalRequestHash.update('\n');
  canonicalRequestHash.update(signed_headers);
  canonicalRequestHash.update('\n');
  canonicalRequestHash.update(hexpayload);

  uint8_t cononicalRequestHashOut[Sha256::LENGTH];

  canonicalRequestHash.finalize(cononicalRequestHashOut);
  return HexEncode(cononicalRequestHashOut, Sha256::LENGTH);
}

std::string AwsAuthenticator::getCredntialScopeDate(SystemTime now) {

  std::time_t timenow = now;
  std::ostringstream credentials_scope_date_stream;
  credentials_scope_date_stream << std::put_time(std::gmtime(&timenow),"%Y%m%d");

  std::string credentials_scope_date = credentials_scope_date_stream.str();
  return credentials_scope_date;
}

std::string
AwsAuthenticator::getCredntialScope(const std::string &region,
                                    const std::string &credentials_scope_date) {

  std::stringstream credential_scope_stream;
  credential_scope_stream << credentials_scope_date << "/" << region << "/"
                          << (service_) << "/aws4_request";
  return credential_scope_stream.str();
}

std::string AwsAuthenticator::computeSignature(
    const std::string &region, const std::string &credentials_scope_date,
    const std::string &credential_scope, const std::string &request_date_time,
    const std::string &hashed_canonical_request) {
  static const std::string aws_request = "aws4_request";

  HMACSha256 sighmac;
  unsigned int out_len = sighmac.length();

  uint8_t out[Sha256::LENGTH];

  sighmac.init(first_key_);
  sighmac.update(credentials_scope_date);
  sighmac.finalize(out, &out_len);

  recusiveHmacHelper(sighmac, out, out_len, region);
  recusiveHmacHelper(sighmac, out, out_len, service_);
  recusiveHmacHelper(sighmac, out, out_len, aws_request);

  std::string nl(AwsAuthenticatorValues::Newline);
  std::string alg(AwsAuthenticatorValues::Algorithm);
  recusiveHmacHelper<std::initializer_list<const std::string *>>(
      sighmac, out, out_len,
      {&alg, &nl, &request_date_time, &nl,
       &credential_scope, &nl, &hashed_canonical_request});

  return HexEncode(out, out_len);
}

void AwsAuthenticator::sign(const HeaderMap &request_headers, 
            const HeaderList &headers_to_sign,
            const std::string &region) {

  // we can't use the date provider interface as this is not the date header,
  // plus the date format is different. use slow method now, optimize in the
  // future.
  auto now = time_source_.systemTime()/10e9;

  std::string sig = signWithTime(request_headers, headers_to_sign, region, now);
  addRequestHeader("authorization", sig);
}

std::string AwsAuthenticator::signWithTime(const HeaderMap &request_headers, 
    const HeaderList &headers_to_sign,
    const std::string &region, SystemTime now) {
  request_headers_ = &request_headers;

  std::string request_date_time = addDate(now);

  auto &&preparedHeaders = prepareHeaders(headers_to_sign);
  std::string canonical_headers = std::move(preparedHeaders.first);
  std::string signed_headers = std::move(preparedHeaders.second);

  std::string hexpayload = getBodyHexSha();

  fetchUrl();

  std::string hashed_canonical_request = computeCanonicalRequestHash(
       method_, canonical_headers, signed_headers, hexpayload);
  std::string credentials_scope_date = getCredntialScopeDate(now);
  std::string CredentialScope =
      getCredntialScope(region, credentials_scope_date);

  std::string signature =
      computeSignature(region, credentials_scope_date, CredentialScope,
                       request_date_time, hashed_canonical_request);

  std::stringstream authorizationvalue;

  authorizationvalue << AwsAuthenticatorValues::Algorithm
                     << " Credential=" << (*access_key_) << "/"
                     << CredentialScope << ", SignedHeaders=" << signed_headers
                     << ", Signature=" << signature;
  return authorizationvalue.str();
}

AwsAuthenticator::Sha256::Sha256() { sha256_init(&context_); }

void AwsAuthenticator::Sha256::update(const std::string &data) {
  update(data.data(), data.size());
}

void AwsAuthenticator::Sha256::update(const std::string_view data) {
  update(data.data(), data.size());
}

void AwsAuthenticator::Sha256::update(const uint8_t *bytes, size_t size) {
  sha256_update(&context_, bytes, size);
}

void AwsAuthenticator::Sha256::update(const char *chars, size_t size) {
  update(reinterpret_cast<const uint8_t *>(chars), size);
}

void AwsAuthenticator::Sha256::update(char c) { update(&c, 1); }

void AwsAuthenticator::Sha256::finalize(uint8_t *out) {
  sha256_final(&context_, out);
}

AwsAuthenticator::HMACSha256::HMACSha256() {
}

AwsAuthenticator::HMACSha256::~HMACSha256() {}

size_t AwsAuthenticator::HMACSha256::length() const {
  return Sha256::LENGTH;
}

void AwsAuthenticator::HMACSha256::init(const std::string &data) {
  init(reinterpret_cast<const uint8_t *>(data.data()), data.size());
}

void AwsAuthenticator::HMACSha256::init(const uint8_t *bytes, size_t size) {
    hmac_sha256_init(&context_, bytes, size);
}

void AwsAuthenticator::HMACSha256::update(const std::string &data) {
  update(reinterpret_cast<const uint8_t *>(data.c_str()), data.size());
}

void AwsAuthenticator::HMACSha256::update(const std::string_view data) {
  update(reinterpret_cast<const uint8_t *>(data.data()), data.size());
}
void AwsAuthenticator::HMACSha256::update(
    std::initializer_list<const std::string *> strings) {
  for (auto &&str : strings) {
    update(*str);
  }
}

void AwsAuthenticator::HMACSha256::update(const uint8_t *bytes, size_t size) {
  hmac_sha256_update(&context_, bytes, size);
}

void AwsAuthenticator::HMACSha256::finalize(uint8_t *out,
                                            unsigned int *out_len) {
  hmac_sha256_final(&context_, out, *out_len);
}