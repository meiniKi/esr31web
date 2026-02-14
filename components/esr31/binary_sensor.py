import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID
from . import esr31_ns, ESR31Component, CONF_ESR31_ID

DEPENDENCIES = ['esr31']

CONF_BINARY_OUTPUT = 'binary_output'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_ESR31_ID): cv.use_id(ESR31Component),
    cv.Optional(CONF_BINARY_OUTPUT): binary_sensor.binary_sensor_schema(),
})


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ESR31_ID])
    
    if CONF_BINARY_OUTPUT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_BINARY_OUTPUT])
        cg.add(parent.set_binary_sensor(sens))
