#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

// using abbrev 'BPC' for 'BIOS POST Codes'

namespace esphome {
  namespace bpc {

  class BPC : public esphome::Component, public uart::UARTDevice {
    public:
      float get_setup_priority() const override { return setup_priority::HARDWARE; }
      void setup() override;
      void loop() override;

      void set_post_code_sensor(sensor::Sensor *sensor) { this->post_code_sensor_ = sensor; }
      void set_post_code_text_sensor(text_sensor::TextSensor *sensor) { this->post_code_text_sensor_ = sensor; }
      void set_realtime(time::RealTimeClock *time) { this->real_time_ = time; }
      void do_realtime_self_check() const; // Note: method blocks caller for at least 1s

    private: // methods
      /** Update sensor with new post code received at timestamp
      * @param code POST code
      * @param timestamp milliseconds from boot
      */
      void update_post_code(const uint8_t code, const uint32_t timestamp);

      /** Format text sensor string with post code received at timestamp
      * @param code POST code
      * @param timestamp milliseconds from boot
      * @param deltaToLast delta timestamp in milliseconds from last POST code
      */
      std::string format_post_code_text_sensor(const uint8_t code, const uint32_t timestamp, const uint32_t deltaToLast);

      /** If a valid real-time source is available, return a string representation of the given
       *  timestamp as real time (incl. milliseconds) in current timezone. Format is HH:MM:SS.SSS
       * @return on error, the string is empty
       */
      std::string get_realtime(const uint32_t timestamp) const;

    private: // attributes
      HighFrequencyLoopRequester highFreqLoopRequester_;
      sensor::Sensor          * post_code_sensor_{nullptr};
      text_sensor::TextSensor * post_code_text_sensor_{nullptr};
      time::RealTimeClock * real_time_{nullptr};
      // last received POST code value
      uint8_t  last_post_code_value_{0};
      // timestamp when last POST code value was received in milliseconds from boot
      uint32_t last_post_code_timestamp_{0};
  }; // class BPC

  } // namespace esphome
} // namespace bpc
