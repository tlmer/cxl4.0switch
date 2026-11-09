// -----------------------------------------------------------------------------
// cxl_cpmu.h -- CXL Performance Monitoring Unit register block (CXL 4.0
// section 8.2.7, Version=1 layout).
//
//   Default profile (unchanged): one configurable counter unit, one Event
//   Capabilities register, HDM filter, 64-bit counter, freeze, no interrupt.
//
//   Expanded profile (opt-in through configure()):
//     - 1..64 counter units (Capability NUM_COUNTERS);
//     - free-run (type 0), fixed-function (type 1) and configurable (type 2)
//       units, with a 5-bit event-group index into 1..32 Event Capabilities
//       registers;
//     - per-counter overflow status (RW1C), freeze bits, HDM filter,
//       interrupt-on-overflow (INT capability + MSI_N vector) and
//       configurable-event validation against the selected event group.
//
//   Field positions follow the Linux CXL PMU driver
//   (drivers/perf/cxl_pmu.c, CXL_PMU_* macros), the consumer-side anchor:
//     000h CPMU Capability       [5:0] NUM_COUNTERS-1, [15:8] width,
//                                [24:20] NUM_EVN_CAP_REG-1, [39:32] filters,
//                                [47:44] MSI_N, [48] writeable-when-frozen,
//                                [49] freeze, [50] int, [63:60] version
//     010h Overflow Status       per-counter RW1C
//     018h Freeze                per-counter
//     100h Event Capabilities[n] [31:0] events, [47:32] group, [63:48] vendor
//     200h Counter Config[n]     [1:0] type, [8] enable, [9] int-on-overflow,
//                                [10] freeze-on-overflow, [11] edge,
//                                [12] invert, [23:16] threshold,
//                                [55:24] events, [63:59] event-group index
//     400h Filter Config[n]     4 * (f + n * 8); this profile implements
//                                filter 0 (HDM) only
//     C00h Counter Data[n]       64-bit counter
//
//   Fixed-function counters keep their type, event-group index and event
//   mask (HwInit); configurable counters may change group and events, but a
//   group reserved by a fixed-function counter cannot be selected and the
//   event mask must be a subset of that group's supported events.
//
//   This is a model register block, not a claim that the block is wired into
//   a particular Register Locator DVSEC instance.  A platform can map it as
//   CPMU register block ID 04h and feed event increments through add_event().
// -----------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <cstdint>

namespace cxl_tlm {

// Per-counter state for counters above unit 0.  Counter 0 keeps the legacy
// public members on CxlCpmu so existing users and tests compile unchanged.
struct CxlCpmuCounter {
    std::uint64_t config = 0x02ull | (1ull << 16);  // configurable, threshold 1
    std::uint64_t data = 0;
    std::uint32_t filter_hdm = 0;
    bool          filter_enabled = false;
    std::uint64_t events_counted = 0;
    std::uint64_t overflows = 0;
    std::uint64_t rejected = 0;
};

struct CxlCpmu {
    static constexpr std::size_t kMaxCounters = 64;
    static constexpr std::size_t kMaxEventCaps = 32;

    static constexpr std::uint64_t kCapability =
        (64ull << 8) |              // Counter Width = 64
        (1ull << 32) |              // HDM filter supported (filters field bit0)
        (1ull << 48) |              // Counters writable while frozen
        (1ull << 49) |              // Counter Freeze Support
        (1ull << 60);               // Version = 1
    static constexpr std::uint64_t kEventCapabilities =
        0x00000003ull |             // Event IDs 0 and 1
        (1ull << 32) |              // Event Group ID = 1
        (0x1E98ull << 48);          // model Event Vendor ID
    static constexpr std::uint64_t kConfigDefault =
        0x02ull |                   // Configurable Counter Unit
        (1ull << 16);               // Threshold = 1

    // Counter-0 public state (kept for source compatibility).
    std::uint64_t overflow_status = 0;      // per-counter RW1C bits
    std::uint64_t freeze = 0;               // per-counter freeze bits
    std::uint64_t counter_config = kConfigDefault;
    std::uint64_t counter_data = 0;
    std::uint32_t filter_hdm = 0;           // counter 0 / filter 0 (HDM index)
    bool          filter_enabled = false;
    std::uint64_t events_counted = 0;
    std::uint64_t overflows = 0;
    std::uint64_t rejected = 0;

