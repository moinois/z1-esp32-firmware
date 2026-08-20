# Hardware-in-the-loop testing

This document describes the current HIL fixture coverage and how to run it.
Chronological investigation notes and superseded results are retained in the
[HIL evidence history](hardware-testing-history.md), not mixed into the current
status.

A physical `PASS` is conformance evidence only for the declared fixture and
firmware image. A mock pass verifies target composition and error handling, but
never substitutes for electrical, timing, media, RF, or sensor conformance.

## Current fixture coverage

| Area | Current physical evidence | Remaining physical evidence |
|---|---|---|
| Native USB | Descriptor and endpoint identity, framed requests, large file transfer, repeated reset/re-enumeration, OTA lifecycle, and two cable unplug/replug cycles without restarting ESP, Wi-Fi, or controller | Forced endpoint stall, saturation, and electrical fault injection |
| TCP, HTTP, and Wi-Fi | Four-client capacity, overflow and slot reuse, fragmentation, discovery, scans, diagnostics, static/API traffic, interrupted requests, credential persistence, and concurrent USB load | Long RF-loss, coexistence, and resource-endurance runs |
| BLE and BLUFI | Machine-name advertising, GATT lifecycle, wire errors, DH/AES/CRC security, fragmentation, encrypted provisioning, concurrency, and post-OTA advertising recovery | Long RF coexistence and repeated provisioning endurance |
| SD and configuration | Physical listing, configuration download/MD5, recoverable unique-name mutations, card removal/reinsertion, USB/TCP equality, and mock FAT fault paths | Injected physical FAT/media failures and long write endurance |
| Controller and UART | Physical version, status, diagnostics, repeated coexistence reads; all firmware/configuration/factory transfer families through the controller mock | Electrical/timing faults and end-to-end physical controller firmware update |
| Camera and preview | Physical OV3660 JPEG streaming, reconnect/successor sessions, concurrent transports, and indexed AVI preview from physical SD | Long camera-to-SD recording, retention, and thermal/resource endurance |
| OTA and persistence | Direct OTA across both partitions, rollback, receive-timeout recovery, SPIFFS replacement, identity/counter/Wi-Fi persistence, and mock NVS failures | Aggregate package with physical controller payload and repeated power-loss/endurance scenarios |
| Web interface | Physical SPIFFS installation, assets, MIME types, configuration/diagnostics APIs, persistence, and error paths | Cross-browser visual and accessibility verification |
| CANopen | Portable protocol, dictionary, SDO/PDO, heartbeat, error, M942, and target TWAI composition | Active CAN peer, bus timing, electrical levels, arbitration, and fault injection |
| Diagnostics and heartbeat | Physical healthy-boot capture where a COM fixture exists, SD log mirroring, reset-reason diagnostics, and target heartbeat composition | Z1 has no non-invasive COM fixture; GPIO0 level and one-second accuracy require a probe |

The implementation status and exact remaining gaps are summarized in
[requirements.md](requirements.md). Tests grouped by specification section are
listed in [test-traceability.md](test-traceability.md).

## Fixture profiles

### Physical Makera Z1

The available Z1 exposes native USB, Wi-Fi, BLE, SD, controller UART, and an
OV3660 camera. It has no non-invasive diagnostic COM connection and currently
has no active CAN peer. Use the physical declarations only when those devices
are actually installed:

```sh
Z1_HIL_SD=1 Z1_HIL_CAMERA=1 Z1_HIL_CONTROLLER=1 Z1_HIL_BLE=1 \
python3 -m pytest tests/hardware -m "hardware and not mutating and not destructive"
```

The target IP is discovered from UDP port 3333. Set `Z1_HIL_HOST` only when
automatic discovery is unavailable. Native USB is identified by VID:PID
`303a:4002`.

### Development board with mocks

Mocks can independently replace SD, camera, controller, NVS, and selected
network fault boundaries. Live USB, Wi-Fi, and BLE may remain enabled while the
other adapters are mocked. Declare exactly the flashed profile so reports
cannot confuse simulator evidence with physical evidence:

```sh
Z1_HIL_MOCK_SD=1 Z1_HIL_MOCK_CAMERA=1 Z1_HIL_MOCK_CONTROLLER=1 \
Z1_HIL_BLE=1 python3 -m pytest tests/hardware \
  -m "hardware and not mutating and not destructive"
```

Mock selection is a build-time property. Environment variables only describe
the already flashed image; they do not enable mocks in live firmware.

## Safety and isolation

| Marker | Default | Permission |
|---|---|---|
| `readonly` | Enabled when its fixture is detected | Queries and observation only |
| `mutating` | Disabled | Requires `Z1_ALLOW_MUTATION=1`; tests must restore or remove their unique data |
| `destructive` | Disabled | Requires `Z1_ALLOW_DESTRUCTIVE=1` and a documented recovery image/path |

Device detection grants permission only for read-only cases. The suite never
infers permission to flash, erase, replace firmware, change credentials, or
write physical media.

Reset-sensitive BLE and USB cases must run in isolated processes. Use the
orchestrator for a complete ordered run:

```sh
python3 tools/run_hil_isolated.py
```

It runs read-only work before mutating and destructive groups, isolates BLE
nodes, serializes USB reset/disconnect cases, waits for every process to finish,
and applies per-test timeouts. Do not start concurrent HIL processes against
the same USB interface or target.

## Running focused suites

Install host dependencies:

```sh
python3 -m pip install -r requirements-hil.txt
```

Common selections:

```sh
# All currently available read-only fixtures
python3 -m pytest tests/hardware \
  -m "hardware and not mutating and not destructive"

# Native USB only
python3 -m pytest tests/hardware/test_usb.py -q

# Recoverable mutations
Z1_ALLOW_MUTATION=1 python3 -m pytest tests/hardware -m mutating

# Explicitly destructive OTA checks
Z1_ALLOW_DESTRUCTIVE=1 \
Z1_HIL_OTA_IMAGE=build/mainboard_firmware.bin \
python3 -m pytest tests/hardware/test_ota.py
```

Write a machine-readable report with:

```sh
python3 -m pytest tests/hardware --hil-report build/hil-results.json
```

Report vocabulary:

- `PASS`: the declared fixture executed and matched the assertion;
- `FAIL`: the declared fixture executed and behavior was wrong;
- `SKIP`: capability, permission, or fixture was unavailable.

Only a reviewed `PASS` may update physical evidence in
[requirements.md](requirements.md).

## Environment variables

| Variable | Meaning |
|---|---|
| `Z1_HIL_HOST` | Optional target IPv4 override; otherwise UDP discovery is used |
| `Z1_HIL_SERIAL` | Diagnostic serial device when a real COM fixture exists |
| `Z1_HIL_SD`, `Z1_HIL_CAMERA`, `Z1_HIL_CONTROLLER`, `Z1_HIL_CAN`, `Z1_HIL_BLE` | Declares installed physical fixtures |
| `Z1_HIL_MOCK_SD`, `Z1_HIL_MOCK_CAMERA`, `Z1_HIL_MOCK_CONTROLLER`, `Z1_HIL_MOCK_NETWORK`, `Z1_HIL_MOCK_NVS` | Declares mock adapters in the flashed build |
| `Z1_HIL_MACHINE_NAME` | Exact BLE machine name expected by identity tests |
| `Z1_HIL_AP` | Declares that SoftAP behavior may be exercised by the selected tests |
| `Z1_HIL_WIFI_SSID`, `Z1_HIL_WIFI_PASSWORD` | Recovery-safe credentials for explicitly mutating provisioning tests |
| `Z1_HIL_OTA_IMAGE` | Valid raw application image used by destructive OTA cases |
| `Z1_HIL_SPIFFS_IMAGE` | Valid SPIFFS image used by destructive `/updateffs` cases |
| `Z1_HIL_PREVIEW_FILE` | Existing AVI below `/sd/videos` for physical preview |
| `Z1_HIL_STATIC_ASSET`, `Z1_HIL_STATIC_ASSET_TIMEOUT` | Optional private large web asset and bounded timeout |
| `Z1_HIL_USB_RESET` | Explicitly permits recoverable native USB reset cases |
| `Z1_ALLOW_MUTATION` | Enables recoverable mutations when set to `1` |
| `Z1_ALLOW_DESTRUCTIVE` | Enables firmware/partition replacement when set to `1` |

