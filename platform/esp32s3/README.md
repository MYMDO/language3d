# ESP32-S3 display boundary

This is intentionally a minimal platform skeleton. The engine uses the same
indexed 8-bit framebuffer contract as RP2350B. A concrete ESP-IDF driver can
later bind the presentation call to the selected LCD peripheral / GDMA path.
No panel controller or pinout is assumed here.
