# Makera Z1 communications mainboard firmware

This repository contains the C++ implementation of the ESP32-S3 communications
mainboard firmware. The normative specification is maintained separately in the
sibling `../spec` repository. This repository does not copy or modify that
specification.

The implementation is currently in active development. Passing tests and a
successful target build demonstrate only the areas listed under **Implemented
scope**; they do not yet represent full product conformance.

## Implemented scope

The first core-to-periphery development slice contains:

- common CRC-16/CCITT, BLUFI CRC-16, and CRC-32/ISO-HDLC algorithms;
- common binary frame encoding;
- incremental UART/TCP and USB stream recovery policies;
- in-band command escaping and NUL termination;
- normalized filesystem paths with both separator forms and bounded parent
  traversal;
- host-transfer start parsing, escaped and trimmed path validation, cache
  sidecar mapping, and bounded hexadecimal MD5 extraction;
- four-entry host-transfer start admission with retained connection identity
  and a one-frame latest-value owner mailbox;
- host download with MD5 selection, compressed-sidecar preference, 8192-byte
  blocks, reread retry, terminal reporting, wrong-command limits, and timeout;
- host upload with dual-file lifecycle, sequenced 8192-byte writes, firmware
  partial finalization, retry cycles, cancellation cleanup, and timeout;
- common filesystem path cleanup with escaped text, trimming, suffix removal,
  normalization, and bounded directory-list option parsing;
- directory listing with enumeration-order filtering, escaped names, optional
  size and UTC metadata, bounded response chunks, and terminal completion;
- directory creation, recursive removal, move, cache-side effects, and the
  fixed file-type reply behind a replaceable filesystem command port;
- MD5 command path validation, exact error mapping, 4096-byte hash reads, and
  lowercase success formatting behind a separate hash port;
- SD-card startup mounting, sampled debounce, transition-specific retry,
  logging order, and whole-MiB capacity policy behind an SD port;
- reusable SD configuration-line parsing and literal-space-before-escape token
  parsing for configuration and future WLAN commands;
- active/default configuration restore and save with truncating bytewise copy,
  partial-file retention, close-error handling, and exact responses;
- lazy live configuration loading with bounded chunks, 100-entry capacity,
  retained duplicates, first-match updates, and specified truncation behavior;
- cached, fresh-SD, and retained-live configuration lookup with source-specific
  packet types, cache lifetime, and exact result text;
- live configuration updates and SD temporary-file rewrites with delimiter and
  suffix preservation, ordered unlink/rename, cleanup, and exact responses;
- bounded diagnostic capture with unconditional prior-output forwarding,
  nonblocking 48-record admission, ANSI removal, and UTC/uptime prefixes;
- buffered diagnostic log sessions with synchronization markers, strict
  periodic flush, size rotation, short-write tracking, and bounded shutdown;
- connectivity machine-name derivation with MAC fallback and wrapping
  congestion-based access-point channel selection;
- required station-scan then AP/station startup with explicit radio, DHCP,
  hostname, socket-capacity, and failure settings;
- bounded Wi-Fi scan filtering with raw-SSID duplicate semantics, strongest
  observation retention, stable RSSI ordering, and exact result formatting;
- bounded WLAN command parsing and user-scan orchestration with exact scan
  settings, progress, failure, truncation, and completion responses;
- bounded manual station connection state with two-phase polling, retained
  event data, nonfatal credential persistence, and exact host responses;
- saved station credential loading with exact selection/security settings,
  delayed startup, bounded retries, event updates, and RSSI fallback;
- bounded UDP discovery payloads with status extraction, subnet broadcasts,
  socket recreation, periodic station-first sends, and three-copy bursts;
- optional BLE-only BLUFI lifecycle and secured provisioning commands with
  retained credentials, exact status reports, scans, and diagnostic handling;
- per-connection BLUFI security negotiation with bounded DH material, exact
  salt/MD5 key derivation, retained readiness, and sequence-seeded AES-CFB;
- BLUFI frame envelopes with per-connection sequence state, security-mode
  selection, plaintext checksums, data-only encryption, and acknowledgements;
- MTU-bounded BLUFI data fragmentation with remaining-length prefixes and
  retained, exact-length incoming message reassembly;
