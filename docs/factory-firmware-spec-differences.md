# Factory firmware comparison with the current specification

## Scope

This report compares the firmware installed on a physical Makera Z1 with the
current local specification at commit
`4c0c3f898d9d86616c39c5bf02db1976d274af37` (2026-08-11). The device reported:

```text
Firmware version: 0.1.13
Build date:       2026.08.05
ESP-IDF:          v5.4.1
Machine name:     Makera_Z1_010356
Station address:  192.168.8.196
```

The read-only physical regression completed with 41 PASS, 7 FAIL, 7 SKIP, and
60 deselected cases. The machine exposed physical USB, Wi-Fi, BLE/BLUFI, SD,
camera, and motion-controller communication. The JSON evidence is generated
locally as `build/hil-physical-readonly-20260819.json` and is intentionally not
treated as evidence for a repository-built firmware image.

## Confirmed specification difference

### WEB-020 — invalid camera-resolution JSON status

The request:

```http
POST /api/camera/resolution HTTP/1.1
Content-Type: application/json

not-json
```

produced:

```http
HTTP/1.1 400 Bad Request
Content-Type: text/html

Invalid JSON
```

WEB-020 requires HTTP 500 with the exact body `Invalid JSON`. The body matches,
but the status code does not. This is a confirmed behavioral difference between
the installed factory firmware and the current specification.

## Observed test differences that are not specification violations

### BLUFI security negotiation

The factory firmware returned the BLUFI error-information frame
`49 04 00 01 08` for the test Diffie-Hellman parameter set. Error value `8`
means public-key generation failure. BLESEC-002 explicitly permits error 8 for
that failure, so a test that requires this particular parameter set to succeed
is stricter than the specification. The result was reproduced in an isolated
run and is not classified as factory-firmware nonconformance.

### Wi-Fi diagnostics HTTP endpoint

`GET /api/wifi/diagnostics` returned HTTP 404. The current specification does
not define this endpoint or `NET-DIAG-001`; these are local implementation and
diagnostic extensions. Its absence in factory firmware is therefore not a
specification difference.

### Configuration Web UI assets

The factory firmware serves Makera's bundled Web UI rather than this project's
`webui` assets. WEB-011 specifies static-file path/open/error behavior, not the
HTML, CSS, or JavaScript content used by this project's configuration UI. The
asset-marker failure is expected and is not a normative difference.

### UDP discovery capacity rerun

The combined suite encountered local UDP bind error `EADDRINUSE` when reopening
port 3333. The capacity test passed when rerun in isolation. This was host test
infrastructure contention, not observed firmware behavior.

## Expected skips

The run skipped COM diagnostics while native USB was connected, mock-only
controller and camera cases, the unavailable CAN fixture, and an optional
private static-asset check. None of these skips is conformance evidence.

## Reversible pre-installation baseline

Additional USB-only checks were run before replacing the factory image. They
were restricted to changes whose original value or complete cleanup could be
verified:

- the 106343-byte `/js/BSO9Vk.js` factory asset completed without an HTTP
  transport stall;
- a unique directory could be created, listed, and removed below `/sd`;
- a normative 16384-byte USB upload to `/sd` completed, downloaded
  byte-identically, and left no target or MD5-sidecar artifact after cleanup;
- a TCP upload survived a socket disconnect when the host reconnected on the
  same logical slot and resent the outstanding block from the acknowledged
  boundary, then completed byte-identically and was removed;
- SoftAP SSID, password, and enable state were read before mutation, changed,
  restored, and re-read successfully;
- native USB recovered after a bus reset and discarded a deliberately partial
  receive frame across a second reset;
- physical camera streaming, HTTP, USB, and controller reads remained usable
  together after station recovery; and
- `/sd/config.txt` remained byte-identical to the pre-test backup: 5791 bytes,
  MD5 `f55bf8ae0242dd735b79580b7cdb3d5c`.

The factory firmware resolved absolute `/name` paths against the filesystem
root, as HFT-004 specifies, and required `/sd/name` to address the card. The
project formerly added a user-path sandbox that mapped host paths beneath
`/sd`; those historical sandbox-specific physical tests are not valid factory
baseline cases without changing their paths.

The factory upload followed HFTU-003: the host sent `0xb1` immediately after
the accepted start and the firmware replied with `0xb2`. The repository HIL
upload helper currently also accepts a non-normative target `0xb1` prompt before
sending the host MD5 packet, so it was not used for this factory upload.

An obsolete HIL case sent `wlan -s`, but the current NET-041 defines only `-d`
and `-e`; `-s` is parsed as an SSID rather than a save option. The case has been
removed. A following manual reconnect initially failed and temporarily stopped
ports 80, 82, and 2222, then succeeded on a bounded USB retry using the original
credentials. All three services and the original station address were verified
after recovery.

Two camera cases run back-to-back exposed a short ownership recovery window:
the immediate successor WebSocket closed before its first frame, whereas the
same combined camera/USB/HTTP/controller case passed after a two-second pause.
This is retained as a factory stability observation, not yet classified as a
normative violation.

Waiting passively after a TCP upload reconnect produced no unsolicited replay
of the outstanding `0xb3` request and eventually reached the normal transfer
timeout. OWN-008 requires only that the same logical owner may continue. When
the host instead resent the outstanding sequence immediately, the factory
firmware accepted it and completed the transfer. The HIL case now models that
host-driven continuation rather than requiring behavior absent from the spec.

No OTA, SPIFFS replacement, partition-table write, NVS fault injection,
recording command, motion command, or CAN transmission was attempted. Exact
factory restoration after those operations is not available through the
current fixture.

## Physical SD removal, preview, and endurance

The physical SD card was removed and reinserted while factory firmware was
running. With the card absent, `/sd/config.txt` download and MD5 failed,
`ftype` reported `nc`, and `ls /sd` offered only the FILE-015 completion reply.
USB, controller, HTTP, video-server, and TCP services remained responsive.
After reinsertion, the card remounted without a restart, the original directory
listing returned, and `config.txt` still matched the backup byte for byte.

Existing factory AVI recordings were then exercised through `/ws_preview`.
Four recordings from different series produced successful `open` responses.
One valid 300-frame recording returned the specified `open` and `meta` objects
followed by three indexed JPEG WebSocket messages; the session was stopped
without modifying the file. The smallest inspected AVI returned the normative
422 damaged/invalid-format response, demonstrating file-specific validation
rather than a general preview failure.

Five media/controller endurance cycles completed 25 physical test executions:
live-camera disconnect/reconnect, concurrent camera/USB/HTTP/controller reads,
physical AVI preview, and repeated controller queries. All passed when media
sessions were separated by the observed two-second factory recovery interval.
A further ten read-only USB/TCP/HTTP transport-stress cases passed in 110.04
seconds, including four-client waves and capacity recovery. Ten consecutive
isolated native-USB bus resets also re-enumerated and recovered successfully.

## Conclusion

The physical fixture is suitable for validating repository firmware across
USB, Wi-Fi, BLE, SD, camera, and controller paths. For the factory firmware,
the only confirmed difference found by this regression is WEB-020's HTTP status
for invalid JSON. A new run after flashing a repository-built release is
required before any of these physical results can be attributed to this
implementation.
