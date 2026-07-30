# Hardware-in-the-loop testing

The HIL suite validates target integrations that host tests and target builds
cannot prove. Hardware absence is reported as `SKIP`; it is never treated as a
pass or as requirement conformance.

## Safety model

| Marker | Default | Purpose |
|---|---|---|
| `readonly` | Runs when its fixture is detected | Enumeration, status, queries, and observation |
| `mutating` | Requires `Z1_ALLOW_MUTATION=1` | Recoverable filesystem, NVS, and connection changes |
| `destructive` | Requires `Z1_ALLOW_DESTRUCTIVE=1` | OTA, firmware replacement, erase, and factory operations |

Detecting a device grants permission only for read-only tests. The suite does
not flash firmware automatically. Mutating tests must use unique temporary
resources and clean them up. Destructive tests must document recovery before
they are enabled.

## Installation and execution

```sh
python3 -m pip install -r requirements-hil.txt
python3 -m pytest tests/hardware
python3 -m pytest tests/hardware -m usb
python3 -m pytest tests/hardware -m "tcp or sd"
Z1_ALLOW_MUTATION=1 python3 -m pytest tests/hardware -m mutating
Z1_ALLOW_DESTRUCTIVE=1 python3 -m pytest tests/hardware -m destructive
Z1_HIL_HOST=192.168.8.119 Z1_ALLOW_DESTRUCTIVE=1 \
  Z1_HIL_OTA_IMAGE=build/mainboard_firmware.bin \
  python3 -m pytest tests/hardware/test_ota.py
```

| Variable | Meaning | Default |
|---|---|---|
| `Z1_HIL_HOST` | Target IPv4 address for TCP/HTTP tests | `192.168.4.1` |
| `Z1_HIL_SERIAL` | Diagnostic serial device | uniquely detected USB modem |
| `Z1_HIL_SD` | Declares that a physical SD reader and card are installed | disabled |
| `Z1_HIL_MOCK_SD` | Declares that the flashed firmware uses the mock SD profile | disabled |
| `Z1_HIL_MOCK_CAMERA` | Declares that the flashed firmware uses the deterministic camera mock | disabled |
| `Z1_HIL_MOCK_CONTROLLER` | Declares that the flashed firmware uses the deterministic controller-channel mock | disabled |
| `Z1_ALLOW_MUTATION` | Enables recoverable persistent changes when `1` | disabled |
| `Z1_ALLOW_DESTRUCTIVE` | Enables destructive operations when `1` | disabled |
| `Z1_HIL_OTA_IMAGE` | Valid raw ESP-IDF application image for destructive OTA HIL | unset |
| `Z1_HIL_SPIFFS_IMAGE` | Valid SPIFFS image for destructive `/updateffs` HIL | unset |
| `Z1_HIL_WIFI_SSID` | Recovery-safe network used by mutating Wi-Fi HIL | unset |
| `Z1_HIL_WIFI_PASSWORD` | Password paired with `Z1_HIL_WIFI_SSID` | unset |
| `Z1_HIL_USB_RESET` | Permits recoverable native USB reset and re-enumeration | disabled |
| `Z1_HIL_CONTROLLER` | Declares an attached controller fixture | disabled |
| `Z1_HIL_CAN` | Declares an attached CAN fixture | disabled |
| `Z1_HIL_BLE` | Declares an available BLE scanner | disabled |
| `Z1_HIL_WIFI_SSID` | Test-network SSID used by mutating BLUFI provisioning | unset |
| `Z1_HIL_WIFI_PASSWORD` | Test-network password; an empty value is valid | unset |

Camera availability is detected automatically through a valid
`POST /api/camera/resolution` request. The exact controlled sensor-unavailable
response marks camera-dependent HIL as skipped; no environment declaration is
required.

