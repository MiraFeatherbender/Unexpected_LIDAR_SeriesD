2026-02-10: Created chat-mode package with system prompt, few-shots, template, ingest list, and README. (assistant)
2026-02-10: Refactored `io_i2c_oled.c` to remove embedded Hello World UI; added `main/ui/ui_hello.c` and `main/ui/ui_hello.h`. (assistant)
2026-02-10: Updated `main/CMakeLists.txt` to compile `ui/ui_hello.c` and added `ui` to `INCLUDE_DIRS`; updated `main.c` to call `ui_hello_show()`. (assistant)
