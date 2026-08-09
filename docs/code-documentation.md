# Code documentation tree

This document is the map for the source-level API documentation. C++ headers
are the primary documentation surface: public classes and functions use
Doxygen-compatible comments, while implementation comments explain lifecycle,
ownership, protocol ordering, and non-obvious compatibility decisions.

## Architecture tree

```text
components/
├── core/                         Portable policy and protocol logic
│   ├── include/core/             Public core interfaces and value types
│   └── src/
│       ├── can/                  CANopen node, dictionary, PDO, SDO, timing
│       ├── configuration/        Configuration syntax and JSON policies
│       ├── connectivity/         Wi-Fi and discovery policies
│       ├── filesystem/           Path sandbox, syntax, transfer mapping
│       ├── media/                AVI parsing/writing and preview policies
│       ├── network/              Network policy and discovery formatting
│       ├── protocol/             Frames, CRC, text, status, limits
│       ├── update/               Package and update validation policies
│       └── web/                  HTTP, multipart, static-file policies
│
├── application/                  Transport-neutral application services
│   ├── include/application/      Public application interfaces and ports
│   └── src/
│       ├── camera/               Camera settings and resolution policy
│       ├── can/                  CAN service and digital-output monitoring
│       ├── configuration/        Configuration document and live settings
│       ├── connectivity/         WLAN requests and station lifecycle
│       ├── controller/            Controller command and transfer policies
│       ├── diagnostics/          Capture, logging, and bounded rotation
│       ├── playback/             Play, preview, and media session state
│       ├── provisioning/          BLE/BLUFI policy and wire handling
│       ├── runtime/              Runtime state, commands, and routing
│       ├── storage/              File transfer, filesystem, SD lifecycle
│       ├── transport/            TCP queues, dispatch, and framing
│       ├── update/               Firmware update orchestration
│       ├── usb/                  USB protocol state and transmit/receive policy
│       └── web/                  Recording and web-volume application policy
│
main/                             ESP-IDF target composition and adapters
├── can/                            TWAI/CAN hardware adapter
├── configuration/                  POSIX/NVS configuration adapters
├── connectivity/                   ESP Wi-Fi, TCP, and discovery adapters
├── controller/                     UART/controller target adapters
├── diagnostics/                    ESP logging and diagnostic adapters
├── media/                          HTTP, camera, recording, and SPIFFS adapters
├── provisioning/                   ESP BLE/BLUFI adapter
├── runtime/                        FreeRTOS/NVS/runtime adapters
├── storage/                        FAT/POSIX and SD hardware adapters
├── transport/                      TinyUSB and TCP target transports
└── update/                         ESP-IDF OTA and update adapters

tests/                              Verification tree mirroring the domains
├── core/application domain tests   Portable policy and state-machine tests
├── compatibility/                  Golden traces and external behavior
└── hardware/                       Fixture-gated HIL, mock, mutating tests
```

## Documentation rules

- Public headers document purpose, ownership, lifecycle, parameters, return
  values, and failure behavior.
- Internal code is documented where ordering, bounds, concurrency, compatibility
  aliases, or security policy is not obvious from the syntax.
- `components/core` and `components/application` must remain usable without
  ESP-IDF; target details belong in `main` adapters.
- User-path authority is documented in `core/filesystem/sd_user_path.hpp` and
  ADR-016. Normative `/sd/...` response formatting is documented at the
  response sites and in the requirements matrix.
- Requirement IDs belong in tests and requirements documentation, not in every
  implementation comment.

## Generated API documentation

The comments are compatible with Doxygen. A future generated site should use
`components/core/include`, `components/application/include`, and selected
target headers as its input roots. `src/` and `main/` implementation files may
be included for internal documentation, but generated output should distinguish
portable APIs from ESP-IDF adapters.

Generated documentation is an artifact and should remain outside source
control, alongside other build output under `build/`.

Release builds use the same factory and interfaces as development builds, but
must be invoked with `--live --release`. This enables size optimization and
compile-time live selection; mock implementations remain available for HIL
builds without becoming part of the release configuration.

Fault-injection boundaries follow the same rule: live builds link no-op NVS
and network hook implementations, while the stateful mock hook sources are
added only when their Kconfig mock is selected. This keeps test controls out of
the release image without duplicating the production adapters.

The repository includes `docs/Doxyfile`. Generate the local site with:

```bash
doxygen docs/Doxyfile
open build/doxygen/html/index.html
```

Doxygen currently reports a small number of undocumented aggregate-result
members in the CAN SDO and update-package headers; these are candidates for a
later comment-only cleanup.
