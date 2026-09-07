#pragma once
namespace l3d {
namespace playtest {

// CLI entry points (desktop-only, console stdio, no SDL).
// Returns a process exit code, or -1 when argv holds no playtest mode
// (caller then boots the game normally).
int playtest_cli(int argc, char** argv);

} // namespace playtest
} // namespace l3d
