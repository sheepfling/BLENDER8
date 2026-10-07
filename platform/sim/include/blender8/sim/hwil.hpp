#pragma once
#include "blender8/sim/components.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
namespace b8::sim {
enum class LinkClock { logical_lockstep, physical_realtime };
struct MotorLinkCapabilities {
    LinkClock clock=LinkClock::logical_lockstep;
    Tick sample_period_us=1;
    bool independent_inhibit=true;
};
struct MotorPinRequest {Tick time_us;std::uint64_t sequence;bool pwm;bool enable;};
struct TachPinReply {
    Tick time_us;std::uint64_t sequence;bool tach;bool valid;
    // Optional independently measured/modelled case attachment, never invented from tach.
    std::optional<double> case_c=std::nullopt;
};
class MotorPinTransport {
public:
    virtual ~MotorPinTransport()=default;
    [[nodiscard]] virtual MotorLinkCapabilities capabilities() const noexcept=0;
    [[nodiscard]] virtual TachPinReply exchange(const MotorPinRequest& request)=0;
    // Must be safe to call on error/destruction. A physical implementation needs its own
    // local output timeout; a process/USB link alone cannot guarantee loss-of-host safety.
    virtual void inhibit() noexcept=0;
};
class HardwareLinkError : public std::runtime_error {
public:using std::runtime_error::runtime_error;
};
// A tested lockstep protocol adapter, not a USB/DAQ driver. It intentionally REJECTS
// wall-clock hardware: an as-fast-as-possible simulator must never pretend that its 1us
// step is an on-time physical transaction. Implement a paced, buffered hardware scheduler
// and measured target transport before enabling physical_realtime in another adapter.
class LockstepMotorAdapter final:public MotorDevice {
public:
    LockstepMotorAdapter(MotorPorts ports,std::shared_ptr<MotorPinTransport> link);
    ~LockstepMotorAdapter() override;
    void advance_one_us() override;
    void inhibit_host_failure() noexcept override;
    [[nodiscard]] bool healthy() const noexcept{return healthy_;}
private:
    MotorPorts ports_;std::shared_ptr<MotorPinTransport> link_;Driver tach_driver_;
    Tick time_=0;std::uint64_t sequence_=0;bool healthy_=true;
};
}
