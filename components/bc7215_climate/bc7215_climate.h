#pragma once
#include "esphome/core/component.h"
#include "esphome/components/climate/climate.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string>
#include <bc7215.h>
#include <bc7215ac.h>

namespace esphome::bc7215_climate {
class BC7215Climate : public climate::Climate, public Component {
 public:
  BC7215Climate() : serial_(1), board_(serial_, 27, 26), ac_(board_) {}
  void setup() override;
  void loop() override;
  void dump_config() override;
  void temperature_delta(int delta);
  void next_mode();
  void next_fan();
  void toggle_power();
  void begin_learning();
  void cancel_learning();
  void save_learning();
  void next_learning_match();
  bool learning() const { return learn_state_ != LearnState::IDLE; }
  bool learning_review() const { return learn_state_ == LearnState::REVIEW; }
  int learning_match() const { return learn_match_; }
  const char *learning_status() const {
    if (learn_state_ == LearnState::CAPTURE) return "TANITAS: FAN SPEED";
    if (learn_state_ == LearnState::REVIEW) return "TANITAS: MENTES?";
    return "IR TANITAS";
  }
  bool paired() const { return paired_; }
  std::string pairing_identity() const;
  void save_ota_state();
  void clear_ota_state();
  bool ota_restored() const { return ota_restored_; }
  bool is_powered() const { return powered_; }
  bool around_u_known() const { return around_u_known_; }
  bool around_u_enabled() const { return around_u_enabled_; }
  float around_u_temperature() const {
    return around_u_enabled_ && (millis() - around_u_last_rx_ms_ < 1500000UL)
               ? around_u_temperature_ : NAN;
  }
  int set_temperature() const { return temp_; }
  const char *hvac_label() const {
    if (!powered_) return "KIKAPCSOLVA";
    static const char *const names[] = {"AUTO", "HUTES", "FUTES", "PARATLANITAS", "VENTILATOR"};
    return mode_ >= 0 && mode_ < 5 ? names[mode_] : "ISMERETLEN";
  }
  const char *fan_label() const {
    static const char *const names[] = {"AUTO", "ALACSONY", "KOZEPES", "MAGAS"};
    return fan_ >= 0 && fan_ < 4 ? names[fan_] : "ISMERETLEN";
  }
  bool ready() const { return ready_; }
  bool sending() const { return pending_; }
  bool ir_timeout() const { return timed_out_; }
  uint32_t sent_count() const { return sent_count_; }
  uint32_t error_count() const { return error_count_; }
  uint32_t rx_count() const { return rx_count_; }
  uint32_t rx_error_count() const { return rx_error_count_; }
  bool receiving() const { return receiving_; }
  bool tx_fault() const { return tx_fault_; }

 protected:
  enum class LearnState : uint8_t { IDLE, CAPTURE, REVIEW };
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;
  void transmit_();
  void start_receive_();
  void stop_receive_();
  void handle_received_();
  void restore_ota_state_();
  bool reload_pairing_();
  void handle_learn_capture_();
  void finish_learning_(bool save);
  bool whirlpool_frame_(const bc7215DataMaxPkt_t &pkt) const;
  void decode_around_u_();
  void publish_();
  int mode_index_() const;
  HardwareSerial serial_;
  BC7215 board_;
  BC7215AC ac_;
  bool ready_{false}, pending_{false}, timed_out_{false};
  bool receiving_{false}, tx_fault_{false};
  bool powered_{false};
  bool ota_restored_{false};
  bool paired_{false};
  uint16_t paired_bit_len_{0};
  uint8_t paired_signature_{0};
  int8_t paired_match_{0};
  LearnState learn_state_{LearnState::IDLE};
  uint32_t learn_started_{0};
  int8_t learn_match_{0};
  bc7215FormatPkt_t learned_format_{};
  bc7215DataMaxPkt_t learned_data_{};
  bool around_u_known_{false}, around_u_enabled_{false};
  float around_u_temperature_{NAN};
  uint32_t around_u_last_rx_ms_{0};
  int temp_{25}, mode_{1}, fan_{1}, last_button_{2};
  uint32_t started_{0}, sent_count_{0}, error_count_{0};
  uint32_t rx_count_{0}, rx_error_count_{0};
};
}  // namespace esphome::bc7215_climate
