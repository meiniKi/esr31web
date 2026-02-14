import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN

DEPENDENCIES = ['arduino']
AUTO_LOAD = ['sensor', 'binary_sensor']

esr31_ns = cg.esphome_ns.namespace('esr31')
ESR31Component = esr31_ns.class_('ESR31Component', cg.Component)

CONF_ESR31_ID = 'esr31_id'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ESR31Component),
    cv.Required(CONF_PIN): pins.internal_gpio_input_pin_schema,
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
