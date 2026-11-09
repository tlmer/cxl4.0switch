// -----------------------------------------------------------------------------
// cxl_ide.h -- CXL Integrity and Data Encryption (IDE) model.
//
//   Provides the IDE capability state that the component register block
//   exposes plus the key/stream management API:
//
//     - up to 8 key slots, each with a TX and RX key and per-direction IV
//       counters; a slot becomes valid only when both keys are programmed;
//     - up to 4 IDE streams; a stream is enabled by binding it to a valid
//       key slot; disabling a stream clears its binding;
//     - `protect()`/`unprotect()` call an owner-provided cipher hook (a real
//       AES-GCM/IDE implementation or a test cipher), increment the IV and
//       count operations.  No crypto is implemented in this header: the hook
//       is the same owner-boundary pattern used by the media/security models.
//
//   Model register map (relative to the IDE block):
//     +00 Capability  RO  [7:0] key slots, [15:8] streams
//     +04 Control     RW  bit0 IDE Enable
//     +08 Status      RO  bit0 Enabled, [15:8] active stream count
//     +0C Key Index   RW  [3:0] selected slot
//     +10..+2F Selected slot TX key (8 dwords)
//     +30..+4F Selected slot RX key (8 dwords)
//     +50 Stream Map  RW  [3:0] stream, [7:4] key slot, [31] bind/clear
//     +54 IV (TX)     RO  low dword of the selected stream's TX IV
// -----------------------------------------------------------------------------
#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>

namespace cxl_tlm {

struct CxlIde {
    static constexpr unsigned kSlots = 8;
    static constexpr unsigned kStreams = 4;
    static constexpr std::uint16_t kSize = 0x58;

    struct Key {
        bool valid = false;
        bool tx_set = false;
        bool rx_set = false;
        std::array<std::uint8_t, 32> tx{};
        std::array<std::uint8_t, 32> rx{};
        std::uint64_t tx_iv = 0;
        std::uint64_t rx_iv = 0;
    };

    std::uint32_t ctl = 0;
    std::uint8_t key_index = 0;
    std::array<Key, kSlots> keys{};
    std::array<int, kStreams> stream_slot{{-1, -1, -1, -1}};

    // Owner-provided cipher hook: (input, length, output, encrypt?).  Returns
    // false on an unsupported key/stream or a cipher error.
    std::function<bool(const std::uint8_t*, std::size_t, std::uint8_t*, bool)>
        cipher;

    std::uint64_t key_adds = 0, key_removes = 0, stream_enables = 0;
    std::uint64_t protect_ops = 0, unprotect_ops = 0, replay = 0, errors = 0;

    bool enabled() const { return (ctl & 0x1u) != 0; }

    std::uint32_t capability() const { return (std::uint32_t)kSlots |
                                              ((std::uint32_t)kStreams << 8); }
    int active_streams() const {
        int n = 0;
        for (int s : stream_slot) if (s >= 0) ++n;
        return n;
    }
    std::uint32_t status() const {
        return (enabled() ? 1u : 0u) |
               ((std::uint32_t)(active_streams() & 0xFFu) << 8);
    }

    bool add_key(unsigned slot, const std::uint8_t* tx, const std::uint8_t* rx) {
        if (slot >= kSlots || !tx || !rx) { ++errors; return false; }
        std::memcpy(keys[slot].tx.data(), tx, 32);
        std::memcpy(keys[slot].rx.data(), rx, 32);
        keys[slot].tx_set = keys[slot].rx_set = true;
        keys[slot].valid = true;
        keys[slot].tx_iv = 0;
        keys[slot].rx_iv = 0;
        ++key_adds;
        return true;
    }

    // Set one half of a key slot (IDE_KM SET_KEY): `tx` selects TX (true) or
    // RX (false).  The slot becomes valid once both halves are present.
    bool set_key_half(unsigned slot, bool tx, const std::uint8_t* key) {
        if (slot >= kSlots || !key) { ++errors; return false; }
        Key& k = keys[slot];
        std::memcpy((tx ? k.tx : k.rx).data(), key, 32);
        (tx ? k.tx_set : k.rx_set) = true;
        k.valid = k.tx_set && k.rx_set;
        if (k.valid) ++key_adds;
        return true;
    }

    bool remove_key(unsigned slot) {
        if (slot >= kSlots) { ++errors; return false; }
        for (auto& s : stream_slot) if (s == (int)slot) s = -1;
        keys[slot] = Key{};
        ++key_removes;
        return true;
    }

