# Host provisioning tools

The repository's GitHub build setup is documented in
[`docs/ci-and-release.md`](../docs/ci-and-release.md). Pull requests and pushes
to `main` run host tests/coverage plus live and all-mock ESP-IDF builds; a
published GitHub Release additionally packages and attaches firmware images.

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

For a complete HIL run, execute the groups sequentially so reset and OTA
tests cannot leave a transport handle open for the next group:

```sh
Z1_ALLOW_MUTATION=1 Z1_ALLOW_DESTRUCTIVE=1 \
python3 tools/run_hil_isolated.py
```

The runner waits for each pytest process to exit and stops before starting the
next group if a timeout or failure occurs. Reconnect or re-enumerate USB/COM
between groups when a reset or OTA test intentionally disconnects the target.

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
export ESP_IDF_DIR="${ESP_IDF_DIR:-$HOME/.espressif/v5.4.4/esp-idf}"
source "$ESP_IDF_DIR/export.sh"
python3 tools/build_firmware.py --live
python3 tools/build_firmware.py --mock sd
python3 tools/build_firmware.py --mock-all
python3 tools/build_firmware.py --live --alt_webui
python3 tools/build_firmware.py --live --release \
  --mainboard-version 0x00010203
python3 tools/build_firmware.py --live --release --compact \
  --mainboard-version 0x00010203
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

After ESP-IDF activation, the helper uses the local `IDF_PATH` and
`IDF_PYTHON_ENV_PATH` variables to find the SDK script and Python environment.
This also supports installations where activation defines `idf.py` as a shell
function rather than placing an executable named `idf.py` in `PATH`.

Mock adapter source files are conditionally added to the target component only
when their corresponding mock switch is enabled. Live and release builds retain
only the small no-op fault-state boundaries needed by production code; mock
command handlers and stateful mock implementations are compile-time excluded.

`--alt_webui` (also accepted as `--alt-webui`) explicitly builds the SPIFFS
partition from the local, ignored `webui-alt` directory. The command fails
if that directory is missing or empty; it never silently falls back to the
public `webui` tree. The selected directory name is recorded in
`hardware-selection.json`, while its files remain ignored and are not copied
into tracked build inputs. Because the SPIFFS image uses ESP-IDF's
`FLASH_IN_PROJECT`, combining this option with `--flash` flashes the selected
Web UI together with the firmware:

```text
python3 tools/build_firmware.py --live --alt_webui --flash \
  --port /dev/cu.usbmodem...
```

Ordinary builds and release builds continue to package the repository's
public `webui` directory unless this local-only option is explicitly supplied.

`--release` additionally creates `firmware.bin` in the selected build
directory. Release mode requires `--live`, enables ESP-IDF size optimization,
and rejects mock adapter selections. It also defaults release logging to
warnings, retaining errors while removing non-normative informational log
strings. DIAG-025 and DIAG-034--DIAG-036 are intentional narrow exceptions:
the BLUFI and controller-transfer INFO call sites are retained and enabled per
tag because the specification requires those records. The shared factory remains in the source
tree, but compile-time `if constexpr` selection plus linker garbage collection
removes unreferenced mock paths from the live image. `--mainboard-version`
supplies the aggregate package metadata and requires `--release`; it does not
change the ESP-IDF application version.

Non-release development builds use `partitions-dev.csv`, which preserves the
larger 2 MiB OTA slots and 1 MiB SPIFFS volume used before the specification
layout was restored. Release builds always use the specification table in
`partitions.csv`. The selected table is recorded in
`hardware-selection.json` and printed by the build command, so a USB flash
operation can be audited before it changes the device layout.

`--compact` requires `--release` and enables the optional compact profile. The
standard release profile already enables the production-safe reductions needed
to fit the specification OTA slot:
assertions remain active but omit verbose failure text, ESP-IDF check macros
omit their diagnostic strings, Bluedroid stack logging is disabled, and the
bootloader keeps warnings/errors but omits informational messages. The exact
generated symbols are:

```text
CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_SILENT=y
CONFIG_COMPILER_OPTIMIZATION_CHECKS_SILENT=y
CONFIG_BT_STACK_NO_LOG=y
CONFIG_BOOTLOADER_LOG_LEVEL_WARN=y
# Camera policy: development enables every listed sensor symbol; compact and
# release enable only OV3660_SUPPORT and disable every other listed symbol.
```

The generated `hardware-selection.json` records whether `compact` was selected.
The normal release profile remains the recommended default; compact remains an
optional profile that requires separate hardware validation.

Use `idf.py size-components` and `idf.py size-files` on the release build before
publishing it. The generated `hardware-selection.json` records that the build
was a release and that no mocks were selected.

### Development partition warning

`tools/build_firmware.py --live` and the mock profiles are development builds.
They use `partitions-dev.csv`, which is intentionally larger than the
specification/release layout and can produce a larger image. Before using a
development image on a device previously flashed with a release layout, do one
complete USB/serial flash with `--flash` so the development partition table is
written. The first flash may erase or invalidate existing data because the
partition offsets differ. Use `--release` for the normative layout and OTA
images.

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

## Local code coverage

Generate a coverage report for the portable host-tested firmware code:

```sh
python3 tools/host_coverage.py
```

The complete host test suite is executed before the report is generated.
The script selects one coherent Clang/LLVM installation for compilation,
profile merging, and reporting. It uses Xcode Command Line Tools when complete
and otherwise supports the keg-only Homebrew `llvm` package.

Open the HTML report with:
```sh
open build/host-coverage/coverage/index.html
```
Coverage output is isolated from the ordinary host-tests build.
Each successful release run uploads this directory as a workflow artifact and
publishes the latest report plus `badge.svg` to the generated `coverage`
branch. The README badge reads that static file, so it reports the measured
line coverage of the latest release rather than a manually maintained value.
