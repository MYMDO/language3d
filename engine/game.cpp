#include "game.h"
#include "l3d_math.h"
#include "assets.h"
#include <algorithm>

namespace l3d {
namespace {
constexpr i32 PLAYER_RADIUS_RAW = 11796; // 0.18
inline bool blocked(const Game& g, Fx x, Fx y) {
    const Fx r = Fx::from_raw(PLAYER_RADIUS_RAW);
    return g.solid((x-r).to_int(), (y-r).to_int()) ||
           g.solid((x+r).to_int(), (y-r).to_int()) ||
           g.solid((x-r).to_int(), (y+r).to_int()) ||
           g.solid((x+r).to_int(), (y+r).to_int());
}
inline i64 dist2_raw(const Vec2& a,const Vec2& b){
    const i64 dx=i64(a.x.raw)-b.x.raw, dy=i64(a.y.raw)-b.y.raw;
    return (dx*dx+dy*dy)>>16;
}
}

bool Game::solid(i32 x,i32 y) const { const AssetPackView world = assets_.valid() ? assets_ : builtin_assets(); return doors_.blocks(x,y,world); }

void Game::reset(){ player_=Player{}; npc_=NPC{}; progress_=Progress{}; sprites_.clear(); const AssetPackView world = assets_.valid() ? assets_ : builtin_assets(); doors_.init(world); sprites_.add(SpriteEntity{npc_.pos,1,0,32,64}); mode_=Mode::Explore; status_="EXPLORE AND FIND THE NPC"; }

void Game::update(const InputState& in, u32 dt_ms){
    if(in.escape_pressed){ mode_=Mode::Explore; return; }
    if(in.help_pressed){ mode_=Mode::Help; return; }
    if(in.progress_pressed){ mode_= mode_==Mode::Progress ? Mode::Explore : Mode::Progress; return; }
    doors_.update(dt_ms);
    if(mode_==Mode::Explore){
        i32 delta = 0;
        if(in.left)  delta -= i32((u32(ControlTuning::TURN_SPEED) * dt_ms) / 1000u);
        if(in.right) delta += i32((u32(ControlTuning::TURN_SPEED) * dt_ms) / 1000u);
        if(in.mouse_x != 0)
            delta += i32(in.mouse_x) * i32(ControlTuning::MOUSE_TURN_PER_PIXEL);
        player_.angle_turn = static_cast<u16>(player_.angle_turn + delta);
        const Fx s = TrigLut::sin16(player_.angle_turn);
        const Fx c = TrigLut::cos16(player_.angle_turn);
        i32 fwd = 0;
        if(in.up) fwd += 1;
        if(in.down) fwd -= 1;
        const i32 turn_sign = fwd;
        const i32 strafe = (in.strafe_right ? 1 : 0) - (in.strafe_left ? 1 : 0);
        if(turn_sign || strafe){
            const i64 move_scale = (i64(ControlTuning::MOVE_SPEED_RAW) * dt_ms) / 1000;
            const i64 strafe_scale = (i64(ControlTuning::STRAFE_SPEED_RAW) * dt_ms) / 1000;
            const i64 dx_raw = (i64(c.raw) * move_scale * turn_sign - i64(s.raw) * strafe_scale * strafe) >> 16;
            const i64 dy_raw = (i64(s.raw) * move_scale * turn_sign + i64(c.raw) * strafe_scale * strafe) >> 16;
            Fx nx = Fx::from_raw(player_.pos.x.raw + i32(dx_raw));
            Fx ny = Fx::from_raw(player_.pos.y.raw + i32(dy_raw));
            if(!blocked(*this, nx, player_.pos.y)) player_.pos.x = nx;
            if(!blocked(*this, player_.pos.x, ny)) player_.pos.y = ny;
        }
        if(in.interact_pressed){
            if(doors_.toggle_near(player_.pos, 1)) status_="DOOR OPENING / CLOSING";
            else if(dist2_raw(player_.pos,npc_.pos)<(i64(4)<<16)) mode_=Mode::Dialogue;
            else status_="MOVE CLOSER TO THE NPC";
        }
        if(in.language_pressed) mode_=Mode::Quiz;
        if(dist2_raw(player_.pos,npc_.pos)<(i64(671089)<<0)) status_="NPC NEARBY - PRESS E";
        else if(!in.interact_pressed) status_="EXPLORE AND FIND THE NPC";
    } else if(mode_==Mode::Dialogue){
        if(in.language_pressed) mode_=Mode::Quiz;
    } else if(mode_==Mode::Quiz){
        if(in.answer==1){ progress_.prioritize=std::min<u8>(5,progress_.prioritize+1); status_="CORRECT - MASTERY INCREASED"; mode_=Mode::Dialogue; }
        else if(in.answer==2 || in.answer==3) status_="NOT QUITE - TRY AGAIN";
    }
}

void Game::render(SoftwareRaycaster& r,Framebuffer8& fb){
    r.set_assets(assets_.valid() ? assets_ : builtin_assets());
    r.set_doors(&doors_);
    r.begin(fb);
    if(mode_==Mode::Explore){ r.draw_world(fb,player_,npc_); r.draw_sprites(fb,player_,sprites_.data(),sprites_.size()); r.draw_hud(fb,"OBJECTIVE: FIND THE NPC",status_); }
    else r.draw_mode(fb,mode_,progress_);
    r.end(fb);
}
}