    // Expanded profile.
    std::uint64_t cap = kCapability;
    std::uint32_t num_counters = 1;
    std::uint32_t num_event_caps = 1;
    std::uint64_t event_caps[kMaxEventCaps] = {};
    CxlCpmuCounter extra[kMaxCounters];     // units 1..num_counters-1
    std::uint64_t irq_assertions = 0;
    std::uint64_t config_rejected = 0;

    CxlCpmu() { event_caps[0] = kEventCapabilities; }

    // ---- counter configuration field helpers ----------------------------
    static std::uint8_t  counter_type(std::uint64_t cfg)
    { return (std::uint8_t)(cfg & 0x3ull); }
    static std::uint32_t counter_events(std::uint64_t cfg)
    { return (std::uint32_t)((cfg >> 24) & 0xFFFFFFFFull); }
    static std::uint8_t  counter_group(std::uint64_t cfg)
    { return (std::uint8_t)((cfg >> 59) & 0x1Full); }
    static bool counter_int_on_overflow(std::uint64_t cfg)
    { return (cfg & (1ull << 9)) != 0; }
    static bool counter_freeze_on_overflow(std::uint64_t cfg)
    { return (cfg & (1ull << 10)) != 0; }

    static constexpr std::uint64_t kTypeFreeRun = 0;
    static constexpr std::uint64_t kTypeFixed = 1;
    static constexpr std::uint64_t kTypeConfigurable = 2;

    // ---- profile configuration ------------------------------------------
    bool configure(std::size_t counters, std::size_t event_cap_regs,
                   bool interrupt_supported = false,
                   std::uint32_t msi_vector = 0)
    {
        if (counters < 1 || counters > kMaxCounters ||
            event_cap_regs < 1 || event_cap_regs > kMaxEventCaps)
            return false;
        num_counters = (std::uint32_t)counters;
        num_event_caps = (std::uint32_t)event_cap_regs;
        counter_config = kConfigDefault;
        counter_data = 0;
        filter_hdm = 0;
        filter_enabled = false;
        for (std::size_t i = 1; i < counters; ++i) extra[i - 1] = CxlCpmuCounter();
        for (std::size_t i = 0; i < kMaxEventCaps; ++i) event_caps[i] = 0;
        event_caps[0] = kEventCapabilities;

        // Update only the fields configure() owns; any other capability bits
        // a platform set beforehand (e.g. extra filter support) survive.
        cap &= ~(0x3Full | (0x1Full << 20) | (0xFull << 44) | (1ull << 50));
        cap |= ((std::uint64_t)counters - 1);
        cap |= (((std::uint64_t)event_cap_regs - 1) << 20);
        if (interrupt_supported) {
            cap |= (1ull << 50);
            cap |= (((std::uint64_t)msi_vector & 0xFull) << 44);
        }
        reset_runtime();
        return true;
    }

    bool set_event_capabilities(std::size_t idx, std::uint64_t value)
    {
        if (idx >= num_event_caps) return false;
        event_caps[idx] = value;
        return true;
    }

    bool set_fixed_counter(std::size_t idx, std::uint32_t group,
                           std::uint32_t events, std::uint8_t threshold = 1)
    {
        if (idx >= num_counters || group >= num_event_caps) return false;
        if (events & ~event_cap_supported(group)) return false;
        std::uint64_t& cfg = config_ref(idx);
        cfg &= ~(0x3ull | (0xFFFFFFFFull << 24) | (0x1Full << 59) |
                 (0xFFull << 16));
        cfg |= kTypeFixed;
        cfg |= ((std::uint64_t)events << 24);
        cfg |= ((std::uint64_t)(group & 0x1F) << 59);
        cfg |= ((std::uint64_t)threshold << 16);
        return true;
    }

    bool set_configurable_counter(std::size_t idx, std::uint32_t group,
                                  std::uint32_t events,
                                  std::uint8_t threshold = 1)
    {
        if (idx >= num_counters || group >= num_event_caps) return false;
        if (group_reserved(group, idx)) return false;
        if (events & ~event_cap_supported(group)) return false;
        std::uint64_t& cfg = config_ref(idx);
        cfg &= ~(0x3ull | (0xFFFFFFFFull << 24) | (0x1Full << 59) |
                 (0xFFull << 16));
        cfg |= kTypeConfigurable;
        cfg |= ((std::uint64_t)events << 24);
        cfg |= ((std::uint64_t)(group & 0x1F) << 59);
        cfg |= ((std::uint64_t)threshold << 16);
        return true;
    }