- BLUFI control/data subtype dispatch with exact protocol-version, Wi-Fi
  status, and ordered Wi-Fi-list payload encoding;
- BLUFI GATT read/write policy with a shared bounded prepared-write aggregate,
  response-before-decode ordering, reset, and connection-bound notification
  retry;
- a composed per-connection BLUFI pipeline joining envelope validation,
  acknowledgements, fragment reassembly, negotiation, product dispatch, and
  framed error delivery through narrow outer ports;
- ordered, case-sensitive local-command prefix recognition and size limits;
- controller-to-host and host-to-controller routing precedence;
- controller-forwarding suppression, size, and capacity admission;
- independent file-transfer and streamed-play ownership identities;
- independent drift-free 300 ms status and 500 ms diagnostic query schedules;
- bounded controller status, diagnostic, and version snapshot retention;
- pending-status selection, fallback status, RSSI insertion, running-state
  detection, and local snapshot response generation;
- controller UART activity alarms, bounded FIFO admission, write spacing, and
  failure-diagnostic policy;
- an ESP-IDF controller-UART adapter with shared, tested hardware and receive
  settings, intentionally not started before its routing destinations exist;
- common controller-transfer operation decoding, big-endian geometry and index
  parsing, response construction, and independently paced family inboxes;
- controller firmware-transfer state, retained geometry, block addressing,
  progress events, traffic suppression, error handling, and conditional timeout;
- controller configuration-transfer state with 255-byte input filtering,
  record truncation, fixed 512-byte geometry, and bounded response aggregation;
- controller factory-data transfer with family-specific record eligibility,
  negotiated limits, one-based selection, and completion-only file removal;
- streamed-play preparation with escaped normalized paths, CRC identity,
  bounded file size, rate-limited console errors, and local status replies;
- streamed-play controller start validation, big-endian acceptance response,
  observer notification, retry-preserving rejection, and terminal cleanup;
- streamed-play logical-line reading with 129-byte source chunks, embedded-NUL
  behavior, long-line collapse, 64-byte wire limiting, and distinct EOF/failure;
- streamed-play data requests with bounded line aggregation, position-aware
  rewind, retained retransmission, exact request prefixes, and EOF cleanup;
- streamed-play goto scanning with line and logical-byte progress, strict
  greater-than-100-ms pacing, target/EOF reports, and nonterminal failures;
- aggregate firmware-header, size, flag, and checksum validation;
- aggregate update loading with exact cleanup paths, classified read failures,
  destructive format-error ordering, controller reset fallback, and delegated
  ESP32-S3 image verification;
- bounded update-file deletion with busy and permission recovery, exact retry
  delays, and final manual-removal broadcasts;
- update phase recovery with a four-slot nonblocking persistence queue, shared
  error throttling, controller progress, and transient success reporting;
- validated aggregate application with controller-only handoff, inactive
  mainboard OTA sequencing, active-write abort policy, and exact terminal
  phase/delete/restart ordering;
- controller-update handoff monitoring with immediate drift-free reset checks,
  transfer-channel suppression, staged-content failure gates, and completion;
- coalesced aggregate-update boot and case-sensitive local-command triggers;
- immutable persistent serial-number get/set commands with exact syntax,
  capacity admission, namespace/key, character policy, and responses;
- wall-clock query and positive-decimal setting with protocol silence,
  first-boot scheduling, UTC rendering fallback, and exact diagnostics;
- persistent first-boot, whole-second power-on, and streamed-play machine-time
  accounting with per-save fractional discard and silent write failures;
- persisted `sys-time` reporting and first-boot-only `clearftm` handling with
  exact capacity, fallback, UTC, and response behavior;
- aggregate machine-status composition from replaceable transfer, recording,
  SD-capacity, update-phase, and station-RSSI sources;
- portable CANopen node identity, addressed and broadcast NMT transitions,
  communication reset, producer-heartbeat timing, and error-state policy;
- complete CANopen communication, SDO, PDO, identity, and digital-I/O object
  dictionary with little-endian access and mapping validation;
- expedited CANopen SDO uploads and downloads with exact scalar sizing,
  standard abort responses, disabled block transfer, and write effects;
- concurrent `M942` forwarding and acknowledgement with single-exercise
  admission, rotated digital-I/O patterns, bounded SDO retries, and deadline;
