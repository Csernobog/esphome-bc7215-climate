import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.const import CONF_ID

AUTO_LOAD = ["climate"]
ns = cg.esphome_ns.namespace("bc7215_climate")
BC7215Climate = ns.class_("BC7215Climate", climate.Climate, cg.Component)
CONFIG_SCHEMA = climate.climate_schema(BC7215Climate).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    cg.add_library("Preferences", None)
    cg.add_library("https://github.com/bitcode-tech/bc7215ac.git", None)