BLE HIL uses the host adapter through Bleak. With `Z1_HIL_BLE=1`, absence of
the required `BLUFI_DEVICE` advertisement is a failure rather than a fixture
skip. The read-only baseline validates service UUID `0xffff`, characteristics
`0xff01` and `0xff02`, their write/notify properties, and the fixed outgoing
read value. Encrypted provisioning remains gated until this baseline passes.
On macOS, the terminal/Codex Python process must also be allowed under System
Settings, Privacy & Security, Bluetooth. A CoreBluetooth unavailable or
unauthorized result is a fixture skip rather than product evidence.
The initial declared-adapter run on 2026-07-27 exposed an invalid service-UUID
argument in the target advertising configuration. After correcting the ESP-IDF
128-bit input representation for advertised UUID `0xffff`, a freshly flashed
build logged `Advertising started as BLUFI_DEVICE`; both advertisement/service
discovery and the standard GATT schema/fixed-read checks passed physically.
The expanded read-only fixture also verifies advertising recovery after a
disconnect, three complete connection/read/disconnection cycles, notification
subscription, a response-bearing invalid-envelope write, and continued GATT
health after that rejected protocol input.
The wire-level fixture additionally checks exact version and status responses,
Wi-Fi-list record encoding, ignored unknown controls, deterministic negotiation
errors, Diffie-Hellman negotiation, salted MD5 key derivation, AES-CFB128,
CRC-16, and encrypted fragmented Wi-Fi-list delivery. These checks are
read-only because they do not stage credentials or request station changes.

The separately gated provisioning test requires `Z1_ALLOW_MUTATION=1`,
`Z1_HIL_WIFI_SSID`, `Z1_HIL_WIFI_PASSWORD`, and `Z1_HIL_HOST`. It negotiates a
fresh secure BLUFI session, sends encrypted SSID and password frames, requests
association, and waits up to 30 seconds for Wi-Fi diagnostics at the declared
host. Use only credentials that keep the board reachable from the test host.
The gated test passed physically on 2026-07-30 using the board's existing test
network: encrypted credential delivery and the connect request completed, and
the diagnostics endpoint returned connected at the expected IPv4 address.
Credentials are intentionally absent from reports and documentation.

Two robustness fixtures are also available. Declaring `Z1_HIL_HOST` adds eight
concurrent Wi-Fi-diagnostics requests while an encrypted BLE status exchange is
in flight. Declaring `Z1_HIL_SERIAL` together with `Z1_ALLOW_MUTATION=1` resets
the target during an active GATT connection, requires a disconnect callback,
and then requires `BLUFI_DEVICE` to resume advertising after boot. Both cases
passed physically on 2026-07-30; the reset case observed the disconnect and
rediscovered the advertisement after the target booted.

Additional read-only negative-wire fixtures exercise sequence rejection with a
correct retry in the same connection, acknowledgement-before-product-response
ordering, and the exact checksum-error report. All three passed physically on
2026-07-30. The same run rechecked GATT discovery, minimum MTU/write capacity,
and the fixed outgoing read.

The read-only suite also exercises repeated native USB requests, recovery after
unframed USB noise, bytewise fragmented TCP input, the four-client TCP limit,
four-client concurrency, six consecutive four-client capacity waves,
simultaneous USB/TCP commands, WLAN scanning, runtime
and serial-number reads, monotonic Wi-Fi diagnostics, concurrent HTTP requests,
and recovery after an interrupted multipart request. Persistent Wi-Fi changes,
USB reset, application OTA, and SPIFFS replacement remain separately gated.

The combined mock HIL run on 2026-07-30 initially exposed an internal-memory
limit after mixed HTTP, controller, SD, and TCP traffic. The fourth 8192-byte
TCP client task could not be created with only about 19--22 KiB of internal heap
free. TCP client stacks now use PSRAM through the ESP-IDF capability-aware task
API, retain their task control blocks in internal RAM, and fall back to internal
stack allocation when external allocation is unavailable. The complete
60-test HIL run then passed all 26 executable cases without a reset before TCP
stress; six additional four-client waves also passed and returned the active
count to zero after every wave.

