# HIL evidence history

This append-only-style archive preserves chronological investigations, intermediate
failures, superseded results, and report references formerly kept in the current
HIL guide. For current fixture coverage and execution instructions, use
[hardware-testing.md](hardware-testing.md).

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

`pytest-timeout` applies a ten-minute per-test safety limit. Individual transport
deadlines remain the primary assertions; the outer limit prevents a lost host
USB handle or fixture deadlock from blocking an unattended HIL run forever.

| Variable | Meaning | Default |
|---|---|---|
| `Z1_HIL_HOST` | Target IPv4 address for TCP/HTTP tests; when unset, the station address is read from UDP discovery on port 3333 | unset (automatic discovery) |
| `Z1_HIL_SERIAL` | Diagnostic serial device | uniquely detected USB modem |
| `Z1_HIL_SD` | Declares that a physical SD reader and card are installed | disabled |
| `Z1_HIL_MOCK_SD` | Declares that the flashed firmware uses the mock SD profile | disabled |
| `Z1_HIL_CAMERA` | Declares that a physical camera is installed | disabled |
| `Z1_HIL_MOCK_CAMERA` | Declares that the flashed firmware uses the deterministic camera mock | disabled |
| `Z1_HIL_MOCK_CONTROLLER` | Declares that the flashed firmware uses the deterministic controller-channel mock | disabled |
| `Z1_ALLOW_MUTATION` | Enables recoverable persistent changes when `1` | disabled |
| `Z1_ALLOW_DESTRUCTIVE` | Enables destructive operations when `1` | disabled |
| `Z1_HIL_OTA_IMAGE` | Valid raw ESP-IDF application image for destructive OTA HIL | unset |
| `Z1_HIL_SPIFFS_IMAGE` | Valid SPIFFS image for destructive `/updateffs` HIL | unset |
| `Z1_HIL_WIFI_SSID` | Recovery-safe network used by mutating Wi-Fi HIL | unset |
| `Z1_HIL_PREVIEW_FILE` | Existing read-only AVI below `/sd/videos` used for physical preview playback | unset |
| `Z1_HIL_WIFI_PASSWORD` | Password paired with `Z1_HIL_WIFI_SSID` | unset |
| `Z1_HIL_USB_RESET` | Permits recoverable native USB reset and re-enumeration | disabled |
| `Z1_HIL_CONTROLLER` | Declares an attached controller fixture | disabled |
| `Z1_HIL_CAN` | Declares an attached CAN fixture | disabled |
| `Z1_HIL_BLE` | Declares an available BLE scanner | disabled |
| `Z1_HIL_MACHINE_NAME` | Exact configured machine name for BLE identity verification | unset |
| `Z1_HIL_WIFI_SSID` | Test-network SSID used by mutating BLUFI provisioning | unset |
| `Z1_HIL_WIFI_PASSWORD` | Test-network password; an empty value is valid | unset |

UDP discovery listeners enable address and port reuse where the host supports
it. This allows HIL to observe port-3333 announcements while MakeraStudio is
running, without terminating or otherwise interfering with that client.

BLE reset recovery uses `Z1_HIL_SERIAL` when a diagnostic UART fixture exists.
On native-USB-only hardware it instead uses the valid `Z1_HIL_OTA_IMAGE` as a
destructive same-image reboot fixture and requires BLE disconnect, USB
disappearance/re-enumeration, and renewed advertising before passing.

Camera availability is detected through a valid `POST /api/camera/resolution`
request. The exact controlled sensor-unavailable response marks camera-dependent
HIL as skipped. Physical-image conformance additionally requires
`Z1_HIL_CAMERA=1`; the deterministic mock instead requires
`Z1_HIL_MOCK_CAMERA=1`, so the two evidence classes cannot be confused.

BLE HIL uses the host adapter through Bleak. With `Z1_HIL_BLE=1`, absence of
an `MK_`-prefixed machine-name advertisement is a failure rather than a fixture
skip. Set `Z1_HIL_MACHINE_NAME` to additionally require the exact name after
the normative 23-byte truncation. The read-only baseline validates the
conditional BWF-002 service-UUID advertisement rule, service UUID `0xffff`, characteristics
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
This is retained as historical evidence for the superseded fixed-name build,
not as validation of the current machine-named advertising contract.
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
and then requires the `MK_`-prefixed device to resume advertising after boot. Both cases
passed physically on 2026-07-30; the reset case observed the disconnect and
rediscovered the then-current fixed-name advertisement after the target booted.

Additional read-only negative-wire fixtures exercise sequence rejection with a
correct retry in the same connection, acknowledgement-before-product-response
ordering, and the exact checksum-error report. All three passed physically on
2026-07-30. The same run rechecked GATT discovery, minimum MTU/write capacity,
and the fixed outgoing read.

The updated machine-name advertising build was flashed and exercised on
2026-08-08. The fixture found the exact MAC-derived
`MK_Makera_Z1_0274` name, observed the BWF-002 service entry appropriate for
that suffix length, and passed all 14 available read-only identity, GATT,
lifecycle, error, negotiation, encryption, and fragmentation cases. The
separately gated BLE/HTTP concurrency case was skipped in that isolated run.
The mutating COM-reset fixture also passed and rediscovered the same name after
boot; serial diagnostics contained no fatal startup records.

