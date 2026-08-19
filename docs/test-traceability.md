# Specification test traceability

This document maps the current automated tests to the public specification.
Each entry describes what the test exercises and lists the normative requirement
IDs it covers. `Host` means the portable C++ suite; `HIL` means a target test
under `tests/hardware`. HIL tests may be gated by the required physical or mock
fixture. A skipped HIL case is not conformance evidence.

The mapping is intentionally test-oriented: it records executable cases and
their requirement markers, while [`requirements.md`](requirements.md) records
implementation state and reviewed evidence. When a test has several parameter
variants, the variants are grouped under one description.

## 01 — Hardware and startup

| Test | What it verifies | Requirements |
|---|---|---|
| `tests/hardware/test_diagnostics.py::test_diagnostic_serial_port_is_detectable` | Detects the application diagnostic serial fixture without confusing it with the bootloader port. | HW-001, DIAG-001 |
| `tests/hardware/test_diagnostics.py::test_reset_emits_healthy_boot_diagnostics` | Resets the target, captures the boot/application log, and rejects panic, watchdog, or stack-failure signatures. | BOOT-001, DIAG-001 |
| `tests/hardware/test_persistence_reboot.py::test_runtime_identity_and_wifi_persist_across_ota_reboot` | Confirms identity, first-boot state, runtime values, and station credentials survive an OTA reboot. | BOOT-001–BOOT-003, RUN-010, RUN-030–RUN-032, RUN-043, NET-010, NET-017 |
| `tests/runtime/test_persistent_store_initialization.cpp` | Verifies the bounded two-round exhausted/version recovery, fatal first erase failure, and final initialization after a failed general-recovery erase. | BOOT-001–BOOT-003, BOOT-015 |

Portable startup and adapter-selection cases are in `tests/runtime/` and
`tests/connectivity/`, especially `test_hardware_adapter_selection.cpp`,
`test_live_initialization.cpp`, and `test_connectivity_startup.cpp`.

## 02 — Framing and transports

| Test group | What it verifies | Requirements |
|---|---|---|
| `tests/protocol/test_frame.cpp` and `tests/protocol/test_crc.cpp` | Frame envelope, lengths, CRCs, tails, and recovery from malformed input. | FRM-001–FRM-016 |
| `tests/usb/test_usb_receive_staging.cpp`, `test_usb_protocol_state.cpp` | USB staging capacity, whole-session discard, disconnect clearing, and protocol activation state. | USB-004–USB-006, USB-009–USB-011 |
| `tests/usb/test_usb_transmit_queue.cpp`, `test_usb_transmit_drain.cpp`, `test_usb_transmit_progress.cpp` | FIFO capacity, admission, partial writes, completion removal, failure latching, purge, and no-progress timeout. | USB-007–USB-010 |
| `tests/transport/test_tcp_client_session.cpp`, `test_tcp_transmit_queue.cpp`, `test_tcp_frame_sender.cpp` | TCP receive/send buffering, short writes, retry policy, and session cleanup. | TCP-003–TCP-010 |
| `tests/hardware/test_usb.py`, `test_transport_stress.py`, `test_tcp.py` | Native USB descriptors/round trips, fragmented TCP frames, four-client capacity, fifth-client rejection, slot reuse, and post-stress recovery. | USB-001–USB-003, TCP-003–TCP-013 |

## 03 — Routing and commands

| Test | What it verifies | Requirements |
|---|---|---|
| `tests/runtime/test_local_command_classifier.cpp`, `test_local_command_queue.cpp` | Case-sensitive command recognition, argument boundaries, queue capacity, and serialization. | CMD-001–CMD-005 |
| `tests/hardware/test_tcp_local_routing.py` | Origin-aware local replies, controller-originated broadcasts, serial/runtime routing, and response isolation. | ROUTE-001–ROUTE-018, CMD-004, REC-001, RUN-010, RUN-040, UART-003 |
| `tests/hardware/test_mock_cross_transport.py` | USB/TCP shared configuration, filesystem ownership, and recovery after cross-transport contention. | ROUTE-001, OWN-001–OWN-003, CFG-010, CFG-030–CFG-031 |

## 04 — Motion-board protocols

