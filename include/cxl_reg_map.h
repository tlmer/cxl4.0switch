// -----------------------------------------------------------------------------
// cxl_reg_map.h -- frozen CXL register address map (M0.6 single source).
//
//   Sources:
//     CXL 4.0 r1.0 Section 8.1.3  PCIe DVSEC for CXL Devices (ID 0000h)
//     CXL 4.0 r1.0 Section 8.1.5  CXL Extensions DVSEC for Ports (ID 0003h)
//     CXL 4.0 r1.0 Section 8.1.9  Register Locator DVSEC (ID 0008h)
//     CXL 4.0 r1.0 Section 8.2.3  Component register layout (Table 8-73)
//     CXL 4.0 r1.0 Section 8.2.4  CXL.cache / CXL.mem capability array
//     CXL 4.0 r1.0 Section 8.2.4.20 HDM Decoder (Table 8-115)
//     CXL 4.0 r1.0 Section 8.2.9.4  Mailbox registers (Figure 8-14)
//
//   Config-space placement is model policy (DVSEC structures may live anywhere
//   in extended config space); the internal offsets below are spec-frozen.
// -----------------------------------------------------------------------------
#pragma once
#include <cstdint>

namespace cxl_tlm {

// --------------------------------------------------------------- DVSEC IDs
constexpr std::uint16_t kCxlDvsecVendorId   = 0x1E98;
constexpr std::uint16_t kCxlDvsecIdDevice   = 0x0000;   // PCIe DVSEC for CXL Devices
constexpr std::uint16_t kCxlDvsecIdPort     = 0x0003;   // CXL Extensions DVSEC for Ports
constexpr std::uint16_t kCxlDvsecIdRegLoc   = 0x0008;   // Register Locator
constexpr std::uint16_t kCxlDvsecIdMld      = 0x0009;   // MLD (P1/P2)
constexpr std::uint32_t kCxlDvsecLenDevice  = 0x3C;     // Table 8-4
constexpr std::uint32_t kCxlDvsecLenPort    = 0x28;     // Table 8-30
constexpr std::uint32_t kCxlDvsecRevDevice  = 0x3;      // non-RCD must be >= 1
constexpr std::uint32_t kCxlDvsecRevPort    = 0x0;

// ---------------------------------------------- model config-space placement
// Extended capability IDs (PCIe): DVSEC = 0x000B.
constexpr std::uint16_t kPcieExtCapIdDvsec = 0x000B;
// Extended capability space starts at 0x100 (PCIe).  The first DVSEC is the
// chain head; the Register Locator follows.
constexpr std::uint16_t kCfgExtCapStart     = 0x100;
constexpr std::uint16_t kCfgDvsecDeviceOff  = 0x100;    // Type0 device
constexpr std::uint16_t kCfgDvsecPortOff    = 0x100;    // Type1 root/USP/DSP
constexpr std::uint16_t kCfgDvsecRegLocOff  = 0x200;    // all components

// BAR policy: BAR0 = Component registers (64 KB).  A Type-3 device also has
// BAR2 = CXL Device registers (64 KB) so that both register blocks can be at a
// 64-KB-aligned offset in their own BAR (Table 8-50 requires A[15:0]=0).
constexpr std::uint16_t kCfgBar0Offset = 0x10;          // BAR0 (32-bit, 64 KB)
constexpr std::uint16_t kCfgBar2Offset = 0x18;          // BAR2 (32-bit, 64 KB, device only)
constexpr std::uint32_t kCfgBarSize     = 0x10000;      // 64 KB
constexpr std::uint32_t kCfgBarMask     = 0xFFFF0008u;  // 32-bit memory, prefetchable
constexpr std::uint32_t kRegLocBirBar0  = 0;            // BIR 000b = BAR10h
constexpr std::uint32_t kRegLocBirBar2  = 2;            // BIR 010b = BAR18h

// ------------------------------------------------- DVSEC structure layout
constexpr std::uint16_t kDvsecExtCapHdr   = 0x00;       // PCIe extended cap header
constexpr std::uint16_t kDvsecVendorRevLen= 0x04;       // vendor/rev/len (Table 8-4)
constexpr std::uint16_t kDvsecId          = 0x08;

// PCIe DVSEC for CXL Devices (within the structure, Table 8-2/8-4)
namespace dvsec_dev {
constexpr std::uint16_t kCapability  = 0x0A;   // Table 8-5
constexpr std::uint16_t kControl     = 0x0C;   // Table 8-7
constexpr std::uint16_t kStatus      = 0x0E;   // Table 8-9
constexpr std::uint16_t kControl2    = 0x10;   // Table 8-11
constexpr std::uint16_t kStatus2     = 0x12;   // Table 8-13
constexpr std::uint16_t kLock        = 0x14;   // Table 8-14
constexpr std::uint16_t kCapability2 = 0x16;   // Table 8-15
constexpr std::uint16_t kRange1SizeHi= 0x18;
constexpr std::uint16_t kRange1SizeLo= 0x1C;
constexpr std::uint16_t kRange1BaseHi= 0x20;
constexpr std::uint16_t kRange1BaseLo= 0x24;
constexpr std::uint16_t kRange2SizeHi= 0x28;
constexpr std::uint16_t kRange2SizeLo= 0x2C;
constexpr std::uint16_t kRange2BaseHi= 0x30;
constexpr std::uint16_t kRange2BaseLo= 0x34;
} // namespace dvsec_dev

// CXL Extensions DVSEC for Ports (Table 8-30 and 8.1.5.1..8.1.5.11)
namespace dvsec_port {
constexpr std::uint16_t kStatus          = 0x0A;
constexpr std::uint16_t kControl         = 0x0C;
constexpr std::uint16_t kAltBusBase      = 0x0E;
constexpr std::uint16_t kAltBusLimit     = 0x0F;
constexpr std::uint16_t kAltMemBase      = 0x10;
constexpr std::uint16_t kAltMemLimit     = 0x12;
constexpr std::uint16_t kAltPrefBase     = 0x14;
constexpr std::uint16_t kAltPrefLimit    = 0x16;
constexpr std::uint16_t kAltPrefBaseHi   = 0x18;
constexpr std::uint16_t kAltPrefLimitHi  = 0x1C;
constexpr std::uint16_t kRcrbBase        = 0x20;
} // namespace dvsec_port

// ------------------------------------------------------------------- regloc
constexpr std::uint16_t kRegLocFirstEntry = 0x0C;       // first Offset Low (Figure 8-6)
constexpr std::uint32_t kRegLocEntryStride = 0x08;      // Low + High pair
// Register Offset Low (Table 8-50)
constexpr std::uint32_t kRegLocBirMask    = 0x00000007u;   // [2:0] BAR indicator
constexpr std::uint32_t kRegLocBlockIdSh  = 8;             // [15:8] block identifier
constexpr std::uint32_t kRegLocBlockIdMask = 0x0000FF00u;
constexpr std::uint32_t kRegLocOffsetHi16 = 0xFFFF0000u;   // A[31:16] of the BAR offset
// Block identifiers (Table 8-50)
enum RegBlockId : std::uint8_t {
    REGBLK_EMPTY    = 0x00,
    REGBLK_COMPONENT= 0x01,
    REGBLK_BAR_VIRT = 0x02,
    REGBLK_DEVICE   = 0x03,
    REGBLK_CPMU     = 0x04,
    REGBLK_CHMU     = 0x05,
    REGBLK_DVS      = 0xFF,
};

// ------------------------------------------------------- component registers
// Table 8-73 (offsets from the component BAR base; 64 KB BAR).
constexpr std::uint32_t kCompIoReserved      = 0x0000;  // 0x0000..0x0FFF
constexpr std::uint32_t kCompCacheMemPrimary = 0x1000;  // 0x1000..0x1FFF
constexpr std::uint32_t kCompCacheMemExt     = 0x2000;  // 0x2000..0xDFFF
constexpr std::uint32_t kCompArbMux          = 0xE000;  // 0xE000..0xE3FF
constexpr std::uint32_t kCompReservedTail     = 0xE400; // 0xE400..0xFFFF
constexpr std::uint32_t kCompBarSize         = 0x10000; // 64 KB BAR0

// Primary range capability array (Table 8-75/8-78).
constexpr std::uint32_t kCapHdrOffset        = 0x00;    // CXL Capability Header
constexpr std::uint32_t kCapArrayOffset      = 0x04;    // n x 4 bytes
// Standard capability block offsets (relative to the component BAR base).
// The array pointer fields are 12-bit values in 4-byte units relative to the
// CXL Capability Header register (0x1000).  M1.6 re-verifies against CTL.
constexpr std::uint32_t kCapRasBlock         = 0x2000;
constexpr std::uint32_t kCapSecurityBlock    = 0x3000;
constexpr std::uint32_t kCapLinkBlock        = 0x4000;
constexpr std::uint32_t kCapHdmBlock         = 0x5000;
constexpr std::uint32_t kCapExtSecurityBlock = 0x6000;
constexpr std::uint32_t kCapIdeBlock         = 0x7000;
constexpr std::uint32_t kCapSnoopFilterBlock = 0x8000;
constexpr std::uint32_t kCapTimeoutBlock     = 0x9000;
constexpr std::uint32_t kCapCacheMemExtBlock = 0xA000;
constexpr std::uint32_t kCapBiRouteBlock     = 0xB000;
constexpr std::uint32_t kCapBiDecoderBlock   = 0xC000;
constexpr std::uint32_t kCapCacheIdRouteBlock= 0xD000;
constexpr std::uint32_t kCapCacheIdDecodeBlock=0xD800;
constexpr std::uint32_t kCapExtHdmBlock      = 0xE400;  // extended range
constexpr std::uint32_t kCapEmdBlock          = 0xE800; // extended range

// Capability IDs (Table 8-74).
enum CapId : std::uint16_t {
    CAP_NULL          = 0x0,
    CAP_CXL           = 0x1,
    CAP_RAS           = 0x2,
    CAP_SECURITY      = 0x3,
    CAP_LINK          = 0x4,
    CAP_HDM           = 0x5,
    CAP_EXT_SECURITY  = 0x6,
    CAP_IDE           = 0x7,
    CAP_SNOOP_FILTER  = 0x8,
    CAP_TIMEOUT       = 0x9,
    CAP_CACHEMEM_EXT  = 0xA,
    CAP_BI_ROUTE      = 0xB,
    CAP_BI_DECODER    = 0xC,
    CAP_CACHEID_ROUTE = 0xD,
    CAP_CACHEID_DEC   = 0xE,
    CAP_EXT_HDM       = 0xF,
    CAP_EMD           = 0x10,
};

// --------------------------------------------------- HDM decoder (Table 8-115)
namespace hdm {
constexpr std::uint32_t kCapability      = 0x00;  // Table 8-116
constexpr std::uint32_t kGlobalControl   = 0x04;  // Table 8-118
constexpr std::uint32_t kDecoderStride   = 0x20;
constexpr std::uint32_t kBaseLow         = 0x10;  // 0x20*n + 0x10
constexpr std::uint32_t kBaseHigh        = 0x14;
constexpr std::uint32_t kSizeLow         = 0x18;
constexpr std::uint32_t kSizeHigh        = 0x1C;
constexpr std::uint32_t kControl         = 0x20;
constexpr std::uint32_t kTargetListLow   = 0x24;  // switches/host bridges
constexpr std::uint32_t kDpaSkipLow      = 0x24;  // devices
constexpr std::uint32_t kTargetListHigh  = 0x28;
constexpr std::uint32_t kDpaSkipHigh     = 0x28;
constexpr std::uint32_t kDecoderRegOff(int n, std::uint32_t reg) {
    return kDecoderStride * (std::uint32_t)n + reg;
}
} // namespace hdm

// ------------------------------------------------------ mailbox (Figure 8-14)
namespace mbx {
constexpr std::uint32_t kCapabilities  = 0x00;   // Table 8-204
constexpr std::uint32_t kControl       = 0x04;   // Table 8-206 (doorbell)
constexpr std::uint32_t kCommand       = 0x08;   // Table 8-207
constexpr std::uint32_t kStatus        = 0x10;   // Table 8-209
constexpr std::uint32_t kBackground    = 0x18;   // Table 8-210
constexpr std::uint32_t kPayload       = 0x20;   // Command Payload registers
} // namespace mbx

// ----------------------------------------------------------- encoding helpers
inline std::uint32_t regloc_low(std::uint32_t bir, std::uint32_t block_id,
                                std::uint64_t bar_offset) {
    return (bir & kRegLocBirMask) |
           ((block_id & 0xFFu) << kRegLocBlockIdSh) |
           ((std::uint32_t)((bar_offset >> 16) & 0xFFFFu) << 16);
}
inline std::uint32_t regloc_high(std::uint64_t bar_offset) {
    return (std::uint32_t)((bar_offset >> 32) & 0xFFFFFFFFu);
}
inline std::uint64_t regloc_offset(std::uint32_t low, std::uint32_t high) {
    return (((std::uint64_t)high) << 32) | ((std::uint64_t)(low & kRegLocOffsetHi16) << 16);
}

} // namespace cxl_tlm
