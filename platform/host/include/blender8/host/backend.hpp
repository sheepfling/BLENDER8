#pragma once
#include "blender8/device.hpp"
#include <cstdint>
namespace b8::host {
// Link/binding-side contract, deliberately NOT exported by blender8_sdk.
// A hardware target supplies its own ordered MMIO implementation of these transactions.
// An HWIL bridge must preserve byte-pair latching and register side effects; it may not
// cache arbitrary reads or equate a remote bus transaction to a host pointer dereference.
class RegisterBackend {
public:
    virtual ~RegisterBackend() = default;
    [[nodiscard]] virtual std::uint8_t read8(Reg reg) = 0;
    virtual void write8(Reg reg,std::uint8_t value) = 0;
    virtual void idle() = 0;
};
class ScopedBinding {
public:
    explicit ScopedBinding(RegisterBackend& backend);
    ~ScopedBinding();
    ScopedBinding(const ScopedBinding&)=delete;
    ScopedBinding& operator=(const ScopedBinding&)=delete;
    [[nodiscard]] static RegisterBackend& current();
private:
    static thread_local RegisterBackend* active_;
};
}
