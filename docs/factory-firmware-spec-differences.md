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

## Conclusion

The physical fixture is suitable for validating repository firmware across
USB, Wi-Fi, BLE, SD, camera, and controller paths. For the factory firmware,
the only confirmed difference found by this regression is WEB-020's HTTP status
for invalid JSON. A new run after flashing a repository-built release is
required before any of these physical results can be attributed to this
implementation.
