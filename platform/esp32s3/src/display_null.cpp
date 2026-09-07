#include "esp32s3_display.h"
#include "core.h"
namespace { alignas(16) static uint8_t g_fb[2][l3d::MemoryBudget::FRAMEBUFFER8]; static int g_back=0; static int g_busy=0; }
extern "C" int esp32s3_display_init(void){g_back=0;g_busy=0;return 1;}
extern "C" uint8_t* esp32s3_display_acquire_framebuffer(void){return g_busy?nullptr:g_fb[g_back];}
extern "C" int esp32s3_display_present(const uint8_t* p,uint16_t w,uint16_t h,uint32_t s){if(g_busy||!p||w!=l3d::Config::WIDTH||h!=l3d::Config::HEIGHT||s<w)return 0;g_back^=1;return 1;}
extern "C" int esp32s3_display_is_busy(void){return g_busy;}
extern "C" void esp32s3_display_wait_idle(void){g_busy=0;}
extern "C" void esp32s3_display_shutdown(void){g_busy=0;}
