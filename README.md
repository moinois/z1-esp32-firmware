![Release](https://github.com/moinois/z1-esp32-firmware/actions/workflows/release-firmware.yml/badge.svg)
[![Coverage](https://raw.githubusercontent.com/moinois/z1-esp32-firmware/coverage/badge.svg)](https://github.com/moinois/z1-esp32-firmware/tree/coverage)

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
- target POSIX/mbedTLS file-transfer adapters for cache directories, MD5
  calculation, file I/O, and queued TCP response frames;
- per-TCP-session transfer dispatch with monotonic runtime polling and
  disconnect cleanup;
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
  parsing shared by configuration and WLAN commands;
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
- persistent SoftAP channel, password, and enable startup settings plus shared
  USB/TCP `ap`, `M482`, and `M483` control and query handling;
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
  observer notification, retry-preserving rejection, terminal cleanup, queued
  controller output, and USB-then-TCP host error broadcasts;
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
- queued persistent first-boot, whole-second power-on, and streamed-play machine-time
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
- one-shot camera setting loading from literal `*mainboard.` records with
  31-byte signed-32 parsing, overflow saturation, field-specific normalization,
  DIAG-022 conversion records, all 15 mappings, and UXGA fallback;
- exact camera DVP/SCCB/XCLK pins, JPEG/PSRAM/buffer policy, UXGA startup,
  sensor orientation, stream-size application, and capture timeout contract;
- exact main/video HTTP listener limits and web-volume mount/SPIFFS format
  settings, including 512-byte parser storage and 63-byte names;
- fixed-buffer static-path truncation, ordered MIME selection, missing-file
  text, double-empty EOF chunking, and the fixed firmware identity JSON;
- HTTP response status/content policy, method matching, URI query exclusion,
  and exact parser-error responses;
- bounded JSON-prefix parsing with embedded-NUL termination and first,
  case-insensitive member lookup;
- camera-resolution endpoint validation, 63-byte request limits, exact 500
  input-error responses,
  [CAM-001](https://github.com/f355/esp32_cnc_spec/blob/main/08-media-and-web.md#cam-001)
  normalization, and exact sensor success/failure responses;
- ESP-IDF `esp32-camera` integration for the configured DVP/SCCB camera, sensor
  orientation, startup allocation, and `/api/camera/resolution` routing;
- multipart Content-Type boundary extraction with the 512-byte storage limit,
  exact suffix preservation, and support for an empty boundary;
- specification-compatible block-local multipart extraction without joining
  header or boundary fragments across receive blocks;
- case-sensitive main/video HTTP route selection with query exclusion and
  GET-only static fallback;
- WebSocket live-control ownership dispatch with camera JPEG frame capture and
  binary frame transmission on `start_stream`;
- Preview WebSocket `open` admission with `/sd/videos` path policy, AVI parsing,
  metadata formatting, and text-frame response transmission;
- Persistent preview session state for play, pause, resume, seek, and stop
  commands with session ownership and conflict responses;
- Indexed AVI JPEG frames are now read through the tested bounded reader and
  sent as binary preview WebSocket frames for play/resume/seek commands;
- Preview playback now continues in a FreeRTOS task with frame-period timing,
  asynchronous WebSocket sends, EOS termination, and generation-based cancel;
- Playback checks the ESP-IDF WebSocket connection state on each frame and
  releases the preview session when receive/send fails or the socket closes;
- Camera adapter exposes bounded JPEG capture and current-dimension queries to
  the recording task without coupling storage to the sensor driver;
- Recording file adapter writes finalized AVI buffers with short-write handling,
  flush, `fsync`, and durable close through the mounted FAT/POSIX VFS;
- Recording task composes request/play state, configured recording dimensions,
  JPEG capture, tested AVI segment policy, and one-second FreeRTOS scheduling;
- static-file serving through replaceable file/response ports, exact 404
  behavior, MIME selection, 256-byte chunks, an EOF empty chunk, and the
  terminating empty chunk;
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
- firmware-wide host-output scheduling with independent 32-frame download-data
  and non-download capacities, retained destinations, bounded listing wait,
  no-host cleanup, and source-specific overflow purge behavior;
- USB-before-TCP broadcast expansion and deterministic TCP slot delivery from
  each selected one-download-plus-other output group;
- TCP whole-frame sender policy with short-write continuation, temporary-failure
  retry, and permanent-failure connection shutdown;
- per-client TCP receive composition with incremental frames, stable identity,
  and delegated global response admission;
- target TCP tasks now retain per-connection identities and make explicit routing decisions;
- callback-based TCP dispatch ports for controller, local, file, and play services;
- mutex-protected TCP-to-controller forwarding through the existing UART output scheduler;
- target TCP receive processing routed through the shared callback dispatcher;
- target TCP sockets deliver globally selected frames with whole-frame retry semantics;
- transport-neutral local-command family classification shared by TCP, USB,
  and controller response adapters;
- origin-aware TCP dispatch context for per-client response queueing;
- TCP-local M951/M952 recording control with per-client `0xa2 ok\n` responses;
- TCP-local serial-number get/set using NVS persistence and origin-aware replies;
- TCP-local `sys-time` and `clearftm` using persisted runtime counters and UTC formatting;
- TCP-local filesystem mutation and `ftype` commands using POSIX storage operations;
- TCP-local WLAN scan command using ESP-IDF blocking scan parameters;
- ESP-IDF WLAN station adapter for bounded manual connect/disconnect polling;
- WLAN station credentials persisted through the shared NVS adapter after IPv4 assignment;
- portable `wlan` request parser with exact `-d`/`-e` option precedence;
- TCP WLAN connect/disconnect response flow over the tested station policy;
- UDP discovery adapter for long-lived and command-triggered broadcast bursts;
- TCP station success updates discovery with the assigned station subnet;
- discovery subnet selection uses the actual ESP-IDF STA netmask;
- connect-triggered discovery bursts occur only after subnet state is ready;
- successful WLAN disconnect clears station-specific discovery state;
- ESP-IDF Wi-Fi/IP event adapter synchronizes discovery state on station disconnect and IPv4 assignment;
- read-only `GET /api/wifi/diagnostics` reports live RSSI, channel, authentication, IPv4 state, boot-lifetime lifecycle counters, the last disconnect reason, and the bounded persistent event log without exposing credentials or BSSID;
- ESP-IDF connectivity-startup adapter performs station-only congestion scanning and AP/STA startup;
- ESP-IDF automatic-connection adapter loads saved credentials, schedules initial association, and retries station disconnects;
- ESP-IDF BLUFI lifecycle adapter initializes BLE/Bluedroid, the standard BLUFI profile, and advertising;
- mbedTLS BLUFI crypto adapter provides bounded DH, MD5, and AES-CFB128 operations;
- ESP-IDF BLUFI provisioning adapter provides Wi-Fi configuration, scanning, status/list reports, and advertising controls;
- ESP-IDF BLUFI callback adapter forwards supported BLE events into the tested provisioning policy;
- BLUFI callback table now uses the portable security session for negotiation, AES-CFB128, and CRC callbacks;
- Wi-Fi/IP event adapter forwards station association, address-ready, and disconnect state to BLUFI provisioning;
- Controller UART task now emits drift-free periodic status and diagnostic queries through the tested scheduler;
- ESP32 image validator checks application headers, bounded segments, and image checksum before update acceptance;
- ESP-IDF OTA adapter selects inactive partitions, writes/finalizes images, selects boot, aborts failures, and stages controller images;
- OTA controller-only completion now queues the exact `0xa2 reset\n` frame through the serialized UART forwarder;
- Firmware update task composes SD aggregate loading, validation, OTA application, controller staging, and restart effects after SD startup;
- Local `upgrade` and `reset` commands now coalesce into a thread-safe update-processing request;
- OTA phase publication persists the specified byte in NVS namespace `ota_state`, key `phase`;
- Update boot processing reads the persisted OTA phase and clears completed phase 4 before retrying aggregate work;
- HTTP target responses preserve the portable 400, 404, 405, 413, and 500 status mapping;
- HTTP POST `/update` initializes OTA before multipart inspection, processes independent receive blocks up to 1024 bytes, treats nonpositive receive results as end-of-input, and defers block-write failures to finalization;
- HTTP POST `/updateffs` selects and erases SPIFFS before multipart inspection, then offers extracted blocks at successive wrapping 32-bit offsets without unmounting or checking partition bounds;
- shared atomic recording-request state available to media tasks;
- recording eligibility policy requiring request plus play/controller running state;
- the fixed ESP32-S3 flash partition table;
- ESP-IDF target defaults for flash, PSRAM, CPU, watchdogs, FAT, and sockets;
- persistent-store initialization with erase-and-retry recovery; and
- the nonfatal GPIO0 heartbeat service.

The current host suite has 844 C++ tests, and the Python tooling suite has 49
tests. The latest portable coverage report is 96.22% lines, 98.51% functions,
and 86.72% branches; regenerate it with `python3 tools/host_coverage.py`.
The firmware also builds successfully as an ESP32-S3 application using ESP-IDF
5.4.1. Detailed requirement state is kept
in [`docs/requirements.md`](docs/requirements.md). Material design choices are
recorded in the [`Architecture Decision Log`](docs/architecture-decisions.md).
The source layout and API-comment organization are mapped in the
[`Code documentation tree`](docs/code-documentation.md).
Generate the local Doxygen site with `doxygen docs/Doxyfile`.
Optional physical verification is provided by the
[`hardware-in-the-loop suite`](docs/hardware-testing.md).
The observed differences between the current specification and Makera factory
firmware `0.1.13` are recorded separately in the
[`factory-firmware comparison`](docs/factory-firmware-spec-differences.md), so
factory behavior is not mistaken for evidence about this implementation.
GitHub pull requests and pushes to `main` run the host suite plus live and
all-mock ESP-IDF builds; the published-release workflow builds and attaches
release binaries and publishes the coverage badge. See
[`docs/ci-and-release.md`](docs/ci-and-release.md).

## Architecture

The design follows a ports-and-adapters structure. Deterministic product rules
remain independent from ESP-IDF, while hardware and operating-system behavior
is implemented at the outer edge.

Public headers mirror their implementation domains. The two component include
roots are exported separately, so application code includes
`application/<domain>/<header>.hpp` and portable core code includes
`core/<domain>/<header>.hpp`; the former receives the latter through the public
`core` component dependency. There is deliberately no additional umbrella
`firmware/` include namespace.

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

### ESP-IDF target layer and composition root

`main/main.cpp` is the target-only composition root. It creates adapters and
starts services in the order required by the startup contract. Domain-grouped
adapters under `main/` provide USB, TCP, UART, SD/FAT, NVS, Wi-Fi/BLE, CAN,
camera, HTTP, OTA, diagnostics, recording, and retention integration.

Shared target infrastructure remains below those adapters: `FrameSink`
separates response delivery from NVS-backed commands, `PosixFile` owns mounted
VFS handles and common I/O/MD5 behavior, and `EspWifiScanner` owns
transport-independent ESP-IDF scan conversion. These stay in the target layer
because they intentionally depend on ESP-IDF, POSIX VFS, or mbedTLS.

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
│   └── src/               Implementations grouped by domain
├── components/application/ Routing, ownership, and application services
│   ├── include/           Stable public application interfaces
│   └── src/               Domain-grouped service implementations
├── docs/                  Requirement traceability and architecture decisions
├── main/                  Composition root plus domain-grouped ESP-IDF adapters
├── tests/                 Host tests and optional hardware-in-the-loop tests
├── pytest.ini             HIL discovery, safety, and marker configuration
├── requirements-hil.txt   Host-only HIL dependencies
├── CMakeLists.txt         Host/ESP-IDF build entry point
├── CMakePresets.json      Reproducible host-test preset
├── partitions.csv         Normative 16 MiB flash layout for ESP32-S3-N16R8
└── sdkconfig.defaults     Normative target defaults
```

The implementation and test directories use domain names such as `can`,
`configuration`, `connectivity`, `media`, `storage`, `transport`, and `update`.
Public component include paths remain stable so this physical organization does
not change the libraries' API.

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
- ESP-IDF 5.4.x installed for ESP32-S3 (5.4.1 is used by CI)
- an activated ESP-IDF shell (`IDF_PATH` and tool paths exported)

Verify the target environment with:

```sh
idf.py --version
cmake --version
ninja --version
xtensa-esp32s3-elf-g++ --version
```

## Running host tests

When using the ESP-IDF tool installation, load its toolchain environment first
so CMake and Ninja are available in `PATH`:

```sh
export ESP_IDF_DIR="${ESP_IDF_DIR:-$HOME/.espressif/v5.4.4/esp-idf}"
source "$ESP_IDF_DIR/export.sh"
```

From this repository:

```sh
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests
```

For an optimized host regression run, use the separate Release preset:

```sh
cmake --preset host-tests-release
cmake --build --preset host-tests-release
ctest --preset host-tests-release
```

This is a CMake host build only. It does not select the ESP32 release profile,
the firmware partition table, or the `--release` packaging mode; those remain
the responsibility of `tools/build_firmware.py`.

The macOS presets select the active Command Line Tools macOS SDK explicitly so
Apple Clang can find its libc++ headers (`<array>`, `<cstddef>`, and friends).
If that SDK path does not exist, install or repair Xcode Command Line Tools;
do not work around it by adding random system include directories. Host build
output is written under `/private/tmp`, not into the source repository.

## Building the ESP32-S3 firmware

Activate ESP-IDF, then use the repository build helper. It selects the hardware
profile, partition table, Web UI source, and writes an audit manifest:

```sh
export ESP_IDF_DIR="${ESP_IDF_DIR:-$HOME/.espressif/v5.4.4/esp-idf}"
source "$ESP_IDF_DIR/export.sh"
python3 tools/build_firmware.py --live --build-dir build
```

The application image is produced at:

```text
build/mainboard_firmware.bin
```

The helper defaults to a development profile: without `--release`, it selects
`partitions-dev.csv` and may produce a larger image than a release build. A
device coming from the release layout must be flashed once over the full
USB/serial flash connection so the development partition table is installed;
this can erase or invalidate existing data when offsets change. Use
`--release` for the normative partition table and OTA-compatible images.

Useful firmware build variants are:

```sh
# Development live firmware (larger development partitions)
python3 tools/build_firmware.py --live --build-dir build-dev

# Development firmware with every implemented mock adapter
python3 tools/build_firmware.py --mock-all --build-dir build-mock

# Specification/release firmware and aggregate update package
python3 tools/build_firmware.py --live --release \
  --mainboard-version 0x00010000 --build-dir build-release

# Optional additional size reductions (still uses the specification table)
python3 tools/build_firmware.py --live --release --compact \
  --mainboard-version 0x00010000 --build-dir build-compact
```

Flash a selected build explicitly when the partition table must be written:

```sh
python3 tools/build_firmware.py --live --flash \
  --port /dev/cu.usbmodem1234561 --build-dir build-dev
```

The generated application image is named by ESP-IDF in
`<build-dir>/project_description.json`; for the current target it is normally
`<build-dir>/mainboard_firmware.bin`. Release mode additionally creates
`<build-dir>/firmware.bin`.

For a manually built application image, create a specification-compliant
mainboard-only aggregate update package with:

```sh
python3 tools/package_firmware.py --mainboard build-dev/mainboard_firmware.bin
```

The result is `build-dev/firmware.bin`; release builds create this package
automatically. See [`tools/README.md`](tools/README.md) for
version metadata and alternate paths.

## Running hardware-in-the-loop tests

Hardware tests automatically skip unavailable fixtures and never count a skip
as conformance. Install their host-only dependencies and run:

```sh
python3 -m pip install -r requirements-hil.txt
python3 -m pytest tests/hardware
```

The HIL configuration applies a ten-minute per-test timeout so a disconnected
fixture cannot leave a run indefinitely blocked. Unavailable fixtures are
reported as `SKIP`, not as conformance passes.

Read-only tests run when their device is detected. Recoverable mutation and
destructive update tests require separate opt-in environment flags. See
[`docs/hardware-testing.md`](docs/hardware-testing.md) for detection rules,
safety levels, fixture variables, and requirement-level JSON reports.

## Flashing an ESP32-S3-N16R8 development board

The target defaults and partition table are configured for the 16 MB flash and
8 MB octal PSRAM on an ESP32-S3-N16R8. Activate ESP-IDF and flash the board on
the serial device reported by macOS:

```sh
export ESP_IDF_DIR="${ESP_IDF_DIR:-$HOME/.espressif/v5.4.4/esp-idf}"
source "$ESP_IDF_DIR/export.sh"
idf.py -p /dev/cu.usbmodem1234561 flash monitor
```

If automatic reset does not enter the ROM downloader, hold `BOOT`, press and
release `RESET`/`EN`, release `BOOT`, and repeat the command. The serial device
name may change after reconnecting; inspect `/dev/cu.usb*` when needed. The
firmware pin map is for the specified mainboard hardware, so a generic
development board can validate boot, USB, and logging but not the camera, SD,
CAN, or controller peripherals without the corresponding wiring.

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

### Bluetooth provisioning

BLUFI is a Bluetooth Low Energy GATT provisioning service, not a Bluetooth
Classic or operating-system pairing interface. Do not use the macOS or Windows
pairing settings. A provisioning client scans for the advertised name, connects
directly to service `0xffff`, enables notifications on characteristic `0xff02`,
writes BLUFI frames to `0xff01`, and completes the BLUFI Diffie-Hellman security
negotiation before sending Wi-Fi credentials. Link-level pairing is deliberately
disabled.

The advertised name is `MK_` followed by at most the first 23 bytes of the
configured or MAC-derived machine name. The default is therefore
`MK_Makera_Z1_XXXX`. Advertising uses one byte-exact legacy payload with no
scan response; transmit power, service UUID `0xffff`, and preferred connection
interval are included only at the suffix lengths permitted by BWF-002. Clients
must therefore discover the device by its `MK_` identity rather than require
the service UUID to be present in every advertisement.

- The HTTP server and SPIFFS static-file support are implemented, but the
  browser-based configuration interface itself is not. No HTML, CSS, or
  JavaScript application assets are currently included; its product scope,
  authentication policy, missing APIs, packaging, and browser tests remain
  explicit backlog items under `PROJ-WEBUI-001`.

Most deterministic behavior and target compositions listed above are
implemented, host-tested where portable, and target-built. Remaining work is
primarily physical verification and fixture-specific integration: raw
byte-exact GATT coverage beyond the standard BLUFI callback bridge, controller
and CAN rigs, RF/coexistence measurements, camera/recording endurance, SD/FAT
failure injection, resource-exhaustion stress, and long-running timing
validation. Direct OTA alternation, rollback, previous-partition reuse, and
recovery after an injected receive timeout have physical HIL evidence.

The optional HIL framework provides USB, TCP, HTTP/WebSocket, Wi-Fi diagnostic,
BLE/BLUFI, SD/filesystem, and explicitly gated mutating or destructive checks.
BLE advertising, GATT, encrypted provisioning, cross-transport load, and reset
recovery have physical evidence. Controller, CAN, camera, SD-dependent, and
recording tests remain skipped when their fixtures are unavailable.

## Conformance policy

Code presence is not treated as proof of conformance. A requirement is complete
only after its deterministic behavior has a passing test and its hardware or
timing behavior has an appropriate target or physical-device verification.
Known coverage and gaps are recorded in `docs/requirements.md`.
Anonymized interoperability observations and their resulting decisions are
tracked in `docs/field-feedback.md`.
