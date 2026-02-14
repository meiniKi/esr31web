#include "esr31.h"
#include "esphome/core/log.h"

namespace esphome {
namespace esr31 {

static const char *const TAG = "esr31";

void ESR31Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESR31...");
  
  if (this->pin_ != nullptr) {
    this->pin_->setup();
    this->isr_pin_ = this->pin_->to_isr();
  }
  
  // Initialize state machine
  this->state_ = STATE_INIT;
}

void ESR31Component::loop() {
  this->handle_decoding();
}

void ESR31Component::handle_decoding() {
  uint32_t now = millis();
  
  switch (this->state_) {
    case STATE_INIT:
      // Initialize all values to be ready to receive a frame
      this->pin_->detach_interrupt();
      this->rx_bits_ = 0;
      this->rx_pos_ = POS_START;
      this->hsync_ = 0;
      this->last_micros_ = 0;
      for (uint16_t i = 0; i < NR_DATA_BYTES; i++) {
        this->rx_data_[i] = 0x00;
      }
      // Attach interrupt for sync detection (falling edge)
      this->state_ = STATE_SYNC;
      this->pin_->attach_interrupt(ESR31Component::gpio_intr, this, gpio::INTERRUPT_FALLING_EDGE);
      ESP_LOGD(TAG, "State: INIT -> SYNC");
      break;

    case STATE_SYNC:
      // Wait for sync to be received (handled in ISR)
      break;

    case STATE_SYNC_DONE:
      // Sync received, prepare receiving data
      this->pin_->detach_interrupt();
      // Attach interrupt for data reception (any edge)
      this->state_ = STATE_REC;
      this->pin_->attach_interrupt(ESR31Component::gpio_intr, this, gpio::INTERRUPT_ANY_EDGE);
      ESP_LOGD(TAG, "State: SYNC_DONE -> REC");
      break;

    case STATE_REC:
      // Wait for data to be received (handled in ISR)
      break;

    case STATE_ERR:
      // There was some error - wait 10 seconds before retry
      this->pin_->detach_interrupt();
      if (this->next_retry_time_ == 0) {
        // First time in error state, set retry time
        ESP_LOGW(TAG, "Error in decoding, will retry in 10 seconds...");
        this->next_retry_time_ = now + 10000;
      } else if (now >= this->next_retry_time_) {
        // Time to retry
        this->next_retry_time_ = 0;
        this->state_ = STATE_INIT;
      }
      break;

    case STATE_DONE:
      // Full frame received, parse and publish - wait 10 seconds before next frame
      this->pin_->detach_interrupt();
      if (this->next_retry_time_ == 0) {
        // First time in done state, parse and set next frame time
        ESP_LOGD(TAG, "Frame received");
        this->parse_frame();
        this->next_retry_time_ = now + 10000;
      } else if (now >= this->next_retry_time_) {
        // Time for next frame
        this->next_retry_time_ = 0;
        this->state_ = STATE_INIT;
      }
      break;
  }
}

void IRAM_ATTR ESR31Component::gpio_intr(ESR31Component *arg) {
  if (arg->state_ == STATE_SYNC) {
    arg->pin_sync_received();
  } else if (arg->state_ == STATE_REC) {
    arg->pin_data_received();
  }
}

void IRAM_ATTR ESR31Component::pin_sync_received() {
  uint32_t now = micros();
  
  if (this->last_micros_ != 0UL && now > this->last_micros_) {
    uint32_t dt = now - this->last_micros_;
    if (dt > BIT_SPACING_MIN_MICROS && dt < BIT_SPACING_MAX_MICROS) {
      // Another edge of sync-type
      this->hsync_++;
    } else {
      // Not another ONE - maybe not the sync block?
      this->hsync_ = 0;
    }
  } else if (now < this->last_micros_) {
    // micros() overflowed, ignore this frame
    this->state_ = STATE_INIT;
    return;
  }

  if (this->hsync_ >= 16) {
    this->state_ = STATE_SYNC_DONE;
  }
  this->last_micros_ = now;
}

void IRAM_ATTR ESR31Component::pin_data_received() {
  uint32_t now = micros();
  uint8_t val = !this->isr_pin_.digital_read();

  if (now < this->last_micros_) {
    // micros() overflowed, ignore this frame
    this->state_ = STATE_INIT;
    return;
  }

  uint32_t dt = now - this->last_micros_;
  if (dt > BIT_SPACING_MIN_MICROS && dt < BIT_SPACING_MAX_MICROS) {
    // Edge within sampling region, take sample
    this->process_bit(val);
    this->last_micros_ = now;
    if (this->rx_bits_ >= (NR_DATA_BYTES << 3)) {
      this->state_ = STATE_DONE;
    }
    return;
  }
  
  if (dt > BIT_SPACING_MAX_MICROS) {
    // Timeout! There must have been an edge
    this->state_ = STATE_ERR;
  }
}

void IRAM_ATTR ESR31Component::process_bit(uint8_t val) {
  if (this->rx_pos_ == POS_START) {
    // Start bit
    this->rx_pos_ = POS_DATA;
    if (val != 0) {
      this->state_ = STATE_ERR;
    }
    return;
  }

  if (this->rx_pos_ == POS_DATA) {
    this->rx_data_[this->rx_bits_ / 8] |= (val != 0) << (this->rx_bits_ % 8);

    if ((this->rx_bits_ % 8) == 7) {
      // Last data bit
      this->rx_pos_ = POS_STOP;
    }
    this->rx_bits_++;
    return;
  }

  if (this->rx_pos_ == POS_STOP) {
    // Stop bit
    this->rx_pos_ = POS_START;
    if (val != 1) {
      this->state_ = STATE_ERR;
    }
    return;
  }
}

void ESR31Component::parse_frame() {
  // Validate frame header
  if (this->rx_data_[0] != 0x70 || this->rx_data_[1] != 0x8f) {
    ESP_LOGW(TAG, "Invalid frame header: 0x%02X 0x%02X", this->rx_data_[0], this->rx_data_[1]);
    return;
  }

  // Validate checksum
  uint8_t checksum = 0;
  for (uint16_t i = 0; i < NR_DATA_BYTES - 1; i++) {
    checksum += this->rx_data_[i];
  }
  checksum &= 0xFF;
  
  if (checksum != this->rx_data_[NR_DATA_BYTES - 1]) {
    ESP_LOGW(TAG, "Checksum mismatch: expected 0x%02X, got 0x%02X", 
             checksum, this->rx_data_[NR_DATA_BYTES - 1]);
    return;
  }

  // Parse sensor values
  // Temperature Sensor 1 (bytes 2-3)
  float temp1 = (float)((((this->rx_data_[3] - 0x20) << 8) | this->rx_data_[2])) / 10.0f;
  
  // Temperature Sensor 2 (bytes 4-5)
  float temp2 = (float)((((this->rx_data_[5] - 0x20) << 8) | this->rx_data_[4])) / 10.0f;
  
  // Temperature Sensor 3 (bytes 6-7)
  float temp3 = (float)((((this->rx_data_[7] - 0x20) << 8) | this->rx_data_[6])) / 10.0f;
  
  // Relay Output 1: Pump (byte 20, bit 0)
  bool relay1 = (this->rx_data_[20] & 0x01) != 0;

  ESP_LOGD(TAG, "Temp1: %.1f°C, Temp2: %.1f°C, Temp3: %.1f°C, Relay1: %s",
           temp1, temp2, temp3, relay1 ? "ON" : "OFF");

  // Publish values
  if (this->sensor1_ != nullptr) {
    this->sensor1_->publish_state(temp1);
  }
  if (this->sensor2_ != nullptr) {
    this->sensor2_->publish_state(temp2);
  }
  if (this->sensor3_ != nullptr) {
    this->sensor3_->publish_state(temp3);
  }
  if (this->binary_sensor_ != nullptr) {
    this->binary_sensor_->publish_state(relay1);
  }
}

void ESR31Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ESR31 Solar Controller:");
  LOG_PIN("  Pin: ", this->pin_);
  LOG_SENSOR("  ", "Temperature Sensor 1", this->sensor1_);
  LOG_SENSOR("  ", "Temperature Sensor 2", this->sensor2_);
  LOG_SENSOR("  ", "Temperature Sensor 3", this->sensor3_);
  LOG_BINARY_SENSOR("  ", "Relay Output 1", this->binary_sensor_);
}

}  // namespace esr31
}  // namespace esphome
