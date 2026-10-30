// -----------------------------------------------------------------------------
// cxl_pcie_base.h -- PCIe base register layer used by the CXL configuration
// space.
//
//   The CXL switch is a PCIe switch with CXL extensions.  This class is the
//   thin CXL-facing adapter over the complete PCIe model configuration space
//   (pcie_cfg_space): Type 0 / Type 1 header, Command (MSE/IOE/BME), BAR
//   sizing and programming, and Type 1 bridge windows are all handled by the
//   PCIe model -- never re-implemented here.
//
//   The PCIe model type is kept out of this public header on purpose: the CXL
//   delivery exposes CXL headers only.  The implementation is in
//   cxl_pcie_base.cpp.
// -----------------------------------------------------------------------------
#pragma once
#include <cstdint>
#include <memory>
#include <string>

namespace cxl_tlm {

class CxlPcieBase {
public:
    CxlPcieBase();
    ~CxlPcieBase();
    CxlPcieBase(const CxlPcieBase& other);
    CxlPcieBase& operator=(const CxlPcieBase& other);

    // Type 0 (bridge == false) or Type 1 (bridge == true), nbar slots.
    void init(bool bridge, int nbar, bool msi_cap_en);

    void set_identity(std::uint16_t vid, std::uint16_t did, std::uint8_t rev,
                      std::uint8_t prog_if, std::uint8_t sub_class,
                      std::uint8_t base_class);
    void set_bar(int i, std::uint64_t size, bool is64, bool prefetch);
    void set_pm(bool bridge);

    // Byte-granular access over the PCIe base registers.  Sizes are 1/2/4 and
    // the offset must be size-aligned.  The PCIe model uses 4-byte registers
    // with byte enables; this adapter converts the access.
    bool read(std::uint16_t off, unsigned size, std::uint32_t* val,
              std::string* err = nullptr) const;
    bool write(std::uint16_t off, unsigned size, std::uint32_t val,
               std::string* err = nullptr);

    std::uint64_t bar_size(int i) const;
    std::uint64_t bar_base(int i) const;
    bool mse() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cxl_tlm
