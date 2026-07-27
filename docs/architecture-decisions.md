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
| [ADR-001](#adr-001) | Keep specification and implementation in separate repositories | Accepted | 2026-07-20 |
| [ADR-002](#adr-002) | Use ports and adapters around a portable C++ core | Accepted | 2026-07-20 |
| [ADR-003](#adr-003) | Develop deterministic behavior test-first on the host | Accepted | 2026-07-20 |
| [ADR-004](#adr-004) | Use explicit bounded state and value results | Accepted | 2026-07-20 |
| [ADR-005](#adr-005) | Build target code without C++ exceptions or RTTI | Accepted | 2026-07-20 |
| [ADR-006](#adr-006) | Pin the target implementation profile to ESP-IDF 5.4.1 | Accepted | 2026-07-20 |
| [ADR-007](#adr-007) | Separate logical file ownership from physical play ownership | Accepted | 2026-07-20 |
| [ADR-008](#adr-008) | Keep generated build state outside version control | Accepted | 2026-07-20 |
| [ADR-009](#adr-009) | Assign constants to the innermost owning module | Accepted | 2026-07-20 |
| [ADR-010](#adr-010) | Share target mechanisms below transport adapters | Accepted | 2026-07-26 |
| [ADR-011](#adr-011) | Keep physical verification optional, explicit, and safety-gated | Accepted | 2026-07-26 |

---

<a id="adr-001"></a>
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

<a id="adr-002"></a>
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

<a id="adr-003"></a>
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

<a id="adr-004"></a>
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

<a id="adr-005"></a>
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

<a id="adr-006"></a>
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

<a id="adr-007"></a>
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

<a id="adr-008"></a>
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

---

<a id="adr-009"></a>
## ADR-009: Assign constants to the innermost owning module

- **Status:** Accepted
- **Date:** 2026-07-20

### Context

Protocol identifiers, binary-format fields, service limits, and hardware
settings were previously expressed at several use sites as numeric literals.
That obscured their meaning and made a coordinated protocol change easy to
apply incompletely. A single global constants module would instead couple
otherwise independent architectural partitions.

### Decision

Give every nontrivial fixed value a descriptive name in the innermost module
that owns its meaning. Shared wire identifiers and limits live in the portable
core protocol contract. Algorithm and file-format values remain private to
their core implementation. Service capacities and timing remain private to the
application service. Hardware and RTOS settings remain in target configuration
or target adapters.

Dependencies continue to point inward; constants must not introduce a reverse
dependency between core, application, and target partitions.

### Consequences

- Call sites state the purpose of protocol and policy values.
- A shared wire change has one authoritative definition.
- Unrelated services may retain equal-valued constants without becoming
  coupled.
- Literal zero, one, byte offsets, and bit shifts may remain where they express
  a local operation more clearly than an additional name would.

---

<a id="adr-010"></a>
## ADR-010: Share target mechanisms below transport adapters

- **Status:** Accepted
- **Date:** 2026-07-26

### Context

TCP, USB, and controller-UART adapters developed repeated NVS result mapping,
runtime command behavior, serial-number behavior, POSIX file ownership, MD5
calculation, Wi-Fi scan conversion, and frame-response construction. These are
target mechanisms rather than product policy, but leaving a copy in each
transport allowed error handling and resource ownership to drift.

Moving them into the portable core would be equally misleading because they
depend on ESP-IDF, mounted POSIX VFS, mbedTLS, NVS, or a concrete transport
delivery contract.

### Decision

Keep reusable target mechanisms under `main/` and compose them into narrow
transport adapters:

- `FrameSink` owns only delivery of an already constructed protocol frame;
- shared NVS command ports own persistence mapping and delegate delivery;
- `PosixFile` and related functions own target file handles, bounded I/O,
  synchronization, path metadata, parent creation, and MD5 calculation;
- `EspWifiScanner` owns ESP-IDF scan execution and record conversion; and
- common controller-transfer wire encoding remains in the application transfer
  module while family-specific state machines remain separate.

Prefer composition and small interfaces over a transport inheritance hierarchy.
Do not consolidate equal-looking behavior whose hardware mode, lifetime, or
error contract differs.

### Consequences

- Persistence, file, hash, and scan behavior has one target implementation.
- TCP, USB, and UART adapters retain only origin-specific delivery and policy.
- Shared target utilities are target-built but are not linked into host tests.
- Changes to target utilities require both a target build and all portable host
  tests because they cross several integration paths.

---

<a id="adr-011"></a>
## ADR-011: Keep physical verification optional, explicit, and safety-gated

- **Status:** Accepted
- **Date:** 2026-07-26

### Context

Many requirements have deterministic host coverage and successful ESP-IDF
builds but still need physical evidence. Development machines do not always
have the board, SD card, RF environment, controller, CAN fixture, camera, or BLE
scanner attached. Making ordinary tests fail solely because a fixture is absent
would discourage routine execution, while silently treating absence as success
would overstate conformance. Some physical checks can also change NVS, files,
network association, partitions, or firmware.

### Decision

Maintain a separate pytest HIL suite with runtime fixture discovery:

1. absent fixtures or dependencies produce `SKIP` with a precise reason;
2. `SKIP` never counts as physical evidence;
3. read-only checks may run automatically after positive device detection;
4. recoverable mutation requires `Z1_ALLOW_MUTATION=1`;
5. destructive operations require `Z1_ALLOW_DESTRUCTIVE=1` and a documented
   recovery procedure;
6. firmware flashing is never implied by device detection; and
7. tests carry requirement IDs and can emit a reviewed JSON evidence report.

The C++ host suite remains dependency-free. Pytest, PyUSB, and pyserial are
host-only HIL dependencies and are never part of the firmware build graph.

### Consequences

- The same command is useful with no hardware, one board, or a complete rig.
- CI can distinguish unavailable equipment from observed product failures.
- Physical conformance claims remain auditable at requirement level.
- Fixture drivers and destructive recovery procedures must be added before the
  corresponding skipped capability gates can become executable evidence.