| Test group | What it verifies | Requirements |
|---|---|---|
| `tests/controller/test_controller_query.cpp`, `test_controller_link.cpp`, `test_controller_snapshots.cpp` | Periodic queries, link activity, snapshot retention, and status formatting. | LPC-001–LPC-003, STAT-001–STAT-010, UART-003–UART-009 |
| `tests/controller/test_controller_*_transfer.cpp`, `tests/diagnostics/test_controller_diagnostics.cpp` | Per-family inbox bounds and diagnostics; firmware, configuration, and factory state machines; retained and malformed geometry; low-32-bit addressing; pre-read allocation; zero-size panic policy; response failures; cancellation; frame-driven timeout; and staged-file handling. | BOOT-012, DIAG-021, LPC-010–LPC-019, LPCFW-001–LPCFW-006, LPCCFG-001–LPCCFG-006, LPCFAC-001–LPCFAC-005, UPD-053–UPD-054 |
| `tests/hardware/test_optional_fixtures.py` | Fragmented mock-controller reads and all three transfer families through the target composition. | LPC-010–LPC-019, LPCFW-001–LPCFW-006, LPCCFG-001–LPCCFG-006, LPCFAC-001–LPCFAC-005 |
| `tests/hardware/test_physical_controller.py` | Reads physical controller version, status, and diagnostics over USB, then repeats safe status queries to detect stalled UART routing. | LPC-001, UART-003 |

## 05 — Host file transfer

| Test group | What it verifies | Requirements |
|---|---|---|
| `tests/storage/test_file_transfer_paths.cpp`, `test_file_transfer_admission.cpp` | Root-relative path normalization, literal `gcodes/` cache paths, owner admission, pending starts, and release ordering. | HFT-001–HFT-014, HFT-020–HFT-025, OWN-001–OWN-008 |
| `tests/storage/test_file_upload.cpp`, `test_file_download.cpp`, `test_file_hash_command.cpp` | Upload/download framing, cycle delays, combined retry histories, MD5 sidecars, signed dynamic geometry, modulo data offsets, omitted responses, queue-full diagnostics, missing/invalid metadata, and terminal responses. | HFTU-001–HFTU-011, HFTD-001–HFTD-010, HFT-020–HFT-025, DIAG-039, FILE-027–FILE-029 |
| `tests/hardware/test_sd_storage.py`, `test_file_transfer_recovery.py`, `test_mock_transfer_errors.py` | Target round trips, large frames, recovery after bounded silence, cancellation cleanup, MD5 recovery, resolved paths, and owner reuse. | HFT-004, HFT-010–HFT-011, HFT-020–HFT-025, HFTU-001–HFTU-011, HFTD-001–HFTD-010, OWN-003, OWN-008 |

## 06 — Storage and configuration

| Test group | What it verifies | Requirements |
|---|---|---|
| `tests/storage/test_sd_card_lifecycle.cpp`, `test_sd_access_diagnostic.cpp` | Mount/unmount ordering, absence, capacity policy, and diagnostic reason mapping. | SD-001–SD-008 |
| `tests/storage/test_sd_user_path.cpp`, `test_filesystem_syntax.cpp` | Central firmware-owned SD paths, `/`-only separators, root-relative resolution, component normalization, and cache mapping. | FILE-001–FILE-005, FILE-030–FILE-031, HFT-004, HFT-010–HFT-011 |
| `tests/storage/test_directory_listing.cpp`, `test_filesystem_commands.cpp` | Listing, type replies, mkdir/rm/mv, truncation and allocation-failure behavior. | FILE-011–FILE-026, DIAG-028 |
| `tests/configuration/test_configuration_files.cpp`, `test_configuration_document.cpp`, `test_configuration_get.cpp`, `test_configuration_set.cpp` | Bytewise config copies, long filename `config.default`, parsing, namespaces, hashes, and bounded responses. | CFG-001–CFG-006, CFG-010, CFG-015–CFG-016, CFG-020–CFG-023, CFG-030–CFG-034 |
| `tests/hardware/test_mock_cross_transport.py::test_configuration_default_and_restore_supports_long_filename` | Positive target verification of `/sd/config.default` under current SD-009 long-filename policy. | SD-009, CFG-001, CFG-004–CFG-006 |
| `tests/hardware/test_mock_sd_faults.py`, `test_sd_storage.py` | Mock volume lifecycle, latched media faults, full-volume recovery, diagnostics, and transfer cleanup. | SD-001–SD-010, FILE-010, FILE-015, LOG-001–LOG-006 |
| `tests/hardware/test_physical_sd_readonly.py` | Lists the installed card, downloads its existing configuration with MD5 verification, and compares USB and TCP bytes without writing media. | SD-001, CFG-001, HFTD-001 |

