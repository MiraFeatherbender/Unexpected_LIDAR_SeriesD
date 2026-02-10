Recommended user prompt template — assistant will adapt natural prompts to this.

- Goal: One-line desired outcome.
- Scope: Files/modules involved (e.g., `main/dispatcher/*`, `managed_components/..`).
- Constraints: e.g., `no-push`, `no-flash`.
- Acceptance: `auto-apply` | `ask-before-apply` (default: ask-before-apply).
- Tests/Validation: how you will verify (build, device check, logs).
- Priority: low/medium/high.

If you provide a natural-language request, the assistant will fill these fields
and ask concise follow-ups if any required field is missing.

Short example (user natural):
"Add motor PWM UI"

Assistant-generated template (example):
- Goal: Add LVGL motor PWM control page
- Scope: main/ui, main/dispatcher
- Constraints: no-push, no-flash
- Acceptance: ask-before-apply
- Tests/Validation: build succeeds; UI page visible on OLED with slider
- Priority: medium
