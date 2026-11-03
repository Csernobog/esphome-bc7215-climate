# Design notes and limitations

## Components

- `BC7215Climate`: ESPHome `climate::Climate` + `Component`; owns UART1, BC7215 module, `BC7215AC` decoder.
- Normal receive: `startCapture` → `signalCaptured` → `parse` → HA climate state.
- Learn: `IDLE` → `CAPTURE` → `REVIEW` → explicit SAVE/CANCEL.
- Persisted pairing: `Preferences` NVS with original library-compatible packed binary structures.
- HA diagnostics: protocol identity, IR TX/RX counters, timeouts and Around U read-only sensors exposed by example YAML.

## Compatibility

The BC7215AC dependency implements the actual A/C IR protocol recognition. It uses an IR reference frame captured from the remote. Supported brands/models are constrained by that library and available frames; **the integration does not provide universal support for every IR A/C**. There is no manufacturer/model recognition API for arbitrary learned signals, only a signature, bit length, and match index.

### Profile selection

`ac_profile: whirlpool` (default) keeps the existing Whirlpool-specific Around U decoding and HA safety lock exactly as tested. `ac_profile: generic` skips Whirlpool frame interpretation and disables this vendor-specific lock for regular IR A/C operations. In generic mode the Around U state remains unknown and its temperature is unavailable; examples should omit its entities. The profile is chosen at compile time by ESPHome config, not stored in NVS. The pairing payload and storage format are unchanged.

This does not automatically provide full interoperability for every manufacturer, nor does it establish physical power-state feedback. In particular the decoder-level `power=2` toggle remains an estimate in both profiles.

### Whirlpool-specific interpretation

`whirlpool_frame_` detects an observed 168-bit Whirlpool frame starting `83 06` and validates its XOR checksum. `decode_around_u_` interprets bytes 11–13 of this observed family. Other brands will not produce Whirlpool Around U state and may remain conservatively locked for HA control. **This must be generalized/configurable before claiming universal A/C support.**

For the tested Whirlpool, power value `2` is a **toggle** command. The firmware estimates ON/OFF by flipping the prior observed state; a missed remote signal or physical AC action can invalidate that estimate.

## Known pending engineering tasks

1. Make UART, MOD and BUSY GPIO pins configurable in the ESPHome schema (currently fixed).
2. Add genuinely configurable, per-brand power-state mappings and extended profile implementations.
3. Audit `Current Temperature: 0.00°C`; show unavailable when no valid measurement exists.
4. Reduce blocking `stopCapture`/UART calls that exceed ESPHome's 50 ms component time budget.
5. Add a recovery button for `bc7215_prev` and review failure behavior on partial NVS writes.
6. Pin a tested upstream BC7215AC library revision and run independent build/OTA tests on this repository.
7. Review multi-segment protocols and non-Whirlpool devices.
8. Define release licensing, version tags and CI build smoke checks.

No claim of CI or hardware test is made for the *extracted* repository until it is tested independently.