- fixed normal-mode 1 Mbit/s TWAI timing, pins, accept-all filtering, queue
  capacities, and an ESP-IDF classic-CAN frame adapter;
- composed CANopen NMT, object-dictionary, and SDO service effects with one
  10 ms cycle surface, consistent error state, diagnostics, and delayed reset;
- 50 ms CAN digital-output observation with exact startup records, first-value
  reporting, change collapsing, lowercase hexadecimal, and DO2 extraction;
- optional target CAN startup with an immediate boot-up cycle, drift-free
  10 ms receive/service processing, 50 ms output sampling, and nonfatal setup;
- one-shot camera setting loading with strict signed decimals, field-specific
  normalization, all 15 frame-size mappings, and UXGA dimension fallback;
- exact camera DVP/SCCB/XCLK pins, JPEG/PSRAM/buffer policy, UXGA startup,
  sensor orientation, stream-size application, and capture timeout contract;
- exact main/video HTTP listener limits and web-volume mount/SPIFFS format
  settings, including 512-byte parser storage and 63-byte names;
- static-file path construction, MIME selection, missing-file text, chunk size,
  and the fixed firmware identity JSON payload;
- HTTP response status/content policy, method matching, URI query exclusion,
  and exact parser-error responses;
- bounded JSON-prefix parsing with embedded-NUL termination and first,
  case-insensitive member lookup;
- camera-resolution endpoint validation, 63-byte request limits, CAM-001
  normalization, and exact sensor success/failure responses;
- multipart Content-Type boundary extraction with the 512-byte limit and
  matching outer-quote removal;
- streaming first-part multipart extraction with split-safe headers and
  boundaries, header overflow handling, and end-of-input behavior;
- case-sensitive main/video HTTP route selection with query exclusion and
  GET-only static fallback;
- static-file serving through replaceable file/response ports, exact 404
  behavior, MIME selection, 256-byte chunks, and terminating empty chunk;
- nonfatal web-volume startup with format-and-retry policy and an ESP-IDF
  SPIFFS adapter;
- ESP-IDF main/video HTTP listener startup using the exact independent limits,
  ports, waits, wildcard setting, and nonfatal failure behavior;
- main-server firmware-info and wildcard static-file handlers connected to the
  portable policy and SPIFFS-backed VFS;
- direct application OTA transaction ordering, exact failure responses,
  structural-finalization delegation, and delayed restart handoff;
- direct raw web-volume update ordering, capacity rejection, empty-image
  success, exact failure text, and delayed restart handoff;
- live-video command matching, single-socket ownership, cross-socket
  preemption, and disconnect cleanup policy;
- compact, ordered and JSON-escaped live/preview preemption responses;
- preview path allow-list and literal traversal rejection;
- RIFF/AVI preview acceptance, retained metadata/index parsing, and bounded
  indexed JPEG frame reads;
- preview JSON namespace, command, selector normalization, and bounded text
  request policy;
