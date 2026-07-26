# Host provisioning tools

`provision_wifi.py` sends a WLAN credential-save command through the ESP32-S3
native USB vendor interface. Credentials are supplied at runtime and are not
stored in the repository.

## Install

The tool uses PyUSB. Install it in the active development Python environment:

```text
python3 -m pip install pyusb
```

For optional physical regression tests, install the complete host-only set and
follow the safety gates in `docs/hardware-testing.md`:

```text
python3 -m pip install -r requirements-hil.txt
python3 -m pytest tests/hardware
```

On macOS, connect the board's `USB` connector, not only the `COM` connector.
The native firmware interface has VID `0x303a` and PID `0x4002`.

## Provision station Wi-Fi

Run the tool from the implementation repository:

```text
python3 tools/provision_wifi.py '<SSID>' '<PASSWORD>'
```

The tool waits for the firmware's success or failure response. It returns zero
only after the firmware has reported that the credentials were saved. The
firmware stores them in its NVS Wi-Fi namespace for later automatic connection
attempts.

If the device is not found, verify that the native `USB` cable is connected and
that the firmware has enumerated as `MakeraZ1 (USB)`. The `COM` connector is
reserved for UART diagnostics.

## Package mainboard firmware

`package_firmware.py` wraps a bootable ESP32-S3 application image in the
32-byte aggregate format required for `/sd/firmware.bin`. With a completed
target build, run:

```text
python3 tools/package_firmware.py \
  --mainboard build/mainboard_firmware.bin
```

This writes `build/firmware.bin`. Supply opaque version metadata or alternate
paths when needed:

The aggregate header's format version is currently `1` and is independent of
the component metadata passed with `--mainboard-version` and
`--controller-version`. Those switches populate unsigned 32-bit package fields;
they do not change the ESP-IDF application version or the human-readable value
returned by the runtime `version` command.

```text
python3 tools/package_firmware.py --mainboard path/to/mainboard.bin \
  --output path/to/firmware.bin --mainboard-version 0x00010002
```

Add a controller image to create a combined package. Its bytes are placed after
the mainboard image as required by UPD-013:

```text
python3 tools/package_firmware.py \
  --mainboard build/mainboard_firmware.bin \
  --controller path/to/controller_firmware.bin \
  --controller-version 0x00020001
```

Without `--controller`, the controller size, version, and flag remain clear.
Supplying `--controller-version` without `--controller`, or an empty controller
file, is rejected.

Build a controller-only package explicitly with:

```text
python3 tools/package_firmware.py --controller path/to/controller_firmware.bin \
  --controller-version 0x00020001
```

This clears the mainboard flag, size, and version. The controller image begins
immediately after the 32-byte header. At least one of `--mainboard` and
`--controller` is required, and each version switch requires its corresponding
image switch.
