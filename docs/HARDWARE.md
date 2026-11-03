# Hardware and wiring

Tested target: **classic ESP32 TTGO T-Display (135×240 ST7789V)** and BC7215 IR transceiver. ESPHome uses Arduino framework and a hardware UART.

| ESP32 | BC7215 module | Direction |
| --- | --- | --- |
| GPIO25 (UART RX) | TX | BC7215 → ESP32 |
| GPIO33 (UART TX) | RX | ESP32 → BC7215 |
| GPIO27 | MOD / mode | ESP32 → BC7215 |
| GPIO26 | BUSY | BC7215 → ESP32 |
| 3V3 | 3.3 V VCC | Supply |
| GND | GND | Shared ground |

The serial bus is **19200 baud, 8 data bits, no parity, 2 stop bits** (`SERIAL_8N2`). Module must be suitable for **3.3 V logic**. Check your specific board's silkscreen before connecting; GPIO numbering refers to the ESP32.

The pin map is configurable in the ESPHome `climate:` block. The defaults match the tested TTGO wiring:

```yaml
climate:
  - platform: bc7215_climate
    name: Klima
    uart_rx_pin: GPIO25
    uart_tx_pin: GPIO33
    mod_pin: GPIO27
    busy_pin: GPIO26
```

The four GPIOs must be distinct. ESPHome checks input/output capabilities of the chosen ESP32 pins. Use **internal** GPIOs (not I/O expanders) and avoid conflicts with other peripherals. Omitting the four options preserves the original pins.

### TTGO T-Display

The provided example uses SPI CLK GPIO18, MOSI GPIO19, display backlight GPIO4, physical buttons GPIO0 and GPIO35, and a 135×240 ST7789V screen. Both keys have short/long-press actions. GPIO0 is an ESP32 boot strap pin; avoid holding it down while resetting the board.

See `examples/ttgo_t_display.yaml` for the tested interface and HA entities.

### Safety

Do not attach BC7215 logic pins to 5 V ESP32 GPIO. Power off both devices before rewiring. The integration sends IR commands to a real HVAC system; test cautiously.