The read-only suite also exercises repeated native USB requests, recovery after
unframed USB noise, bytewise fragmented TCP input, the four-client TCP limit,
four-client concurrency, six consecutive four-client capacity waves,
simultaneous USB/TCP commands, WLAN scanning, runtime
and serial-number reads, monotonic Wi-Fi diagnostics, concurrent HTTP requests,
bounded TCP-service recovery after a physical station scan, simultaneous
USB/TCP requests, and recovery after an interrupted multipart request. Persistent Wi-Fi changes,
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

The SD mock validates the complete FAT, VFS, POSIX, path-normalization, and
public-command stack without a physical reader. It allocates a fresh 512 KiB
block device in PSRAM on every boot and mounts it at `/sd`. It is deliberately
volatile and emits a prominent `TEST BUILD` diagnostic. The generic builder
discovers available mock switches from Kconfig and supports any combination in
the standard reusable `build/` tree:

```sh
export ESP_IDF_DIR="${ESP_IDF_DIR:-$HOME/.espressif/v5.4.4/esp-idf}"
source "$ESP_IDF_DIR/export.sh"
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

The 2026-08-01 mock-SD endurance run completed 20 upload/download/MD5/rename/
download/delete cycles with varied 1--4 KiB payloads through native USB. The
same build also retained both values from simultaneous USB and TCP
`config-set` requests. `ConfigurationFileStore` serializes the complete
read-modify-temporary-write-rename transaction because both transports share
`config.tmp`.

Runtime mutation across transports exposed a target-specific constraint:
executing NVS commands directly on a PSRAM-backed TCP client stack reset the
connection when flash access suspended the external-memory cache. TCP
`sn-get`, `sn-set`, `sys-time`, and `clearftm` are therefore dispatched through
one internal-stack worker. On-target validation then passed exact TCP
`clearftm` acknowledgement, USB observation of the erased value,
controller-mock recreation with `mock-time`, and identical TCP/USB readback.
The four-client capacity response and six repeated four-client reuse waves
also passed, proving that the safety fix did not reduce the normative TCP
capacity. Reports are retained as `build/hil-mock-runtime-config-endurance.json`,
`build/hil-tcp-nvs-worker.json`, and `build/hil-tcp-hybrid-capacity.json`.

A later aggregate run exposed cumulative connection-task exhaustion: after
many short TCP sessions, new TCP connections were permanently reset and HTTP
stopped responding while native USB remained healthy and the target had not
rebooted. Each connection previously created a PSRAM-stack task and invoked
ESP-IDF's resource-intensive `vTaskDeleteWithCaps` self-delete path. The target
now creates four permanent PSRAM-stack workers, one per normative client slot,
and dispatches accepted sockets through one-entry slot queues. This preserves
the exact four-client limit, per-connection generation identity, ownership
cleanup, and PSRAM/NVS separation without allocating FreeRTOS tasks per socket.
Target HIL passed 20 four-client waves (80 sessions), fifth-client rejection,
fragmentation, UDP `tcp-full` transitions, subsequent HTTP static assets, and a
mutating configuration request. The report is
`build/hil-tcp-permanent-workers.json`.

The final all-mock regression then completed all 94 collected cases in 1312.56
seconds: 66 PASS, 27 capability/safety SKIP, with no FAIL. The historical
`config.default`/8.3 XFAIL is no longer valid against the current SD-009
long-filename policy and must not be used for current conformance results. This
single run includes the 20-cycle
mock-SD endurance case, latched storage faults, controller and camera mocks,
80-session TCP churn, USB/TCP/UDP/HTTP concurrency, WLAN scan recovery, web UI,
and mutating configuration/storage paths. The retained report is
`build/hil-all-mock-final.json`.

The controller mock also supports `mock-transfer-timeout firmware` and
`mock-transfer-cancel firmware`. Timeout injection schedules an unused firmware
family operation after 5.2 seconds instead of sleeping in the UART callback;
this exercises LPCFW-006 and then proves that ordinary controller routing is
enabled again. Controller cancel preserves `/lpc1768.bin`, and a normal retry
still consumes it. `mock-command sn-get`, `mock-command M951`, and
`mock-command M952` inject controller-origin commands and publish `CMD:<name>:OK`
only after their framed mainboard reply returns through the mock channel. HIL
also holds a TCP upload open while completing controller `sn-get`, demonstrating
that host file ownership does not stall UART command processing. Reports are
retained as `build/hil-mock-controller-faults-final.json` and
`build/hil-mock-controller-concurrency.json`.

Mock-SD fault control is available only when the SD mock is selected:
`mock-sd fail-read`, `fail-write`, and `fail-sync` latch their respective block
device errors until `mock-sd clear`; `unmount`, `mount`, and `status` exercise
the complete VFS/FatFS lifecycle. A live build replies `mock-sd unavailable in
live build` and cannot enable these callbacks. The latched behavior is
intentional because a single sector error can be consumed by FatFS metadata or
cache activity before the public operation under test reaches its payload.

Network fault control is available only when the network mock is selected with
`--mock network` (or as one item in a comma-separated mock selection). The
native USB commands `mock-net fail-tcp-temporary`, `fail-tcp-permanent`,
`fail-discovery-open`, and `fail-discovery-send` arm one deterministic socket
boundary failure; `status` and `clear` inspect or cancel it. Run the target
checks with `Z1_HIL_MOCK_NETWORK=1 Z1_ALLOW_MUTATION=1`. They verify whole-frame
TCP retry, isolation and recovery after a permanent send error, discovery
socket recreation, and resumed periodic UDP output. Live builds expose the
command only to report that the test adapter is unavailable.

The native-USB/network run on 2026-08-09 passed all four injected cases:
temporary TCP send retry retained the complete response, permanent failure
closed only its affected session and allowed an immediate successor, and both
UDP socket-open and send failures were consumed before periodic discovery
resumed. The fixture initially filtered the correct `0x83` text response as if
it were `0xa3`; that test-only packet-type error was corrected before retaining
the passing report as `build/hil-network-fault-usb-fixed.json`.

The 2026-08-01 HIL run verified unmounted access from USB and TCP, ENODEV
diagnostic selection before any host-directory VFS fallback, rejected upload,
fresh remount and transfer recovery, deterministic 16 KiB read failure,
failed FAT write and sync operations, cleanup, reformat, and successful reuse.
The report is retained as `build/hil-mock-sd-faults.json`.

A combined regression on 2026-08-01 exercised an older timer-based HFT-022
implementation and taught the HIL driver to tolerate a retry while completing
the final block. The 2026-08-19 clarification replaced that timer with combined
10 ms receive-cycle accounting, so the old retry timing is retained only as
historical evidence. Duplicate delivery remains safe through sequence handling.
The earlier report is `build/hil-mock-sd-retry-fix.json`; refreshed timing HIL
is pending.

The all-mock camera profile was target-built and OTA-installed on 2026-08-01.
HIL completed five resolution changes spanning frame-size values 1 through 15;
each cycle opened `/ws_video`, received three exact deterministic JPEG frames,
sent `stop_stream`, disconnected, and verified TCP recovery. A second-client
test retained two simultaneous WebSockets, observed exact `vlive` preemption
JSON on the former owner after any already-queued JPEGs, streamed from the new
owner while HTTP Wi-Fi diagnostics and TCP `sys-time` ran concurrently, then
closed both sockets and admitted a successor stream. The report is retained as
`build/hil-mock-camera-lifecycle-final.json`. This is simulator evidence, not
physical sensor conformance.

Recording cannot yet be positively exercised even with both mocks enabled.
At the time of this historical run, REC-001 generated
`/sd/videos/session-YYYYMMDD_HHMMSS.avi` while the then-current SD-009 disabled
long-file-name support. The updated SD-009 now requires names through 255
characters and removes that specification conflict. The recording task also
did not create `/videos`. That run remains historical evidence; the former
normative filename conflict is no longer a current blocker and the recording
case should be rerun against a firmware built from the revised defaults.

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
or controller timing. The controller-originated firmware, configuration, and
factory-transfer handshakes are covered separately with mock SD.

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
ESP-IDF disconnect reason, the current boot's numeric ESP-IDF reset reason, and
the bounded persistent Wi-Fi event log. The reset reason distinguishes a target
reboot from a host-only USB or network interruption after connectivity returns.
It does
not return saved credentials or BSSID. The HTTP HIL suite validates the schema
and sensible radio ranges whenever the endpoint is detected.

## Current fixture coverage

The executable suite covers native USB descriptors, framing recovery and
repeated round trips, TCP listener/capacity/concurrency/framing, cross-transport
operation, HTTP/WebSocket concurrency and interruption recovery, Wi-Fi scan and
diagnostics, runtime reads, an explicitly gated recoverable filesystem
operation, native USB reset, Wi-Fi persistence, destructive delayed-chunk OTA,
and SPIFFS replacement. SD tests require an explicitly declared reader/card because protocol
completion alone cannot prove a mount. Physical controller
version/status/diagnostic reads, BLE, camera detection, and camera streaming
now have dedicated fixture coverage. CAN, RF-control, motion, and recording
still need controlled fixture drivers; unavailable capabilities remain skips.

On 2026-08-19 a complete Makera Z1 fixture passed six new read-only cases:
physical controller version/status/diagnostics plus ten repeated queries,
physical `/sd/config.txt` listing/download with MD5 and identical USB/TCP
content, real JPEG streaming across disconnect/reconnect, and simultaneous
camera, HTTP, USB, and controller reads. The tests issue no motion, spindle,
laser, file-write, configuration-write, or update commands.

The same machine then ran the complete read-only selection against its installed
Makera factory firmware `0.1.13`: 41 passed, 7 failed, 7 skipped, and 60 were
deselected. WEB-020 now specifies the observed HTTP 400 response, so this is no
longer a confirmed specification difference. BLUFI error 8 is permitted by BLESEC-002;
the Wi-Fi diagnostics endpoint and project Web UI asset markers are local
extensions; and the UDP bind failure passed on isolated rerun. See
[`factory-firmware-spec-differences.md`](factory-firmware-spec-differences.md).
These results validate the fixture, not a repository-built firmware image.

The factory camera image was also used as an A/B diagnostic baseline on
2026-08-19. The extracted raw application from
`binaries/firmware-1.1.2.0.1.13.bin` delivered a valid physical OV3660 JPEG,
proving the sensor, cable, and fixture before the repository image was restored
by OTA. The repository release image then passed 2/2 physical camera cases,
including disconnect/successor streaming and simultaneous HTTP, native USB,
and controller reads. Three repeated BLE/native-USB/eight-client-HTTP/Wi-Fi
diagnostic load runs passed. The final configuration preserves BOOT-012 USB
startup ordering, permits ordinary allocations in PSRAM, reserves 16 KiB of
internal DMA heap until the first LIVE-010 camera initialization, and bounds
each camera DMA allocation to 8 KiB.

The final isolated physical regression completed with 35 read-only passes,
27 mutating passes, and two destructive passes, followed by all individually
isolated BLE cases. Eight read-only and 22 mutating cases were capability-gated
for unavailable COM, CAN, or mock-only fixtures. Five separately gated native
USB reset/disconnect/upload-resume cases passed 5/5, and the partial-image OTA
timeout case passed on isolated rerun. A missing physical `/sd/config.txt` was
restored from the byte-identical 5791-byte backup before its two read-only SD
cases passed; this restoration was test-fixture repair rather than generated
test data.

The factory baseline was extended on 2026-08-19 with physical SD removal and
reinsertion while native USB remained connected. Absent-card listing produced
only FILE-015 completion, file type became `nc`, and MD5/download failed while
USB, controller, HTTP, video, and TCP stayed available. Reinsertion remounted
without restart and restored the byte-identical 5791-byte `config.txt` with MD5
`f55bf8ae0242dd735b79580b7cdb3d5c`. An operator-selected existing AVI then
passed preview open/meta and three indexed JPEG frames. Five media/controller
cycles passed 25 physical executions, ten transport-stress cases passed in
110.04 seconds, and ten isolated native-USB resets all recovered. The detailed
factory-only evidence remains in
[`factory-firmware-spec-differences.md`](factory-firmware-spec-differences.md).

A later safe rerun against the still-installed factory image used physical USB,
Wi-Fi, BLE, SD, camera, and controller fixtures. The non-BLE read-only profile
passed 30 cases, skipped eight unavailable/optional fixtures, and exposed five
known factory/local differences: the local diagnostics endpoint, project WebUI
markers, normative M482/M483 labels, and the camera ownership recovery window.
Thirteen isolated BLE cases passed; protected-status and concurrent protected
status stopped at the already documented, specification-permitted BLUFI error
8. Six of seven unique-name SD mutations passed, and the seventh cleaned its
data and sidecar despite an omitted cancel reply. Same-credential USB Wi-Fi
provisioning passed. No OTA, SPIFFS, partition, motion, recording, CAN, or
configuration-file mutation was attempted. Physical controller verification
now ignores unrelated retained `0x90` console output and passed both tests on
rerun.

On 2026-08-09 an all-mock image was flashed from `build-hil-all`. COM reset and
boot diagnostics passed 2/2. The applicable native-USB mutating suite produced
44 PASS and three capability-gated SKIP; the old SD-009/CFG-001 XFAIL is
historical and is no longer part of current conformance results;
the remaining 30-case storage, controller, NVS, Wi-Fi, and USB group passed
without failure. Destructive HIL then installed the generated 1 MiB SPIFFS
image, verified runtime/serial/Wi-Fi persistence across a same-image OTA reboot,
and completed an OTA whose multipart body paused for seven seconds. All three
destructive cases passed. These results are mock-adapter and real ESP32-S3
transport/flash evidence; they do not claim a physical SD card, controller,
camera, or CAN bus.

The destructive update fixtures no longer assume that the scheduled reboot is
complete after a fixed three-second delay. They observe the selected TCP
service become unavailable and then return before a subsequent test may start.
This closes the old-boot race and verifies an actual reboot edge; delayed OTA
additionally requires a responsive newly enumerated native-USB endpoint.

The first 20-cycle USB/mock-SD endurance run failed after approximately eight
minutes with macOS/libusb `errno=5` on one bulk write. The device remained
enumerated and immediately answered `version` through a fresh handle. A clean
full rerun passed in 8:59, so the event is retained as intermittent host-handle
evidence rather than classified as a firmware deadlock. The HIL client now
reports the failed operation, backend codes, stale-handle candidacy, and exact
cycle/file/block context. It also uses a 100 ms post-frame quiescence window
while retaining the full initial response deadline; the unchanged 20-cycle
test passed in 3:09.

File-transfer phases now bypass that general quiescence window once their
protocol-defined terminal frame has been decoded. Unstructured command replies
retain the 100 ms window. On the same 2026-08-09 fixture, the unchanged
20-cycle endurance case fell from 191.45 to 156.73 seconds, and one unchanged
large-frame round fell from 83.05 to 66.00 seconds. The remaining time is
dominated by actual framed USB/file-worker traffic rather than fixed sleeps.

Extended USB stress then passed three rounds of upload/download for 63, 511,
2048, and 8192-byte blocks with files up to 64 KiB. Three separate 12 KiB,
three-block uploads reset the USB bus after block one; each re-enumerated link
continued from block two, completed block three, downloaded identical bytes,
and cleaned up. Repeated control requests and recovery after unframed noise also
passed. Reports remain local under `build/hil-*.json`.

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

Persistence was validated on 2026-07-31 with a destructive same-image OTA
reboot. The fixture erased only `runtime/first_boot`, observed an immediate
`null`, recreated it with a controller-originated time command, and captured
the persisted counters and serial-number response. After OTA and automatic
station recovery at the same address, first boot and machine time were
unchanged, power-on time had not decreased, identity output was identical, and
Wi-Fi diagnostics showed a completed association and acquired address. This
run exposed and corrected an adapter bug where a missing string namespace was
reported as an NVS failure rather than the required missing value; target
readback now returns `sn = null`. Forced NVS driver errors still require an
instrumented persistence adapter and are not claimed by this physical run.

The generated 1 MiB `build/spiffs.bin` was installed through `/updateffs` on
2026-07-31 and survived the required reboot. The target then served the UI
index, CSS, and JavaScript with their exact MIME types, enumerated a fixture
MAINBOARD setting through `GET /api/config`, persisted its update through
`POST /api/config`, and rejected a newline-bearing value. This run found and
corrected both a dangling `Content-Type` pointer in chunked static responses
and the missing CFG-031 unlink before FAT rename. Eight repeated waves of HTTP,
USB, and TCP requests ran together, and both periodic UDP discovery and exact
four-client capacity transitions passed. The in-app browser runtime reported
no available browser backend, so visual/click automation remains pending even
though the installed browser resources and their live APIs passed HIL.

The web volume was refreshed on 2026-08-01 with an explicit current-settings
view. Existing MAINBOARD keys are sorted, marked as existing, and protected
from accidental rename while their values remain editable. New rows are marked
separately, unsaved changes are counted, unchanged records are not posted, and
reload retrieves the authoritative device state. The generated 1 MiB image was
installed through `/updateffs`; after reboot, updated HTML/CSS/JavaScript and
the live configuration read/update/persistence flow passed 2/2 HIL cases in
`build/hil-webui-existing-settings.json`. Browser discovery again returned no
available backend, so this run does not claim visual automation.

The complete mutating collection was also used as an endurance probe. It
exposed a host-fixture defect: session-scoped PyUSB handles remained invalid
after an intentional bus reset and caused unrelated later failures. USB and SD
clients are now function-scoped, explicitly release libusb handles, and all
three reset/re-enumeration cases pass consecutively. A subsequent eight-minute
mixed run passed 30 cases and skipped five unavailable physical fixtures, but
seven later HTTP/TCP cases timed out intermittently. The isolated HTTP, TCP,
UDP, mock-controller, and UI groups have passed; this long mixed-run result is
retained as an open RF/resource-endurance observation rather than being
misreported as conformance evidence.

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
for six seconds and completed after the then-implemented timer retry. The
current clarified test instead requires no retry during that bounded silence.
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

On 2026-08-01 the mixed transport baseline reproduced the open endurance
symptom: nine cases passed before HTTP concurrency timed out, after which the
station services were unreachable and stale native-USB handles reported I/O
errors. A host USB bus reset restored an immediate `ftype /` round trip without
restarting the application, while Wi-Fi remained disconnected. The target RF
scan still found `Away` at -51 dBm, but a complete manual connection attempt
ended with the exact terminal failure `ESP-IDF disconnect reason=4`
(`WIFI_REASON_ASSOC_EXPIRE`/inactivity). This separates current RF association
failure from the earlier TCP slot-capacity fix.

That historical run treated `wlan -s` as a save-only command. The current
specification no longer defines that option: NET-041 recognizes only `-d` and
`-e`, and successful `wlan <ssid> <password>` association persists credentials
under NET-012. The obsolete save-only HIL case has therefore been removed; the
remaining mutation test requires the normative IP and terminal responses plus
HTTP recovery. With Wi-Fi unavailable, ten independent native-USB and
mock-SD cases passed in 209.78 seconds, including repeated requests, malformed
input recovery, file mutations, MD5/cache behavior, logging, cancellation, and
mock-volume exhaustion/recovery. The report is
`build/hil-usb-mock-sd-offline.json`.

The remaining USB transmit fault cases were then moved out of the FreeRTOS task
into `UsbTransmitDrain`, which is the same state machine now composed with the
live TinyUSB endpoint. Its instrumented host port verifies short accepted
writes, flushing every accepted fragment, pop only after whole-frame completion,
strict expiry after more than 500 ms without progress, recovery by draining the
next queued frame, and retaining/restarting the front frame across disconnect.
The queue test also runs four concurrent producers and proves that exactly 30
frames are admitted. This closes the production-composition gap without
claiming that a physical host can force TinyUSB's internal FIFO to stall. Host,
24 tool tests, and the ESP-IDF `sd,controller` mock build passed; the resulting
image is 0x1a6d60 bytes with 17 percent free in each 2 MiB OTA partition.

Controller HIL was made independent of station networking on 2026-08-01 by
routing its status, diagnostic, version, and test-transfer commands through the
native USB protocol fixture. The existing snapshot and all-three-transfer cases
both passed over USB while Wi-Fi remained unavailable. A new test-only command
can inject a one-byte, invalid geometry packet, allowing the target to verify
cancel, preservation of the staged firmware, and a subsequent successful retry.
The updated `sd,controller` image builds successfully at 0x1a6e20 bytes. It was
flashed over COM and boot-checked before moving back to application USB. The
malformed-geometry case passed in isolation, and the subsequent complete
controller group passed all three mock cases: retained snapshots, all three
normal transfer families, and cancellation followed by successful retry. The
external physical-controller declaration remained correctly skipped. Reports
are `build/hil-controller-malformed-geometry.json` and
`build/hil-controller-usb-complete.json`.

The TCP/UDP transport refresh on 2026-08-01 exposed a fixture-ordering issue:
the discovery capacity case opened four host sockets without first proving that
each had been admitted, and it could begin while client tasks from the preceding
stress case still owned slots. The fixture now waits beyond the normative
receive window and requires a normal framed response from every retained client
before checking discovery. After a same-image OTA reset, the isolated transition
and the complete sequential ten-case transport suite passed. Coverage includes
USB repetition and noise recovery, fragmented TCP input, exact fifth-client
rejection, four-client concurrency, six slot-reuse waves, service recovery, and
the discovery transition `tcp-full=0→1→0`. The complete report is
`build/hil-transport-full-after-fixture-fix.json`.

The same transport coverage was refreshed after the controller fault-injection
image was installed on 2026-08-01. One initial USB/TCP coexistence attempt saw
a TCP connection timeout while HTTP remained reachable, but TCP recovered
without a target reset. Ten immediate repetitions of the coexistence case then
passed. A sequential transport run also passed all ten USB, TCP, and UDP cases
in 77.92 seconds, including four concurrent clients, fifth-client overflow,
six capacity-reuse waves, fragmented input, and discovery `tcp-full=0→1→0`.
The first parallel host invocation was discarded because two pytest processes
contended for the single native USB interface. The retained report is
`build/hil-tcp-stress-sequential.json`; the isolated timeout remains motivation
for a longer endurance run rather than evidence of a persistent service loss.

Cross-transport mock-SD HIL on 2026-08-01 then added persistent configuration,
filesystem mutation, and file-owner contention coverage. The run exposed and
corrected a target wiring bug where both USB and TCP passed the complete
`config-get`/`config-set` command to services expecting only the recognized
argument slice. After OTA installation, USB and TCP mutations observed the same
configuration bytes; TCP mkdir/move/remove effects were independently verified
through USB; and an active USB upload rejected TCP before cancel released the
owner for a successful persistent-socket TCP upload. Three cases passed. The
fourth was an explicit XFAIL under the former SD-009 short-name policy,
while CFG-001 requires `/sd/config.default`, whose seven-character extension
could not be created by that former FAT policy. The current SD-009 allows the
name, so the test is now a positive conformance case. No implementation policy
was changed to hide the conflict in that historical run. The report is
`build/hil-mock-cross-transport-final.json`.

The inverse ownership case passed separately: a persistent TCP upload retained
the global file owner while USB received the exact limit response, then TCP
cancel released ownership and USB immediately completed an upload/download.
Together the two directions verify symmetric arbitration rather than only one
preferred transport. The report is `build/hil-mock-ownership-reverse.json`.

Mock-SD file-transfer error HIL was extended on 2026-08-01 with three cases.
A persistent TCP upload accepted block one, consumed 51 duplicate block-one
packets, repeated its outstanding block-two request, and produced exact bytes
when independently downloaded over USB. A download canceled by the USB host
returned the exact terminal message, preserved its source, released ownership,
and allowed an immediate successor upload/download. Finally, both absent and
malformed MD5 sidecars advertised the required fallback digest while retaining
the original data; a normal re-upload recreated a valid sidecar and restored
end-to-end digest verification. All three passed in 93.56 seconds. The counter
injection deliberately uses one persistent TCP task rather than conflating
sequence behavior with USB worker-queue pressure. The report is
`build/hil-mock-transfer-errors-final.json`.

NVS boundary fault control is available only in a build selected with
`--mock nvs` or `--mock-all`. `mock-nvs fail-open` rejects namespace access,
`mock-nvs fail-commit` rejects mutations before flash commit, and
`mock-nvs clear` restores normal operation. The persistent NVS backend is not
replaced. HIL additionally selects the controller mock to create a known
`runtime/first_boot` value, verifies exact protocol failures, clears the fault,
and requires the original value to remain readable. Run the gated case with
`Z1_HIL_MOCK_NVS=1`, `Z1_HIL_MOCK_CONTROLLER=1`, and
`Z1_ALLOW_MUTATION=1`.

This fixture passed on native USB on 2026-08-09 after exposing an ESP-IDF NVS
transaction detail: closing a handle does not discard an uncommitted mutation
from the shared in-memory cache. The adapter therefore consumes an injected
commit fault before calling `nvs_set_*` or `nvs_erase_key`, while retaining the
post-mutation guard for races. The test confirmed that both injected open and
commit failures are reported, the prior `runtime/first_boot` value remains
unchanged, and normal access resumes after clearing the fault. The report is
`build/hil-nvs-fault-usb-fixed.json`; the flashed image also passed the two-case
reset diagnostics in `build/hil-nvs-fixed-boot.json`.

The all-mock regression collected 94 cases on 2026-08-01. Its first pass
recorded 62 PASS, 27 capability-gated SKIP, one expected configuration-name
conflict, and four failures. Two storage failures shared the HIL retry defect
described above. A manual WLAN reconnect then exposed a real polling race:
ESP-IDF could reach `address_ready` before the 100 ms policy poll observed the
transient `associated` state, producing a false ten-second timeout and a
dependent TCP failure. The portable policy now treats an assigned address as
proof of prior association. Host regression, target build and OTA installation
passed; the formerly failing WLAN association and simultaneous USB/TCP cases
both passed in `build/hil-wifi-fast-reconnect.json`. The initial aggregate
report remains `build/hil-all-mock-full.json` as failure evidence; corrected
storage evidence is retained separately rather than rewriting that result.

Additional target tests close two portable-to-target gaps. Four simultaneous
TCP clients now issue recording, serial-number, runtime, and status commands;
all receive the exact response type and payload for their own origin before a
fifth cleanup command verifies slot reuse. A second case sends a TCP-originated
command across the controller TX bridge, consumes the mock's deliberately
fragmented controller-origin frame, runs the mainboard serial service, and
observes the framed UART reply in controller diagnostics. The report is
`build/hil-tcp-local-routing.json`. Download timing is also exercised through
the real ESP timer: ten seconds of inactivity produces the exact terminal
timeout, preserves the source, releases global ownership, and permits a new
upload/download. Separate malformed-protocol injection sends 51 empty data
frames to exercise the exact abort boundary. A separate sequence-zero request
against the mock's ignored seek returns the current content before an explicit
cancel, so both the normative offset behavior and the subsequent source
download pass. Reports are `build/hil-download-timeout.json`
and `build/hil-download-errors.json`.

An alternate local Web UI trace was analysed on 2026-08-08 without adding its
copyrighted files or paths to the repository. The UI attempted an undocumented
WebSocket connection on port 81, while the implemented and specified video
WebSocket on port 82 upgraded successfully. No port-81 endpoint was added
because its protocol is unknown and WEB-001/WEB-003 only define ports 80 and
82. The observed MIME types and 256-byte static-file chunks were also retained:
changing either would contradict WEB-011/WEB-012. An opt-in large-asset HIL
check now accepts a private path through `Z1_HIL_STATIC_ASSET` and a bounded
timeout through `Z1_HIL_STATIC_ASSET_TIMEOUT`. The committed implementation
served the complete local asset in 10.71 seconds; the report is
`build/hil-static-baseline.json`. A trial that combined multiple HTTP chunks
into larger transport writes transferred only 11,214 of 106,343 bytes in 15
seconds, so that unproven implementation was removed before commit.

Portable coverage was regenerated on 2026-08-19 after the specification-alignment,
streamed-play resource/diagnostic, controller-transfer allocation, and listing
allocation work. All 853 host tests passed with 96.20 percent line (9286/9653),
98.52 percent function (931/945), and 86.69 percent branch (3835/4424)
coverage; all 55 Python tooling tests also passed. The larger denominator now
includes the subsequently added production policies and public inline methods,
so the percentages are not directly comparable to the earlier 8151-line
snapshot. USB production policy remains at 100 percent line coverage for
protocol state, receive staging, transmit drain, and timeout tracking; the
remaining USB limitation is physical TinyUSB endpoint control. The generated
report remains local under
`build/host-coverage/coverage/`; release automation publishes the equivalent
report and badge.

The largest uncovered portable source is the play controller at 35 lines;
several are the newly added invariant and invalid-cache corruption diagnostics,
which valid public operations deliberately cannot trigger. Smaller reasonable
future targets are BLUFI wire/fragment errors, runtime-counter failures,
diagnostic-log recovery, and discovery boundaries. AVI and CAN PDO/TPDO retain
defensive bounds and dictionary-corruption guards that cannot be produced
through their validated public APIs. These coverage limits are separate from the
remaining conformance limits: physical SD media, controller UART, CAN bus,
camera behavior, RF-loss endurance, and forced TinyUSB endpoint stalls still
require their corresponding hardware fixtures or lower-level instrumentation.

On 2026-08-19 the refreshed ESP32-S3 mock profile used live native USB, BLE,
and station Wi-Fi with mock SD, camera, controller, and NVS. Read-only HIL
passed 33 cases with ten explicitly unavailable physical-fixture/static-asset
skips. The complete mutating group passed 44 cases in one 328.03-second
process, with five explicitly unavailable serial/network-fault fixture skips.
This includes runtime/storage/transfer endurance, three USB-reset upload
continuations, NVS, controller, filesystem, routing, SoftAP, and web
configuration. Four destructive
non-BLE cases passed, covering interrupted multipart recovery, SPIFFS install,
partial OTA finalization/rollback, and persistent same-image OTA. All 15
read-only BLE cases and encrypted provisioning passed in isolated processes;
the reset case additionally passed through same-image OTA, BLE disconnect,
native-USB disappearance/re-enumeration, and renewed advertising.

`tools/run_hil_isolated.py` gives aggregate read-only, mutating, and destructive
groups exact pytest directory selections, then runs every BLE node in its own
process. A regression test verifies that a BLE node cannot accidentally be
combined with the broad `tests/hardware` selection. USB-reset fixtures release
and rediscover macOS/libusb handles between post-reset packets, and failed OTA
re-enumeration probes dispose every candidate handle before retrying.

That run exposed two target resource defects rather than test timeouts. A 1 ms
controller-consumer delay rounded to zero at the configured 100 Hz FreeRTOS
tick and starved startup before Wi-Fi/USB; the consumers now use the normative
10 ms cycle and PSRAM stacks. Repeated maximum-size USB frames also triggered a
panic reset with the shared 8192-byte worker budget. Receive/decode now uses a
dedicated 16384-byte stack, file/FAT processing uses 12288 bytes, and local
commands retain 8192 bytes. The formerly panicking 12-case endurance sequence
then passed in 207.73 seconds.

The HIL transfer driver was aligned with the revised specification during the
same run: upload start is silent and MD5-first, completion waits for the `0x90`
ownership-release event, HFTU-024 recovery never duplicates a start, and
download output omission uses HFTD-005/HFTD-008 B1/B2/B6 repetition. TCP slot
reuse now accepts a retained prior response under TRN-004. Sequence zero uses
the HFTD-006 modulo offset and, when the mock seek fails, continues from the
existing position rather than requiring the obsolete immediate-abort behavior.

The GPIO0 heartbeat is split at the same production boundary: portable tests
verify the initial high level, exact 1000 ms delay, repeated inversion, and no
side effects after configuration failure. The ESP-IDF adapter retains the
specified push-pull output configuration with both internal pulls and GPIO
interrupts disabled. A target build verifies composition; electrical level and
one-second wall-clock accuracy still require a probe or oscilloscope.

The first physical boot of that refactor exposed a heartbeat-task stack
overflow with the inherited 2048-byte allocation. The task now has a documented
4096-byte stack budget. After rebuilding and reflashing over COM, both serial
discovery and the mutating reset/healthy-boot diagnostic passed without panic,
watchdog, assertion, or stack-overflow output. The report is retained as
`build/hil-heartbeat-boot-fixed.json`; it proves healthy composition but not the
electrical waveform.

## Physical USB cable reconnect recovery (2026-08-20)

Both factory firmware 1.1.2.0.1.13 and the repository firmware could leave the
Z1 native USB port absent after unplugging and reconnecting its cable. Wi-Fi and
controller communication remained alive, and restarting the whole ESP restored
USB, identifying recovery of the USB peripheral rather than a machine reboot as
the safe target. A machine reboot is specifically unsuitable because G-code is
streamed to the controller and a paused job must remain an active machine state.

The target now arms a portable reconnect scheduler from TinyUSB suspend/unmount
callbacks. A normal FreeRTOS worker waits one second, then drives the ESP32-S3
DWC `USB_SRP_BVALID_IN_IDX` low for 100 ms and high again. It retries at a
bounded interval until mount/resume cancels the schedule. Callback code neither
delays nor changes registers, and the recovery never restarts the ESP,
controller link, playback session, or paused/running job. Portable tests verify
the delay, low/high ordering, cancellation, retry, and re-arming behavior.

The release image `build-usb-bvalid-recovery/mainboard_firmware.bin` was
1,480,624 bytes and fitted the normative 0x190000-byte OTA partitions with
0x26850 bytes free. It was installed through `/update`; the expected USB bus
absence and fresh application enumeration both occurred after the scheduled
OTA reboot. Two subsequent manual five-second cable unplug/replug cycles each
re-enumerated as VID:PID `303a:4002`. After both cycles, the two native USB
read-only HIL cases passed, `sn-get`/`diagnose` replied, HTTP and TCP remained
reachable at `192.168.8.196`, and controller status remained idle. Wi-Fi
diagnostics retained `station_starts=1`, `associations=1`, and
`disconnections=0`, proving that neither physical cycle restarted the ESP or
network service.

## Pending-fixture reduction campaign (2026-08-20)

The physical-evidence matrix was audited row by row and its 94 genuine pending
fixtures were classified by actionability. After the Z1 returned online, a
focused read-only campaign passed 12/12 SoftAP/Wi-Fi query, runtime, USB, TCP
fragmentation, four-client capacity, fifth-client rejection, slot-reuse, and
recovery cases. The report is
`build/hil-pending-readonly-audit.json`.

A recoverable-mutation campaign then passed 13 cases and skipped only the
mock-only full-volume injection. It physically covered SD upload/download,
MD5, rename/delete, G-code cache mapping, cancellation cleanup, network-pause
continuation, download timeout/error recovery, SoftAP mutation/restoration, and
the configuration API. The report is
`build/hil-pending-mutating-audit.json`.

That campaign exposed a fixture-safety defect: the configuration API test
removed `/sd/config.txt` instead of restoring the pre-test file. The original
5791-byte backup was immediately restored and verified with SHA-256
`1dd558bd52ab15f025c0ac96bddda0d76766afbec2b0efccc2ddabdf1379e27a`.
The test now captures the original bytes, restores them in cleanup, and verifies
the restored download. Its isolated rerun passed and retained the same digest;
the report is `build/hil-config-restore.json`.

## DIAG-025 release-log verification (2026-08-20)

The first physical custom-data check proved BLUFI frame acceptance but found no
mandatory informational records in the enabled SD diagnostic mirror. Release
logging globally defaults to WARN for size, so the target adapter was changed
to retain and enable only the normative `APP_BLUFI` and `Custom Data` INFO tags.

Clean standard and compact release builds both produced a `0x169830`-byte
application with `0x267d0` bytes free in the specification's `0x190000` OTA
slots. The standard image was installed through `/update`; HTTP completion,
USB bus absence, and fresh USB enumeration all passed. The physical DIAG-025
case then passed with the exact length and lowercase hexadecimal records in
`/sd/serial.log`. Its report is `build/hil-diag025-fixed.json`.

## Physical live-video preemption (2026-08-20)

Two real-camera WebSocket clients exposed that the second owner could stream
while the first never received its mandatory MEDIA-003 record. Live JPEG and
arbiter control sends could overlap on the old socket. The target now serializes
those sends with a bounded mutex. After OTA installing the `0x169890`-byte
release image, the physical test passed with real JPEGs and the exact compact
old-owner JSON. The report is `build/hil-pending-media-preemption.json`.
