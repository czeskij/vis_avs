// Preset I/O (relocated)
#pragma once
#include "effect_oscstar.h"
#include <optional>
#include <string>

bool save_oscstar_preset(const std::string& path, const OscStarParams& p);
std::optional<OscStarParams> load_oscstar_preset(const std::string& path);
