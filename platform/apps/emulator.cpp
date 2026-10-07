#include "blender8/firmware.hpp"
#include "blender8/sim/session.hpp"
#include <fstream>
#include <iostream>
#include <string>
#if defined(B8_TARGET_B16)
constexpr auto selected_device=b8::sim::DeviceProfile::b16;
#else
constexpr auto selected_device=b8::sim::DeviceProfile::b8;
#endif
#ifndef B8_FIRMWARE_NAME
#define B8_FIRMWARE_NAME "student"
#endif
int main(int argc, char** argv) {
    using namespace b8::sim;
    try {
        SessionOptions options;
        std::string trace_path;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--bench") options.bench = true;
            else if (argument == "--legacy02") options.profile = BoardProfile::legacy02;
            else if (argument == "--bus-trace" && i + 1 < argc) trace_path = argv[++i];
            else if (argument == "--help") {
                std::cout << "B8 emulator. Commands on stdin; one JSON response per line.\n"
                    "snapshot | run US | trace US SAMPLE_US | schedule AT_US FIXTURE_COMMAND | quit\n"
                    "Physical commands: speed N, pulse 0/1, stop 0/1, jar 0/1, power 0/1, load 0..1, jam 0/1.\n"
                    "--bench disables firmware, permits raw write ADDRESS VALUE, never auto-feeds WDT.\n";
                return 0;
            } else throw std::invalid_argument("unknown/missing emulator option");
        }
        // Trace outlives Session, including shutdown.
        std::ofstream trace;
        Session session({firmware::vectors, firmware::reset, firmware::step, B8_FIRMWARE_NAME, selected_device}, options);
        if (!trace_path.empty()) {
            trace.open(trace_path);
            if (!trace) throw std::runtime_error("cannot open bus trace");
            trace << "time_us,operation,address,value,reset_serial\n";
            session.set_bus_trace([&](const BusTrace& e) {
                trace << e.time_us << ',' << e.operation << ',' << static_cast<unsigned>(e.reg)
                      << ',' << static_cast<unsigned>(e.value) << ',' << e.reset_serial << '\n';
            });
        }
        std::cout << session.hello() << '\n' << std::flush;
        std::string line;
        while (!session.closed() && std::getline(std::cin, line))
            std::cout << session.execute(line) << '\n' << std::flush;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "B8 emulator: " << error.what() << '\n';
        return 2;
    }
}
