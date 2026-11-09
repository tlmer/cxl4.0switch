// -----------------------------------------------------------------------------
// cxl_security.h -- CXL Type-3 security state + Sanitize / Secure Erase.
//
//   Header-only state machine for the device-side security mailbox commands.
//   It owns no media: the integrating owner (CxlType3Commands / device memory)
//   applies the media effects and event-log purge on completion.
//
//   Opcodes (Linux 6.18 drivers/cxl/cxlmem.h:553-560):
//     0x4400 Sanitize                 no payload, background
//     0x4401 Secure Erase             no payload
//     0x4500 Get Security State       no input, 4 B output
//     0x4501 Set Passphrase           0x60 B input
//     0x4502 Disable Passphrase       0x40 B input
//     0x4503 Unlock                   0x20 B input
//     0x4504 Freeze Security          no payload
//     0x4505 Passphrase Secure Erase  0x40 B input
//
//   Payload layouts (Linux drivers/cxl/cxlmem.h:818-838):
//     set pass:     type[0] reserved[1..31] old_pass[0x20..0x3F]
//                   new_pass[0x40..0x5F]
//     disable/pass erase:
//                   type[0] reserved[1..31] pass[0x20..0x3F]
//     unlock:       pass[0x00..0x1F] (no type byte)
//     type: 0 = master passphrase, 1 = user passphrase.
//
//   Security state flags (Linux drivers/cxl/cxlmem.h:810-815):
//     0x01 User Passphrase Set   0x02 Master Passphrase Set
//     0x04 Locked                0x08 Frozen
//     0x10 User Passphrase Limit 0x20 Master Passphrase Limit
//
//   Return codes: 0x13 Invalid Security State, 0x14 Incorrect Passphrase
//   (CXL spec Table 8-208; QEMU CXL_MBOX_INVALID_SECURITY_STATE /
//   CXL_MBOX_INCORRECT_PASSPHRASE).  0x14 is added here as a local constant so
//   this file can land while other builds hold cxl_mailbox.h; fold it into
//   CxlMbxRc when the next full rebuild is done.
//
//   Anchored behaviour:
//     * Sanitize has no input payload and is a background operation.  Once it
//       starts the device is media-disabled; it stays media-disabled across an
//       interrupted reset until a successful Sanitize completes.  Sanitize
//       deletes event logs but does not reset usage statistics (owner).
//     * Security applied to user data must be disabled before Sanitize.
//     * Secure Erase requires security disabled or unlocked (Linux checks the
//       locked bit before sending it).
//     * The master passphrase may only be set while security is disabled
//       (user passphrase not set).
//     * Setting the user passphrase locks the device on Conventional Reset.
//     * Passphrase Secure Erase can use the master passphrase when the user
//       passphrase is lost; afterwards the user passphrase is disabled and the
//       master passphrase remains unchanged.
//
//   Model choices (documented, not invented as spec text):
//     * Unlock is allowed while Frozen: Freeze forbids passphrase changes, not
//       unlocking.  Disable/Set/Passphrase-Erase are rejected while Frozen.
//     * Freeze is idempotent (Success) when already frozen.
//     * Wrong-length payloads are always rejected with Invalid Payload Length;
//       an unknown passphrase type (not 0/1) with Invalid Input.
//     * Media-disabled gating of individual media-access commands is the
//       owner's responsibility; this block exposes `media_disabled`.
// -----------------------------------------------------------------------------
#pragma once

#include "cxl_mailbox.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace cxl_tlm {

enum CxlSecurityFlag : std::uint32_t {
    kSecUserPassSet   = 0x01,
    kSecMasterPassSet = 0x02,
    kSecLocked        = 0x04,
    kSecFrozen        = 0x08,
    kSecUserPassLimit = 0x10,
    kSecMasterPassLimit = 0x20,
};

// Return codes added by the security command set (CXL Table 8-208).
constexpr std::uint16_t kMbxRcInvalidSecurityState = 0x0013;
constexpr std::uint16_t kMbxRcIncorrectPassphrase  = 0x0014;

struct CxlSecurityState {
    static constexpr std::size_t kPassLen = 32;

    // Get Security State flags.
    std::uint32_t flags = 0;

    // Media / sanitize state.
    bool sanitize_active = false;
    bool media_disabled = false;

    // Stored passphrases (model only; no claim of secure storage).
    std::uint8_t user_pass[kPassLen] = {};
    std::uint8_t master_pass[kPassLen] = {};
    bool user_pass_set = false;
    bool master_pass_set = false;

