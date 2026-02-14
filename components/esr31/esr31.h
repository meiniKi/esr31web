#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace esr31 {

// State machine states
enum ESR31RxState {
  STATE_ERR,
  STATE_INIT,
  STATE_SYNC,
  STATE_SYNC_DONE,
  STATE_REC,
  STATE_DONE
};

// Position within one data byte
enum RxBytePos {
  POS_START,
  POS_DATA,
  POS_STOP
};

static const uint8_t NR_DATA_BYTES = 31;  // number of data bytes in one frame

class ESR31Component : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }
  
  void set_sensor1(sensor::Sensor *sensor) { sensor1_ = sensor; }
  void set_sensor2(sensor::Sensor *sensor) { sensor2_ = sensor; }
  void set_sensor3(sensor::Sensor *sensor) { sensor3_ = sensor; }
  void set_binary_sensor(binary_sensor::BinarySensor *sensor) { binary_sensor_ = sensor; }

 protected:
  InternalGPIOPin *pin_{nullptr};
  sensor::Sensor *sensor1_{nullptr};
  sensor::Sensor *sensor2_{nullptr};
  sensor::Sensor *sensor3_{nullptr};
  binary_sensor::BinarySensor *binary_sensor_{nullptr};
  
  // Timing constants (in microseconds)
  static constexpr uint32_t BIT_SPACING_MICROS = 2049;
  static constexpr uint32_t BIT_SPACING_TOL_MICROS = 200;
  static constexpr uint32_t BIT_SPACING_MIN_MICROS = BIT_SPACING_MICROS - BIT_SPACING_TOL_MICROS;
  static constexpr uint32_t BIT_SPACING_MAX_MICROS = BIT_SPACING_MICROS + BIT_SPACING_TOL_MICROS;
  
  // State machine variables
  ESR31RxState state_{STATE_INIT};
  uint16_t hsync_{0};           // number of sync edges received
  uint32_t last_micros_{0};     // micros at last sampling point
  uint32_t next_retry_time_{0}; // timestamp for next retry after error/done
  
  // Data reception variables
  uint8_t rx_data_[NR_DATA_BYTES];  // receive buffer
  uint16_t rx_bits_{0};             // number of bits received since frame start
  RxBytePos rx_pos_{POS_START};     // position in one receive byte
  
  // ISR storage for edge detection
  ISRInternalGPIOPin isr_pin_;
  
  void handle_decoding();
  void process_bit(uint8_t val);
  void parse_frame();
  void publish_values();
  
  static void gpio_intr(ESR31Component *arg);
  void pin_sync_received();
  void pin_data_received();
};

}  // namespace esr31
}  // namespace esphome
