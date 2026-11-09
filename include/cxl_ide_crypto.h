// -----------------------------------------------------------------------------
// cxl_ide_crypto.h -- AES-256-GCM provider for the CXL IDE key/stream model.
//
//   `CxlIde` owns keys, streams and IV counters; this provider performs the
//   actual AES-256-GCM seal/open using OpenSSL (EVP).  The 12-byte IV is the
//   stream's 64-bit counter in big-endian form (4 zero bytes || counter), and
//   the 16-byte tag is returned to the caller.  RX replay is rejected against
//   the per-key rx_iv counter.
//
//   Scope: this is the model's cryptographic provider.  The RTL EP / channel
//   contract does not carry the IDE integrity-tag field, so the secure channel
//   wiring is opt-in at the model layer; the customer link-layer IP owns real
//   IDE framing.
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_ide.h"
#include "cxl_msg.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace cxl_tlm {

struct CxlIdeAesGcm {
    std::uint64_t seals = 0, opens = 0;
    std::uint64_t auth_fail = 0, replay = 0, bad_state = 0;

    // `aad` binds message-header bytes into the GCM tag (CxlIdeChan passes the
    // decoded M2S.RwD header).  The default empty AAD keeps plain data-record
    // callers unchanged.
    bool seal(CxlIde& ide, unsigned stream, const std::uint8_t* pt,
              std::size_t len, std::vector<std::uint8_t>* ct,
              std::array<std::uint8_t, 12>* iv,
              std::array<std::uint8_t, 16>* tag,
              const std::uint8_t* aad = nullptr, std::size_t aad_len = 0);

    bool open(CxlIde& ide, unsigned stream,
              const std::array<std::uint8_t, 12>& iv,
              const std::uint8_t* ct, std::size_t len,
              const std::array<std::uint8_t, 16>& tag,
              std::vector<std::uint8_t>* pt,
              const std::uint8_t* aad = nullptr, std::size_t aad_len = 0);
};

// Model-level IDE data-path wrapper for a CXL.mem M2S.RwD channel message.
// The message is decoded, its 64-byte data field is sealed/opened with the
// AES-256-GCM provider, and the message is re-encoded with every header field
// unchanged.  The IV/tag are carried out of band (the channel contract has no
// IDE field; the customer link-layer IP owns the real IDE framing).
struct CxlIdeChan {
    CxlIdeAesGcm gcm;
    std::uint64_t sealed = 0, opened = 0, rejected = 0, skipped = 0;

    bool seal_rwd(CxlIde& ide, unsigned stream, const CxlPacked& in,
                  CxlPacked* out, std::array<std::uint8_t, 12>* iv,
                  std::array<std::uint8_t, 16>* tag);
    bool open_rwd(CxlIde& ide, unsigned stream, const CxlPacked& in,
                  const std::array<std::uint8_t, 12>& iv,
                  const std::array<std::uint8_t, 16>& tag, CxlPacked* out);
};

} // namespace cxl_tlm
