#include "esphome/core/log.h"
#include "esphome/core/time.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "bpc.h"

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
    (void)UARTDevice::read_byte(&data);
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
      this->update_post_code(data, ts);

      if (!this->highFreqLoopRequester_.is_high_frequency()) {
        this->highFreqLoopRequester_.start();
      }
    }
  }

  // end high frequency loop mode after 200ms from last received post code
  const uint32_t endTs = millis();
  if (endTs > (this->last_post_code_timestamp_ + 200U)) {
    if (this->highFreqLoopRequester_.is_high_frequency()) {
       this->highFreqLoopRequester_.stop();
    }
  }
}

void BPC::do_realtime_self_check() const {
  for (unsigned i=1; i <=11; i++) {
    // sleep 100ms
    esphome::delay(100U);

    std::stringstream ss;
    struct timespec ts_mono, ts_real;
    int ret_mono, ret_real;
    ESPTime espRealTime = {0}; // all members set to 0

    uint32_t millisec = millis();
    ret_mono = ::clock_gettime(CLOCK_MONOTONIC, &ts_mono);
    ret_real = ::clock_gettime(CLOCK_REALTIME, &ts_real);
    if (this->real_time_) {
      espRealTime = this->real_time_->now();
    }

    // save default formatting
    std::ios_base::fmtflags def_format(ss.flags());

    ss << "#" << std::setfill(' ') << std::setw(2) << +i << "| ";
    ss.flags(def_format);
    ss << " millis(): " << +millisec;
    ss << ", MONOTONIC: ";
    if (ret_mono == 0) {
      ss << +(ts_mono.tv_sec) << "." << std::setfill('0') << std::setw(3) << +(ts_mono.tv_nsec / 1000000U) << "";
    } else {
      ss << "failed";
    }
    ss << ", REALTIME: ";
    if (ret_real == 0) {
      ss << +(ts_real.tv_sec) << "." << std::setfill('0') << std::setw(3) << +(ts_real.tv_nsec / 1000000U) << "";
    } else {
      ss << "failed";
    }
    ss << ", Real ESPTime local: ";
    if (espRealTime.is_valid()) {
      char buf[ESPTime::STRFTIME_BUFFER_SIZE];
      std::size_t len = espRealTime.strftime_to(buf, "%H:%M:%S");
      if (len > 0) {
        ss << buf;
      } else {
        ss << "???";
      }
    } else {
      ss << "invalid";
    }
    ESP_LOGI(LOGTAG, "%s", ss.str().c_str());
  } // for loop
}

void BPC::update_post_code(const uint8_t code, const uint32_t timestamp) {
  const uint32_t deltaToLast = timestamp - this->last_post_code_timestamp_;

  // if code is on ignore list, return but print to log
  if (std::find(this->codes_ignored_.begin(), this->codes_ignored_.end(), code) != this->codes_ignored_.end()) {
      ESP_LOGI(LOGTAG, "POST 0x%02X @ %u | %u ms | ignored", code, timestamp, deltaToLast);
      return;
  } else {
      ESP_LOGI(LOGTAG, "POST 0x%02X @ %u | %u ms", code, timestamp, deltaToLast);
  }

  if (this->post_code_sensor_ != nullptr) {
    this->post_code_sensor_->publish_state(static_cast<float>(code));
  }
  if (this->post_code_text_sensor_ != nullptr) {
    const std::string s = this->format_post_code_text_sensor(code, timestamp, deltaToLast);
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
  // if a real time source is available and has valid time, add it
  std::string realTimeString = this->get_realtime(timestamp);
  if (!realTimeString.empty()) {
    ss << " | " << realTimeString;
  }

  // add delta time to last POST code
    ss << " | Δ "
     << +deltaToLast
     << " ms"
    ;

  // if a description for that code is available, add it
  if (const auto search = this->code_descriptions_.find(code); search != this->code_descriptions_.end()) {
    ss << " | " << search->second;
  }

  return ss.str();
}

std::string BPC::get_realtime(const uint32_t timestamp) const {
  std::string str;

  if (this->real_time_ != nullptr) { // see if we can expect a valid CLOCK_REALTIME
    struct timespec ts_real;
    /* keep millis() and clock_gettime() together closely to have precise time measurements */
    const uint32_t nowTimestamp = millis();
    int ret = ::clock_gettime(CLOCK_REALTIME, &ts_real);
    const uint32_t timestampDiff = nowTimestamp - timestamp;

    if (ret != -1) {
      uint32_t real_time_millis = static_cast<uint32_t>(ts_real.tv_nsec) / 1000000U;
      // warn if tv_nsec was bigger than expected and clamp it at 999 ms
      if (real_time_millis > 999U) {
        real_time_millis = 999U;
        ESP_LOGW(LOGTAG, "clock_gettime(REALTIME) %us %uns (clamping to %ums)", ts_real.tv_sec, ts_real.tv_nsec, real_time_millis);
      }

      // reduce ts_real and real_time_millis by timestampDiff
      if (real_time_millis >= timestampDiff) {
        // within a second, easy...
        real_time_millis -= timestampDiff;
        // no change to ts_real.tv_sec
      } else {
        // affects seconds
        // e.g. current real time: 55s 100ms, timestampDiff: 150ms; results in 54s 950ms
        real_time_millis = (real_time_millis + 1000U) - timestampDiff;
        ts_real.tv_sec -= 1; // reduce 1 sec
      }
      ESPTime espRealTime = ESPTime::from_epoch_local(ts_real.tv_sec);
      if (espRealTime.is_valid()) {
        // format string HH:MM:SS.SSS
        char buf[ESPTime::STRFTIME_BUFFER_SIZE];
        std::size_t len = espRealTime.strftime_to(buf, "%H:%M:%S");
        if (len > 0) {
          std::stringstream ss;
          ss << buf << ".";
          ss << std::setfill('0') << std::setw(3) << +real_time_millis;
          str = ss.str();
        }
      } else {
        ESP_LOGW(LOGTAG, "from_epoch_local(%u) failed is_valid()", ts_real.tv_sec);
      }
    }
  }
  return str;
}

} // namespace bpc
} // namespace esphome
