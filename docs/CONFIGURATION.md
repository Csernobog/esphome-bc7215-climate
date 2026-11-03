# Configuration and Home Assistant entities

## ESPHome integration

Use `external_components` and a `climate` platform entry:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/Csernobog/esphome-bc7215-climate
      ref: v1.0.0
    components: [bc7215_climate]

climate:
  - platform: bc7215_climate
    name: Klima
    id: ac
    uart_rx_pin: GPIO25
    uart_tx_pin: GPIO33
    mod_pin: GPIO27
    busy_pin: GPIO26
    ac_profile: whirlpool
```

The code uses an ESP32 `HardwareSerial(1)`, 19200 baud, 8N2; an ESP32 Arduino framework is required. The pins shown are the **tested TTGO defaults**.

## Configuration options

| Option | Values | Default | Description |
| --- | --- | --- | --- |
| `uart_rx_pin` | input-capable internal GPIO | GPIO25 | ESP32 RX connected to BC7215 TX |
| `uart_tx_pin` | output-capable internal GPIO | GPIO33 | ESP32 TX connected to BC7215 RX |
| `mod_pin` | output-capable internal GPIO | GPIO27 | BC7215 mode control |
| `busy_pin` | input-capable internal GPIO | GPIO26 | BC7215 BUSY input |
| `ac_profile` | `whirlpool`, `generic` | `whirlpool` | Enables or disables Whirlpool-specific Around U decoding and safety lock |

Distinct GPIO numbers are enforced by validation. The default Whirlpool profile is intentionally backward-compatible with the tested system. `generic` has **not** been validated on another A/C model; using it does not grant universal protocol support.

## Home Assistant

`climate` entity supports OFF, AUTO, COOL, HEAT, DRY and FAN_ONLY modes; fan AUTO, LOW, MEDIUM, HIGH; target range 16–30 °C with 1 °C increments. The firmware estimates the indoor unit's state from received/sent IR frames and does not receive confirmation from the actual appliance.

The examples expose auxiliary entities via ESPHome YAML, not automatically from the C++ component:

| Entity label (TTGO example) | Type | Purpose |
| --- | --- | --- |
| Klíma | climate | Estimated AC mode, fan and target temperature |
| IR párosítva | binary_sensor | NVS profile initialized |
| IR vétel aktív | binary_sensor | BC7215 normal receive loop active |
| IR adás folyamatban | binary_sensor | BC7215 TX pending |
| IR adási hiba - újraindítás szükséges | binary_sensor | Timeout/fault indicator |
| IR protokoll azonosito | text_sensor | Stored signature, frame length, match index |
| Fogadott IR parancsok | sensor | RX count since boot |
| Sikertelen IR dekódolások | sensor | Decode failures since boot |
| IR adások száma | sensor | TX count since boot |
| IR timeoutok száma | sensor | Send timeouts since boot |
| Whirlpool Around U | binary_sensor | Physical remote Around U ON/OFF in `whirlpool` profile |
| Whirlpool Around U hőmérséklet | sensor | Physical remote temperature when Around U ON |
| IR tanítás indítása/mentése/következő egyezés/megszakítása | button | Pairing workflow |

`SIG 0x36 | 168 bit | match 0` identifies the *stored IR representation*, not an appliance make/model/serial number.

**Around U controls:** Whirlpool mode locks most HA-originating commands until a valid remote frame establishes Around U is OFF, and while Around U is ON. Explicit OFF remains allowed. It never toggles the remote's Around U feature. Generic mode deliberately has no vendor-specific Around U lock.

**Current Temperature caveat:** The ESPHome climate entity may publish `0.00°C` even when no valid remote measurement is available. Do not use that climate attribute for thermostatic automation; rely on the separate Around U temperature sensor, which reports unavailable when disabled. This is a known v1.0.0 limitation.

## TTGO physical buttons

Normal: F short temp+, F long fan cycle, L short temp−, L long HVAC mode cycle. During learning: F short cancel, F long next match, L short save, L long cancel. The physical power button is exposed as a Home Assistant button in the TTGO example.

## Build and versioning

The example uses `ref: v1.0.0`, currently maintained as a **version branch** at the release snapshot, rather than an annotated Git tag. Do not move this branch after publishing; a formal GitHub release/tag should be created separately when the repository owner finalizes distribution. For development only, change `ref` to a named feature branch and optionally set `refresh: 0s`. The upstream Bitcode library currently resolves from a GitHub URL without a pinned SHA, so the complete build is **not yet fully reproducible**.

See [hardware](HARDWARE.md), [learning](LEARNING.md), and [architecture](ARCHITECTURE.md).
