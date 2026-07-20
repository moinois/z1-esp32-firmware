# Requirement traceability

This file records implemented and verified specification slices. A requirement
is marked complete only when its portable logic has a host test and any required
ESP-IDF behavior has either a target test or a documented hardware test.

| Area | Implemented | Host verified | Hardware verified |
|---|---|---|---|
| FRM-001--FRM-016 common framing and recovery | Core implementation | Partial: 001, 004, 010, 011, 013, 015, 016 | No |
| ESC-001--ESC-002 escaping | Core implementation | Yes | Not required |
| HFT-004 path normalization | Core implementation | Yes | Not required |
| HFT-001--HFT-003 transfer start and path validation | Core implementation | Yes | Service integration pending |
| HFT-010--HFT-011 cache path mapping | Core implementation | Yes | Directory adapter pending |
| HFT-013 cached MD5 extraction | Core implementation | Yes | Storage adapter pending |
| HFT-014 cache base preparation | Download port contract | Invocation verified | Filesystem adapter pending |
| HFT-005--HFT-007 start admission and owner mailbox | Application implementation | Yes | Ownership and service integration pending |
| HFTD-001--HFTD-010 host download | Application implementation with replaceable port | Yes | Filesystem, transport, and ownership adapters pending |
| HFT-020--HFT-021, HFT-023, HFT-025 download timing and errors | Application implementation | Yes for download | Runtime clock integration pending |
| HFTU-001--HFTU-010 host upload | Application implementation with replaceable port | Yes | Filesystem, transport, and ownership adapters pending |
| HFT-020--HFT-022, HFT-024--HFT-025 upload timing and retries | Application implementation | Yes for upload | Runtime clock integration pending |
| FILE-001--FILE-003 common filesystem path syntax | Core implementation | Yes | Not required |
| FILE-010 directory-list argument and option parsing | Core implementation | Yes | Not required |
| FILE-011--FILE-015 directory listing | Application implementation with replaceable port | Yes | Filesystem and response adapters pending |
| FILE-020--FILE-026 filesystem mutations and type reply | Core parsing and application implementation with replaceable port | Yes | Filesystem and response adapters pending |
| FILE-027--FILE-029 MD5 command | Application implementation with replaceable metadata and hash port | Yes | Filesystem, MD5, and response adapters pending |
| SD-001--SD-008 SD-card lifecycle and capacity | Application implementation with replaceable port | Yes for policy and ordering | GPIO, SDMMC, FAT, and logging adapters pending |
| SD-009--SD-010 FAT filename, sector, and lock policy | Target defaults | Build verification only | Physical-card verification pending |
| CFG-002--CFG-003 SD configuration line parsing | Core implementation | Yes | Not required |
| CFG-010 configuration command tokenization | Core implementation | Yes | Command-service integration pending |
| CFG-001, CFG-004--CFG-006 configuration file copies | Application implementation with replaceable bytewise file port | Yes | Filesystem and response adapters pending |
| CFG-020--CFG-023 live configuration view | Core chunk parser and application implementation with replaceable load port | Yes | Filesystem and command adapters pending |
| CFG-011--CFG-013 configuration reads | Application implementation with replaceable live/SD read port | Yes | Filesystem and response adapters pending |
| CFG-030--CFG-034 configuration updates | Core rewrite logic and application implementation with replaceable persistence port | Yes | Filesystem and response adapters pending |
| LOG-001--LOG-006 diagnostic capture and record formatting | Application implementation with replaceable output/buffer/clock port | Yes | ESP-IDF log-hook and writer integration pending |
| LOG-007--LOG-013 diagnostic log writer | Application implementation with replaceable buffered-file port | Yes for session, flush, rotation, short write, and shutdown policy | Filesystem/task integration pending |
| NET-001--NET-002 machine name | Core implementation | Yes | Configuration/MAC adapter integration pending |
| NET-004--NET-005 AP channel selection | Core implementation | Yes for counting, wrapping, tie, and fallback | ESP-IDF Wi-Fi adapter pending |
| NET-003--NET-005, NET-008 connectivity startup | Application implementation with replaceable Wi-Fi/network port | Yes for order, scan settings, AP/STA settings, fallback, and fatal failures | ESP-IDF Wi-Fi and network-interface adapters pending |
| NET-006--NET-007 socket capacity and hostname | Target defaults and application startup configuration | Exact values host verified; target build verified | Runtime resource-exhaustion verification pending |
| NET-010--NET-017 saved credentials and automatic retry | Application implementation with replaceable storage, scheduler, station, and clock port | Yes for storage outcomes, exact configuration, scheduling, retry, and nonfatal failures | ESP-IDF station-event, task, and persistent-store adapters pending |
| NET-020--NET-026 manual station lifecycle | Application implementation with replaceable station API and persistence port | Yes for connection phases, event data, RSSI, persistence outcome, errors, and disconnect retention | ESP-IDF station-event and persistent-store adapters pending |
| NET-030--NET-033 Wi-Fi scan policy | Core result policy and application command implementation with replaceable Wi-Fi port | Yes | ESP-IDF Wi-Fi adapter pending |
| NET-040--NET-043 host WLAN parsing and scan command | Core parser and application implementation with replaceable Wi-Fi port | Yes | Command routing, transport, and ESP-IDF Wi-Fi adapters pending |
| NET-044--NET-046 host WLAN connect/disconnect responses | Application implementation with replaceable response and discovery port | Yes | Command routing, transport, discovery, and ESP-IDF Wi-Fi adapters pending |
| DISC-001--DISC-008 UDP discovery | Core formatting/address policy and application implementation with replaceable socket/clock port | Yes for payloads, ordering, timing, recreation, and ignored failures | UDP socket, controller-status, and event integration pending |
| BLE-001--BLE-006 provisioning identity and lifecycle | Application implementation with replaceable BLUFI/security port plus BLE-only target defaults | Yes for optional startup, identity, security reset, advertising, and credential retention | ESP-IDF BLUFI adapter pending |
| BLE-010--BLE-016 provisioning commands | Application implementation with replaceable Wi-Fi/report/diagnostic port | Yes for security gates, credentials, connect/disconnect, reports, scan, errors, and custom data | BLUFI wire dispatcher and ESP-IDF Wi-Fi adapter pending |
| BLESEC-001--BLESEC-006 BLUFI security | Application implementation with replaceable allocation/DH/MD5/AES port; common BLUFI CRC in core | Yes for negotiation shapes, limits, salt, key/IV policy, errors, and readiness lifecycle | ESP-IDF mbedTLS adapter and wire integration pending |
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
| LPC-015--LPC-019 common transfer state behavior | Firmware-family implementation | Yes for firmware family | Configuration and factory families pending |
| LPCFW-001--LPCFW-006 controller firmware transfer | Application implementation with replaceable port | Yes | Storage, routing, and status adapters pending |
| LPCCFG-001--LPCCFG-006 controller configuration transfer | Application implementation with replaceable port | Yes | Storage and routing adapters pending |
| LPCFAC-001--LPCFAC-005 controller factory-data transfer | Application implementation with replaceable port | Yes | Storage and routing adapters pending |
| PLAY-001--PLAY-007 streamed-play preparation and status | Application implementation with replaceable port | Yes for preparation and status | Storage, ownership, and routing adapters pending |
| PLAY-010--PLAY-011, PLAY-018 play controller lifecycle | Application implementation with replaceable port | Start, validation, observer, and terminal cases | Data and goto exchange pending |
| PLAY-007, PLAY-014 streamed-play logical lines | Application implementation with replaceable source | NUL, chunk, length, EOF, and failure cases | Data and goto aggregation pending |
| PLAY-012--PLAY-017 streamed-play data exchange | Application implementation with replaceable source | Validation, aggregation, rewind, retry, EOF, and cleanup | Storage and routing adapters pending |
| PLAY-019--PLAY-020 streamed-play goto exchange | Application implementation with monotonic-time port | Validation, scan, pacing, target, EOF, and failure cases | Storage and routing adapters pending |
| PLAY-021 streamed-play family inbox | Shared application family inbox | Yes | Routing adapter pending |
| UPD-010--UPD-014 package validation | Core implementation | Partial: 010--013 | No |
| BOOT-001--BOOT-003 persistent-store recovery | ESP-IDF composition root | Build verification only | No |
| HW-002 partition table | Target configuration | Build verification only | No |
| HW-004--HW-005 CPU, flash, PSRAM, security | Target defaults | Build verification only | No |
| HW-060--HW-061 heartbeat | ESP-IDF composition root | Build verification only | No |

All unlisted requirements remain pending. This table deliberately avoids
claiming conformance based only on code presence.
