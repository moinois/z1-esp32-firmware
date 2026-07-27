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
| `Z1_ALLOW_MUTATION` | Enables recoverable persistent changes when `1` | disabled |
| `Z1_ALLOW_DESTRUCTIVE` | Enables destructive operations when `1` | disabled |
| `Z1_HIL_OTA_IMAGE` | Valid raw ESP-IDF application image for destructive OTA HIL | unset |
| `Z1_HIL_CONTROLLER` | Declares an attached controller fixture | disabled |
| `Z1_HIL_CAN` | Declares an attached CAN fixture | disabled |
| `Z1_HIL_BLE` | Declares an available BLE scanner | disabled |

Camera availability is detected automatically through a valid
`POST /api/camera/resolution` request. The exact controlled sensor-unavailable
response marks camera-dependent HIL as skipped; no environment declaration is
required.

Native USB is detected by VID `0x303a` and PID `0x4002`. TCP detection opens a
connection to port 2222. The current public directory command cannot
distinguish an empty mounted SD card from a missing mount because both end with
the same completion frame. SD tests therefore require `Z1_HIL_SD=1` in
addition to USB detection; set it only when a reader and card are installed.

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

The executable suite covers native USB descriptors and read-only round trips,
TCP listener/read-only round trips, HTTP/WebSocket behavior, Wi-Fi diagnostics,
an explicitly gated recoverable filesystem operation, and destructive delayed-
chunk OTA. SD tests require an explicitly declared reader/card because protocol
completion alone cannot prove a mount. Controller, CAN, BLE, RF-control, and
recording still need dedicated fixture drivers. Capability-gate tests document
missing controller, CAN, BLE, and camera fixtures as skips; the camera gate
probes the firmware automatically.
