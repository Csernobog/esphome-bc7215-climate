# IR learning and NVS

## Pairing sequence

1. If an existing profile is working, check it still controls/decodes the A/C before attempting replacement.
2. On the **physical remote**, select **COOL, 25 °C**. The mode symbol can be hard to see; verify by an ordinary IR command in the ESPHome log where `mode=1` corresponds to COOL for the tested Whirlpool.
3. Home Assistant → **IR learning start**. The BC7215 enters capture mode, with a 30-second timeout.
4. Aim the remote at the BC7215 and press **FAN SPEED** once.
5. Wait for `IR LEARN ... ACCEPTED`. A `REJECTED` result cancels and reloads the old profile.
6. Review match index. For the tested Whirlpool, `match 0` worked; other models may need the **next match** operation.
7. Press **IR learning save**, or **cancel** to keep the old pairing.

The TTGO example also offers button shortcuts while reviewing: F short cancels, L short saves, F long selects next match.

**Never save a rejected/unverified learning attempt.** Saving does not transmit IR or power off the physical A/C, but the local *estimated* state is set to OFF.

## Persistent storage

- Active protocol: ESP32 NVS namespace `bc7215 init`, keys `format`, `data`, `matchIndex`, `tempUnit`.
- Previous active profile backup: `bc7215_prev` (same keys), written before an explicit save when there is an existing profile.
- OTA state snapshot: `bc7215_ota` (independent of pairing).

The save path writes the active profile, reads it back for byte equality, reinitializes the decoder, and attempts rollback on failure. It is **not an atomic transaction across NVS keys**: unexpected power loss mid-save is not fully protected. The previous profile is separately backed up, but automated recovery on subsequent cold boot from `bc7215_prev` is not yet implemented.

Do **not** erase NVS, flash with `erase_flash`, or reformat the device during ordinary ESPHome updates. OTA ordinarily preserves pairing.

## Validation after saving

1. Confirm logs: `previous NVS profile backup OK`, `NVS readback OK`, `SAVED AND VERIFIED`.
2. Send normal commands using the physical remote; confirm that mode, fan and setpoint update in HA.
3. Restart and confirm `IR paired` and `IR protocol identity`; then test receiving another IR command.
4. Test HA→IR transmission only after Around U is confirmed OFF and the physical device is in a known state.

## Whirlpool Around U

Around U is **read only**. The physical remote manages it and sends periodic measurements while active. HA commands are intentionally restricted while it is ON or not yet known following boot; explicit OFF is allowed. The separate temperature sensor shows unavailable/NaN when disabled.

## Observed learning caveat

The tested Whirlpool decoder repeatedly refused samples from the wrong remote mode and accepted the sample only when the remote was **genuinely** in COOL at 25 °C. The protocol identification describes the IR encoding, not an appliance's unique serial number or recognized manufacturer.
