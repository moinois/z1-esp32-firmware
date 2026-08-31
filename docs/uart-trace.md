# Controller UART trace

The controller UART trace is an opt-in diagnostic capture of the raw byte
chunks submitted to and received from the LPC1768 link. It is independent of
`/sd/serial.log`, which mirrors formatted ESP diagnostic records.

## Capture

1. Create an empty `/uart-trace.enable` file on the user-visible SD volume.
2. Leave the SD card inserted and restart the controller.
3. Reproduce the controller-link problem.
4. Delete `uart-trace.enable`. The firmware closes the capture within
   approximately one second.
5. Download `uart-trace.bin` through the normal file interface, or power the
   controller down before removing the card and copy it on another computer.

The trace task notices creation or removal of the sentinel within approximately
one second. UART RX and TX paths only copy chunks into a fixed nonblocking RAM
queue; they never wait for SD I/O. Records are flushed by a separate
low-priority task. A full queue drops new trace records without changing
controller traffic. Capture stops at 4 MiB.

## Format and decoding

Each little-endian record contains a 20-byte header followed by its payload:

| Offset | Size | Meaning |
| --- | ---: | --- |
| 0 | 4 | ASCII magic `Z1UT` |
| 4 | 1 | Format version, currently `1` |
| 5 | 1 | Direction: `0` RX from LPC, `1` TX to LPC |
| 6 | 2 | Payload byte count |
| 8 | 8 | ESP monotonic timestamp in microseconds |
| 16 | 4 | Sequence number; a gap proves that trace records were dropped |

Decode a copied trace with:

```sh
python3 tools/decode_uart_trace.py uart-trace.bin
```

The capture proves which bytes the ESP UART driver accepted or returned. It
does not prove that an electrical TX waveform reached the receiving chip; use
a passive logic analyzer when that distinction matters.
