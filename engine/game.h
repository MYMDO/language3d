#pragma once
#include "core.h"
#include "renderer.h"
#include "assets.h"
#include "entities.h"
#include "world.h"
#include <cstdint>
#include <string_view>

namespace l3d {

class Game {
public:
    void reset();
    void set_assets(const AssetPackView& assets) { assets_ = assets; }
    void update(const InputState& input, u32 dt_ms);
    void render(SoftwareRaycaster& renderer, Framebuffer8& framebuffer);
    const Player& player() const { return player_; }
    const NPC& npc() const { return npc_; }
    EntityPool<8>& sprites() { return sprites_; }
    const EntityPool<8>& sprites() const { return sprites_; }
    const Progress& progress() const { return progress_; }
    const DoorSystem<8>& doors() const { return doors_; }
    Mode mode() const { return mode_; }
    std::string_view status() const { return status_; }
    bool solid(i32 x, i32 y) const;
private:
    Player player_{};
    NPC npc_{};
    Progress progress_{};
    Mode mode_{Mode::Explore};
    const char* status_{"EXPLORE AND FIND THE NPC"};
    AssetPackView assets_{};
    EntityPool<8> sprites_{};
    DoorSystem<8> doors_{};
};

}
