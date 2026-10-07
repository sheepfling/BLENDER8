#pragma once
#include "blender8/sim/board.hpp"
#include <functional>
#include <string>
#include <string_view>
namespace b8::sim {
// Validated physical stimulus/bench operation. No access from firmware.
// Parsing is non-mutating; scheduled commands capture values, never caller references.
[[nodiscard]] std::function<void(Board&)> parse_fixture_command(std::string_view line,bool allow_register_writes=false);
// Dense trace points omit repeated capture histories; standalone snapshots include them.
[[nodiscard]] std::string snapshot_json(Board& board, bool captures = true);
[[nodiscard]] std::string json_string(std::string_view text);
}
