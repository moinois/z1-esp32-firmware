# Anonymized field feedback

This log records externally observed behavior that can influence implementation,
tests, documentation, or specification proposals. Reports are anonymized and
exclude user names, host addresses, machine identifiers, credentials, and
screenshots. A report is evidence about interoperability, not by itself a
normative requirement.

Status vocabulary:

- `Confirmed`: reproduced or directly supported by implementation/tests.
- `Actioned`: resulted in a repository change.
- `Spec decision`: useful evidence, but implementation must wait for an accepted
  normative change.
- `Candidate`: retained for a future explicit design decision or fixture.

## 2026-07 client-initialization and transport reports

| ID | Anonymized observation or proposal | Assessment | Result or follow-up |
|---|---|---|---|
| FB-001 | A development board was discovered over native USB and framed commands reached `/sd` file handling. | `Confirmed`. The log proves descriptor discovery, vendor bulk transport, frame/CRC acceptance, and command routing. A 64-byte USB packet is not an application-frame boundary. | Existing USB framing and HIL cover this behavior; no production change required. |
| FB-002 | Desktop software remained in initialization after `/config.txt` could not be downloaded from an unmounted SD card. | `Confirmed` as an interoperability limitation. HFTD-003 requires the firmware to return `0xb5` on open/MD5 failure, which the implementation does. | The golden-trace test records both successful config transfer and the normative missing-file failure. A mock must be an explicit simulator mode, never an automatic production fallback. |
| FB-003 | A failed `/gcodes` open appeared in logs while the desktop client waited for directory initialization. | `Confirmed` at the diagnostic layer, but the implementation already sends the FILE-015 terminal `0x84` even when open fails. | Golden-trace coverage protects the mandatory terminal response. No mock empty directory is added to normal firmware. |
| FB-004 | Operating-system Bluetooth pairing did not work, although BLE advertising was visible on one platform. | Expected behavior. BLUFI is BLE GATT provisioning with link pairing and Classic Bluetooth disabled. | `Actioned`: user documentation now explicitly distinguishes direct GATT provisioning from OS pairing. |
| FB-005 | The desktop client's BLE scan may filter case-sensitively for `Makera_Z1_*`, while the specification requires `BLUFI_DEVICE`. | Strong compatibility evidence, but changing the name would violate BLE-002 and BWF-002. | `Spec decision`: propose deriving the BLE name from the same configured machine name used by UDP discovery. Do not change production behavior before acceptance. |
| FB-006 | Discovery machine-name suffixes of different lengths were observed and accepted by the client. | `Confirmed`. Discovery is variable-length comma-separated text and the implementation bounds the complete payload rather than fixing name length. | No change required. Existing tests cover configured and MAC-derived names. |
| FB-007 | Reliable discovery should use the active station subnet broadcast and the built-in AP broadcast at about two packets per second. | `Confirmed`. The implementation sends every 500 ms to the directed STA broadcast and to the AP broadcast, with additional three-copy bursts. | No change required. |
| FB-008 | A captured production session supplied exact request ordering and representative status, version, file-transfer, listing, completion, and keepalive frames. | Useful compatibility evidence. Values tied to a particular machine are unsuitable as universal defaults, but the protocol ordering is reusable. | `Actioned`: a sanitized golden-trace host test now verifies framing and the initialization-critical response sequence. |
| FB-009 | Boards without SD/controller hardware would be easier to exercise if they returned static configuration, empty directories, and motionless controller data. | `Candidate`. Automatic fallback would conceal missing hardware and change specified failure semantics. | Consider a separately enabled simulator build backed by the existing storage/controller ports. No production fallback is authorized. |
| FB-010 | More BLE logs were requested for connection, MTU, subscription, negotiation, protection mode, notifications, disconnect, scan, and IP acquisition. | `Candidate`. These are useful diagnostics if they avoid credentials and excessive packet logging. | Track as a diagnostic enhancement; existing HIL already verifies these wire-level phases physically. |

## Traceability rules

When feedback causes a change, update the applicable row with the resulting
test, documentation section, commit, or specification pull request. If a later
finding disproves an observation, retain the row and append the correction so
the decision history remains visible.
