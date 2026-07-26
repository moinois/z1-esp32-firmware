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
```

| Variable | Meaning | Default |
|---|---|---|
| `Z1_HIL_HOST` | Target IPv4 address for TCP/HTTP tests | `192.168.4.1` |
| `Z1_HIL_SERIAL` | Diagnostic serial device | uniquely detected USB modem |
| `Z1_HIL_SD` | Declares that a physical SD reader and card are installed | disabled |
| `Z1_ALLOW_MUTATION` | Enables recoverable persistent changes when `1` | disabled |
| `Z1_ALLOW_DESTRUCTIVE` | Enables destructive operations when `1` | disabled |
| `Z1_HIL_CONTROLLER` | Declares an attached controller fixture | disabled |
| `Z1_HIL_CAN` | Declares an attached CAN fixture | disabled |
| `Z1_HIL_BLE` | Declares an available BLE scanner | disabled |

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
listener acceptance, framed read-only TCP and USB `ftype` round trips, native
USB descriptor/endpoint checks, and SD root listing through USB. TCP first exposed
a real `tcp_client` stack overflow; the client-task stack was increased from
4096 to 8192 bytes, the firmware was rebuilt and reflashed, and the same checks
then passed. USB and SD were verified after moving the cable from COM to native
USB. The attached camera failed its boot probe with
`ESP_ERR_NOT_SUPPORTED` and is not conformant yet.

The mutation-enabled USB run on the same date exposed incorrect command-
argument wiring, which was corrected and reflashed. Its remaining `mkdir`
failure is not valid filesystem evidence because this board may have neither
an SD reader nor a card. The earlier directory-list `PASS` was also invalid:
the protocol's completion-only response does not prove a mounted volume. Both
results are treated as fixture-unavailable rather than requirement evidence.

## Current fixture coverage

The initial executable suite covers native USB descriptors and read-only round
trips, TCP listener/read-only round trips, SD directory access, an explicitly
gated recoverable filesystem operation, and diagnostic-port discovery.
Controller, CAN, BLE, camera, RF association, recording, and OTA need dedicated
fixture drivers before they can produce physical evidence. Capability-gate
tests document missing controller, CAN, and BLE fixtures as skips.
