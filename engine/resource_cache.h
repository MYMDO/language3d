#pragma once
#include "core.h"
#include <cstddef>
#include <array>

namespace l3d {

// Allocation-free cache for immutable resource blobs. The cache stores views
// into caller-owned packed assets; it never copies payloads and never frees them.
template <size_t Slots>
class ResourceCache {
public:
    struct Entry {
        u16 id{0xFFFFu};
        const u8* data{nullptr};
        u32 size{0};
        bool valid() const { return id != 0xFFFFu && data != nullptr; }
    };

    void clear() {
        for (auto& e : entries_) e = {};
    }

    bool put(u16 id, const u8* data, size_t size) {
        if (!data || size > 0xFFFFFFFFu) return false;
        for (auto& e : entries_) {
            if (e.valid() && e.id == id) { e.data = data; e.size = static_cast<u32>(size); return true; }
        }
        for (auto& e : entries_) {
            if (!e.valid()) { e.id = id; e.data = data; e.size = static_cast<u32>(size); return true; }
        }
        return false;
    }

    const Entry* get(u16 id) const {
        for (const auto& e : entries_) if (e.valid() && e.id == id) return &e;
        return nullptr;
    }

    size_t size() const {
        size_t n = 0;
        for (const auto& e : entries_) if (e.valid()) ++n;
        return n;
    }

private:
    std::array<Entry, Slots> entries_{};
};

} // namespace l3d
