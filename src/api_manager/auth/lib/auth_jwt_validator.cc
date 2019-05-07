// Copyright (C) Extensible Service Proxy Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////
//
#include "src/api_manager/auth/lib/auth_jwt_validator.h"

// Implementation of JWT token verification.

// Support public keys in x509 format or JWK (Json Web Keys).
// -- Sample x509 keys
// {
// "8f3e950b309186540c314ecf348bb14f1784d79d": "-----BEGIN
// CERTIFICATE-----\nMIIDHDCCAgSgAwIBAgIIYJnxRhkHEz8wDQYJKoZIhvcNAQEFBQAwMTEvMC0GA1UE\nAxMmc2VjdXJldG9rZW4uc3lzdGVtLmdzZXJ2aWNlYWNjb3VudC5jb20wHhcNMTUw\nNjE2MDEwMzQxWhcNMTUwNjE3MTQwMzQxWjAxMS8wLQYDVQQDEyZzZWN1cmV0b2tl\nbi5zeXN0ZW0uZ3NlcnZpY2VhY2NvdW50LmNvbTCCASIwDQYJKoZIhvcNAQEBBQAD\nggEPADCCAQoCggEBAMNKd/jkdD+ifIw806pawXZo656ycjL1KB/kUJbPopTzKKxZ\nR/eYJpd5BZIZPnWbXGvoY2kGne8jYJptQLLHr18u7TDVMpnh41jvLWYHXJv8Zd/W\n1HZk4t5mm4+JzZ2WUAx881aiEieO7/cMSIT3VC2I98fMFuEJ8jAWUDWY3KzHsXp0\nlj5lJknFCiESQ8s+UxFYHF/EgS8S2eJBvs2unq1a4NVan/GupA1OB5LrlFXm09Vt\na+dB4gulBrPh0/AslRd36uiXLRFnvAr+EF25WyZsUcq0ANCFx1Rd5z3Fv/5zC9hw\n3EeHEpc+NgovzPJ+IDHfqiU4BLTPgT70DYeLHUcCAwEAAaM4MDYwDAYDVR0TAQH/\nBAIwADAOBgNVHQ8BAf8EBAMCB4AwFgYDVR0lAQH/BAwwCgYIKwYBBQUHAwIwDQYJ\nKoZIhvcNAQEFBQADggEBAK2b2Mw5W0BtXS+onaKyyvC9O2Ysh7gTjjOGTVaTpYaB\nDg2vDgqFHM5xeYwMUf8O163rZz4R/DusZ5GsNav9BSsco9VsaIp5oIuy++tepnVN\niAdzK/bH/mo6w0s/+q46v3yhSE2Yd9WzKS9eSfQ6Yw/6y1rCgygTIVsdtKwN2u9L\nXj3RQGcHk7tgrICETqeMULZWMJQG2webNAu8bqkONgo+JP54QNVCzWgCznbCbmOR\nExlMusHMG60j8CxmMG/WLUhX+46m5HxVVx29AH8RhqpwmeFs17QXpGjOMW+ZL/Vf\nwmtv14KGfeX0z2A2iQAP5w6R1r6c+HWizj80mXHWI5U=\n-----END
// CERTIFICATE-----\n",
// "0f980915096a38d8de8d7398998c7fb9152e14fc": "-----BEGIN
// CERTIFICATE-----\nMIIDHDCCAgSgAwIBAgIIVhHsCCeHFBowDQYJKoZIhvcNAQEFBQAwMTEvMC0GA1UE\nAxMmc2VjdXJldG9rZW4uc3lzdGVtLmdzZXJ2aWNlYWNjb3VudC5jb20wHhcNMTUw\nNjE3MDA0ODQxWhcNMTUwNjE4MTM0ODQxWjAxMS8wLQYDVQQDEyZzZWN1cmV0b2tl\nbi5zeXN0ZW0uZ3NlcnZpY2VhY2NvdW50LmNvbTCCASIwDQYJKoZIhvcNAQEBBQAD\nggEPADCCAQoCggEBAKbpeArSNTeYN977uD7GxgFhjghjsTFAq52IW04BdoXQmT9G\nP38s4q06UKGjaZbvEQmdxS+IX2BvswHxiOOgA210C4vRIBu6k1fAnt4JYBy1QHf8\n6C4K9cArp5Sx7/NJcTyu0cj/Ce1fi2iKcvuaQG7+e6VsERWjCFoUHbBohx9a92ch\nMVzQU3Bp8Ix6err6gsxxX8AcrgTN9Ux1Z2x6Ahd/x6Id2HkP8N4dGq72ksk1T9y6\n+Q8LmCzgILSyKvtVVW9G44neFDQvcvJyQljfM996b03yur4XRBs3dPS9AyJlGuN3\nagxBLwM2ieXyM73Za8khlR8PJMUcy4vA6zVHQeECAwEAAaM4MDYwDAYDVR0TAQH/\nBAIwADAOBgNVHQ8BAf8EBAMCB4AwFgYDVR0lAQH/BAwwCgYIKwYBBQUHAwIwDQYJ\nKoZIhvcNAQEFBQADggEBAJyWqYryl4K/DS0RCD66V4v8pZ5ja80Dr/dp8U7cH3Xi\nxHfCqtR5qmKZC48CHg4OPD9WSiVYFsDGLJn0KGU85AXghqcMSJN3ffABNMZBX5a6\nQGFjnvL4b2wZGgqXdaxstJ6pnaM6lQ5J6ZwFTMf0gaeo+jkwx9ENPRLudD5Mf91z\nLvLdp+gRWAlZ3Avo1YTG916kMnGRRwNJ7xgy1YSsEOUzzsNlQWca/XdGmj1I3BW/\nimaI/QRPePs3LlpPVtgu5jvOqyRpaNaYNQU7ki+jdEU4ZDOAvteqd5svXitfpB+a\nx5Bj4hUbZhw2U9AMMDmknhH4w3JKeKYGcdQrO/qVWFQ=\n-----END
// CERTIFICATE-----\n"
// }
//
// -- Sample JWK keys
// {
// "keys": [
//  {
//   "kty": "RSA",
//   "alg": "RS256",
//   "use": "sig",
//   "kid": "62a93512c9ee4c7f8067b5a216dade2763d32a47",
//   "n":
//   "0YWnm_eplO9BFtXszMRQNL5UtZ8HJdTH2jK7vjs4XdLkPW7YBkkm_2xNgcaVpkW0VT2l4mU3KftR-6s3Oa5Rnz5BrWEUkCTVVolR7VYksfqIB2I_x5yZHdOiomMTcm3DheUUCgbJRv5OKRnNqszA4xHn3tA3Ry8VO3X7BgKZYAUh9fyZTFLlkeAh0-bLK5zvqCmKW5QgDIXSxUTJxPjZCgfx1vmAfGqaJb-nvmrORXQ6L284c73DUL7mnt6wj3H6tVqPKA27j56N0TB1Hfx4ja6Slr8S4EB3F1luYhATa1PKUSH8mYDW11HolzZmTQpRoLV8ZoHbHEaTfqX_aYahIw",
//   "e": "AQAB"
//  },
//  {
//   "kty": "RSA",
//   "alg": "RS256",
//   "use": "sig",
//  "kid": "b3319a147514df7ee5e4bcdee51350cc890cc89e",
//   "n":
//   "qDi7Tx4DhNvPQsl1ofxxc2ePQFcs-L0mXYo6TGS64CY_2WmOtvYlcLNZjhuddZVV2X88m0MfwaSA16wE-RiKM9hqo5EY8BPXj57CMiYAyiHuQPp1yayjMgoE1P2jvp4eqF-BTillGJt5W5RuXti9uqfMtCQdagB8EC3MNRuU_KdeLgBy3lS3oo4LOYd-74kRBVZbk2wnmmb7IhP9OoLc1-7-9qU1uhpDxmE6JwBau0mDSwMnYDS4G_ML17dC-ZDtLd1i24STUw39KH0pcSdfFbL2NtEZdNeam1DDdk0iUtJSPZliUHJBI_pj8M-2Mn_oA8jBuI8YKwBqYkZCN1I95Q",
//   "e": "AQAB"
//  }
// ]
// }

