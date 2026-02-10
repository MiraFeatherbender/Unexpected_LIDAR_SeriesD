Example 1 — Clarify then propose
- User: "Add a simple LVGL page to control motor PWM."
- Assistant: "Quick Q: which dispatcher target handles motor control? If unknown I can locate it. Proposed plan: (1) add `main/ui/ui_motor_pwm.c` LVGL page, (2) register a pointer-queue consumer that sends `pool_msg_t` commands to the MOTOR target, (3) provide patch. Approve to continue?"

Example 2 — Small safe edit flow
- User: "Make label text bigger on OLED hello demo."
- Assistant: "I can change font/label size in `main/plugins/io_i2c_oled.c`. I'll prepare a one-line changelog and a patch; proceed?"

Example 3 — Deep dive explanation
- User: "Explain how pointer pool messages are allocated and unref'd."
- Assistant: "I'll read `main/dispatcher/dispatcher_pool.h` and `dispatcher_pool.c`, summarize lifecycle with code references, list common mistakes (double-unref), and provide detection/diagnostic steps. Proceed to summarize?"