    bool enable_stream(unsigned stream, int slot) {
        if (stream >= kStreams || slot < 0 || slot >= (int)kSlots ||
            !keys[(std::size_t)slot].valid || !enabled()) {
            ++errors;
            return false;
        }
        stream_slot[stream] = slot;
        ++stream_enables;
        return true;
    }

    void disable_stream(unsigned stream) {
        if (stream < kStreams) stream_slot[stream] = -1;
    }

    bool protect(unsigned stream, const std::uint8_t* in, std::size_t n,
                 std::uint8_t* out) {
        if (stream >= kStreams || stream_slot[stream] < 0 || !enabled() ||
            !cipher || !in || !out) {
            ++errors;
            return false;
        }
        Key& k = keys[(std::size_t)stream_slot[stream]];
        if (!cipher(in, n, out, true)) { ++errors; return false; }
        ++k.tx_iv;
        ++protect_ops;
        return true;
    }

    bool unprotect(unsigned stream, const std::uint8_t* in, std::size_t n,
                   std::uint8_t* out, std::uint64_t iv) {
        if (stream >= kStreams || stream_slot[stream] < 0 || !enabled() ||
            !cipher || !in || !out) {
            ++errors;
            return false;
        }
        Key& k = keys[(std::size_t)stream_slot[stream]];
        if (iv < k.rx_iv) { ++replay; return false; }   // IV replay
        if (!cipher(in, n, out, false)) { ++errors; return false; }
        k.rx_iv = iv + 1;
        ++unprotect_ops;
        return true;
    }

    // ---- Component register block access ---------------------------------
    bool read32(std::uint16_t off, std::uint32_t* v) const {
        if (!v) return false;
        switch (off) {
        case 0x00: *v = capability(); return true;
        case 0x04: *v = ctl; return true;
        case 0x08: *v = status(); return true;
        case 0x0C: *v = key_index & 0xFu; return true;
        case 0x54: {
            const int s = (key_index < kStreams) ? stream_slot[key_index] : -1;
            *v = (s >= 0) ? (std::uint32_t)keys[(std::size_t)s].tx_iv : 0u;
            return true;
        }
        default: break;
        }
        if (off >= 0x10 && off < 0x30) {                 // TX key dwords
            const unsigned d = (off - 0x10) / 4;
            const Key& k = keys[key_index % kSlots];
            std::uint32_t x = 0;
            for (int i = 0; i < 4; ++i)
                x |= (std::uint32_t)k.tx[d * 4 + (std::size_t)i] << (8 * i);
            *v = x;
            return true;
        }
        if (off >= 0x30 && off < 0x50) {                 // RX key dwords
            const unsigned d = (off - 0x30) / 4;
            const Key& k = keys[key_index % kSlots];
            std::uint32_t x = 0;
            for (int i = 0; i < 4; ++i)
                x |= (std::uint32_t)k.rx[d * 4 + (std::size_t)i] << (8 * i);
            *v = x;
            return true;
        }
        return false;
    }

    // Key material is written through the key registers; stream binding via
    // +50.  Status/capability are RO.
    bool write32(std::uint16_t off, std::uint32_t v) {
        if (off == 0x04) { ctl = v & 0x1u; return true; }
        if (off == 0x0C) { key_index = (std::uint8_t)(v & 0xFu); return true; }
        if (off >= 0x10 && off < 0x30) {
            const unsigned d = (off - 0x10) / 4;
            Key& k = keys[key_index % kSlots];
            for (int i = 0; i < 4; ++i)
                k.tx[d * 4 + (std::size_t)i] = (std::uint8_t)(v >> (8 * i));
            return true;
        }
        if (off >= 0x30 && off < 0x50) {
            const unsigned d = (off - 0x30) / 4;
            Key& k = keys[key_index % kSlots];
            for (int i = 0; i < 4; ++i)
                k.rx[d * 4 + (std::size_t)i] = (std::uint8_t)(v >> (8 * i));
            return true;
        }
        if (off == 0x50) {
            const unsigned stream = v & 0xFu;
            const int slot = (int)((v >> 4) & 0xFu);
            if (v & 0x80000000u) {
                return enable_stream(stream, slot);
            }
            disable_stream(stream);
            return true;
        }
        return false;
    }
};

} // namespace cxl_tlm