- preview open/meta response formatting from accepted AVI metadata;
- preview open admission, session-ID formatting, and bounded frame-buffer policy;
- preview play, pause, resume, seek, and stop state transitions;
- exact preview command and conflict response formatting;
- preview frame scheduling, failure termination, and EOS policy;
- preview WebSocket text-only and empty-message admission;
- ESP-IDF WebSocket route registration for `/ws_video` and `/ws_preview`;
- ESP-IDF WebSocket frame receive adapters connected to the text-input policy;
- in-memory MJPEG AVI writer with finalized header, padded frames, and `idx1` table;
- recording segment UTC naming and interval/close policy;
- M951/M952 recording command state and `0xa2` `ok` responses;
- storage usage monitoring and recording-file retention candidate policy;
- storage retention service with delete sequencing and usage refresh;
- ESP-IDF POSIX directory enumeration, regular-file checks, timestamps, and deletion adapter;
- FAT usage reporting through `esp_vfs_fat_info` for retention decisions;
- SDMMC slot-1 mount lifecycle with GPIO2 detect and specified four-bit pins;
- SD diagnostic log adapter with buffered append, rotation, markers, and shutdown drain;
- ESP-IDF `vprintf` diagnostic hook preserving console output and bounded capture;
- SD-monitor draining of captured diagnostic records into the log writer;
- reusable ESP-IDF NVS scalar/string adapter with explicit missing-key handling;
- NVS runtime-counter backend for `first_boot`, `pon_s`, and `mach_s`;
- runtime-counter startup task with periodic whole-second persistence;
- first-boot notification bridge from wall-clock handling to runtime persistence;
- runtime play-state observer bridge for machine-seconds accounting;
- ESP-IDF wall-clock adapter with UTC formatting and first-boot notification;
- target wall-clock command dispatcher for `time` general-command frames;
- controller UART receive loop with framed command dispatch;
- wall-clock response encoding and transmission over controller UART;
- NVS-backed serial-number service with framed UART responses;
- M951/M952 recording control wired through recognition, state policy, and UART response;
- TCP control listener on port 2222 with bounded client slots and keepalive options;
- per-client TCP receive loops with independent stream decoders and overflow rejection;
- bounded TCP transmit FIFO retaining complete frames until transport completion;
- TCP whole-frame sender policy with short-write continuation and temporary-failure retry;
- TCP overflow rejection wired to the whole-frame sender with a one-second call timeout;
- per-client TCP session composition for incremental frames, stable identity, and queued responses;
- target TCP tasks now retain per-connection identities and make explicit routing decisions;
- callback-based TCP dispatch ports for controller, local, file, and play services;
- mutex-protected TCP-to-controller forwarding through the existing UART output scheduler;
- target TCP receive processing routed through the shared callback dispatcher;
- target TCP clients drain queued frames with whole-frame retry semantics;
- transport-neutral local-command family classification for future TCP response ports;
- origin-aware TCP dispatch context for per-client response queueing;
- TCP-local M951/M952 recording control with per-client `0xa2 ok\n` responses;
- shared atomic recording-request state available to media tasks;
- recording eligibility policy requiring request plus play/controller running state;
- the fixed ESP32-S3 flash partition table;
- ESP-IDF target defaults for flash, PSRAM, CPU, watchdogs, FAT, and sockets;
- persistent-store initialization with erase-and-retry recovery; and
- the nonfatal GPIO0 heartbeat service.

The current host suite has 578 tests. The firmware also builds successfully as
an ESP32-S3 application using ESP-IDF 5.4.1. Detailed requirement state is kept
in [`docs/requirements.md`](docs/requirements.md). Material design choices are
recorded in the [`Architecture Decision Log`](docs/architecture-decisions.md).

## Architecture

The design follows a ports-and-adapters structure. Deterministic product rules
remain independent from ESP-IDF, while hardware and operating-system behavior
is implemented at the outer edge.

```text
Host/Controller protocols
          |
          v
Application services and ownership state
          |
          v
Portable domain and wire-protocol core
          |
          v
Narrow storage, transport, clock, media, and persistence ports
          |
          v
ESP-IDF adapters: FreeRTOS, UART, USB, TCP/IP, SDMMC, NVS, camera, TWAI
```

This separation has three purposes:

1. Product behavior can be tested on the development machine without hardware.
2. A peripheral implementation can be replaced without rewriting protocol
   rules or unrelated services.
3. ESP-IDF types and lifetime rules do not leak into the portable core.

Constants follow the same dependency direction. Shared packet identifiers and
wire limits are defined by the core protocol contract, service-specific timing
and capacities stay beside their application service, and hardware values stay
in target configuration. This keeps numeric meaning explicit without creating
cross-partition dependencies.

### Portable core

`components/core` contains deterministic C++ code shared by host tests and the
target build. It owns byte transformations and validation, but it does not call
GPIO, filesystem, networking, clock, allocation-policy, or RTOS APIs.

The core uses explicit byte containers and lightweight non-owning views. It is
built without RTTI or C++ exceptions on the target. Invalid input is represented
through empty or typed result values rather than exceptions.

### Application and service layer

`components/application` contains transport-neutral policies above the wire
core. Its first module selects routing destinations and maintains independent
logical file-transfer and physical play-connection ownership. Future services
in this layer will own queues, timeouts, and protocol state. They depend on
narrow abstract ports for operations such as file access, packet delivery,
persistence, and monotonic time.

### ESP-IDF composition root