With a diagnostic COM adapter, the mutating diagnostic test pulses reset using
DTR/RTS, captures 20 seconds at 115200 baud, requires both the ESP32 boot banner
and application startup output, rejects panic/watchdog/stack/assert/abort
signatures, and saves the raw evidence as `build/hil-diagnostic-boot.log`.
The first physical run on 2026-07-27 captured 13,021 bytes and passed: ESP-ROM,
the application `MAIN` startup, the expected unsupported-camera diagnostic,
Wi-Fi association/address acquisition, and OTA validation were present, with
none of the forbidden fatal signatures.

Native USB is detected by VID `0x303a` and PID `0x4002`. TCP detection opens a
connection to port 2222. SD behavior may be exercised through either transport.
Physical storage requires `Z1_HIL_SD=1`; the explicit mock target profile uses
`Z1_HIL_MOCK_SD=1`. Neither declaration is inferred from a successful empty
directory response.

### Mock SD target profile

The SD mock validates the complete FAT, VFS, POSIX, path-sandbox, and
public-command stack without a physical reader. It allocates a fresh 512 KiB
block device in PSRAM on every boot and mounts it at `/sd`. It is deliberately
volatile and emits a prominent `TEST BUILD` diagnostic. The generic builder
discovers available mock switches from Kconfig and supports any combination in
the standard reusable `build/` tree:

```sh
source /Users/moinois/esp/esp-idf/export.sh
python3 tools/build_firmware.py --mock sd
python3 tools/build_firmware.py --mock camera
python3 tools/build_firmware.py --mock controller
python3 tools/build_firmware.py --mock camera,controller,sd
python3 tools/build_firmware.py --mock-all
python3 tools/build_firmware.py --mock sd --flash -p /dev/cu.usbmodem...
Z1_HIL_MOCK_SD=1 Z1_HIL_HOST=192.168.8.119 Z1_ALLOW_MUTATION=1 \
  python3 -m pytest tests/hardware/test_sd_storage.py
```

`CONFIG_Z1_MOCK_ALL_HARDWARE` selects every implemented mock adapter;
`CONFIG_Z1_MOCK_SD_HARDWARE` selects only SD, and
`CONFIG_Z1_MOCK_CAMERA_HARDWARE` and `CONFIG_Z1_MOCK_CONTROLLER_HARDWARE`
select the camera and controller channel independently. The SD capacity and
mandatory post-allocation PSRAM reserve are independently configurable. No
mock switch is enabled in `sdkconfig.defaults`. Every invocation explicitly enables
or disables all discovered adapters and records the effective choice in
`build/hardware-selection.json`, preventing stale selections when the build
directory is reused. `--live` creates an explicit all-live selection.

The camera mock follows the same initialization, configuration, resolution,
capture, recording, WebSocket streaming, and OTA-deinitialization surface as
the physical adapter. It emits the deterministic JPEG marker sequence
`ff d8 ff d9`; this validates framing and routing but is not image-quality,
sensor, pin, timing, or electrical conformance. Declare the flashed selection
with `Z1_HIL_MOCK_CAMERA=1` to enable its positive streaming HIL check.

The controller mock implements the same byte channel as the physical UART
adapter. It decodes frames with the production controller stream policy and
returns deterministic version, idle-status, and diagnostic frames for the
periodic `?` and `diagnose` requests. This exercises scheduling, framing,
snapshot retention, RSSI diagnostic aggregation, and TCP reply routing. Declare
the flashed selection with `Z1_HIL_MOCK_CONTROLLER=1` to enable the positive
HIL check. It does not validate UART pins, baud rate, electrical behavior,
controller timing, or the controller firmware-transfer handshake.

The combined camera/controller/SD mock profile was built and flashed on
2026-07-30. Controller HIL passed for deterministic idle status, diagnostic
data with runtime RSSI aggregation, and the composed controller/mainboard
version response through TCP. The camera WebSocket fixture also passed again
on the same image, confirming that the independently selected mocks coexist.

The mutation-gated SD suite includes a native-USB upload/download round trip,
MD5 verification, cache-preserving rename and delete, traversal confinement,
and exact `gcodes` token mapping. These transfers require the board's native
`USB` connector because ownership must remain on one transport for the complete
multi-frame exchange. Tests clean up their unique files and directories even
after an assertion failure.

