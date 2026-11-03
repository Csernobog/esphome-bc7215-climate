import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import climate

AUTO_LOAD = ["climate"]
ns = cg.esphome_ns.namespace("bc7215_climate")
BC7215Climate = ns.class_("BC7215Climate", climate.Climate, cg.Component)

CONF_UART_RX_PIN = "uart_rx_pin"
CONF_UART_TX_PIN = "uart_tx_pin"
CONF_MOD_PIN = "mod_pin"
CONF_BUSY_PIN = "busy_pin"


def validate_pin_uniqueness(config):
    values = [
        config[CONF_UART_RX_PIN],
        config[CONF_UART_TX_PIN],
        config[CONF_MOD_PIN],
        config[CONF_BUSY_PIN],
    ]
    if len(set(values)) != len(values):
        raise cv.Invalid("BC7215 uart_rx_pin, uart_tx_pin, mod_pin and busy_pin must be distinct")
    return config


CONFIG_SCHEMA = cv.All(
    climate.climate_schema(BC7215Climate).extend(
        {
            cv.Optional(CONF_UART_RX_PIN, default=25): pins.internal_gpio_input_pin_number,
            cv.Optional(CONF_UART_TX_PIN, default=33): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_MOD_PIN, default=27): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_BUSY_PIN, default=26): pins.internal_gpio_input_pin_number,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_pin_uniqueness,
)


async def to_code(config):
    var = await climate.new_climate(
        config,
        config[CONF_UART_RX_PIN],
        config[CONF_UART_TX_PIN],
        config[CONF_MOD_PIN],
        config[CONF_BUSY_PIN],
    )
    await cg.register_component(var, config)
    cg.add_library("Preferences", None)
    cg.add_library("https://github.com/bitcode-tech/bc7215ac.git", None)
