![Release](https://github.com/moinois/z1-esp32-firmware/actions/workflows/release-firmware.yml/badge.svg)
[![Coverage](https://raw.githubusercontent.com/moinois/z1-esp32-firmware/coverage/badge.svg)](https://github.com/moinois/z1-esp32-firmware/tree/coverage)

# Makera Z1 communications mainboard firmware

This repository contains the C++ implementation of the ESP32-S3 communications
mainboard firmware. The normative specification is maintained separately in the
sibling `../spec` repository. This repository does not copy or modify that
specification.

The implementation is currently in active development. Passing tests and a
successful target build demonstrate the capabilities in the linked inventory;
they do not by themselves represent full physical product conformance.

## Project status

The production implementation covers the current specification in portable
code and ESP-IDF target composition. Reviewed physical evidence exists for
native USB, Wi-Fi/BLE, SD, controller UART, OV3660 video, SPIFFS replacement,
and OTA recovery. Remaining work is primarily fixture-dependent conformance and
endurance evidence rather than known missing production modules.

- [Implemented capabilities](docs/implemented-scope.md) — detailed feature inventory.
- [Requirement status](docs/requirements.md) — concise completion state and explicit gaps.
- [Physical verification evidence](docs/physical-verification-evidence.md) — detailed target/HIL evidence behind the concise requirement statuses.
- [Physical verification backlog](docs/physical-verification-backlog.md) — pending fixture work grouped by what can be run now and what equipment it needs.
- [Test traceability](docs/test-traceability.md) — specification IDs mapped to executable tests.
- [HIL status](docs/hardware-testing.md) — current fixture coverage and how to run it.

Current verification: 856 portable C++ tests and 58 Python tooling tests pass.
The latest coverage snapshot is 96.20% lines, 98.52% functions, and 86.69%
branches. See the linked documents for the evidence boundary; a successful build
or a mock test does not by itself establish physical conformance.

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

Repository releases are created from the **Create firmware release** workflow
in GitHub Actions. Enter a SemVer version without the leading `v`, select the
source ref, and leave **Create the tag and GitHub Release** unchecked for the
first run. That dry run builds, tests, checksum-verifies, and uploads the same
workflow artifacts without creating a GitHub tag or Release. Select the publish
checkbox only after those artifacts have been inspected. Stable versions from
refs other than `main` are published as prereleases unless the explicit
stable-release override is selected.

Select an approved LPC image with **controller payload**. The `none` choice
leaves LPC untouched; a versioned choice downloads and checksum-verifies the
exact controller release asset listed in `release/controller-payloads.json`
and includes it in the aggregate `firmware.bin` package.

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

- The default SPIFFS image includes the browser-based configuration interface
  and its configuration and diagnostics APIs. Target HIL covers installation,
  asset delivery, configuration reload/update/persistence, and API errors;
  automated cross-browser visual and accessibility verification remains under
  `PROJ-WEBUI-001`.

Most deterministic behavior and target compositions listed above are
implemented, host-tested where portable, and target-built. Physical Z1 evidence
now covers native USB, Wi-Fi/BLE coexistence, SD reads and recoverable writes,
the controller link, OV3660 live video, SPIFFS replacement, and OTA reboot and
timeout recovery. Remaining work is primarily fixture-specific or endurance
evidence: a driven CAN bus, RF-loss/coexistence measurements, physical
camera-to-SD recording endurance, injected FAT/media failures,
resource-exhaustion stress, and long-running timing validation. Direct OTA
alternation, rollback, previous-partition reuse, and recovery after an injected
receive timeout have physical HIL evidence.

The optional HIL framework provides USB, TCP, HTTP/WebSocket, Wi-Fi diagnostic,
BLE/BLUFI, SD/filesystem, and explicitly gated mutating or destructive checks.
BLE advertising, GATT, encrypted provisioning, cross-transport load, and reset
recovery have physical evidence. Controller, camera, and SD fixtures have also
been exercised on a Makera Z1; CAN and physical recording remain gated when
their dedicated fixtures are unavailable.

## Conformance policy

Code presence is not treated as proof of conformance. A requirement is complete
only after its deterministic behavior has a passing test and its hardware or
timing behavior has an appropriate target or physical-device verification.
Known coverage and gaps are recorded in `docs/requirements.md`.
Anonymized interoperability observations and their resulting decisions are
tracked in `docs/field-feedback.md`.