## 07 — Connectivity

| Test group | What it verifies | Requirements |
|---|---|---|
| `tests/provisioning/test_blufi_*.cpp` | BLUFI identity, GATT, framing, security, fragmentation, acknowledgements, and product payloads. | BLE-001–BLE-016, BLESEC-001–BLESEC-006, BWF-003–BWF-045 |
| `tests/hardware/test_ble_blufi.py` | Live advertising, GATT, encrypted status/list/error flows, Wi-Fi provisioning, reconnect, BLE + USB + HTTP/Wi-Fi concurrency, and OTA-reset disconnect/re-enumeration lifecycle. | BLE-001–BLE-017, BLESEC-001–BLESEC-006, BWF-003–BWF-045, WEB-001, WEBUP-004, USB-004 |
| `tests/connectivity/test_station_connection.cpp`, `test_wlan_command.cpp`, `test_wlan_request.cpp` | WLAN parsing, connect/disconnect policy, credentials, bounded scans, and responses. | NET-001–NET-046 |
| `tests/hardware/test_runtime_and_wifi.py`, `test_udp_discovery.py`, `test_mock_network_faults.py` | Live scan/diagnostics/discovery and injected TCP/discovery socket failure recovery. | NET-001–NET-046, DISC-001–DISC-008, TCP-003, DISC-008 |

## 08 — Media and web

| Test group | What it verifies | Requirements |
|---|---|---|
| `tests/media/test_avi_writer.cpp`, `test_recording_policy.cpp` | AVI structure, frame/index accounting, recording names, intervals, and active-state policy. | AVI-010–AVI-013, REC-001–REC-010 |
| `tests/media/test_preview_*.cpp` | Preview path validation, command/session policy, metadata, frame pacing, cancellation, and WebSocket input. | PREV-001–PREV-029, WEB-003, WEB-008 |
| `tests/hardware/test_physical_preview.py` | Existing physical-SD AVI open, metadata, indexed JPEG delivery, and bounded stop. | PREV-010, PREV-012–PREV-014, PREV-028 |
| `tests/hardware/test_mock_camera_lifecycle.py` | Camera resolution, WebSocket frame lifecycle, disconnect, successor sessions, and concurrent HTTP/TCP use with the mock. | CAM-001–CAM-015, PREV-001–PREV-029 |
| `tests/hardware/test_physical_camera.py` | Receives physical JPEG frames across reconnects and verifies streaming alongside HTTP, USB, and physical-controller status traffic. | HW-040, LIVE-001, LIVE-005, USB-004, UART-003 |
| `tests/hardware/test_web_ui.py`, `test_http.py`, `test_http_stress.py` | Static assets, MIME types, configuration API, validation, concurrency, and interrupted requests. | WEB-001–WEB-020, WEBUP-002, WEBUP-004, CFG-020–CFG-034 |

## 09 — Firmware update

| Test | What it verifies | Requirements |
|---|---|---|
| `tests/update/test_update_*.cpp`, `test_direct_*_update.cpp` | Package parsing, component validation, phase persistence, deletion, rollback, and update orchestration. | UPD-004–UPD-005, UPD-010–UPD-014, UPD-020–UPD-023, UPD-040–UPD-043, UPD-060–UPD-063 |
| `tests/update/test_update_controller.cpp`, `test_update_trigger.cpp` | Staged-controller reset scheduling, channel suppression, terminal phase transitions, repeat completion, and the independent monitor's one-attempt startup policy. | UPD-053–UPD-055 |
| `tests/hardware/test_ota.py`, `test_persistence_reboot.py` | Delayed/partial multipart OTA, timeout finalization, SPIFFS replacement, reboot recovery, rollback partition behavior, and persistence. | WEBUP-003, WEBUP-012, WEBUP-020, WEBUP-022, UPD-023, UPD-040–UPD-043 |

