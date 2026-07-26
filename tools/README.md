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
