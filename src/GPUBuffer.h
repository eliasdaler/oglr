#pragma once

#include <cstdint>

struct GPUBuffer {
    std::uint32_t buffer{}; // GL handle
    std::size_t size{};
};
