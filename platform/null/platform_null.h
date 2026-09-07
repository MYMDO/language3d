#pragma once
// Test/support hooks for the null Platform API backend.
//
// Implemented by platform/null/platform_null.cpp, linked into host tests
// and headless tools — never into shipping game binaries.
#include "l3d_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

// Queue a scripted input event (FIFO, depth 32). Returns 1 if accepted.
int l3d_pf_null_push(const l3d_event_t* ev);
// Number of successful present calls since init.
uint32_t l3d_pf_null_presents(void);
// Last presented indexed8 frame (240x240, row-major).
const uint8_t* l3d_pf_null_last_frame(void);
// Last fullscreen flag passed to l3d_pf_set_fullscreen.
int l3d_pf_null_fullscreen(void);

#ifdef __cplusplus
}
#endif