Create an empty logical `/serial.log` to opt into a second diagnostic copy while
UART remains active. The firmware never creates this sentinel automatically,
flushes each accepted record, and stops before the file would exceed 384 KiB.
It neither truncates old evidence nor changes the rotating normative log. The
mock copy is useful for USB-only failures but disappears with the entire PSRAM
volume after a full target reset. The SD suite creates, reads, and removes this
sentinel in its diagnostic-mirror test.

## Requirement evidence

Tests carry `requirement` markers. Write a machine-readable report with:

```sh
python3 -m pytest tests/hardware --hil-report build/hil-results.json
```

- `PASS`: the physical check ran and passed;
- `FAIL`: the fixture was present but observed behavior was wrong;
- `SKIP`: a dependency, permission, or fixture was unavailable.

Only a reviewed `PASS` may be recorded as physical evidence in
`docs/requirements.md`. A skipped or merely collected test leaves the existing
verification gap unchanged.

The reviewed 2026-07-26 runs against the freshly flashed build passed TCP
listener acceptance, framed read-only TCP and USB `ftype` round trips, and native
USB descriptor/endpoint checks. An SD root-list command completed but did not
prove that storage was mounted and is not conformance evidence. TCP first exposed
a real `tcp_client` stack overflow; the client-task stack was increased from
4096 to 8192 bytes, the firmware was rebuilt and reflashed, and the same checks
then passed. USB was verified after moving the cable from COM to native USB.
The attached camera failed its boot probe with
`ESP_ERR_NOT_SUPPORTED` and is not conformant yet.

The mutation-enabled USB run on the same date exposed incorrect command-
argument wiring, which was corrected and reflashed. Its remaining `mkdir`
failure is not valid filesystem evidence because this board may have neither
an SD reader nor a card. The earlier directory-list `PASS` was also invalid:
the protocol's completion-only response does not prove a mounted volume. Both
results are treated as fixture-unavailable rather than requirement evidence.

A destructive direct-OTA run on 2026-07-26 used COM monitoring and Wi-Fi HTTP.
Version 2 was the known baseline in `ota_0`; raw version 3 was uploaded as the
first multipart part to `/update`. The target returned the specified success
text, restarted after two seconds, booted `ota_1` at `0x220000`, and reported
`App version: 3`. A second upload then installed raw version 4 into the now
inactive, previous partition `ota_0`; the target again returned the success
text, restarted, booted at `0x20000`, and reported `App version: 4`. Both TCP HIL
checks passed after each update. This verifies OTA alternation in both directions
(`ota_0` to `ota_1` and `ota_1` back to `ota_0`). The first run also exposed and
fixed a target edge case: camera deinitialization must succeed idempotently when
camera probing already failed during boot.

The same run enabled the ESP-IDF bootloader rollback state machine and verified
the failure path. A valid but deliberately fault-injected version 5 was uploaded
from healthy version 4 in `ota_0` to inactive `ota_1`. Version 5 booted once from
`0x220000`, restarted before calling `esp_ota_mark_app_valid_cancel_rollback()`,
and was rejected automatically. The next boot selected the previous healthy
version 4 from `ota_0` at `0x20000`. Normal firmware marks itself valid only after
the critical `app_main` startup sequence completes; failures that restart before
that point therefore retain the previous image. The fault injection is available
only when building with `Z1_OTA_ROLLBACK_TEST_FAILURE` set.

Multipart reception for both `/update` and `/updateffs` treats a temporary
five-second socket timeout as a recoverable transport gap. It permits at most
six retries after timeouts (about 30 seconds of retry allowance), resets the counter whenever data
arrives, and logs timeout counts plus progress every 256 KiB. This bound avoids
abandoning a valid upload during a brief Wi-Fi interruption without retaining
an HTTP worker forever. `test_ota_survives_receive_timeout` sends 4 KiB, pauses
for seven seconds, then completes a real OTA update; it is destructive and also
requires an explicit valid image through `Z1_HIL_OTA_IMAGE`.

