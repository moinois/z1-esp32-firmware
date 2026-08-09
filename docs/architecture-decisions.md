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
| [ADR-012](#adr-012) | Sandbox user-controlled filesystem paths beneath the SD volume | Accepted | 2026-07-27 |
| [ADR-013](#adr-013) | Select live and mock hardware through one target factory | Accepted | 2026-07-30 |

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

When multiple modules implement one contract, its values live in a focused
domain header rather than being repeated or added to a global catch-all. This
applies to update-package layout and phases, host file-transfer limits,
controller-transfer geometry, runtime persistence keys, configuration source
names, and camera frame-size bounds. Equal numeric values from independent
requirements remain independent constants.

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

---

<a id="adr-012"></a>
## ADR-012: Sandbox user-controlled filesystem paths beneath the SD volume

- **Status:** Accepted
- **Date:** 2026-07-27

### Context

ESP-IDF exposes a global VFS root containing mounts such as `/sd` and
`/spiffs`. Passing host paths directly to POSIX would allow a command such as
`ls /` to observe that implementation namespace and could direct uploads away
from removable storage.

### Decision

All USB-, TCP-, controller-, and browser-supplied filesystem paths use `/` as
the logical SD root and pass through one portable resolver before target I/O.
The physical result is exactly `/sd` or a descendant. A leading `/sd` remains a
compatibility alias. An exact `gcodes` path component selects the canonical
`/sd/gcodes` primary and cache trees; text merely containing `gcodes` does not.
The physical mount point is defined once by the core SD-path policy; fixed
firmware paths are composed from that definition instead of embedding `/sd`.
User-visible command responses and play or transfer status convert physical
paths back to their logical `/...` form so implementation details do not leak.

The target publishes successful mount and unmount state atomically. Failed SD
operations log that state before classifying POSIX errno, allowing an absent
card to be distinguished from a missing path without changing command response
contracts such as `FILE-015`.

This sandbox is an implementation security policy retained independently of
whether the upstream specification states the confinement as explicitly. The
upstream repository remains the normative external reference and is not
modified by this project; the local policy narrows user-controlled storage I/O
to the SD volume without changing the documented wire responses.

### Consequences

- User input cannot select `/spiffs` or another VFS mount.
- Path normalization and cache mapping are transport-independent and host-tested.
- Existing clients that send `/sd/...` remain compatible.
- Users consistently see `/config.txt`, `/gcodes/...`, and `/` regardless of
  the physical VFS mount point.
- `ls` retains its specified completion-only wire response on open failure;
  the detailed cause is diagnostic evidence instead.

---

<a id="adr-013"></a>
## ADR-013: Select live and mock hardware through one target factory

- **Status:** Accepted
- **Date:** 2026-07-30

### Context

Some requirement paths cannot be exercised without optional peripherals. Mock
logic distributed across startup and adapters would make mixed live/mock builds
hard to review and could allow a production path to select simulation by
accident. Host fakes validate application policy but do not exercise the target
VFS and ESP-IDF integration used by the firmware.

### Decision

Select hardware implementations exactly once in a target adapter factory.
`CONFIG_Z1_MOCK_ALL_HARDWARE` enables every implemented mock, while a separate
Kconfig switch enables each peripheral independently. Both global and specific
switches default off. Application and core code remain unaware of the selected
implementation.

The first implementation is a volatile, configurable PSRAM block device below
FatFS. It mounts at the normal `/sd` path so target tests retain the production
VFS, POSIX, path-sandbox, command, and transport layers. One generic builder
uses the standard `build/` tree and writes every live/mock choice explicitly,
retains a configured PSRAM reserve, and emits an explicit test-build diagnostic.
Mock results never count as physical SD conformance.

The camera implementation extends the same factory with a common lifecycle,
resolution, capture, settings, and OTA-deinitialization surface. Its mock
returns a deterministic marker-only JPEG frame so target WebSocket and recording
composition can run without probing camera pins. This is protocol-fixture data,
not a simulation of sensor pixels or timing.

The controller implementation selects a shared byte-channel interface in the
same factory. Its deterministic mock consumes and emits production-framed
controller messages, allowing the real scheduler, decoder, snapshot store, and
host routing to run unchanged. It models version, status, diagnostics, and all
three controller transfer families. Seven-byte read fragmentation exercises
the production decoder, but electrical and physical UART timing remain outside
its stated evidence.

### Consequences

- Live/mock selection has one reviewable composition point.
- The same public HIL tests can exercise physical or simulated SD storage.
- The simulated volume is deterministic and empty after every reset.
- Electrical, driver, removal, timing, and real-media behavior still requires
  physical HIL.
- A later CAN mock extends the same factory and global
  selection policy without adding compile-time branches to application code.

---

<a id="adr-014"></a>
## ADR-014: Enable a bounded diagnostic mirror through an SD sentinel

- **Status:** Accepted
- **Date:** 2026-07-30

### Context

Native USB HIL occupies the connection used for host protocol traffic, so UART
diagnostics may be unavailable while reproducing a transport or storage fault.
Always creating another log would consume removable-media capacity and alter
the normative `GeneralInfo.log` behavior.

### Decision

UART remains the unconditional diagnostic destination. When storage is mounted
and the user-visible `/serial.log` sentinel already exists, the asynchronous SD
consumer also appends captured records to its internal `/sd/serial.log` path.
The mirror never creates the sentinel and never reports its own file failures
through the captured logger. Every accepted record is flushed immediately.

The mirror stops before a complete record would exceed 384 KiB. It does not
truncate existing evidence. Replacing the saturated file with a new empty
sentinel permits activation on the next diagnostic record. The normative
rotating log remains independent.

### Consequences

- USB-only HIL can retrieve diagnostics through the file-transfer protocol.
- A 512 KiB mock volume retains about 128 KiB for FAT metadata and test files.
- Mirror failures and saturation cannot suppress UART output.
- The PSRAM-backed mock file remains volatile and cannot survive a full reset.

---

<a id="adr-015"></a>
## ADR-015: Serve a LAN configuration UI from a separately updatable volume

- **Status:** Accepted
- **Date:** 2026-07-31

### Context

The product requires browser-based configuration, while the normative web
volume already supports independent SPIFFS replacement through `/updateffs`.
Configuration entries are dynamic and namespaced in `/config.txt`; duplicating
a fixed field list in JavaScript would drift as firmware settings evolve.

### Decision

Build `webui/` into the `spiffs` partition on every ESP-IDF build and retain
`build/spiffs.bin` as a separately installable artifact. The UI retrieves the
current MAINBOARD namespace through `GET /api/config` and updates one setting
per `POST /api/config` JSON request. Keys and values are bounded and must not
contain line endings. Target persistence continues through the existing
`ConfigurationFileStore`, including CFG-031 unlink-before-rename behavior.

The first LAN release adds no separate HTTP authentication. This matches the
existing unauthenticated update, diagnostics, camera, TCP, USB, and BLUFI
management model; access to the machine network is the trust boundary. Adding
authentication later is a protocol and provisioning decision, not a UI-only
change. The page exposes firmware identity and privacy-bounded Wi-Fi health but
never returns stored Wi-Fi passwords.

Existing settings and newly entered settings are intentionally distinguished.
Keys loaded from the device are read-only in the page because the API updates
or creates one key but does not define rename or delete semantics. Their values
remain editable. The save action sends only changed and newly added records,
then reloads the authoritative document so the page cannot silently diverge
from the persisted file.

### Consequences

- New configuration keys appear without rebuilding the UI.
- Operators can see the current value before changing it and can explicitly
  reload values changed through USB, TCP, or another browser.
- Firmware and UI images can be updated independently.
- Browser POSTs cannot inject extra configuration lines.
- Anyone with network access to the device can change configuration; deployments
  requiring a stronger boundary must isolate the machine network until an
  authenticated protocol is specified.

<a id="adr-016"></a>
## ADR-016: Sandbox all user file paths below the SD mount point

- **Status:** Accepted
- **Date:** 2026-08-09

### Context

The specification describes several logical path forms, including absolute
paths and compatibility aliases. Treating those forms as unrestricted host
filesystem paths would allow a user command to address unrelated VFS mounts or
internal files. The firmware has one intended user storage root: `/sd`.

### Decision

Every path supplied by a user through USB, TCP, HTTP, playback, preview, or
controller-originated file commands is normalized and resolved beneath `/sd`.
The `/sd` mount point is defined centrally. Logical response paths may omit the
physical prefix, but this presentation rule never expands the accessible root.
Internal target code may use physical paths directly only through the shared
path helpers.

This is a deliberate security and interoperability clarification. If a literal
reading of an upstream path rule appears to allow access outside `/sd`, the
sandbox takes precedence; the implementation does not expose other VFS names
or permit traversal. The upstream specification remains unchanged.

### Consequences

- All user-controlled file operations share one traversal boundary.
- USB, TCP, HTTP, preview, playback, and controller paths cannot diverge in
  their filesystem authority.
- Internal paths remain explicit and reviewable through the central mount-point
  helpers.
- Requirements evidence must distinguish this local security decision from
  normative upstream conformance.