    // Statistics (never silent).
    std::uint64_t get_state_calls = 0;
    std::uint64_t set_ok = 0, set_bad_old = 0, set_denied = 0;
    std::uint64_t disable_ok = 0, disable_denied = 0;
    std::uint64_t unlock_ok = 0, unlock_fail = 0;
    std::uint64_t freeze_calls = 0;
    std::uint64_t sanitize_started = 0, sanitize_completed = 0,
                  sanitize_denied = 0, sanitize_busy = 0;
    std::uint64_t secure_erase_ok = 0, secure_erase_denied = 0;
    std::uint64_t pass_erase_ok = 0, pass_erase_fail = 0;
    std::uint64_t bad_payload = 0, bad_type = 0;

    bool user_set() const { return (flags & kSecUserPassSet) != 0; }
    bool master_set() const { return (flags & kSecMasterPassSet) != 0; }
    bool locked() const { return (flags & kSecLocked) != 0; }
    bool frozen() const { return (flags & kSecFrozen) != 0; }

    void clear()
    {
        flags = 0;
        sanitize_active = false;
        media_disabled = false;
        std::memset(user_pass, 0, sizeof(user_pass));
        std::memset(master_pass, 0, sizeof(master_pass));
        user_pass_set = false;
        master_pass_set = false;
    }

    // A Conventional Reset locks the device if a user passphrase is set.  An
    // interrupted Sanitize stays media-disabled until it completes.
    void on_conventional_reset()
    {
        if (user_pass_set) flags |= kSecLocked;
        sanitize_active = false;
    }

    // ---- opcode handlers -----------------------------------------------------
    // Each returns true (handled) and writes the mailbox return code.

