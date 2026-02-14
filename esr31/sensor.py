import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)
from . import esr31_ns, ESR31Component, CONF_ESR31_ID

DEPENDENCIES = ['esr31']

CONF_SENSOR1 = 'sensor1'
CONF_SENSOR2 = 'sensor2'
CONF_SENSOR3 = 'sensor3'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_ESR31_ID): cv.use_id(ESR31Component),
    cv.Optional(CONF_SENSOR1): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_SENSOR2): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_SENSOR3): sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
})


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ESR31_ID])
    
    if CONF_SENSOR1 in config:
        sens = await sensor.new_sensor(config[CONF_SENSOR1])
        cg.add(parent.set_sensor1(sens))
    
    if CONF_SENSOR2 in config:
        sens = await sensor.new_sensor(config[CONF_SENSOR2])
        cg.add(parent.set_sensor2(sens))
    
    if CONF_SENSOR3 in config:
        sens = await sensor.new_sensor(config[CONF_SENSOR3])
        cg.add(parent.set_sensor3(sens))
