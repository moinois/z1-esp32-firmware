# Architecture Decision Log

This log records architecture decisions that materially constrain the firmware
implementation. It complements the normative product specification; it does not
replace or reinterpret product requirements.

## Maintaining this log

Each decision receives a stable sequential identifier. Existing decisions are
not silently rewritten after implementation depends on them. When a decision is
replaced, mark it `Superseded` and link to the replacing decision.

Allowed statuses are `Proposed`, `Accepted`, `Superseded`, and `Rejected`.

## Decision index

| ID | Decision | Status | Date |
|---|---|---|---|
| ADR-001 | Keep specification and implementation in separate repositories | Accepted | 2026-07-20 |
| ADR-002 | Use ports and adapters around a portable C++ core | Accepted | 2026-07-20 |
| ADR-003 | Develop deterministic behavior test-first on the host | Accepted | 2026-07-20 |
| ADR-004 | Use explicit bounded state and value results | Accepted | 2026-07-20 |
| ADR-005 | Build target code without C++ exceptions or RTTI | Accepted | 2026-07-20 |
| ADR-006 | Pin the target implementation profile to ESP-IDF 5.4.1 | Accepted | 2026-07-20 |
| ADR-007 | Separate logical file ownership from physical play ownership | Accepted | 2026-07-20 |
| ADR-008 | Keep generated build state outside version control | Accepted | 2026-07-20 |

---

## ADR-001: Keep specification and implementation in separate repositories

- **Status:** Accepted
- **Date:** 2026-07-20

### Context

The specification has an independent owner and history. Mixing implementation
changes into that repository would blur ownership and risk accidental changes
to the normative source.

### Decision

The `spec` and `implementation` repositories remain sibling directories. The
implementation refers to requirement identifiers but does not modify or copy
the specification.

### Consequences

- Each repository has independent history, remotes, and release policy.
- Requirement traceability uses stable specification identifiers.
- A development checkout must make the specification separately available.

---

## ADR-002: Use ports and adapters around a portable C++ core

- **Status:** Accepted
- **Date:** 2026-07-20

### Context

Most protocol behavior is deterministic, while ESP-IDF APIs combine hardware,
RTOS, allocation, and lifetime concerns. Direct SDK use throughout protocol
logic would make host testing difficult and couple unrelated features.

### Decision

Use three architectural layers:

1. `components/core` owns deterministic byte transformations and validation.
2. `components/application` owns routing, ownership, schedules, protocol state,
   and service coordination.
3. ESP-IDF adapters own RTOS, persistence, filesystems, transports, and hardware.

Dependencies point inward. Core code has no dependency on application or
ESP-IDF. Application code may depend on core and narrow abstract ports. Target
adapters may depend on both.

### Consequences

- Deterministic behavior can be tested without target hardware.
- Peripheral implementations can be replaced behind narrow interfaces.
- ESP-IDF types must be translated at adapter boundaries.
- Composition and lifetime ownership are concentrated in target startup code.

---

## ADR-003: Develop deterministic behavior test-first on the host

- **Status:** Accepted
- **Date:** 2026-07-20

### Context

The specification contains byte-exact rules, unusual recovery paths, and many
edge conditions. Target-only tests would be slow and mix product-logic defects
with hardware and SDK defects.

### Decision

For every deterministic slice:

1. add requirement-named tests that fail because behavior is absent;
2. implement the smallest coherent module that makes them pass;
3. refactor while keeping tests green;
4. build the ESP32-S3 target;
5. update requirement traceability; and
6. commit the completed slice.

The host harness remains dependency-free and is never linked into target
firmware.

### Consequences

- Requirement intent remains visible in test names.
- Host feedback supports extensive malformed-input and boundary testing.
- Hardware timing still requires target or physical-device verification.

---

## ADR-004: Use explicit bounded state and value results

- **Status:** Accepted
- **Date:** 2026-07-20

### Context

The specification defines queue capacities, payload limits, snapshot lengths,
and silent failures. Implicit growth would make exhaustion behavior difficult to
reason about and could change observable behavior.

### Decision

Represent outcomes with explicit value types, optional values, enums, and
bounded containers. Keep capacities next to their owning modules. Avoid
unbounded background accumulation.

### Consequences

- Resource behavior is visible in APIs and tests.
- Callers handle rejected, absent, and silently consumed outcomes explicitly.
- Dynamic containers remain acceptable when enclosing input is already bounded.

---

## ADR-005: Build target code without C++ exceptions or RTTI

- **Status:** Accepted
- **Date:** 2026-07-20

### Context

ESP-IDF disables C++ exceptions and RTTI by default. Enabling them would add
binary cost and failure paths that do not match explicit protocol outcomes.

### Decision

Keep exceptions and RTTI disabled. Functions report failure through typed
results, optional values, unambiguous empty results, or adapter error codes.

### Consequences

- Portable modules compile under the target's language restrictions.
- APIs must state how absence differs from failure.
- Substitution uses narrow interfaces without relying on RTTI.

---

## ADR-006: Pin the target implementation profile to ESP-IDF 5.4.1

- **Status:** Accepted
- **Date:** 2026-07-20

### Context

SDK configuration, drivers, diagnostics, and edge behavior can change between
releases. The specified firmware identity also reports ESP-IDF 5.4.1.

### Decision

Develop and verify the target with ESP-IDF 5.4.1 and its ESP32-S3 GCC toolchain.
An SDK upgrade requires a new decision, complete host tests, a clean target
build, and hardware regression testing.

### Consequences

- SDK-dependent target behavior is reproducible.
- Newer SDK fixes are not adopted automatically.
- Upgrades must assess externally observable changes before adoption.

---

## ADR-007: Separate logical file ownership from physical play ownership

- **Status:** Accepted
- **Date:** 2026-07-20

### Context

File transfer and streamed play have intentionally different disconnect rules.
File ownership survives reuse of a TCP slot or USB re-enumeration, while play
ownership is released with the physical connection.

### Decision

Represent file ownership with transport and logical slot, excluding generation.
Represent play ownership with transport, slot, and connection generation. Keep
both owners independent.

### Consequences

- Reconnected clients can continue file exchanges as specified.
- A reused slot cannot inherit active play ownership.
- Transport adapters must supply changing connection generations.

---

## ADR-008: Keep generated build state outside version control

- **Status:** Accepted
- **Date:** 2026-07-20

### Context

ESP-IDF and host builds generate large machine-specific trees and derived
configuration. Committing them would obscure maintained source changes.

### Decision

Ignore ESP-IDF `build`, generated `sdkconfig`, dependency locks, and managed
components. Store normative inputs in `sdkconfig.defaults` and `partitions.csv`.
Write host test output to the temporary path in `CMakePresets.json`.

### Consequences

- Commits contain maintained source and configuration inputs only.
- Clean checkouts regenerate derived state.
- Dependencies requiring a committed lock file need a later explicit decision.
