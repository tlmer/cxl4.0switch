// -----------------------------------------------------------------------------
// cxl_channel.h -- CXL.cachemem channel flow control and init/deinit FSMs.
//
//   Source: CTL CXL Controller Databook 6.43a:
//     Section 3.5.1 Table 3-3   CX_CXL_CHI_MAXCREDITS (2..8)
//     Section 3.5.2 Table 3-4/3-5, Figures 3-12..3-17: four-state handshake
//     Section 3.5.4/3.5.5: one credit per message; a grant is usable the cycle
//       after it is observed; a received Valid implicitly returns one credit;
//       no ready/halt; credits are tracked per channel.
//
//   App-side view (the model):
//     TX channel (cxl_slv_*): app drives req, receives ack and credits.
//     RX channel (cxl_mst_*): app drives ack and returns credits.
// -----------------------------------------------------------------------------
#pragma once
#include <cstdint>

namespace cxl_tlm {

enum class CxlInitState : uint8_t {
    IDLE = 0, INIT, RUN, DEINIT_MST, DEINIT_SLV
};

const char* cxl_init_state_name(CxlInitState s);

// -------------------------------------------------------------- credit pool
// Sender-side credits (one per message, per channel).
struct CxlCreditPool {
    int max_credits = 3;
    int avail = 0;
    std::uint64_t granted = 0, consumed = 0, starved = 0, overflow = 0;

    void configure(int max);     // 2..8 (DB Table 3-3); values are clamped + flagged
    void reset();                // entering IDLE: no credits available to the app
    bool can_send() const { return avail > 0; }
    bool consume();              // false => starved (never silent)
    bool grant(int n = 1);       // false => overflow (peer granted beyond max)
};

// Receiver-side credit return: one credit the cycle after a Valid message.
struct CxlRxCreditReturn {
    bool pending = false;
    std::uint64_t returned = 0;

    // Call once per cycle with "a Valid message was consumed this cycle".
    // Returns the credit-return bit for THIS cycle (one-cycle delay).
    bool tick(bool valid_this_cycle);
    void reset();
};

// ------------------------------------------------------------ TX init (app)
// App = transmitter. req is the app output; ack is the peer (CTL) input.
struct CxlTxInit {
    CxlInitState st = CxlInitState::IDLE;
    bool req = false;
    bool app_pending = false;            // app has unfinished TX traffic
    std::uint64_t pending_deinit_err = 0;

    bool init();                         // IDLE -> INIT
    bool deinit();                       // RUN -> DEINIT_MST (refused while app_pending)
    void deassert_req();                 // DEINIT_SLV: app completes the deinit
    void step(bool peer_ack);            // one cycle
    bool running() const { return st == CxlInitState::RUN; }
};

// ------------------------------------------------------------ RX init (app)
// App = receiver. ack is the app output; peer_req is the CTL input.
struct CxlRxInit {
    CxlInitState st = CxlInitState::IDLE;
    bool ack = false;
    bool deinit_requested = false;

    bool request_deinit();               // RUN -> DEINIT_SLV
    void step(bool peer_req);            // one cycle
    bool running() const { return st == CxlInitState::RUN; }
};

} // namespace cxl_tlm
