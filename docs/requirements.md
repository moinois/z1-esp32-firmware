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
| STAT-005--STAT-006 status transformation | Core implementation | Yes | No |
| UPD-010--UPD-014 package validation | Core implementation | Partial: 010--013 | No |
| BOOT-001--BOOT-003 persistent-store recovery | ESP-IDF composition root | Build verification only | No |
| HW-002 partition table | Target configuration | Build verification only | No |
| HW-004--HW-005 CPU, flash, PSRAM, security | Target defaults | Build verification only | No |
| HW-060--HW-061 heartbeat | ESP-IDF composition root | Build verification only | No |

All unlisted requirements remain pending. This table deliberately avoids
claiming conformance based only on code presence.
