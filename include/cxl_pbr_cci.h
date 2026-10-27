// -----------------------------------------------------------------------------
// cxl_pbr_cci.h -- PBR switch CCI commands (CXL 4.0 Section 7.7.13/7.7.14).
//
//   Opcodes implemented:
//     5700h Identify PBR Switch
//     5702h Get PBR Link Partner Info
//     570Ch Get LDST/IDT Capabilities
//     570Dh Set LDST/IDT Configuration
//     570Eh/570Fh Get/Set LDST Segment Entries
//     5710h/5711h Get/Set LDST IDT DPID Entries
//     5800h Identify GAE
//     5801h Get PID Interrupt Vector
//     5802h Get PID Access Vectors (GMV[0x200] || VTV[0x200])
//     5803h Get FAST/IDT Capabilities
//     5804h Set FAST/IDT Configuration
//     5805h/5806h Get/Set FAST Segment Entries
//     5807h/5808h Get/Set IDT DPID Entries
//     5900h Identify VCS GAE
//     5901h Get VCS PID Access Vectors
//     5902h Configure VCS PID Access
//
//   vPPB instance 0 = USP; 1..N = DSP vPPBs.  The tables are the model's
//   representation of the switch fabric decoders; an out-of-range vPPB or
//   entry index returns Invalid Input (never silent).
// -----------------------------------------------------------------------------
#pragma once
#include "cxl_mailbox.h"
#include "cxl_pbr.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cxl_tlm {

enum : std::uint16_t {
    PBR_CMD_IDENTIFY        = 0x5700,
    PBR_CMD_LINK_PARTNER    = 0x5702,
    PBR_CMD_GET_LDST_CAPS   = 0x570C,
    PBR_CMD_SET_LDST_CFG    = 0x570D,
    PBR_CMD_GET_LDST_SEG    = 0x570E,
    PBR_CMD_SET_LDST_SEG    = 0x570F,
    PBR_CMD_GET_LDST_IDT    = 0x5710,
    PBR_CMD_SET_LDST_IDT    = 0x5711,
    PBR_CMD_GET_FAST_CAPS   = 0x5803,
    PBR_CMD_SET_FAST_CFG    = 0x5804,
    PBR_CMD_GET_FAST_SEG    = 0x5805,
    PBR_CMD_SET_FAST_SEG    = 0x5806,
    PBR_CMD_GET_FAST_IDT    = 0x5807,
    PBR_CMD_SET_FAST_IDT    = 0x5808,
    PBR_CMD_IDENTIFY_GAE    = 0x5800,
    PBR_CMD_GET_PID_IRQ     = 0x5801,
    PBR_CMD_GET_PID_ACC_VEC = 0x5802,
    PBR_CMD_IDENTIFY_VCS_GAE = 0x5900,
    PBR_CMD_GET_VCS_PID_ACC = 0x5901,
    PBR_CMD_CFG_VCS_PID_ACC = 0x5902,
    PBR_CMD_GET_PID_TARGETS = 0x5703,
    PBR_CMD_CFG_PID_ASSIGN  = 0x5704,
    PBR_CMD_GET_PID_BINDING = 0x5705,
    PBR_CMD_CFG_PID_BINDING = 0x5706,
    PBR_CMD_GET_TABLE_DESC  = 0x5707,
    PBR_CMD_GET_DRT         = 0x5708,
    PBR_CMD_SET_DRT         = 0x5709,
    PBR_CMD_GET_RGT         = 0x570A,
    PBR_CMD_SET_RGT         = 0x570B,
    PBR_CMD_GET_REROUTER    = 0x5712,
    PBR_CMD_SET_REROUTER    = 0x5713,
    PBR_CMD_GET_LAV         = 0x5714,
    PBR_CMD_GET_VCS_LAV     = 0x5715,
    PBR_CMD_CFG_VCS_LAV     = 0x5716,
};

struct CxlPbrSegEntry {
    bool valid = false;
    bool pcie_segment = false;       // LDST: PCIe segment flag
    bool gim = false;                // FAST: GIM segment
    std::uint8_t intlv = 0;          // interleave mode (Table 7-84)
    std::uint8_t gran = 0;           // granularity (Table 7-85)
    std::uint16_t dpid_ix = 0;       // DPID or IDT index
};

struct CxlPbrVppbCfg {
    bool valid = true;
    std::uint16_t local_pid = 0x0FFF;
    std::uint8_t seg_size = 0;       // FSegSz encoding (Table 7-83)
    std::uint16_t num_segments = 16;
    std::uint16_t num_idt = 16;
    std::uint64_t fabric_base = 0, fabric_limit = 0;
    std::vector<CxlPbrSegEntry> ldst_seg;
    std::vector<std::uint16_t> ldst_idt;
    std::vector<CxlPbrSegEntry> fast_seg;
    std::vector<std::uint16_t> fast_idt;
};

struct CxlPbrTarget {
    std::uint16_t target_id = 0;
    std::uint8_t target_type = 0;       // 0=fabric,1=host edge,2=downstream edge
    std::uint8_t instance = 0;
    std::uint8_t vcs_id = 0;
    std::uint8_t phys_port = 0;
    std::uint16_t pid = 0x0FFF;
    bool valid = false;
};

