# ESPHome BC7215 Climate

Custom ESPHome climate component for an **ESP32 + BC7215 infrared transceiver**. Developed from a working TTGO T-Display + Whirlpool SPIW 309L prototype.

> **Status: experimental pre-release.** The full IR learning → NVS save → readback → normal IR decoding workflow was exercised on the reference hardware, including restart. The extracted standalone repository has **not yet been independently compiled and OTA-tested**. Do not assume support for every A/C model.

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
      ref: main
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
| Control actual A/C with newly saved pairing after cold boot | **Not independently verified** |
| Standalone repository build | **Not yet verified** |

**Known caveats:** `Current Temperature` may show `0.00°C` in HA when Around U is off; that is not a measured room temperature. The power state of a toggle-only remote is an estimate and can diverge if IR signals are missed. Some operations block the ESPHome loop for ~100–200 ms; optimization is pending. The library's 25 °C COOL reference requirement and multi-segment behavior are model-specific. See [design notes](docs/ARCHITECTURE.md).

## Project layout

```text
components/bc7215_climate/   # ESPHome custom climate code
examples/minimal_esp32.yaml # headless ESP32 + HA example
examples/ttgo_t_display.yaml # full TTGO T-Display UI example
docs/HARDWARE.md             # wiring and voltage
docs/LEARNING.md             # pairing and NVS protection
docs/ARCHITECTURE.md         # internal design and limitations
```

## Upstream and licensing

This integration uses the separate `bc7215ac` library maintained by bitcode-tech. The external library is fetched as a build dependency, **not vendored** in this repository. Choose and add a license for this repository before a formal public release.