#include "absl/strings/str_cat.h"

extern "C" {
#include <grpc/support/alloc.h>
#include <grpc/support/log.h>
}

#include "grpc_internals.h"

#include <openssl/ec.h>
#include <openssl/hmac.h>
#include <openssl/pem.h>
#include <cstring>
#include <set>
#include <string>

#include "src/api_manager/auth/lib/json_util.h"

using ::google::protobuf::util::error::Code;
using std::chrono::system_clock;

namespace google {
namespace api_manager {
namespace auth {
namespace {

// JOSE header. see http://tools.ietf.org/html/rfc7515#section-4
struct JoseHeader {
  const char *alg;
  const char *kid;
};

// An implementation of JwtValidator, hold ALL allocated memory data.
class JwtValidatorImpl : public JwtValidator {
 public:
  JwtValidatorImpl(const char *jwt, size_t jwt_len);
  Status Parse(UserInfo *user_info);
  Status VerifySignature(const char *pkey, size_t pkey_len);
  system_clock::time_point &GetExpirationTime() { return exp_; }
  ~JwtValidatorImpl();

 private:
  Status ParseImpl();
  Status VerifySignatureImpl(const char *pkey, size_t pkey_len);
  // Parses the audiences and removes the audiences from the json object.
  void UpdateAudience(grpc_json *json);

  // Creates header_ from header_json_.
  Status CreateJoseHeader();
  // Checks required fields and fills User Info from claims_.
  // And sets expiration time to exp_.
  Status FillUserInfoAndSetExp(UserInfo *user_info);
  // Finds the public key and verifies JWT signature with it.
  Status FindAndVerifySignature();
  // Extracts the public key from x509 string (key) and sets it to pkey_.
  Status ExtractPubkeyFromX509(const char *key);
  // Extracts the public key from a jwk key (jkey) and sets it to pkey_.
  Status ExtractPubkeyFromJwk(const grpc_json *jkey);
  Status ExtractPubkeyFromJwkRSA(const grpc_json *jkey);
  Status ExtractPubkeyFromJwkEC(const grpc_json *jkey);
  // Extracts the public key from jwk key set and verifies JWT signature with
  // it.
  Status ExtractAndVerifyJwkKeys(const grpc_json *jwt_keys);
  // Extracts the public key from pkey_json_ and verifies JWT signature with
  // it.
  Status ExtractAndVerifyX509Keys();
  // Verifies signature with public key.
  Status VerifyPubkey(bool log_error);
  Status VerifyPubkeyRSA(bool log_error);
  Status VerifyPubkeyEC(bool log_error);
  // Verifies asymmetric signature, including RS256/384/512 and ES256.
  Status VerifyAsymSignature(const char *pkey, size_t pkey_len);
  // Verifies HS (symmetric) signature.
  Status VerifyHsSignature(const char *pkey, size_t pkey_len);

