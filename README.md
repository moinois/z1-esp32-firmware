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
- ordered, case-sensitive local-command prefix recognition and size limits;
- controller-to-host and host-to-controller routing precedence;
- controller-forwarding suppression, size, and capacity admission;
- independent file-transfer and streamed-play ownership identities;
- independent drift-free 300 ms status and 500 ms diagnostic query schedules;
- controller status validation, running-state detection, and local status
  extension generation;
- aggregate firmware-header, size, flag, and checksum validation;
- the fixed ESP32-S3 flash partition table;
- ESP-IDF target defaults for flash, PSRAM, CPU, watchdogs, FAT, and sockets;
- persistent-store initialization with erase-and-retry recovery; and
- the nonfatal GPIO0 heartbeat service.

The current host suite has 42 tests. The firmware also builds successfully as
an ESP32-S3 application using ESP-IDF 5.4.1. Detailed requirement state is kept
in [`docs/requirements.md`](docs/requirements.md).

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
contains NVS recovery and heartbeat startup; later services remain deliberately
absent until their contract tests exist.

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
├── docs/                  Requirement traceability and design records
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

The following major areas are not yet implemented:

- transport connection-slot allocation and outbound generation checks;
- controller UART scheduling and controller transfer families;
- controller UART transport, activity monitoring, and transfer families;
- host upload and download state machines;
- SD lifecycle, filesystem commands, configuration views, and logging;
- Wi-Fi AP/station behavior, discovery, and host Wi-Fi commands;
- BLE GATT, BLUFI framing, fragmentation, security, and provisioning;
- HTTP, WebSocket, camera, AVI preview, recording, and retention;
- aggregate OTA application and recovery state;
- runtime counters and identity commands; and
- CANopen object dictionary, NMT, SDO/PDO behavior, and `M942`.

These areas will be implemented in dependency order. Hardware validation will
then cover the actual ESP32-S3 module, octal PSRAM, camera sensor, SD card,
controller UART, native USB, Wi-Fi/BLE coexistence, and TWAI transceiver.

## Conformance policy

Code presence is not treated as proof of conformance. A requirement is complete
only after its deterministic behavior has a passing test and its hardware or
timing behavior has an appropriate target or physical-device verification.
Known coverage and gaps are recorded in `docs/requirements.md`.
