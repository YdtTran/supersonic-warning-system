# Worker Execution Prompt Template

Emit exactly this structure, fully filled in. No placeholders, no "TBD".

---

## 🛠 WORKER EXECUTION PROMPT: [Step <ID> — <Title>]

### Context & Prerequisites
- **Dependency state:** <one line per prerequisite step: what it produced, and the exact
  signature/path a Worker needs. Copy from the ledger's "Contracts established".>
- **Repo conventions that apply:** <e.g. "ESP-IDF component project; no `delay()` on the
  network task; telemetry keys are snake_case per docs/architecture/DATA_SCHEMA.md">
- **Target files:**
  - `path/to/file.c` — modify
  - `path/to/file.h` — create

### Objective
<Single-responsibility statement. One sentence of what, one of why. If a public interface is
being introduced, write the exact signature here.>

### Strict Constraints
- Do NOT touch files outside the Target Files list.
- Do NOT rewrite an entire file when a partial edit suffices.
- Follow existing codebase patterns, naming, error handling, and typing conventions.
- <step-specific constraint, e.g. "do not change sdkconfig", "no new lib_deps">

### Acceptance Criteria (DoD)
1. <observable criterion — a symbol exists, a call site is updated, a field is emitted>
2. <observable criterion>
3. Verification: `<exact command, e.g. pio run -e yolo_uno>` exits 0 with no new warnings.
4. <hardware/runtime check if the build cannot prove it, e.g. "flash to COM9; serial log shows
   `warn_state=1` within 2 s of distance < 50 cm">

### Report back
State which criteria passed, paste the verification output tail, and list any deviation from
the Target Files list with justification.

---

## Quality bar before you send it

- [ ] A Worker with zero conversation history could execute this.
- [ ] Every path is real (verify with a file check, don't guess).
- [ ] The verification command matches `AGENTS.md` exactly.
- [ ] The DoD can fail. If nothing could make it fail, it isn't a DoD.