    std::uint32_t event_cap_supported(std::uint32_t group) const
    {
        return group < num_event_caps
                   ? (std::uint32_t)(event_caps[group] & 0xFFFFFFFFull)
                   : 0;
    }

    bool group_reserved(std::uint32_t group, std::size_t except) const
    {
        for (std::size_t i = 0; i < num_counters; ++i) {
            if (i == except) continue;
            const std::uint64_t cfg = config_of(i);
            if (counter_type(cfg) == kTypeFixed &&
                counter_group(cfg) == group)
                return true;
        }
        return false;
    }

    std::uint64_t counter_mask() const
    {
        return num_counters >= 64 ? ~0ull
                                  : ((1ull << num_counters) - 1ull);
    }

    // ---- counter access --------------------------------------------------
    std::uint64_t& config_ref(std::size_t n)
    { return n == 0 ? counter_config : extra[n - 1].config; }
    std::uint64_t& data_ref(std::size_t n)
    { return n == 0 ? counter_data : extra[n - 1].data; }
    std::uint32_t& filter_hdm_ref(std::size_t n)
    { return n == 0 ? filter_hdm : extra[n - 1].filter_hdm; }
    bool& filter_enabled_ref(std::size_t n)
    { return n == 0 ? filter_enabled : extra[n - 1].filter_enabled; }
    std::uint64_t& events_counted_ref(std::size_t n)
    { return n == 0 ? events_counted : extra[n - 1].events_counted; }
    std::uint64_t& overflows_ref(std::size_t n)
    { return n == 0 ? overflows : extra[n - 1].overflows; }
    std::uint64_t& rejected_ref(std::size_t n)
    { return n == 0 ? rejected : extra[n - 1].rejected; }

    std::uint64_t config_of(std::size_t n) const
    { return n == 0 ? counter_config : extra[n - 1].config; }
    std::uint64_t data_of(std::size_t n) const
    { return n == 0 ? counter_data : extra[n - 1].data; }
    std::uint32_t filter_hdm_of(std::size_t n) const
    { return n == 0 ? filter_hdm : extra[n - 1].filter_hdm; }
    bool filter_enabled_of(std::size_t n) const
    { return n == 0 ? filter_enabled : extra[n - 1].filter_enabled; }

    // ---- legacy counter-0 helpers (also usable for any counter) ----------
    bool enabled(std::size_t n = 0) const
    { return n < num_counters && (config_of(n) & (1ull << 8)) != 0; }
    bool frozen(std::size_t n = 0) const
    { return n < num_counters && (freeze & (1ull << n)) != 0; }
    bool global_freeze_on_overflow(std::size_t n = 0) const
    { return n < num_counters && counter_freeze_on_overflow(config_of(n)); }
    std::uint32_t event_mask(std::size_t n = 0) const
    { return n < num_counters ? counter_events(config_of(n)) : 0; }

    void set_enable(bool on, std::size_t n = 0)
    {
        if (n >= num_counters) return;
        std::uint64_t& cfg = config_ref(n);
        if (on) cfg |= (1ull << 8);
        else cfg &= ~(1ull << 8);
    }
    void set_freeze(bool on, std::size_t n = 0)
    {
        if (n >= num_counters) return;
        if (on) freeze |= (1ull << n);
        else freeze &= ~(1ull << n);
    }
    void set_global_freeze_on_overflow(bool on, std::size_t n = 0)
    {
        if (n >= num_counters) return;
        std::uint64_t& cfg = config_ref(n);
        if (on) cfg |= (1ull << 10);
        else cfg &= ~(1ull << 10);
    }
    void set_event_mask(std::uint32_t mask, std::size_t n = 0)
    {
        if (n >= num_counters) return;
        std::uint64_t& cfg = config_ref(n);
        cfg = (cfg & ~(0xFFFFFFFFull << 24)) |
              ((std::uint64_t)mask << 24);
    }

