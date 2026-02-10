# "Unexpected_LIDAR_SeriesD" Chat Mode

This chat-mode package contains the system prompt, interaction examples, and
helper templates created for pairing on the Unexpected_LIDAR_SeriesD project.

Files:
- `system_prompt.txt` — the system prompt to use for the chat-mode.
- `few_shots.md` — short example interactions demonstrating expected behavior.
- `user_prompt_template.md` — recommended template when making requests.
- `ingest_list.json` — files the assistant should index first (code-first).
- `changelog.md` — one-line changelog entries for edits (assistant updates).

Usage:
- Load `system_prompt.txt` into your chat UI as the system prompt for a
  persistent custom mode, and optionally paste the few-shots as examples.
- The assistant will adapt natural user prompts to the template and ask
  clarifying questions when fields are missing.

Constraints enforced by the mode:
- No automatic pushes or flashing of devices.
- Ask-before-apply is the default for code edits.

If you want, I can now (A) create an indexed summary file in the repo for
quick lookups, or (B) draft the initial LVGL motor-PWM UI skeleton and show
the proposed patches. Pick A or B.