  // Not owned.
  const char *jwt;
  int jwt_len;

  JoseHeader *header_;
  grpc_json *header_json_;
  grpc_slice header_buffer_;
  grpc_jwt_claims *claims_;
  grpc_slice sig_buffer_;
  grpc_slice signed_buffer_;

  std::set<std::string> audiences_;
  system_clock::time_point exp_;

  grpc_json *pkey_json_;
  grpc_slice pkey_buffer_;
  BIO *bio_;
  X509 *x509_;
  RSA *rsa_;
  EVP_PKEY *pkey_;
  EVP_MD_CTX *md_ctx_;
  EC_KEY *eck_;
  ECDSA_SIG *ecdsa_sig_;
};

// Gets EVP_MD mapped from an alg (algorithm string).
const EVP_MD *EvpMdFromAlg(const char *alg);

// Gets hash size from HS algorithm string.
size_t HashSizeFromAlg(const char *alg);

// Parses str into grpc_json object. Does not own buffer.
Status DecodeBase64AndParseJson(const char *str, size_t len, grpc_slice *buffer,
                                const char *section_name,
                                grpc_json **output_json);

// Gets BIGNUM from b64 string, used for extracting pkey from jwk.
// Result owned by rsa_.
BIGNUM *BigNumFromBase64String(const char *b64);

// Two helper functions to generate Status
Status ToStatus(const std::string &error_msg) {
  return Status(Code::UNAUTHENTICATED, error_msg);
}

Status ToStatus(grpc_jwt_verifier_status grpc_status) {
  return Status(Code::UNAUTHENTICATED,
                grpc_jwt_verifier_status_to_string(grpc_status));
}

}  // namespace

std::unique_ptr<JwtValidator> JwtValidator::Create(const char *jwt,
                                                   size_t jwt_len) {
  return std::unique_ptr<JwtValidator>(new JwtValidatorImpl(jwt, jwt_len));
}

namespace {
JwtValidatorImpl::JwtValidatorImpl(const char *jwt, size_t jwt_len)
    : jwt(jwt),
      jwt_len(jwt_len),
      header_(nullptr),
      header_json_(nullptr),
      claims_(nullptr),
      pkey_json_(nullptr),
      bio_(nullptr),
      x509_(nullptr),
      rsa_(nullptr),
      pkey_(nullptr),
      md_ctx_(nullptr),
      eck_(nullptr),
      ecdsa_sig_(nullptr) {
  header_buffer_ = grpc_empty_slice();
  signed_buffer_ = grpc_empty_slice();
  sig_buffer_ = grpc_empty_slice();
  pkey_buffer_ = grpc_empty_slice();
}

// Makes sure all data are cleaned up, both success and failure case.
JwtValidatorImpl::~JwtValidatorImpl() {
  if (header_ != nullptr) {
    gpr_free(header_);
  }
  if (header_json_ != nullptr) {
    grpc_json_destroy(header_json_);
  }
  if (pkey_json_ != nullptr) {
    grpc_json_destroy(pkey_json_);
  }
  if (claims_ != nullptr) {
    grpc_jwt_claims_destroy(claims_);
  }
  if (!GRPC_SLICE_IS_EMPTY(header_buffer_)) {
    grpc_slice_unref(header_buffer_);
  }
  if (!GRPC_SLICE_IS_EMPTY(signed_buffer_)) {
    grpc_slice_unref(signed_buffer_);
  }
  if (!GRPC_SLICE_IS_EMPTY(sig_buffer_)) {
    grpc_slice_unref(sig_buffer_);
  }
  if (!GRPC_SLICE_IS_EMPTY(pkey_buffer_)) {
    grpc_slice_unref(pkey_buffer_);
  }
  if (bio_ != nullptr) {
    BIO_free(bio_);
  }
  if (x509_ != nullptr) {
    X509_free(x509_);
  }
  if (rsa_ != nullptr) {
    RSA_free(rsa_);
  }
  if (pkey_ != nullptr) {
    EVP_PKEY_free(pkey_);
  }
  if (md_ctx_ != nullptr) {
    EVP_MD_CTX_destroy(md_ctx_);
  }
  if (eck_ != nullptr) {
    EC_KEY_free(eck_);
  }
  if (ecdsa_sig_ != nullptr) {
    ECDSA_SIG_free(ecdsa_sig_);
  }
}

Status JwtValidatorImpl::Parse(UserInfo *user_info) {
  auto status = ParseImpl();
  if (status.ok()) {
    status = FillUserInfoAndSetExp(user_info);
  }

  return status;
}

// Extracts and removes the audiences from the token.
// This is a workaround to deal with GRPC library not accepting
// multiple audiences.
void JwtValidatorImpl::UpdateAudience(grpc_json *json) {
  grpc_json *cur;
  for (cur = json->child; cur != nullptr; cur = cur->next) {
    if (strcmp(cur->key, "aud") == 0) {
      if (cur->type == GRPC_JSON_ARRAY) {
        grpc_json *aud;
        for (aud = cur->child; aud != nullptr; aud = aud->next) {
          if (aud->type == GRPC_JSON_STRING && aud->value != nullptr) {
            audiences_.insert(aud->value);
          }
        }
        // Replaces the array of audiences with an empty string.
        grpc_json *prev = cur->prev;
        grpc_json *next = cur->next;
        grpc_json_destroy(cur);
        grpc_json *fake_audience = grpc_json_create(GRPC_JSON_STRING);
        fake_audience->key = "aud";
        fake_audience->value = "";
        fake_audience->parent = json;
        fake_audience->prev = prev;
        fake_audience->next = next;
        if (prev) {
          prev->next = fake_audience;
        } else {
          json->child = fake_audience;
        }
        if (next) {
          next->prev = fake_audience;
        }
      } else if (cur->type == GRPC_JSON_STRING && cur->value != nullptr) {
        audiences_.insert(cur->value);
      }
      return;
    }
  }
}

Status JwtValidatorImpl::ParseImpl() {
  // ====================
  // Basic check.
  // ====================
  if (jwt == nullptr || jwt_len <= 0) {
    return ToStatus("Bad JWT format: empty token");
  }

  // ====================
  // Creates Jose Header.
  // ====================
  const char *cur = jwt;
  const char *dot = strchr(cur, '.');
  if (dot == nullptr) {
    return ToStatus("Bad JWT format: should have 2 dots");
  }
  auto status = DecodeBase64AndParseJson(cur, dot - cur, &header_buffer_,
                                         "header", &header_json_);
  if (!status.ok()) {
    return status;
  }
  status = CreateJoseHeader();
  if (!status.ok()) {
    return status;
  }

  // =============================
  // Creates Claims/Payload.
  // =============================
  cur = dot + 1;
  dot = strchr(cur, '.');
  if (dot == nullptr) {
    return ToStatus("Bad JWT format: should have 2 dots");
  }

  // claim_buffer is the only exception that requires deallocation for failure
  // case, and it is owned by claims_ for successful case.
  grpc_slice claims_buffer = grpc_empty_slice();
  grpc_json *claims_json;
  status = DecodeBase64AndParseJson(cur, dot - cur, &claims_buffer, "claims",
                                    &claims_json);
  if (!status.ok()) {
    if (!GRPC_SLICE_IS_EMPTY(claims_buffer)) {
      grpc_slice_unref(claims_buffer);
    }
    return status;
  }

  UpdateAudience(claims_json);

  // Takes ownershp of claims_json and claims_buffer.
  claims_ = grpc_jwt_claims_from_json(claims_json, claims_buffer);

  if (claims_ == nullptr) {
    gpr_log(GPR_ERROR,
            "JWT claims could not be created."
            " Incompatible value types for some claim(s)");
    return ToStatus(
        "Bad JWT format: invalid JWT claims; e.g. wrong data type, iss is not "
        "a string.");
  }

  // issuer is mandatory. grpc_jwt_claims_issuer checks if claims_ is nullptr.
  if (grpc_jwt_claims_issuer(claims_) == nullptr) {
    return ToStatus(
        "Bad JWT format: invalid JWT claims; issuer is mssing but required.");
  }

  // Check timestamp.
  // Passing in its own audience to skip audience check.
  // Audience check should be done by the caller.
  grpc_jwt_verifier_status grpc_status =
      grpc_jwt_claims_check(claims_, grpc_jwt_claims_audience(claims_));
  if (grpc_status != GRPC_JWT_VERIFIER_OK) {
    return ToStatus(grpc_status);
  }

  // =============================
  // Creates Buffer for signature check
  // =============================
  size_t signed_jwt_len = (size_t)(dot - jwt);
  signed_buffer_ = grpc_slice_from_copied_buffer(jwt, signed_jwt_len);
  if (GRPC_SLICE_IS_EMPTY(signed_buffer_)) {
    return ToStatus("Failed to allocate memory");
  }
  cur = dot + 1;
  sig_buffer_ =
      grpc_base64_decode_with_len(cur, jwt_len - signed_jwt_len - 1, 1);
  if (GRPC_SLICE_IS_EMPTY(sig_buffer_)) {
    return ToStatus("Bad JWT format: invalid base64 for signature");
  }
  // Check signature length if the signing algorihtm is ES256. ES256 is the only
  // supported ECDSA signing algorithm.
  if (strncmp(header_->alg, "ES256", 5) == 0 &&
      GRPC_SLICE_LENGTH(sig_buffer_) != 2 * 32) {
    gpr_log(GPR_ERROR, "ES256 signature length is not correct.");
    return ToStatus("Bad JWT format: signature length is not correct");
  }

  return Status::OK;
}

Status JwtValidatorImpl::VerifySignature(const char *pkey, size_t pkey_len) {
  return VerifySignatureImpl(pkey, pkey_len);
}

Status JwtValidatorImpl::VerifySignatureImpl(const char *pkey,
                                             size_t pkey_len) {
  if (pkey == nullptr || pkey_len <= 0) {
    return ToStatus("Bad public key format: Public key is empty");
  }
  if (jwt == nullptr || jwt_len <= 0) {
    return ToStatus("Bad JWT format: JWT is empty");
  }
  if (GRPC_SLICE_IS_EMPTY(signed_buffer_) || GRPC_SLICE_IS_EMPTY(sig_buffer_)) {
    return ToStatus("Bad JWT format: JWT signature is empty");
  }
  if (strncmp(header_->alg, "ES256", 5) == 0 ||
      strncmp(header_->alg, "RS", 2) == 0) {  // Asymmetric keys.
    return VerifyAsymSignature(pkey, pkey_len);
  } else {  // Symmetric key.
    return VerifyHsSignature(pkey, pkey_len);
  }
}

Status JwtValidatorImpl::CreateJoseHeader() {
  const char *alg = GetStringValue(header_json_, "alg");
  if (alg == nullptr) {
    gpr_log(GPR_ERROR, "Missing alg field.");
    return ToStatus("Missing alg field.");
  }
  if (EvpMdFromAlg(alg) == nullptr && strncmp(alg, "ES256", 5) != 0) {
    gpr_log(GPR_ERROR, "Invalid alg field [%s].", alg);
    return ToStatus("Failed to allocate memory.");
  }

  header_ = reinterpret_cast<JoseHeader *>(gpr_malloc(sizeof(JoseHeader)));
  if (header_ == nullptr) {
    gpr_log(GPR_ERROR, "Jose header creation failed");
    return ToStatus("Failed to allocate memory.");
  }
  header_->alg = alg;
  header_->kid = GetStringValue(header_json_, "kid");
  return Status::OK;
}

Status JwtValidatorImpl::FindAndVerifySignature() {
  if (pkey_json_ == nullptr) {
    gpr_log(GPR_ERROR, "The public keys are empty.");
    return ToStatus("The public keys are empty.");
  }

  if (header_ == nullptr) {
    gpr_log(GPR_ERROR, "JWT header is empty.");
    return ToStatus("Bad JWT format: JWT header is empty.");
  }
  // JWK set https://tools.ietf.org/html/rfc7517#section-5.
  const grpc_json *jwk_keys = GetProperty(pkey_json_, "keys");
  if (jwk_keys == nullptr) {
    // Currently we only support JWK format for ES256.
    if (strncmp(header_->alg, "ES256", 5) == 0) {
      return ToStatus("Invalid public key: keys field is missing.");
    }
    // Try x509 format.
    return ExtractAndVerifyX509Keys();
  } else {
    // JWK format.
    return ExtractAndVerifyJwkKeys(jwk_keys);
  }
}

Status JwtValidatorImpl::ExtractAndVerifyX509Keys() {
  // Precondition (checked by caller): pkey_json_ and header_ are not nullptr.
  if (header_->kid != nullptr) {
    const char *value = GetStringValue(pkey_json_, header_->kid);
    if (value == nullptr) {
      gpr_log(GPR_ERROR,
              "Cannot find matching key in key set for kid=%s and alg=%s",
              header_->kid, header_->alg);
      return ToStatus(
          absl::StrCat("Could not find matching key in public key set for kid=",
                       header_->kid));
    }
    if (!ExtractPubkeyFromX509(value).ok()) {
      gpr_log(GPR_ERROR, "Failed to extract public key from X509 key (%s)",
              header_->kid);
      return ToStatus(absl::StrCat(
          "Failed to extract public key from X509 for kid=", header_->kid));
    }
    return VerifyPubkey(/*log_error=*/true);
  }
  // If kid is not specified in the header, try all keys. If the JWT can be
  // validated with any of the keys, the request is successful.
  const grpc_json *cur;
  if (pkey_json_->child == nullptr) {
    gpr_log(GPR_ERROR, "Failed to extract public key from X509 key (%s)",
            header_->kid);
    return ToStatus(absl::StrCat(
        "Failed to extract public key from X509 for kid=", header_->kid));
  }
  for (cur = pkey_json_->child; cur != nullptr; cur = cur->next) {
    if (cur->value == nullptr || !ExtractPubkeyFromX509(cur->value).ok()) {
      // Failed to extract public key from current X509 key, try next one.
      continue;
    }
    if (VerifyPubkey(/*log_error=*/false).ok()) {
      return Status::OK;
    }
  }
  // header_->kid is nullptr. The JWT cannot be validated with any of the keys.
  // Return error.
  gpr_log(GPR_ERROR,
          "The JWT cannot be validated with any of the public keys.");
  return ToStatus("The JWT cannot be validated with any of the public keys.");
}

Status JwtValidatorImpl::ExtractPubkeyFromX509(const char *key) {
  if (bio_ != nullptr) {
    BIO_free(bio_);
  }
  bio_ = BIO_new(BIO_s_mem());
  if (bio_ == nullptr) {
    gpr_log(GPR_ERROR, "Unable to allocate a BIO object.");
    return ToStatus("Unable to allocate a BIO object.");
  }
  if (BIO_write(bio_, key, strlen(key)) <= 0) {
    gpr_log(GPR_ERROR, "BIO write error for key (%s).", key);
    return ToStatus(absl::StrCat("BIO write error for key", key));
  }
  if (x509_ != nullptr) {
    X509_free(x509_);
  }
  x509_ = PEM_read_bio_X509(bio_, nullptr, nullptr, nullptr);
  if (x509_ == nullptr) {
    gpr_log(GPR_ERROR, "Unable to parse x509 cert for key (%s).", key);
    return ToStatus(absl::StrCat("Unable to parse x509 cert for key", key));
  }
  if (pkey_ != nullptr) {
    EVP_PKEY_free(pkey_);
  }
  pkey_ = X509_get_pubkey(x509_);
  if (pkey_ == nullptr) {
    gpr_log(GPR_ERROR, "X509_get_pubkey failed");
    return ToStatus("X509_get_pubkey failed");
  }
  return Status::OK;
}

Status JwtValidatorImpl::ExtractAndVerifyJwkKeys(const grpc_json *jwk_keys) {
  // Precondition (checked by caller): jwk_keys and header_ are not nullptr.
  if (jwk_keys->type != GRPC_JSON_ARRAY) {
    gpr_log(GPR_ERROR,
            "Unexpected value type of keys property in jwks key set.");
    return ToStatus("Unexpected value type of keys property in jwks key set.");
  }

  const grpc_json *jkey = nullptr;

  if (jwk_keys->child == nullptr) {
    gpr_log(GPR_ERROR, "The jwks key set is empty");
    return ToStatus("The jwks key set is empty");
  }

  // JWK format from https://tools.ietf.org/html/rfc7518#section-6.
  for (jkey = jwk_keys->child; jkey != nullptr; jkey = jkey->next) {
    if (jkey->type != GRPC_JSON_OBJECT) continue;
    const char *kid = GetStringValue(jkey, "kid");
    if (kid == nullptr ||
        (header_->kid != nullptr && strcmp(kid, header_->kid) != 0)) {
      continue;
    }
    const char *kty = GetStringValue(jkey, "kty");
    if (kty == nullptr ||
        (strncmp(header_->alg, "RS", 2) == 0 && strncmp(kty, "RSA", 3) != 0) ||
        (strncmp(header_->alg, "ES256", 5) == 0 &&
         strncmp(kty, "EC", 2) != 0)) {
      gpr_log(GPR_ERROR, "Missing or unsupported key type %s.", kty);
      continue;
    }

    if (!ExtractPubkeyFromJwk(jkey).ok()) {
      // Failed to extract public key from this Jwk key.
      continue;
    }

    if (header_->kid != nullptr) {
      return VerifyPubkey(/*log_error=*/true);
    }
    // If kid is not specified in the header, try all keys. If the JWT can be
    // validated with any of the keys, the request is successful.
    if (VerifyPubkey(/*log_error=*/false).ok()) {
      return Status::OK;
    }
  }

  if (header_->kid != nullptr) {
    gpr_log(GPR_ERROR,
            "Cannot find matching key in key set for kid=%s and alg=%s",
            header_->kid, header_->alg);
    return ToStatus(absl::StrCat("Cannot find matching key in key set for kid=",
                                 header_->kid));
  }
  // header_->kid is nullptr. The JWT cannot be validated with any of the keys.
  // Return error.
  gpr_log(GPR_ERROR,
          "The JWT cannot be validated with any of the public keys.");
  return ToStatus("The JWT cannot be validated with any of the public keys.");
}

Status JwtValidatorImpl::ExtractPubkeyFromJwk(const grpc_json *jkey) {
  if (strncmp(header_->alg, "RS", 2) == 0) {
    return ExtractPubkeyFromJwkRSA(jkey);
  } else if (strncmp(header_->alg, "ES256", 5) == 0) {
    return ExtractPubkeyFromJwkEC(jkey);
  } else {
    return ToStatus(absl::StrCat("Not supported alg ", header_->alg));
  }
}

Status JwtValidatorImpl::ExtractPubkeyFromJwkRSA(const grpc_json *jkey) {
  if (rsa_ != nullptr) {
    RSA_free(rsa_);
  }
  rsa_ = RSA_new();
  if (rsa_ == nullptr) {
    gpr_log(GPR_ERROR, "Could not create rsa key.");
    return ToStatus("Could not create rsa key.");
  }

  const char *rsa_n = GetStringValue(jkey, "n");
  rsa_->n = rsa_n == nullptr ? nullptr : BigNumFromBase64String(rsa_n);
  const char *rsa_e = GetStringValue(jkey, "e");
  rsa_->e = rsa_e == nullptr ? nullptr : BigNumFromBase64String(rsa_e);

  if (rsa_->e == nullptr || rsa_->n == nullptr) {
    gpr_log(GPR_ERROR, "Missing RSA public key field.");
    return ToStatus("Missing RSA public key field.");
  }

  if (pkey_ != nullptr) {
    EVP_PKEY_free(pkey_);
  }
  pkey_ = EVP_PKEY_new();
  if (EVP_PKEY_set1_RSA(pkey_, rsa_) == 0) {
    gpr_log(GPR_ERROR, "EVP_PKEY_ste1_RSA failed");
    return ToStatus("EVP_PKEY_ste1_RSA failed");
  }
  return Status::OK;
}

Status JwtValidatorImpl::ExtractPubkeyFromJwkEC(const grpc_json *jkey) {
  if (eck_ != nullptr) {
    EC_KEY_free(eck_);
  }
  eck_ = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  if (eck_ == nullptr) {
    gpr_log(GPR_ERROR, "Could not create ec key.");
    return ToStatus("Could not create ec key.");
  }
  const char *eck_x = GetStringValue(jkey, "x");
  const char *eck_y = GetStringValue(jkey, "y");
  if (eck_x == nullptr || eck_y == nullptr) {
    gpr_log(GPR_ERROR, "Missing EC public key field.");
    return ToStatus("Missing EC public key field.");
  }
  BIGNUM *bn_x = BigNumFromBase64String(eck_x);
  BIGNUM *bn_y = BigNumFromBase64String(eck_y);
  if (bn_x == nullptr || bn_y == nullptr) {
    gpr_log(GPR_ERROR, "Could not generate BIGNUM-type x and y fields.");
    return ToStatus("Could not generate BIGNUM-type x and y fields.");
  }

  if (EC_KEY_set_public_key_affine_coordinates(eck_, bn_x, bn_y) == 0) {
    BN_free(bn_x);
    BN_free(bn_y);
    gpr_log(GPR_ERROR, "Could not populate ec key coordinates.");
    return ToStatus("Could not populate ec key coordinates.");
  }
  BN_free(bn_x);
  BN_free(bn_y);
  return Status::OK;
}

Status JwtValidatorImpl::VerifyAsymSignature(const char *pkey,
                                             size_t pkey_len) {
  pkey_buffer_ = grpc_slice_from_copied_buffer(pkey, pkey_len);
  if (GRPC_SLICE_IS_EMPTY(pkey_buffer_)) {
    return ToStatus("Failed to allocate memory");
  }
  pkey_json_ = grpc_json_parse_string_with_len(
      reinterpret_cast<char *>(GRPC_SLICE_START_PTR(pkey_buffer_)),
      GRPC_SLICE_LENGTH(pkey_buffer_));
  if (pkey_json_ == nullptr) {
    return ToStatus("Invalid JSON for public key");
  }

  return FindAndVerifySignature();
}

Status JwtValidatorImpl::VerifyPubkey(bool log_error) {
  if (strncmp(header_->alg, "RS", 2) == 0) {
    return VerifyPubkeyRSA(log_error);
  } else if (strncmp(header_->alg, "ES256", 5) == 0) {
    return VerifyPubkeyEC(log_error);
  } else {
    return ToStatus(absl::StrCat("Not supported alg ", header_->alg));
  }
}

Status JwtValidatorImpl::VerifyPubkeyEC(bool log_error) {
  if (eck_ == nullptr) {
    gpr_log(GPR_ERROR, "Cannot find eck.");
    return ToStatus("Cannot find eck.");
  }

  uint8_t digest[SHA256_DIGEST_LENGTH];
  SHA256(
      reinterpret_cast<const uint8_t *>(GRPC_SLICE_START_PTR(signed_buffer_)),
      GRPC_SLICE_LENGTH(signed_buffer_), digest);

  if (ecdsa_sig_ != nullptr) {
    ECDSA_SIG_free(ecdsa_sig_);
  }
  ecdsa_sig_ = ECDSA_SIG_new();
  if (ecdsa_sig_ == nullptr) {
    gpr_log(GPR_ERROR, "Could not create ECDSA_SIG.");
    return ToStatus("Could not create ECDSA_SIG.");
  }

  BN_bin2bn(GRPC_SLICE_START_PTR(sig_buffer_), 32, ecdsa_sig_->r);
  BN_bin2bn(GRPC_SLICE_START_PTR(sig_buffer_) + 32, 32, ecdsa_sig_->s);
  if (ECDSA_do_verify(digest, SHA256_DIGEST_LENGTH, ecdsa_sig_, eck_) == 0) {
    if (log_error) {
      gpr_log(GPR_ERROR, "JWT signature verification failed.");
    }
    ERR_clear_error();
    return ToStatus("JWT signature verification failed.");
  }
  return Status::OK;
}

Status JwtValidatorImpl::VerifyPubkeyRSA(bool log_error) {
  if (pkey_ == nullptr) {
    gpr_log(GPR_ERROR, "Cannot find public key.");
    return ToStatus("Cannot find public key.");
  }
  if (md_ctx_ != nullptr) {
    EVP_MD_CTX_destroy(md_ctx_);
  }
  md_ctx_ = EVP_MD_CTX_create();
  if (md_ctx_ == nullptr) {
    gpr_log(GPR_ERROR, "Could not create EVP_MD_CTX.");
    return ToStatus("Could not create EVP_MD_CTX.");
  }
  const EVP_MD *md = EvpMdFromAlg(header_->alg);
  GPR_ASSERT(md != nullptr);  // Checked before.

  if (EVP_DigestVerifyInit(md_ctx_, nullptr, md, nullptr, pkey_) != 1) {
    gpr_log(GPR_ERROR, "EVP_DigestVerifyInit failed.");
    return ToStatus("EVP_DigestVerifyInit failed.");
  }
  if (EVP_DigestVerifyUpdate(md_ctx_, GRPC_SLICE_START_PTR(signed_buffer_),
                             GRPC_SLICE_LENGTH(signed_buffer_)) != 1) {
    gpr_log(GPR_ERROR, "EVP_DigestVerifyUpdate failed.");
    return ToStatus("EVP_DigestVerifyUpdate failed.");
  }
  if (EVP_DigestVerifyFinal(md_ctx_, GRPC_SLICE_START_PTR(sig_buffer_),
                            GRPC_SLICE_LENGTH(sig_buffer_)) != 1) {
    if (log_error) {
      gpr_log(GPR_ERROR, "JWT signature verification failed.");
    }
    ERR_clear_error();
    return ToStatus("JWT signature verification failed.");
  }
  return Status::OK;
}

Status JwtValidatorImpl::VerifyHsSignature(const char *pkey, size_t pkey_len) {
  const EVP_MD *md = EvpMdFromAlg(header_->alg);
  GPR_ASSERT(md != nullptr);  // Checked before.

  pkey_buffer_ = grpc_base64_decode_with_len(pkey, pkey_len, 1);
  if (GRPC_SLICE_IS_EMPTY(pkey_buffer_)) {
    gpr_log(GPR_ERROR, "Unable to decode base64 of secret");
    return ToStatus(
        "Invalid base64 encoded HS symmetric key. "
        "Most likely JWT alg is HS, but public key is ES or RSA.");
  }

  unsigned char res[HashSizeFromAlg(header_->alg)];
  unsigned int res_len = 0;
  HMAC(md, GRPC_SLICE_START_PTR(pkey_buffer_), GRPC_SLICE_LENGTH(pkey_buffer_),
       GRPC_SLICE_START_PTR(signed_buffer_), GRPC_SLICE_LENGTH(signed_buffer_),
       res, &res_len);
  if (res_len == 0) {
    gpr_log(GPR_ERROR, "Cannot compute HMAC from secret.");
    return ToStatus("Cannot compute HMAC from the public key.");
  }

  if (res_len != GRPC_SLICE_LENGTH(sig_buffer_) ||
      CRYPTO_memcmp(reinterpret_cast<void *>(GRPC_SLICE_START_PTR(sig_buffer_)),
                    reinterpret_cast<void *>(res), res_len) != 0) {
    gpr_log(GPR_ERROR, "JWT signature verification failed.");
    return ToStatus("JWT signature verification failed.");
  }
  return Status::OK;
}

Status JwtValidatorImpl::FillUserInfoAndSetExp(UserInfo *user_info) {
  // Required fields.
  const char *issuer = grpc_jwt_claims_issuer(claims_);
  if (issuer == nullptr) {
    gpr_log(GPR_ERROR, "Missing issuer field.");
    return ToStatus("Bad JWT format: missing issuer field.");
  }
  if (audiences_.empty()) {
    gpr_log(GPR_ERROR, "Missing audience field.");
    return ToStatus("Bad JWT format: missing audience field.");
  }
  const char *subject = grpc_jwt_claims_subject(claims_);
  if (subject == nullptr) {
    gpr_log(GPR_ERROR, "Missing subject field.");
    return ToStatus("Bad JWT format: missing subject field.");
  }
  user_info->issuer = issuer;
  user_info->audiences = audiences_;
  user_info->id = subject;

  // Optional field.
  const grpc_json *grpc_json = grpc_jwt_claims_json(claims_);

  char *json_str =
      grpc_json_dump_to_string(const_cast<::grpc_json *>(grpc_json), 0);
  if (json_str != nullptr) {
    user_info->claims = json_str;
    gpr_free(json_str);
  }

  const char *email = GetStringValue(grpc_json, "email");
  user_info->email = email == nullptr ? "" : email;
  const char *authorized_party = GetStringValue(grpc_json, "azp");
  user_info->authorized_party =
      authorized_party == nullptr ? "" : authorized_party;
  exp_ = system_clock::from_time_t(grpc_jwt_claims_expires_at(claims_).tv_sec);

  return Status::OK;
}

const EVP_MD *EvpMdFromAlg(const char *alg) {
  if (strcmp(alg, "RS256") == 0 || strcmp(alg, "HS256") == 0) {
    return EVP_sha256();
  } else if (strcmp(alg, "RS384") == 0 || strcmp(alg, "HS384") == 0) {
    return EVP_sha384();
  } else if (strcmp(alg, "RS512") == 0 || strcmp(alg, "HS512") == 0) {
    return EVP_sha512();
  } else {
    return nullptr;
  }
}

// Gets hash byte size from HS algorithm string.
size_t HashSizeFromAlg(const char *alg) {
  if (strcmp(alg, "HS256") == 0) {
    return 32;
  } else if (strcmp(alg, "HS384") == 0) {
    return 48;
  } else if (strcmp(alg, "HS512") == 0) {
    return 64;
  } else {
    return 0;
  }
}

Status DecodeBase64AndParseJson(const char *str, size_t len, grpc_slice *buffer,
                                const char *section_name,
                                grpc_json **output_json) {
  grpc_json *json;
  *output_json = nullptr;

  *buffer = grpc_base64_decode_with_len(str, len, 1);
  if (GRPC_SLICE_IS_EMPTY(*buffer)) {
    gpr_log(GPR_ERROR, "Invalid base64.");
    return ToStatus(
        absl::StrCat("Bad JWT format: Invalid base64 in ", section_name));
  }
  json = grpc_json_parse_string_with_len(
      reinterpret_cast<char *>(GRPC_SLICE_START_PTR(*buffer)),
      GRPC_SLICE_LENGTH(*buffer));
  if (json == nullptr) {
    gpr_log(GPR_ERROR, "JSON parsing error.");
    return ToStatus(
        absl::StrCat("Bad JWT format: Invalid JSON in ", section_name));
  }
  *output_json = json;
  return Status::OK;
}

BIGNUM *BigNumFromBase64String(const char *b64) {
  BIGNUM *result = nullptr;
  grpc_slice bin;

  if (b64 == nullptr) return nullptr;
  bin = grpc_base64_decode(b64, 1);
  if (GRPC_SLICE_IS_EMPTY(bin)) {
    gpr_log(GPR_ERROR, "Invalid base64 for big num.");
    return nullptr;
  }
  result =
      BN_bin2bn(GRPC_SLICE_START_PTR(bin), GRPC_SLICE_LENGTH(bin), nullptr);
  grpc_slice_unref(bin);
  return result;
}

}  // namespace
}  // namespace auth
}  // namespace api_manager
}  // namespace google