## 10 — Runtime and CAN

| Test group | What it verifies | Requirements |
|---|---|---|
| `tests/runtime/test_runtime_*.cpp`, `test_wall_clock.cpp`, `test_heartbeat.cpp` | Runtime counters, first-boot/time semantics, serial number, wall clock, heartbeat, and command formatting. | RUN-001–RUN-043 |
| `tests/can/test_canopen_*.cpp`, `test_can_pending_transmitter.cpp`, `test_can_output_monitor.cpp` | Node identity, dictionary, SDO/PDO, heartbeat/error, retry queue, and diagnostic composition. | CAN-001–CAN-015, HW-053, OD-004 |
| `tests/hardware/test_nvs_fault_injection.py`, `test_mock_runtime_config_endurance.py` | Target NVS open/commit faults, recovery, runtime sharing, persistence, and endurance. | BOOT-001–BOOT-003, RUN-010, RUN-030–RUN-043 |

## 11 — BLUFI wire protocol

The portable BLUFI wire tests are in `tests/provisioning/test_blufi_wire.cpp`,
`test_blufi_fragment.cpp`, `test_blufi_security.cpp`, and
`test_blufi_product.cpp`. They cover envelope, sequence/checksum handling,
encryption scope, fragmentation/reassembly, GATT writes, acknowledgements, and
product report payloads: `BWF-003`, `BWF-005`–`BWF-007`, `BWF-010`–`BWF-023`,
`BWF-030`–`BWF-045`, and `BLESEC-001`–`BLESEC-006`. Live confirmation is in
`tests/hardware/test_ble_blufi.py` and is fixture-gated by `Z1_HIL_BLE`.

## 12 — CANopen object dictionary

`tests/can/test_canopen_dictionary.cpp`, `test_canopen_sdo.cpp`,
`test_canopen_sdo_client.cpp`, `test_canopen_sdo_mailbox.cpp`, and
`test_canopen_service.cpp` cover dictionary lookup, expedited SDO access,
mailbox ordering, abort codes, and service composition: `OD-001`–`OD-004`,
`CAN-001`–`CAN-015`. Physical bus timing and electrical behavior remain
fixture-gated.

## 13 — Diagnostic output

| Test group | What it verifies | Requirements |
|---|---|---|
| `tests/diagnostics/test_*diagnostic*.cpp` | Record formatting, timestamps, bounded capture, rotation, shutdown drain, filesystem/controller/update/playback diagnostics, and BLUFI custom-data records. | DIAG-001–DIAG-044, LOG-001–LOG-013 |
| `tests/hardware/test_diagnostics.py` | Physical boot/reset diagnostic output and fatal-signature rejection. | BOOT-001, DIAG-001 |
| `tests/hardware/test_sd_storage.py::test_serial_log_sentinel_mirrors_diagnostics` | Opt-in `/serial.log` mirror, flush, diagnostic content, and cleanup via production USB/SD paths. | LOG-001, LOG-006 |

## Running the mapped suites

Portable tests:

```sh
cmake --build build/host-coverage
ctest --test-dir build/host-coverage --output-on-failure
python3 -m pytest tools/tests -q
```

Read-only HIL with live Wi-Fi/BLE and mock storage/camera/controller:

```sh
Z1_HIL_HOST=192.168.8.119 Z1_HIL_MOCK_SD=1 \
Z1_HIL_MOCK_CONTROLLER=1 Z1_HIL_MOCK_CAMERA=1 Z1_HIL_BLE=1 \
Z1_HIL_WIFI_SSID=Away Z1_HIL_WIFI_PASSWORD=SailWithMe \
python3 -m pytest tests/hardware -m 'hardware and not mutating and not destructive'
```

Mutating and destructive cases require explicit opt-in environment variables;
see [`hardware-testing.md`](hardware-testing.md). Reports from a run should be
reviewed together with the fixture declarations and never interpreted as
physical evidence when a mock adapter was selected.