`main/main.cpp` is the target-only composition root. It creates adapters and
starts services in the order required by the startup contract. It currently
contains NVS recovery, heartbeat startup, and the optional CANopen service;
later services remain deliberately absent until their contract tests exist.

### Configuration artifacts

- `partitions.csv` defines the exact NVS, PHY, OTA-data, dual-application, and
  SPIFFS layout.
- `sdkconfig.defaults` records target defaults that are part of the product
  contract.
- Generated `sdkconfig`, `build`, managed components, and dependency lock files
  are intentionally ignored.

## Repository layout

```text
implementation/
├── components/core/       Portable C++ protocol and domain logic
│   ├── include/           Public core interfaces
│   └── src/               Core implementations
├── components/application/ Routing, ownership, and application services
├── docs/                  Requirement traceability and architecture decisions
├── main/                  ESP-IDF composition root and target adapters
├── tests/                 Host-side specification tests
├── CMakeLists.txt         Host/ESP-IDF build entry point
├── CMakePresets.json      Reproducible host-test preset
├── partitions.csv         Normative 8 MiB flash layout
└── sdkconfig.defaults     Normative target defaults
```

## Test-driven development workflow

Development proceeds from the core outward:

1. Select a coherent group of normative requirement IDs.
2. Add tests that initially fail because the behavior is absent.
3. Implement only enough production code to satisfy that behavior.
4. Refactor while keeping the test suite green.
5. Build the ESP32-S3 target to detect SDK and target portability issues.
6. Update `docs/requirements.md` without claiming unverified conformance.

Tests use requirement IDs in their names, for example
`frm_013_tcp_recovers_sync_inside_rejected_candidate`. The small in-repository
test harness avoids adding a third-party testing framework to the firmware or
host dependency graph.

## Prerequisites

- macOS development tools with a C++17 compiler
- CMake and Ninja
- ESP-IDF 5.4.1 installed for ESP32-S3
- an activated ESP-IDF shell (`IDF_PATH` and tool paths exported)

Verify the target environment with:

```sh
idf.py --version
cmake --version
ninja --version
xtensa-esp32s3-elf-g++ --version
```

## Running host tests

From this repository:

```sh
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests
```

The macOS preset includes the installed Command Line Tools libc++ include path
explicitly because that installation does not expose the path automatically to
CMake. Host build output is written under `/private/tmp`, not into the source
repository.

## Building the ESP32-S3 firmware

Activate ESP-IDF, then run:

```sh
idf.py set-target esp32s3
idf.py build
```

The application image is produced at:

```text
build/mainboard_firmware.bin
```

To inspect size allocation:

```sh
idf.py size
idf.py size-components
```

Do not flash production hardware until the configured flash/PSRAM variant and
the board's strapping-pin levels have been verified.

## Code conventions

- Code, file comments, and API documentation are written in English.
- Files and functions have one clear purpose.
- Names describe behavior rather than implementation accidents.
- Hardware constants remain near their owning adapter or target configuration.
- Portable code avoids dynamic polymorphism unless substitution genuinely
  requires it.
- Readability takes precedence over small source-level or binary-size gains.
- Target binary size is controlled through module boundaries, linker garbage
  collection, bounded data structures, and avoiding unnecessary dependencies.

## Current limitations and next steps

The following major areas remain:

- composition-root wiring and ESP-IDF adapters for the implemented portable
  transport, storage, connectivity, controller, BLUFI, and update services;
- a raw ESP-IDF GATT adapter for the byte-exact BLUFI stack, kept separate from
  ESP-IDF's independent standard-BLUFI protocol owner;
- HTTP, WebSocket, camera, AVI preview, recording, and retention;
- runtime service adapters and composition-root wiring;
- CANopen PDO behavior and `M942` target command/SDO-client wiring; and
- physical-device verification of hardware and coexistence behavior.

These areas will be implemented in dependency order. Hardware validation will
then cover the actual ESP32-S3 module, octal PSRAM, camera sensor, SD card,
controller UART, native USB, Wi-Fi/BLE coexistence, and TWAI transceiver.

## Conformance policy

Code presence is not treated as proof of conformance. A requirement is complete
only after its deterministic behavior has a passing test and its hardware or
timing behavior has an appropriate target or physical-device verification.
Known coverage and gaps are recorded in `docs/requirements.md`.