    bool get_security_state(const std::vector<std::uint8_t>& in,
                            std::vector<std::uint8_t>* out,
                            std::uint16_t* rc)
    {
        ++get_state_calls;
        if (!in.empty()) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        out->assign(4, 0);
        put32le(*out, 0, flags);
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    bool set_passphrase(const std::vector<std::uint8_t>& in, std::uint16_t* rc)
    {
        if (in.size() != 0x60) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        const std::uint8_t type = in[0];
        if (type > 1) {
            ++bad_type;
            *rc = MBX_RC_INVALID_INPUT;
            return true;
        }
        if (frozen()) {
            ++set_denied;
            *rc = MBX_RC_INVALID_SECURITY;
            return true;
        }
        const std::uint8_t* oldp = in.data() + 0x20;
        const std::uint8_t* newp = in.data() + 0x40;
        if (type == 0) {                              // master
            // Master passphrase may only be set while security is disabled.
            if (user_pass_set) {
                ++set_denied;
                *rc = MBX_RC_INVALID_SECURITY;
                return true;
            }
            if (master_pass_set && !pass_match(master_pass, oldp)) {
                ++set_bad_old;
                *rc = kMbxRcIncorrectPassphrase;
                return true;
            }
            std::memcpy(master_pass, newp, kPassLen);
            master_pass_set = true;
            flags |= kSecMasterPassSet;
        } else {                                      // user
            if (user_pass_set && !pass_match(user_pass, oldp)) {
                ++set_bad_old;
                *rc = kMbxRcIncorrectPassphrase;
                return true;
            }
            std::memcpy(user_pass, newp, kPassLen);
            user_pass_set = true;
            flags |= kSecUserPassSet;
            // Setting the passphrase does not lock immediately; the lock
            // takes effect on the next Conventional Reset.
            flags &= ~kSecLocked;
        }
        ++set_ok;
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    bool disable_passphrase(const std::vector<std::uint8_t>& in,
                            std::uint16_t* rc)
    {
        if (in.size() != 0x40) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        const std::uint8_t type = in[0];
        if (type > 1) {
            ++bad_type;
            *rc = MBX_RC_INVALID_INPUT;
            return true;
        }
        if (frozen()) {
            ++disable_denied;
            *rc = MBX_RC_INVALID_SECURITY;
            return true;
        }
        const std::uint8_t* pass = in.data() + 0x20;
        if (type == 0) {
            if (!master_pass_set) {
                ++disable_denied;
                *rc = MBX_RC_INVALID_SECURITY;
                return true;
            }
            if (!pass_match(master_pass, pass)) {
                *rc = kMbxRcIncorrectPassphrase;
                return true;
            }
            std::memset(master_pass, 0, kPassLen);
            master_pass_set = false;
            flags &= ~kSecMasterPassSet;
        } else {
            if (!user_pass_set) {
                ++disable_denied;
                *rc = MBX_RC_INVALID_SECURITY;
                return true;
            }
            if (!pass_match(user_pass, pass)) {
                *rc = kMbxRcIncorrectPassphrase;
                return true;
            }
            std::memset(user_pass, 0, kPassLen);
            user_pass_set = false;
            flags &= ~(kSecUserPassSet | kSecLocked);
        }
        ++disable_ok;
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    bool unlock(const std::vector<std::uint8_t>& in, std::uint16_t* rc)
    {
        if (in.size() != 0x20) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        if (!user_pass_set) {
            ++unlock_fail;
            *rc = MBX_RC_INVALID_SECURITY;
            return true;
        }
        if (!pass_match(user_pass, in.data())) {
            ++unlock_fail;
            *rc = kMbxRcIncorrectPassphrase;
            return true;
        }
        flags &= ~kSecLocked;
        ++unlock_ok;
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    bool freeze_security(const std::vector<std::uint8_t>& in, std::uint16_t* rc)
    {
        ++freeze_calls;
        if (!in.empty()) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        flags |= kSecFrozen;
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    // Sanitize: no payload, background operation.  The owner completes it via
    // complete_sanitize() and must purge event logs (not counters).
    bool sanitize(const std::vector<std::uint8_t>& in, std::uint16_t* rc)
    {
        if (!in.empty()) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        if (user_pass_set) {                          // security must be disabled
            ++sanitize_denied;
            *rc = MBX_RC_INVALID_SECURITY;
            return true;
        }
        if (sanitize_active) {
            ++sanitize_busy;
            *rc = MBX_RC_BUSY;
            return true;
        }
        sanitize_active = true;
        media_disabled = true;
        ++sanitize_started;
        *rc = MBX_RC_BACKGROUND;
        return true;
    }

    // Secure Erase: no payload.  Requires security disabled or unlocked.
    bool secure_erase(const std::vector<std::uint8_t>& in, std::uint16_t* rc)
    {
        if (!in.empty()) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        if (user_pass_set && locked()) {
            ++secure_erase_denied;
            *rc = MBX_RC_INVALID_SECURITY;
            return true;
        }
        if (sanitize_active) {
            ++secure_erase_denied;
            *rc = MBX_RC_BUSY;
            return true;
        }
        ++secure_erase_ok;
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    // Passphrase Secure Erase: may use the master passphrase when the user
    // passphrase is lost.  Afterwards the user passphrase is disabled and the
    // master passphrase is unchanged.
    bool passphrase_secure_erase(const std::vector<std::uint8_t>& in,
                                 std::uint16_t* rc)
    {
        if (in.size() != 0x40) {
            ++bad_payload;
            *rc = MBX_RC_INVALID_PAYLOAD_LEN;
            return true;
        }
        const std::uint8_t type = in[0];
        if (type > 1) {
            ++bad_type;
            *rc = MBX_RC_INVALID_INPUT;
            return true;
        }
        if (frozen()) {
            ++pass_erase_fail;
            *rc = MBX_RC_INVALID_SECURITY;
            return true;
        }
        if (sanitize_active) {
            ++pass_erase_fail;
            *rc = MBX_RC_BUSY;
            return true;
        }
        const std::uint8_t* pass = in.data() + 0x20;
        // The type selects which stored passphrase authenticates the request
        // (Linux sends type=master with the master passphrase when the user
        // passphrase is lost).  A user-type request is not satisfied by the
        // master passphrase.
        if (type == 1) {
            if (!user_pass_set) {
                ++pass_erase_fail;
                *rc = MBX_RC_INVALID_SECURITY;
                return true;
            }
            if (!pass_match(user_pass, pass)) {
                ++pass_erase_fail;
                *rc = kMbxRcIncorrectPassphrase;
                return true;
            }
        } else {
            if (!master_pass_set) {
                ++pass_erase_fail;
                *rc = MBX_RC_INVALID_SECURITY;
                return true;
            }
            if (!pass_match(master_pass, pass)) {
                ++pass_erase_fail;
                *rc = kMbxRcIncorrectPassphrase;
                return true;
            }
        }
        // The operation disables the user passphrase.  The master passphrase
        // (if set) remains.
        std::memset(user_pass, 0, kPassLen);
        user_pass_set = false;
        flags &= ~(kSecUserPassSet | kSecLocked);
        ++pass_erase_ok;
        *rc = MBX_RC_SUCCESS;
        return true;
    }

    // Background completion hook (owner calls it after the media operation).
    void complete_sanitize()
    {
        if (!sanitize_active) return;
        sanitize_active = false;
        media_disabled = false;
        ++sanitize_completed;
    }

    static bool pass_match(const std::uint8_t* stored, const std::uint8_t* given)
    {
        // Constant-time comparison is not a goal for a behavioural model; the
        // stored value is the model's own copy.
        return std::memcmp(stored, given, kPassLen) == 0;
    }

private:
    static void put32le(std::vector<std::uint8_t>& v, std::size_t off,
                        std::uint32_t x)
    {
        v[off + 0] = (std::uint8_t)(x & 0xFF);
        v[off + 1] = (std::uint8_t)((x >> 8) & 0xFF);
        v[off + 2] = (std::uint8_t)((x >> 16) & 0xFF);
        v[off + 3] = (std::uint8_t)((x >> 24) & 0xFF);
    }
};

} // namespace cxl_tlm