## Evidence by area

### USB and host transports

Native USB read-only HIL verifies exact identity and bulk endpoints plus framed
round trips. Stress fixtures cover maximum-size frames, large upload/download,
MD5, partial transfer recovery, repeated bus resets, simultaneous TCP/HTTP, and
worker recovery. On 2026-08-20, release firmware additionally passed two manual
five-second cable unplug/replug cycles. USB re-enumerated without restarting the
ESP, Wi-Fi, controller communication, or machine state.

TCP HIL verifies four stable clients, fifth-client rejection, slot reuse,
fragmented frames, discovery capacity state, and service recovery after mixed
USB/HTTP load. HTTP fixtures cover normative status/body behavior, static
content, WebSockets, diagnostics, configuration APIs, and bounded interruption
recovery.

### Connectivity

Wi-Fi evidence covers station association, address acquisition, scanning,
diagnostics, UDP discovery, persisted credentials, and recovery after OTA.
BLUFI evidence covers advertisement identity, GATT properties, connection
cycles, protocol errors, security negotiation, encrypted fragmented messages,
credential provisioning, and coexistence with USB and HTTP.

These results do not model RF attenuation, interference, access-point failure,
or long-duration coexistence.

### Storage, configuration, and diagnostics

Physical SD tests verify existing data without writing, then separately gated
unique-name mutations verify cleanup and restoration. Card removal/reinsertion
is tested without restarting USB or network services. The mock block device
adds deterministic unmount, full-volume, read, write, and sync failures; those
are target-composition evidence, not physical-media conformance.

A pre-existing `/serial.log` sentinel enables an SD mirror of runtime
diagnostics while UART remains active. It is useful on USB-only machines but
cannot capture boot output before SD mount and does not replace a COM fixture.

### Controller, camera, and CAN

Physical controller tests are intentionally read-only: version, status,
diagnostics, repeated queries, and coexistence. Transfer-family state machines,
timeouts, malformed replies, cancellation, queue pressure, and UART
fragmentation are exercised with the controller mock.

Physical OV3660 tests verify JPEG framing, stream reconnect, successor ownership,
preview, concurrent USB/HTTP/controller traffic, and that an accepted recording
request cannot create an AVI while the physical controller remains idle. Live and control frames are
serialized through the HTTP server task so socket receive/close processing cannot
race an external camera producer. Ten consecutive physical regression passes
(30 cases) verify repeated disconnect, preemption, and cross-service recovery.
Deterministic mock frames cover target error paths but not sensor quality or timing.

CANopen behavior is host-tested and TWAI composition target-built. No physical
CAN conformance is claimed until an active peer can drive traffic and faults.

### Updates and persistence

Destructive evidence covers direct application OTA in both partition
directions, rollback to the previous valid image, delayed-body recovery,
SPIFFS replacement, USB disappearance/re-enumeration, and Wi-Fi/BLE return.
Persistence checks cover serial identity, first-boot/runtime counters, and saved
Wi-Fi credentials across OTA reboot. Aggregate controller-package installation
still needs its physical update fixture.

## Current verification snapshot

The portable suite contains 856 passing C++ tests. The Python tooling suite
contains 58 passing tests. The latest generated portable coverage snapshot is
96.20% lines, 98.52% functions, and 86.69% branches.

The latest reviewed physical Z1 campaign covered USB, Wi-Fi, BLE, SD,
controller, OV3660 camera, preview, SPIFFS, and OTA. Capability-gated COM and CAN
cases remain skips. Counts from individual campaigns belong in generated HIL
reports or the historical archive rather than this current-status document.

## Historical evidence

Detailed dates, intermediate failures, superseded specification behavior,
temporary build paths, and retained report filenames are available in
[hardware-testing-history.md](hardware-testing-history.md). That archive is
useful for diagnosis and provenance, but the tables and area summaries above
are authoritative for current fixture coverage.