    // ---- counting --------------------------------------------------------
    // Count one event on a counter unit.  Free-run units (type 0) have no
    // event input in this model and are rejected; fixed (type 1) and
    // configurable (type 2) units count the selected events of their group.
    void add_event(std::uint32_t event_id, std::uint64_t count = 1,
                   std::uint32_t hdm = 0, std::size_t counter = 0)
    {
        if (counter >= num_counters || event_id >= 32) {
            ++rejected;
            return;
        }
        const std::uint64_t cfg = config_of(counter);
        const std::uint8_t type = counter_type(cfg);
        const std::uint32_t group = counter_group(cfg);
        const std::uint32_t events = counter_events(cfg);

        if (type == (std::uint8_t)kTypeFreeRun || group >= num_event_caps ||
            (type == (std::uint8_t)kTypeConfigurable &&
             (events & ~event_cap_supported(group)) != 0) ||
            !enabled(counter) || frozen(counter) ||
            (events & (1u << event_id)) == 0 ||
            (filter_enabled_of(counter) && hdm != filter_hdm_of(counter))) {
            ++rejected_ref(counter);
            return;
        }

        events_counted_ref(counter) += count;
        std::uint64_t& data = data_ref(counter);
        if (count > UINT64_MAX - data) {
            data = data + count;                    // wraps modulo 2^64
            overflow_status |= (1ull << counter);
            ++overflows_ref(counter);
            if (counter_freeze_on_overflow(cfg)) freeze |= (1ull << counter);
            if ((cap & (1ull << 50)) && counter_int_on_overflow(cfg))
                ++irq_assertions;
        } else {
            data += count;
        }
    }

    // Interrupt state for the capabilities profile: asserted while any
    // overflow bit remains set on a counter configured with INT_ON_OVRFLW.
    bool interrupt_pending() const
    {
        if ((cap & (1ull << 50)) == 0) return false;
        for (std::size_t n = 0; n < num_counters; ++n) {
            if (counter_int_on_overflow(config_of(n)) &&
                (overflow_status & (1ull << n)) != 0)
                return true;
        }
        return false;
    }
    std::uint32_t irq_vector() const
    { return (std::uint32_t)((cap >> 44) & 0xFull); }

    // Optional host MSI-X hook.  When set, raise_overflow_irq() calls it with
    // the MSI_N vector while an overflow interrupt is pending; the hook
    // returns true when the MSI-X message was actually sent (vector
    // programmed and unmasked).  Without a hook the request is counted as
    // unrouted, and a blocked hook is counted as blocked -- never silently
    // dropped.  The function-pointer form keeps this header dependency-free
    // (the owner can bind `CxlMsixTable::trigger_and_raise`).
    bool (*irq_hook)(void* ctx, std::uint32_t vector) = nullptr;
    void* irq_hook_ctx = nullptr;
    std::uint64_t irq_raised = 0, irq_blocked = 0, irq_unrouted = 0;

    bool raise_overflow_irq()
    {
        if (!interrupt_pending()) return false;
        if (!irq_hook) { ++irq_unrouted; return false; }
        if (!irq_hook(irq_hook_ctx, irq_vector())) { ++irq_blocked; return false; }
        ++irq_raised;
        return true;
    }

    void reset_runtime()
    {
        overflow_status = 0;
        freeze = 0;
        counter_data = 0;
        events_counted = 0;
        overflows = 0;
        rejected = 0;
        filter_hdm = 0;
        filter_enabled = false;
        for (std::size_t i = 0; i < kMaxCounters; ++i) {
            extra[i].data = 0;
            extra[i].events_counted = 0;
            extra[i].overflows = 0;
            extra[i].rejected = 0;
            extra[i].filter_hdm = 0;
            extra[i].filter_enabled = false;
        }
        irq_assertions = 0;
        irq_raised = 0;
        irq_blocked = 0;
        irq_unrouted = 0;
        config_rejected = 0;
    }

