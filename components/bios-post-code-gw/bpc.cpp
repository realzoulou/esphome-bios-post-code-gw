#include "esphome/core/log.h"
#include "bpc.h"

#include <iomanip>
#include <sstream>

// using abbrev 'BPC' for 'BIOS POST Codes'

namespace esphome {
namespace bpc {

static const char *const LOGTAG = "bpc";

void BPC::setup() {
  const uint32_t startTs = millis();
  // ensure sensor state is unavailable
  if (this->post_code_sensor_ != nullptr) {
    this->post_code_sensor_->set_has_state(false);
  }

  // ignore any RX buffer content at cold boot
  uint32_t cnt = 0;
  uint8_t data;
  while (    (UARTDevice::available() > 0)
          && (millis() < (startTs + 100U)) // end loop after 100ms
        )
  {
    read_byte(&data);
    cnt++;
  }
  if (cnt > 0) {
    ESP_LOGW(LOGTAG, "cleared %u RX bytes", cnt);
  }
}

void BPC::loop() {
  const uint32_t startTs = millis();
  uint32_t ts;
  uint8_t data = 0xFF;
  while (    (UARTDevice::available() > 0)
          && (millis() < (startTs + 100U)) // end loop after 100ms
        )
  {
    if (UARTDevice::read_byte(&data)) {
      ts = millis();
      update_post_code(data, ts);

      if (!highFreqLoopRequester_.is_high_frequency()) {
        highFreqLoopRequester_.start();
      }
    }
  }

  // end high frequency loop mode after 200ms from last received post code
  const uint32_t endTs = millis();
  if (endTs > (this->last_post_code_timestamp_ + 200U)) {
    if (highFreqLoopRequester_.is_high_frequency()) {
       highFreqLoopRequester_.stop();
    }
  }
}

void BPC::update_post_code(const uint8_t code, const uint32_t timestamp) {
  const uint32_t deltaToLast = timestamp - this->last_post_code_timestamp_;
  ESP_LOGI(LOGTAG, "POST 0x%02X @ %u | %u ms", code, timestamp, deltaToLast);

  if (this->post_code_sensor_ != nullptr) {
    this->post_code_sensor_->publish_state(static_cast<float>(code));
  }
  if (this->post_code_text_sensor_ != nullptr) {
    const std::string s = format_post_code_text_sensor(code, timestamp, deltaToLast);
    this->post_code_text_sensor_->publish_state(s);
  }
  this->last_post_code_value_     = code;
  this->last_post_code_timestamp_ = timestamp;
}

std::string BPC::format_post_code_text_sensor(const uint8_t code, const uint32_t timestamp, const uint32_t deltaToLast) {
  std::stringstream ss;

  // format POST code
  ss << std::hex << std::uppercase << std::noshowbase << std::setfill('0') << std::setw(2)
     << +code
     << std::dec << std::nouppercase
     << "h"
  ;

  // optionally add delta time to last POST code, if delta time is less than 5s
  if (deltaToLast < 5000U) {
    ss << " | "
     << +deltaToLast
     << " ms"
    ;
  }

  return ss.str();
}

} // namespace bpc
} // namespace esphome
