# pylint: disable=line-too-long, invalid-name, missing-function-docstring, missing-module-docstring, too-many-branches, pointless-statement

import esphome.codegen as cg
from esphome.components import sensor, text_sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
    DEVICE_CLASS_EMPTY,
    STATE_CLASS_MEASUREMENT,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor"]

# using abbrev 'BPC' for 'BIOS POST Codes'
CONF_BPC_ID = "bpc_id"

# sensors
CONF_BPC_CODE = "code"
CONF_BPC_CODE_TEXT ="code_text"

ICON_BCP_CODE= "mdi:numeric"

bpc_ns = cg.esphome_ns.namespace("bpc")
BPC_COMPONENT = bpc_ns.class_("BPC", cg.Component)

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema(
{
  cv.GenerateID(): cv.declare_id(BPC_COMPONENT),
  cv.Optional(CONF_BPC_CODE): sensor.sensor_schema(
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    icon=ICON_BCP_CODE,
    device_class=DEVICE_CLASS_EMPTY,
    state_class=STATE_CLASS_MEASUREMENT,
  ),
  cv.Optional(CONF_BPC_CODE_TEXT): text_sensor.text_sensor_schema(
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    icon=ICON_BCP_CODE,
    device_class=DEVICE_CLASS_EMPTY,
  ),
}
).extend(uart.UART_DEVICE_SCHEMA)
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema("bpc", require_rx=True)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if bpc_code_config := config.get(CONF_BPC_CODE):
        sens = await sensor.new_sensor(bpc_code_config)
        cg.add(var.set_post_code_sensor(sens))

    if bpc_code_text_config := config.get(CONF_BPC_CODE_TEXT):
        sens = await text_sensor.new_text_sensor(bpc_code_text_config)
        cg.add(var.set_post_code_text_sensor(sens))

    # ----- General compiler settings
    # treat warnings as error, abort compilation on first error, check printf format and arguments
    cg.add_build_flag("-Werror -Wfatal-errors -Wformat=2")
    # relax some warnings from being treated as errors due to esphome code
    cg.add_build_flag("-Wno-format-nonliteral -Wno-format")
    # optimize for speed
    cg.add_build_flag("-O2")
    # sanitizers
    cg.add_build_flag("-fstack-protector-all")