struct CxlPbrDrtEntry {
    std::uint8_t type = 0;              // 1=physical port, 2=RGT index
    std::uint8_t target = 0;
};

struct CxlPbrRgtEntry {
    std::uint8_t ports[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    std::uint8_t highest_valid = 0;     // bits[2:0]
    std::uint8_t highest_primary = 0;   // bits[5:3]
    std::uint8_t mode = 0;              // 0=random,1=CA,2=advanced CA
    std::uint8_t mix = 0;               // bits[5:3]
    bool valid = false;
};

struct CxlPbrBinding {
    bool valid = false;
    std::uint16_t pid = 0x0FFF;
    std::uint64_t latency_base = 0;
    std::uint16_t latency = 0;
    std::uint64_t bw_base = 0;
    std::uint16_t bw = 0;
};

struct CxlPbrRerouter {
    std::uint16_t dpid = 0;
    bool pcie_segment = false;
    std::uint8_t segment = 0, bus = 0, dev = 0, fn = 0;
    bool valid = false;
};

struct CxlPbrLinkPartner {
    bool valid = false;
    std::uint8_t port = 0, far_port = 0;
    std::uint16_t pid = 0;           // bits[11:0]
    std::uint8_t dev_type = 0;       // bits[15:12]
    std::array<std::uint8_t, 16> partner_id{};
    std::uint8_t fc_std = 0, fc_uio = 0;
    std::array<std::uint8_t, 16> fm_uuid{}, fm_uuid2{};
};

struct CxlPbrFabric;

struct CxlPbrCci {
    CxlPbrRouter* router = nullptr;
    CxlMailbox* mbx = nullptr;
    CxlPbrFabric* fabric = nullptr;
    int fabric_sw_id = 0;
    std::vector<CxlPbrVppbCfg> vppb;          // index 0 = USP
    std::vector<CxlPbrLinkPartner> partners;
    std::vector<CxlPbrTarget> targets;
    std::map<int, std::map<std::uint16_t, CxlPbrDrtEntry>> drt;   // DRT 0..n-1
    std::map<int, CxlPbrRgtEntry> rgt;
    std::vector<CxlPbrRerouter> rerouter;
    std::map<std::pair<std::uint8_t, std::uint8_t>, CxlPbrBinding> bindings;
    std::vector<std::uint8_t> lav;            // 4096 bits = 512 bytes
    std::map<std::uint8_t, std::vector<std::uint8_t>> vcs_lav;
    std::vector<std::uint8_t> pid_irq;        // 4096 bits = 512 bytes
    std::map<std::uint8_t, std::vector<std::uint8_t>> gmv_vec, vtv_vec;
    std::map<std::uint8_t, bool> vcs_gae;     // VCS IDs that include a GAE
    std::map<std::pair<std::uint8_t, std::uint16_t>, std::uint64_t> acc_latency_base;
    std::map<std::pair<std::uint8_t, std::uint16_t>, std::uint64_t> acc_bw_base;
    std::map<std::pair<std::uint8_t, std::uint16_t>, std::uint16_t> acc_latency;
    std::map<std::pair<std::uint8_t, std::uint16_t>, std::uint16_t> acc_bw;
    std::uint16_t supported_enabled_pids = 0x0FFF;   // Identify GAE bits[11:0]
    std::uint16_t proxy_threads_total = 16;
    std::uint16_t proxy_threads_available = 16;
    std::uint8_t num_drt = 1, num_rgt = 1;
    std::uint8_t routing_caps = 0x01;         // bit0 Random supported
    std::uint64_t commands = 0, invalid = 0, unsupported = 0;

    void init(int n_vppb = 1, CxlPbrRouter* r = nullptr);
    void set_partner(std::uint8_t port, const CxlPbrLinkPartner& p);
    bool add_target(const CxlPbrTarget& t, std::string* err = nullptr);
    bool set_drt(int drt_index, std::uint16_t dpid, const CxlPbrDrtEntry& e,
                 std::string* err = nullptr);
    bool set_rgt(int rgt_index, const CxlPbrRgtEntry& e, std::string* err = nullptr);
    void link_fabric(CxlPbrFabric* f, int sw_id = 0);
    void set_vcs_gae(std::uint8_t vcs_id, bool present = true);
    void raise_pid_irq(std::uint16_t pid);
    bool set_pid_access(std::uint8_t vcs_id, std::uint16_t pid, int op,
                        std::string* err = nullptr);
    bool get_pid_acc_vec(std::uint8_t vcs_id, std::uint64_t start,
                         std::uint64_t len, std::vector<std::uint8_t>* out) const;
    void attach(CxlMailbox* m);

    bool handle(std::uint16_t opcode, const std::vector<std::uint8_t>& in,
                std::vector<std::uint8_t>* out, std::uint16_t* rc);

    // Test/observability helper: resolve an HPA through the FAST segment table
    // of vPPB 0 (simple non-interleaved case).
    bool fast_lookup(std::uint64_t hpa, std::uint16_t* dpid,
                     std::string* err = nullptr) const;
};

} // namespace cxl_tlm