Wi-Fi disconnect backoff is executed by a coalescing reconnect task. The ESP-IDF
event loop therefore no longer sleeps for the policy's retry delays and remains
available to process association, DHCP, and socket-related events.

The first physical delayed-chunk attempt on 2026-07-26 observed the intended
recovery: after the injected seven-second pause the target logged timeout 1/6
and accepted subsequent bytes. The hotspot link then stopped delivering data
for more than the complete retry window after 23 KiB, so the bounded receiver
terminated the request. This proved recovery from the original single-timeout
failure, but did not yet prove a completed delayed-chunk OTA.

A subsequent run on 2026-07-27 completed successfully in 122.30 seconds.
The target recovered after the injected timeout, received all 1,762,088
multipart bytes, validated the image, selected `ota_1`, returned the exact HTTP
success response, restarted, booted at `0x220000`, and marked the image valid.
The run also exposed an undersized Wi-Fi reconnect-task stack during intentional
restart; increasing that task from 3072 to 6144 bytes removed the overflow in
the final passing run. The HIL client now terminates response reception from
HTTP `Content-Length` instead of waiting for the rebooting server to close TCP.

Versions 2 through 5 above were temporary ESP-IDF application versions used to
make partition transitions unambiguous in the COM log. They are neither the
aggregate package-format version nor the product-facing `version` response. The
normal project application version was reset to 1 after the test.

Read-only HTTP HIL on 2026-07-26 verified the exact JSON response from
`GET /api/firmware/info`, the exact 404 response for missing SPIFFS resources,
invalid-JSON rejection by `POST /api/camera/resolution`, and non-multipart
rejection by both update endpoints. A valid camera-resolution request reached
the adapter and returned the expected controlled 500 response because the sensor
is unavailable. Initial WebSocket upgrade requests returned 404 because the
handlers were incorrectly registered after the wildcard route on port 80 and no
handlers were registered on the specified video port 82. The handlers were moved
to the dedicated server, version 1 was installed by OTA, and both `/ws_video` and
`/ws_preview` then returned HTTP 101 on port 82. The combined HTTP/TCP HIL suite
passed all nine checks that existed at that time. The HTTP suite now also
contains the Wi-Fi diagnostics schema check described below.

Wi-Fi diagnostics are available through read-only
`GET /api/wifi/diagnostics`. The response contains current connection state,
RSSI in dBm, radio channel, authentication mode, station IPv4 address,
boot-lifetime association/address/disconnection counters, the latest numeric
ESP-IDF disconnect reason, and the bounded persistent Wi-Fi event log. It does
not return saved credentials or BSSID. The HTTP HIL suite validates the schema
and sensible radio ranges whenever the endpoint is detected.

## Current fixture coverage

The executable suite covers native USB descriptors, framing recovery and
repeated round trips, TCP listener/capacity/concurrency/framing, cross-transport
operation, HTTP/WebSocket concurrency and interruption recovery, Wi-Fi scan and
diagnostics, runtime reads, an explicitly gated recoverable filesystem
operation, native USB reset, Wi-Fi persistence, destructive delayed-chunk OTA,
and SPIFFS replacement. SD tests require an explicitly declared reader/card because protocol
completion alone cannot prove a mount. Controller, CAN, BLE, RF-control, and
recording still need dedicated fixture drivers. Capability-gate tests document
missing controller, CAN, BLE, and camera fixtures as skips; the camera gate
probes the firmware automatically.

On 2026-07-27 the expanded suite was run through native USB and station Wi-Fi
at `192.168.8.119`. The safe run produced 19 PASS, 12 capability/safety SKIP,
and three TCP stress failures. USB repetition/noise recovery, HTTP concurrency
and interrupted-request recovery, WLAN scan, runtime reads, simultaneous
USB/TCP access, Wi-Fi diagnostics, and all earlier HTTP/USB checks passed. A
separately gated native USB reset also passed re-enumeration and command
recovery. TCP capacity stress exposed resets before four stable clients could
be retained; subsequent fragmented and ordinary TCP requests became
intermittently reset while USB and HTTP remained responsive. This is retained
as failing evidence requiring target connection-slot/resource diagnosis, not
reported as a HIL pass. The machine-readable result is written to
`build/hil-current.json`.

