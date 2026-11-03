# ESPHome BC7215 Climate

Custom ESPHome climate component for an **ESP32 + BC7215 infrared transceiver**. Developed from a working TTGO T-Display + Whirlpool SPIW 309L prototype.

> **Version 1.0.0 (Whirlpool-tested release candidate).** The standalone GitHub external component was compiled, installed by OTA and tested with the TTGO T-Display + Whirlpool SPIW 309L on ESPHome 2026.9.1 (November 3, 2026). The generic profile has **not** been hardware-tested on other A/C brands. This is an independent community integration, not an official ESPHome or Bitcode component.

## Features

- ESPHome `climate` entity for Home Assistant; temperature, HVAC mode and fan speed control.
- IR reception and synchronization with physical remote (estimated state, not real feedback from the indoor unit).
- On-device pairing: capture the IR remote's reference command, initialize BC7215AC protocol, review / choose match, save to NVS.
- Explicit save, cancellation, NVS readback verification and backup of the previous pairing under `bc7215_prev`.
- Read-only protocol identity text sensor: `SIG 0x36 | 168 bit | match 0` for the tested Whirlpool.
- TTGO T-Display example with two-button UI and diagnostics.
- Whirlpool-specific **Around U / I FEEL** decoder: read-only reporting and conservative HA-command lock while active or state unknown.
- OTA snapshot of estimated state (separate from persistent protocol pairing).

## Getting started

1. Connect the BC7215 module to an ESP32 following [hardware wiring](docs/HARDWARE.md).
2. Copy [examples/minimal_esp32.yaml](examples/minimal_esp32.yaml) or [examples/ttgo_t_display.yaml](examples/ttgo_t_display.yaml) to your ESPHome configuration directory.
3. Configure Wi-Fi secrets and compile.
4. In Home Assistant open the device, press **IR learning start**; set the physical remote to **COOL / 25 °C** (verify the mode indicator), then press its **FAN SPEED** key once.
5. Confirm `IR LEARN: ... ACCEPTED` in the ESPHome log. Use **IR learning save** only if you intend to replace the stored pairing; otherwise **cancel**.

Read the full [learning and NVS procedure](docs/LEARNING.md) **before saving**.

## AC profiles

Set `ac_profile: whirlpool` (the **default**, retained for existing TTGO installations) to enable the Whirlpool SPIW 309L Around U interpretation and its conservative safety lock. Select `ac_profile: generic` for ordinary BC7215AC learning and climate control on other devices; it does not interpret Whirlpool Around U frames and does not apply the Around U lock. Generic does **not** mean every A/C protocol is supported: recognition still depends on the upstream library and a successful learning sample.

```yaml
climate:
  - platform: bc7215_climate
    name: Air Conditioner
    id: ac
    ac_profile: generic  # whirlpool for the tested SPIW 309L
```

For `generic`, omit the Whirlpool-specific Around U sensor entities from your example configuration. The IR `power=2` toggle interpretation is still estimated and shared by both profiles; additional manufacturers may need their own power mapping. Pairing NVS and learn/save semantics are unchanged by profile selection. **Do not switch profiles to bypass protections on an active Whirlpool installation.**

## External component

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/Csernobog/esphome-bc7215-climate
      ref: v1.0.0
    components: [bc7215_climate]

climate:
  - platform: bc7215_climate
    name: Air Conditioner
    id: ac
```

The component uses Arduino framework. Its four BC7215 GPIOs are configurable with original TTGO defaults (see wiring guide). It pulls the upstream [bitcode-tech/bc7215ac](https://github.com/bitcode-tech/bc7215ac) Arduino library during build.

## Reference testing

On the reference Whirlpool unit we observed:

| Test | Result |
| --- | --- |
| Capture 168-bit remote frame | Passed |
| Recognize COOL 25 °C FAN SPEED reference | Passed |
| Cancel learning and restore existing pairing | Passed |
| Back up old NVS profile | Passed |
| Save, read back, reinitialize pairing | Passed |
| Decode commands after save (COOL/DRY/FAN/HEAT) | Passed |
| Load protocol identity after restart | Passed |
| Configurable GPIO parameters, original TTGO mapping | Passed |
| Whirlpool Around U ON → 20 °C → OFF in HA | Passed after frame-filter fix |
| `ac_profile: generic` on a second A/C | **Not tested** |
| Control actual A/C with newly saved pairing after cold boot | **Not independently verified** |
| Standalone GitHub external component build and OTA | Passed on TTGO / ESPHome 2026.9.1 |

**Known caveats:** `Current Temperature` may show `0.00°C` in HA when Around U is off; that is not a measured room temperature. The power state of a toggle-only remote is an estimate and can diverge if IR signals are missed. Some operations block the ESPHome loop for ~100–200 ms; optimization is pending. The library's 25 °C COOL reference requirement and multi-segment behavior are model-specific. See [design notes](docs/ARCHITECTURE.md).

## Documentation

- [Configuration and HA entities](docs/CONFIGURATION.md)
- [Pinout and physical wiring](docs/HARDWARE.md)
- [IR pairing, NVS backup and recovery boundaries](docs/LEARNING.md)
- [Component architecture, known limitations](docs/ARCHITECTURE.md)
- [Release history](CHANGELOG.md)

## Project layout

```text
components/bc7215_climate/   # ESPHome custom climate code
examples/minimal_esp32.yaml # headless ESP32 + HA example
examples/ttgo_t_display.yaml # full TTGO T-Display UI example
docs/HARDWARE.md             # wiring and voltage
docs/LEARNING.md             # pairing and NVS protection
docs/ARCHITECTURE.md         # internal design and limitations
```

## License and upstream dependency

This repository contains original ESPHome wrapper/integration code. The upstream [bitcode-tech/bc7215ac](https://github.com/bitcode-tech/bc7215ac) dependency is **not included** in this repository: it is downloaded during an ESPHome build. The upstream library uses **its own restrictive license**, including a requirement to use it only together with the designated Bitcode BC7215 hardware and restrictions on reverse engineering/obfuscated components. Read the [upstream LICENSE](https://github.com/bitcode-tech/bc7215ac/blob/main/LICENSE) before reuse or distribution; do not treat the upstream library as MIT/Apache licensed. The integration source itself has not yet been assigned a redistribution license by its owner; this must be resolved before encouraging downstream reuse.