    // ---- register access -------------------------------------------------
    // 4/8-byte accesses; a 4-byte access to a 64-bit register addresses the
    // low or high half.  Reserved offsets return zero and ignore writes.
    bool read(std::uint32_t off, unsigned size, std::uint64_t* val) const
    {
        if (!val || size == 0 || size > 8) return false;
        std::uint64_t v = 0;

        if (off >= 0x400 && off < 0xC00) {
            // Filter Configuration registers are 32-bit, 8 per counter:
            // filter 0 is HDM; other filters are not implemented.
            const unsigned idx = (off - 0x400) >> 2;
            const unsigned f = idx & 7u;
            const unsigned n = idx >> 3;
            v = (n < num_counters && f == 0) ? filter_hdm_of(n) : 0;
            v >>= 8 * (off & 3u);
        } else if (off < 0x100) {
            switch (off & ~0x7u) {
            case 0x000: v = cap; break;
            case 0x010: v = overflow_status; break;
            case 0x018: v = freeze & counter_mask(); break;
            default: v = 0; break;
            }
            v >>= 8 * (off & 7u);
        } else if (off < 0x200) {
            const unsigned idx = (off - 0x100) >> 3;
            v = idx < num_event_caps ? event_caps[idx] : 0;
            v >>= 8 * (off & 7u);
        } else if (off < 0x400) {
            const unsigned n = (off - 0x200) >> 3;
            v = n < num_counters ? config_of(n) : 0;
            v >>= 8 * (off & 7u);
        } else if (off < 0x1000) {
            const unsigned n = (off - 0xC00) >> 3;
            v = n < num_counters ? data_of(n) : 0;
            v >>= 8 * (off & 7u);
        } else {
            v = 0;
        }

        if (size >= 8) *val = v;
        else *val = v & ((1ull << (8 * size)) - 1ull);
        return true;
    }

    bool write(std::uint32_t off, unsigned size, std::uint64_t val)
    {
        if (size != 4 && size != 8) return false;
        const unsigned half = (off & 7u) >= 4 ? 1u : 0u;
        const std::uint64_t wval =
            (size == 8) ? val : ((std::uint64_t)val << (32 * half));
        const std::uint64_t wmask =
            (size == 8) ? ~0ull : (0xFFFFFFFFull << (32 * half));

        if (off >= 0x400 && off < 0xC00) {
            const unsigned idx = (off - 0x400) >> 2;
            const unsigned f = idx & 7u;
            const unsigned n = idx >> 3;
            if (size == 4 && f == 0 && n < num_counters) {
                filter_hdm_ref(n) = (std::uint32_t)val;
                filter_enabled_ref(n) = true;
            }
            return true;
        }
        if (off < 0x100) {
            switch (off & ~0x7u) {
            case 0x010:                       // RW1C per-counter overflow
                overflow_status &= ~(wval & wmask);
                break;
            case 0x018:                       // per-counter freeze
                freeze = (freeze & ~(wmask & counter_mask())) |
                         (wval & wmask & counter_mask());
                break;
            default:
                break;
            }
            return true;
        }
        if (off < 0x200) return true;          // Event Capabilities: HwInit
        if (off < 0x400) {
            const unsigned n = (off - 0x200) >> 3;
            if (n >= num_counters) return true;
            std::uint64_t& cfg = config_ref(n);
            const std::uint64_t old = cfg;
            std::uint64_t merged = (cfg & ~wmask) | (wval & wmask);
            const std::uint64_t type = old & 0x3ull;
            merged = (merged & ~0x3ull) | type;
            if (type == kTypeFixed || type == kTypeFreeRun) {
                // Type, event group and events are HwInit for these units.
                merged = (merged & ~(0xFFFFFFFFull << 24)) |
                         (old & (0xFFFFFFFFull << 24));
                merged = (merged & ~(0x1Full << 59)) |
                         (old & (0x1Full << 59));
            } else if (type == kTypeConfigurable) {
                const std::uint32_t group =
                    (std::uint32_t)((merged >> 59) & 0x1Full);
                const std::uint32_t events =
                    (std::uint32_t)((merged >> 24) & 0xFFFFFFFFull);
                if (group >= num_event_caps ||
                    group_reserved(group, n) ||
                    (events & ~event_cap_supported(group)) != 0) {
                    ++config_rejected;
                    return false;
                }
            }
            cfg = merged;
            return true;
        }
        if (off < 0x1000) {
            const unsigned n = (off - 0xC00) >> 3;
            if (n >= num_counters) return true;
            std::uint64_t& data = data_ref(n);
            data = (data & ~wmask) | (wval & wmask);
            return true;
        }
        return true;
    }
};

} // namespace cxl_tlm
