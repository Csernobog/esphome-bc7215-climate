# Changelog

## v1.0.0 — 2026-11-03

First standalone ESPHome BC7215 climate release for classic ESP32 + Arduino.

### Features
- ESPHome climate entity and IR receive/transmit through BC7215AC library.
- Guided COOL 25 °C IR training; review candidate, next match, explicit save/cancel.
- Persistent NVS pairing with previous-profile backup, readback check and rollback attempt.
- OTA estimated-state snapshot (independent from persistent pairing).
- Four configurable pins: UART RX/TX, MOD and BUSY, with original TTGO defaults.
- Selectable `ac_profile: whirlpool` and `ac_profile: generic`.
- Whirlpool Around U detection, remote temperature, and safety lock.
- Home Assistant diagnostics and TTGO T-Display example.

### Fixes
- Accept valid Whirlpool frames with variable mode/setpoint byte (not just `0x60`/`0x70`); restores Around U ON/OFF and 20 °C reporting.
- Remove temporary IR crosscheck and full HEX diagnostic dumps.

### Validation
- Independently compiled and OTA-installed from the standalone GitHub repository on ESPHome 2026.9.1.
- Verified IR remote HEAT, fan AUTO/HIGH/MEDIUM/LOW, learning NVS backup/readback, saved profile decode, restart identity, and Around U OFF/ON/OFF.
- Other AC brands and alternative GPIO wiring: not yet physically tested.

### Known limitations
- HA Climate current temperature can display 0.00 °C without an active measurement.
- IR capture/stop can block component loop for roughly 100 ms.
- Power-toggle estimation may desynchronize when IR packets are missed.
- NVS updates are not power-loss atomic; automatic recovery from previous pairing not implemented.
- The upstream BC7215AC dependency is not pinned and has a hardware-restricted license.
