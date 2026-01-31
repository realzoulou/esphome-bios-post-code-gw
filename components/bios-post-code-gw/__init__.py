# pylint: disable=line-too-long, invalid-name, missing-function-docstring, missing-module-docstring, too-many-branches, pointless-statement

import esphome.codegen as cg
from esphome.components import sensor, time, text_sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_TIME_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
    DEVICE_CLASS_EMPTY,
    STATE_CLASS_MEASUREMENT,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor", "time"]

# using abbrev 'BPC' for 'BIOS POST Codes'
CONF_BPC_ID = "bpc_id"

# sensors
CONF_BPC_CODE = "code"
CONF_BPC_CODE_TEXT ="code_text"

# configurations
CONF_BPC_POST_CODE_DESCRIPTIONS = "post_code_descriptions"
CONF_BPC_POST_CODE_HEX = "hexcode"
CONF_BPC_POST_CODE_DESC = "desc"
CONF_BPC_POST_CODE_IGNORE_LIST = "ignore"

ICON_BCP_CODE = "mdi:numeric"

bpc_ns = cg.esphome_ns.namespace("bpc")
BPC_COMPONENT = bpc_ns.class_("BPC", cg.Component)

MULTI_CONF = True

# POST code descriptions schema
BPC_DESCRIPTIONS_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_BPC_POST_CODE_HEX): cv.hex_int_range(min=0, max=255),
        cv.Required(CONF_BPC_POST_CODE_DESC): str,
    }
)

# main BPC schema
CONFIG_SCHEMA = cv.Schema(
  {
    cv.GenerateID(): cv.declare_id(BPC_COMPONENT),
    cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
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
    cv.Optional(CONF_BPC_POST_CODE_DESCRIPTIONS): cv.All(cv.ensure_list(BPC_DESCRIPTIONS_SCHEMA)),
    cv.Optional(CONF_BPC_POST_CODE_IGNORE_LIST): cv.ensure_list(cv.hex_int_range(min=0, max=255)),
  }
).extend(uart.UART_DEVICE_SCHEMA)
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema("bpc", require_rx=True)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_TIME_ID in config:
        time_id = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_realtime(time_id))

    if bpc_code_config := config.get(CONF_BPC_CODE):
        sens = await sensor.new_sensor(bpc_code_config)
        cg.add(var.set_post_code_sensor(sens))

    if bpc_code_text_config := config.get(CONF_BPC_CODE_TEXT):
        sens = await text_sensor.new_text_sensor(bpc_code_text_config)
        cg.add(var.set_post_code_text_sensor(sens))

    if bpc_code_descriptions := config.get(CONF_BPC_POST_CODE_DESCRIPTIONS):
        for i in bpc_code_descriptions:
            hexcode = i[CONF_BPC_POST_CODE_HEX]
            desc = i[CONF_BPC_POST_CODE_DESC]
            cg.add(var.set_code_and_description(hexcode, desc))

    if bpc_ignore_list := config.get(CONF_BPC_POST_CODE_IGNORE_LIST):
        for i in bpc_ignore_list:
            cg.add(var.set_code_ignored(i))

    # ----- General compiler settings
    # treat warnings as error, abort compilation on first error, check printf format and arguments
    cg.add_build_flag("-Werror -Wfatal-errors -Wformat=2")
    # relax some warnings from being treated as errors due to esphome code
    cg.add_build_flag("-Wno-format-nonliteral -Wno-format")
    # optimize for speed
    cg.add_build_flag("-O2")
    # sanitizers
    cg.add_build_flag("-fstack-protector-all")
