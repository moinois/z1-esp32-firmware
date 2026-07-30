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

## Build with selected hardware adapters

`build_firmware.py` configures any combination of live and mock adapters in one
reusable generated build directory. It discovers supported adapter names from
`main/Kconfig.projbuild`, so adding a future mock switch does not require a new
build profile.

```text
source /Users/moinois/esp/esp-idf/export.sh
python3 tools/build_firmware.py --live
python3 tools/build_firmware.py --mock sd
python3 tools/build_firmware.py --mock-all
```

As more adapters are implemented, combinations such as `--mock sd,camera`
require no new profile. Comma-separated names and repeated `--mock` switches
are equivalent. Unknown names are rejected and the available names are printed.
Each invocation writes the complete selection to `build/sdkconfig` and an
auditable summary to `build/hardware-selection.json`, then builds the firmware.
The same standard build tree is used for live and mock firmware. Flash the
selected build with:

```text
python3 tools/build_firmware.py --mock sd --flash \
  --port /dev/cu.usbmodem...
```

The normal `idf.py build` path remains governed by `sdkconfig.defaults`, where
all mocks are disabled.

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
the mainboard image as required by
[UPD-013](https://github.com/f355/esp32_cnc_spec/blob/main/09-firmware-update.md#upd-013):

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
