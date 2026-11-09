// -----------------------------------------------------------------------------
// cxl_spdm_secure.h -- OpenSSL-backed SPDM secure session (model profile).
//
//   Wraps CxlSpdmSession and replaces its model-data responses with real
//   cryptography:
//     * ECDSA P-256 identity + self-signed X.509 certificate;
//     * real ECDSA signature over the model transcript digest (CHALLENGE);
//     * X25519 ephemeral key exchange + HKDF-SHA256 key derivation;
//     * HMAC-SHA256 FINISH verification data.
//
//   The SPDM wire framing is the same model-profile framing used by
//   CxlSpdmSession; only the cryptographic values are real.  This is not a
//   conformance SPDM stack (no full transcript/signature formatting rules),
//   but a customer security-stack integration can replace the wrapper.
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_ide.h"
#include "cxl_spdm.h"

#include <openssl/evp.h>
#include <openssl/x509.h>

#include <array>
#include <cstdint>
#include <vector>

namespace cxl_tlm {

struct CxlSpdmSecureSession {
    CxlSpdmSession session;
    EVP_PKEY* identity = nullptr;      // EC P-256
    X509* certificate = nullptr;
    std::vector<std::uint8_t> cert_der;
    EVP_PKEY* ephemeral = nullptr;     // X25519 for the current key exchange
    std::array<std::uint8_t, 32> session_key{};
    CxlIde* ide = nullptr;             // optional key sink for bind()

    // Diagnostics for the directed bench (values actually signed/MACed).
    std::array<std::uint8_t, 32> last_digest{};
    std::array<std::uint8_t, 32> last_finish{};
    std::array<std::uint8_t, 32> last_finish_digest{};

    CxlSpdmSecureSession();
    ~CxlSpdmSecureSession();
    CxlSpdmSecureSession(const CxlSpdmSecureSession&) = delete;
    CxlSpdmSecureSession& operator=(const CxlSpdmSecureSession&) = delete;

    bool init();
    bool verify_challenge(const std::array<std::uint8_t, 32>& digest,
                          const std::array<std::uint8_t, 64>& sig) const;
    // Verify with the public key extracted from the issued certificate (the
    // path a host would use after validating the certificate chain).
    bool verify_cert(const std::array<std::uint8_t, 32>& digest,
                     const std::array<std::uint8_t, 64>& sig) const;
    // Test/host-side helpers (the same primitives the responder uses).
    static bool x25519_keypair(std::array<std::uint8_t, 32>* pub,
                               std::array<std::uint8_t, 32>* priv);
    static bool x25519_derive(const std::array<std::uint8_t, 32>& priv,
                              const std::array<std::uint8_t, 32>& peer_pub,
                              std::array<std::uint8_t, 32>* out);
    static bool hkdf_sha256(const std::uint8_t* secret, std::size_t n,
                            const char* info,
                            std::array<std::uint8_t, 32>* out);
    static bool hmac_sha256(const std::array<std::uint8_t, 32>& key,
                            const std::uint8_t* data, std::size_t n,
                            std::array<std::uint8_t, 32>* out);

    // Attach the standard hooks to `session`.
    void bind();
};

} // namespace cxl_tlm
