// -----------------------------------------------------------------------------
// cxl_spdm.h -- SPDM session model over DOE (model profile).
//
//   The register/DOE transport is in cxl_doe.h.  This block implements the
//   SPDM request/response flow a CXL device exposes for IDE key establishment:
//
//     GET_VERSION (0x84) -> VERSION (0x04)
//     GET_CAPABILITIES (0xE1) -> CAPABILITIES (0x61)
//     NEGOTIATE_ALGORITHMS (0xE3) -> ALGORITHMS (0x63)
//     GET_DIGESTS (0x81) -> DIGESTS (0x01)
//     GET_CERTIFICATE (0x82) -> CERTIFICATE (0x02)
//     CHALLENGE (0x83) -> CHALLENGE_AUTH (0x03)
//     KEY_EXCHANGE (0xE4) -> KEY_EXCHANGE_RSP (0x64)
//     FINISH (0xE5) -> FINISH_RSP (0x65)
//     HEARTBEAT (0xE9) -> HEARTBEAT_ACK (0x69)
//
//   Scope/boundary: this is a **session and key-plumbing model**, not a
//   cryptographic SPDM implementation.  Digest/certificate/signature bytes
//   are deterministic model data; the derived key is produced by an internal
//   mixing function so the IDE data path can be exercised end to end.  Real
//   SPDM authentication/attestation is a customer security-stack integration
//   item (tracked in the status doc), never claimed here.
// -----------------------------------------------------------------------------
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace cxl_tlm {

struct CxlSpdmSession {
    static constexpr unsigned kDigestBytes = 32;
    static constexpr unsigned kKeyBytes = 32;

    bool enabled = false;                 // enabled by the device profile
    std::uint8_t negotiated_version = 0;  // 0 until NEGOTIATE_ALGORITHMS
    std::uint8_t supported_versions[3] = {0x13, 0x12, 0x11};
    bool capabilities_done = false;
    bool algorithms_done = false;
    bool digests_requested = false;
    bool certificate_requested = false;
    bool challenge_done = false;
    bool key_exchange_done = false;
    bool finished = false;
    bool session_up = false;

    std::array<std::uint8_t, kDigestBytes> digest{};
    std::array<std::uint8_t, kKeyBytes> session_key{};
    std::uint64_t requests = 0, unknown_requests = 0, key_installs = 0;

    // Optional sink for the derived key (the IDE block).
    std::function<bool(unsigned slot, const std::uint8_t* key)> install_key;
    // DOE transport entry point.  Kept as a std::function so the DOE header
    // does not require this translation unit's symbols in every model link.
    std::function<bool(const std::vector<std::uint32_t>&,
                       std::vector<std::uint32_t>*)> handler;

    // Cryptographic extension points.  When unset, the session uses the
    // model-data responses (tb_cxl_spdm).  A secure wrapper (OpenSSL) can
    // supply a real certificate, ECDSA challenge signature, ECDHE key
    // exchange and finish MAC without pulling crypto into this translation
    // unit.
    std::function<std::vector<std::uint8_t>()> cert_provider;
    std::function<std::array<std::uint8_t, 64>(
        const std::array<std::uint8_t, 32>& digest)> challenge_signer;
    std::function<bool(const std::vector<std::uint32_t>& request,
                       std::vector<std::uint32_t>* response,
                       std::array<std::uint8_t, 32>* key)> key_exchange_hook;
    std::function<std::array<std::uint8_t, 32>()> finish_provider;

    // Model transcript digest (FNV-1a expansion; the secure wrapper signs
    // this 32-byte value with a real ECDSA key).
    std::array<std::uint8_t, 32> transcript_digest{};
    void update_digest(const std::uint8_t* p, std::size_t n);

    CxlSpdmSession();

    // Process one SPDM payload (DWs after the DOE object header).  Returns
    // false only for a malformed/unknown request; the response is filled on
    // success.
    bool process(const std::vector<std::uint32_t>& request,
                 std::vector<std::uint32_t>* response);

    // Derive the session key from the model transcript (deterministic).
    void derive_key();

private:
    std::uint64_t transcript_ = 0xCBF29CE484222325ull;
    void absorb(const std::uint8_t* p, std::size_t n);
    void absorb_u64(std::uint64_t v);
};

} // namespace cxl_tlm