On 2026-07-30 the TCP resource fix was retested on the same native USB and
station-Wi-Fi fixture. All six transport-stress checks passed: repeated USB,
USB noise recovery, byte-fragmented TCP input, four retained clients plus the
exact fifth-client rejection, four concurrent clients, and six consecutive
capacity-release waves. The result is recorded in
`build/hil-transport-current.json`.

On 2026-07-31 native USB disconnect handling was fault-injected with PyUSB bus
resets. A reset after half of an encoded command discarded the stale receive
fragment and the re-enumerated endpoint returned a valid response to the next
complete command. A separate mock-SD run reset the bus after the target had
requested block one of a 4096-byte upload; the same USB owner reactivated the
protocol, completed the pending block, downloaded identical content with MD5
verification, and removed the fixture file. These cases provide target evidence
for USB receive clearing and OWN-008 continuation without claiming direct
physical injection of TinyUSB transmit-queue saturation or no-progress timing.

The same 2026-07-31 mock-SD build also passed explicit upload cancellation with
both the partial target and its MD5 sidecar absent afterwards, a TCP root query
while USB retained an active upload, and a bounded full-volume run. The latter
filled only the volatile 512 KiB PSRAM-backed FAT volume with maximum-size file
blocks, observed the specified write-error retry, canceled the transfer, and
then completed a new upload/download after the released allocation was reused.
The destructive capacity case is hard-gated to `Z1_HIL_MOCK_SD=1` and therefore
cannot fill a physical card accidentally.

A combined mock-SD/controller image was then built and installed through the
seven-second delayed OTA test. The controller mock limits each read to seven
bytes so target decoding is exercised across frame boundaries. HIL uploaded a
1300-byte controller image plus configuration and factory fixtures, then drove
the C-, D-, and E-family start, geometry, indexed-data, and terminal exchanges.
All three published success diagnostics; configuration remained unchanged,
while the controller-image handoff and factory completion consumed their files
as specified. This verifies target composition and transfer logic but not the
electrical or timing properties of a physical UART/controller.

The same session installed the SD-only mock build through OTA and passed all
eight mutating storage checks over native USB, including multi-block file
upload/download, MD5, rename/delete, path confinement, exact `gcodes` token
mapping, and diagnostic-log sentinel access. This is target-composition evidence
for the production FAT/VFS/USB paths, not physical SD-card conformance. During
descriptor inspection the required strings were found shifted because the
TinyUSB string table omitted index-zero LANGID. A new HIL assertion reproduced
the failure; after adding the LANGID entry and reinstalling through OTA, exact
manufacturer `Espressif`, product `MakeraZ1 (USB)`, and serial `123456` all
passed. Reports are stored in `build/hil-sd-mock-usb.json` and
`build/hil-usb-descriptors-current.json`.

Temporary Wi-Fi interruption recovery was physically exercised on 2026-07-30
against the mock-SD firmware at `192.168.8.119`. One test held an upload idle
for six seconds and observed the required 5.010-second retry before completing.
A second test transferred 128 KiB in 8192-byte blocks, deliberately closed TCP
halfway through, reconnected into the same lowest free logical slot, required
the target to repeat the outstanding sequence, completed the remaining blocks,
and verified the stored MD5. Both passed together; the machine-readable report
is `build/hil-file-transfer-recovery-final.json`.

UDP discovery was then observed directly on the station network. The target
emitted the five-field, no-line-ending payload on port 3333 with its station
IPv4 address and TCP port 2222. A capacity test retained four TCP clients,
observed `tcp-full=1`, closed all four, and observed the following periodic
advertisement return to `tcp-full=0`. Both read-only checks passed on
2026-07-31; AP-subnet delivery and the event/command-triggered three-packet
bursts remain portable-policy evidence rather than physical observations.
