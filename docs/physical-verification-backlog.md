# Physical verification backlog

This document classifies every `Pending fixture` row from
[requirements.md](requirements.md). The detailed evidence remains in
[physical-verification-evidence.md](physical-verification-evidence.md); each
entry below links directly to its evidence row.

The audit is conservative: mock evidence never becomes physical conformance,
and machine-affecting controller/play cases are not grouped with ordinary
recoverable mutations. Re-run this audit whenever a status leaves `Pending fixture`.

## Summary

| Actionability | Rows |
|---|---:|
| Ready on the current Z1 | 0 |
| Ready through mocks or deterministic testhooks | 19 |
| Needs controlled controller or machine interaction | 27 |
| Needs external instrumentation or hardware | 13 |

## Ready on the current Z1

No remaining `Pending fixture` row can be closed safely by the current Z1 and
host alone. Recoverable USB/Wi-Fi/BLE/SD/camera checks have either reached
`Yes` or `Partial`; the latter name their narrower unresolved boundary in the
evidence ledger.

## Ready through mocks or deterministic testhooks

These boundaries are unsafe or unreliable to provoke naturally. They can be implemented and run without new physical equipment, but simulator evidence remains distinct from physical conformance.

- [USB streamed-play preparation](physical-verification-evidence.md#phys-011)
- [DIAG-027 transfer-owner and setup diagnostics](physical-verification-evidence.md#phys-035)
- [UPD-004--UPD-005, UPD-020--UPD-022 aggregate loading and validation effects](physical-verification-evidence.md#phys-103)
- [UPD-060--UPD-063 update-file deletion](physical-verification-evidence.md#phys-104)
- [UPD-001--UPD-003 aggregate paths and triggers](physical-verification-evidence.md#phys-109)
- [UPD-006 update processor initialization](physical-verification-evidence.md#phys-110)
- [DIAG-030 aggregate header diagnostics](physical-verification-evidence.md#phys-131)
- [DIAG-031 persisted update recovery diagnostics](physical-verification-evidence.md#phys-132)
- [DIAG-032 update failure diagnostics](physical-verification-evidence.md#phys-133)
- [DIAG-038 controller-output saturation and non-owner file data](physical-verification-evidence.md#phys-137)
- [DIAG-039 file-transfer capacity diagnostics](physical-verification-evidence.md#phys-138)
- [DIAG-041 live-media resource diagnostics](physical-verification-evidence.md#phys-140)
- [DIAG-042 missing aggregate diagnostics](physical-verification-evidence.md#phys-141)
- [DIAG-043 runtime accounting diagnostics](physical-verification-evidence.md#phys-142)
- [TRN-005--TRN-006 global host-output policy](physical-verification-evidence.md#phys-187)
- [TCP-012--TCP-013 connection capacity and selective acknowledgement](physical-verification-evidence.md#phys-190)
- [TCP origin-aware dispatch](physical-verification-evidence.md#phys-199)

## Needs controlled controller or machine interaction

The hardware is present, but these cases can affect controller firmware, configuration, factory data, or streamed play. Run them only with an agreed idle/service-mode setup, recovery images, and an explicit restoration check.

- [LPC-010--LPC-014 common controller-transfer wire layer](physical-verification-evidence.md#phys-090)
- [LPC-015--LPC-019 common transfer state behavior](physical-verification-evidence.md#phys-091)
- [LPCFW-001--LPCFW-006 controller firmware transfer](physical-verification-evidence.md#phys-092)
- [LPCFAC-001--LPCFAC-005 controller factory-data transfer](physical-verification-evidence.md#phys-094)
- [PLAY-001--PLAY-007 streamed-play preparation and status](physical-verification-evidence.md#phys-095)
- [PLAY-010--PLAY-011, PLAY-018 play controller lifecycle](physical-verification-evidence.md#phys-096)
- [PLAY-007, PLAY-014 streamed-play logical lines](physical-verification-evidence.md#phys-097)
- [PLAY-012--PLAY-017 streamed-play data exchange](physical-verification-evidence.md#phys-098)
- [PLAY-019--PLAY-020 streamed-play goto exchange](physical-verification-evidence.md#phys-099)
- [PLAY-021 streamed-play family inbox](physical-verification-evidence.md#phys-100)
- [PLAY-022--PLAY-023 streamed-play resource failures](physical-verification-evidence.md#phys-101)
- [UPD-053--UPD-055 controller update handoff](physical-verification-evidence.md#phys-108)
- [DIAG-035--DIAG-036 controller-transfer failure and layout diagnostics](physical-verification-evidence.md#phys-135)
- [DIAG-037 streamed-play diagnostics](physical-verification-evidence.md#phys-136)
- [RUN-033--RUN-035 play-state bridge](physical-verification-evidence.md#phys-215)
- [AVI-010--AVI-013 recorded AVI writer](physical-verification-evidence.md#phys-164)
- [REC-003--REC-010 recording segment policy](physical-verification-evidence.md#phys-165)
- [REC-020--REC-024 storage retention policy](physical-verification-evidence.md#phys-167)
- [REC-021--REC-024 storage retention service](physical-verification-evidence.md#phys-168)
- [REC-021--REC-024 filesystem adapter](physical-verification-evidence.md#phys-169)
- [RUN-031 wall-clock first-boot bridge](physical-verification-evidence.md#phys-179)
- [RUN-020--RUN-023 wall-clock target adapter](physical-verification-evidence.md#phys-180)
- [RUN-020--RUN-023 wall-clock command composition](physical-verification-evidence.md#phys-181)
- [RUN-020--RUN-023 controller command wiring](physical-verification-evidence.md#phys-182)
- [RUN-043 controller clearftm wiring](physical-verification-evidence.md#phys-183)
- [RUN-020--RUN-023 wall-clock response transport](physical-verification-evidence.md#phys-184)
- [REC-001 recording command wiring](physical-verification-evidence.md#phys-186)

## Needs external instrumentation or hardware

These cases require an active CAN peer, UART/GPIO measurement, TinyUSB endpoint control, or equivalent electrical instrumentation. The current Z1 alone cannot close them.

- [USB-008 transmit progress timeout](physical-verification-evidence.md#phys-006)
- [USB M942 CAN exercise](physical-verification-evidence.md#phys-014)
- [DIAG-021 controller receive/write diagnostics](physical-verification-evidence.md#phys-084)
- [UART-009 receive retention and candidate checks](physical-verification-evidence.md#phys-086)
- [HW-053, CAN-007--CAN-008 CAN identity and pending transmission](physical-verification-evidence.md#phys-087)
- [CAN-002--CAN-003, CAN-005--CAN-006 CANopen node identity and NMT](physical-verification-evidence.md#phys-116)
- [OD-001--OD-052 CANopen object dictionary](physical-verification-evidence.md#phys-117)
- [CAN-002 SDO configuration](physical-verification-evidence.md#phys-118)
- [CAN-010--CAN-015 M942 digital-I/O exercise](physical-verification-evidence.md#phys-119)
- [CAN-001, HW-050--HW-052 TWAI hardware](physical-verification-evidence.md#phys-120)
- [DIAG-033 CAN digital-output monitoring](physical-verification-evidence.md#phys-121)
- [TCP controller forwarding](physical-verification-evidence.md#phys-195)
- [HW-060--HW-061 GPIO heartbeat](physical-verification-evidence.md#phys-219)
