# Requirement traceability

This file records implemented and verified specification slices. A requirement
is marked complete only when its portable logic has a host test and any required
ESP-IDF behavior has either a target test or a documented hardware test.

| Area | Implemented | Host verified | Hardware verified |
|---|---|---|---|
| FRM-001--FRM-016 common framing and recovery | Core implementation | Partial: 001, 004, 010, 011, 013, 015, 016 | No |
| ESC-001--ESC-002 escaping | Core implementation | Yes | Not required |
| HFT-004 path normalization | Core implementation | Yes | Not required |
| CMD-001--CMD-003 recognition and limits | Core implementation | Partial: 001, 003 | Not required |
| ROUTE-001--ROUTE-018 routing policy | Application implementation | Yes | Transport integration pending |
| OWN-001--OWN-008 ownership policy | Application implementation | Claim, identity, independence, disconnect verified | Terminal service integration pending |
| LPC-001--LPC-002 periodic controller queries | Application implementation | Yes | UART integration pending |
| LPC-003 initial controller status | Application implementation | Yes | UART integration pending |
| STAT-001--STAT-010 controller snapshots and replies | Core and application implementation | Yes | Routing integration pending |
| UART-003--UART-008 controller link policy | Application implementation | Yes | UART adapter integration pending |
| DIAG-021 controller write failure message | Application implementation | Yes | Diagnostic sink integration pending |
| HW-020--HW-022 controller UART configuration | ESP-IDF adapter | Exact settings verified | Adapter startup deferred by BOOT-012 |
| UART-002 bounded controller reads | ESP-IDF adapter | Exact settings verified | Adapter startup deferred by BOOT-012 |
| LPC-010--LPC-014 common controller-transfer wire layer | Application implementation | Partial: 010--012, 014 queue and parsing | Service integration pending |
| UPD-010--UPD-014 package validation | Core implementation | Partial: 010--013 | No |
| BOOT-001--BOOT-003 persistent-store recovery | ESP-IDF composition root | Build verification only | No |
| HW-002 partition table | Target configuration | Build verification only | No |
| HW-004--HW-005 CPU, flash, PSRAM, security | Target defaults | Build verification only | No |
| HW-060--HW-061 heartbeat | ESP-IDF composition root | Build verification only | No |

All unlisted requirements remain pending. This table deliberately avoids
claiming conformance based only on code presence.
