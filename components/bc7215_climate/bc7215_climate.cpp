#include "bc7215_climate.h"
#include "esphome/core/log.h"
#include <cmath>
#include <cstring>
#include <esp_system.h>

namespace esphome::bc7215_climate {
static const char *const TAG = "bc7215_climate";

static constexpr uint32_t OTA_MAGIC = 0xBC721502;
struct OtaSnapshot {
  uint32_t magic;
  uint8_t powered, temp, mode, fan;
  uint32_t checksum;
};
static uint32_t snapshot_checksum(const OtaSnapshot &s) {
  return s.magic ^ (uint32_t(s.powered) << 24) ^ (uint32_t(s.temp) << 16) ^
         (uint32_t(s.mode) << 8) ^ s.fan ^ 0x53A7B19F;
}
void BC7215Climate::clear_ota_state() {
  ::Preferences p;
  if (p.begin("bc7215_ota", false)) {
    p.remove("snapshot");
    p.end();
  }
}
void BC7215Climate::save_ota_state() {
  if (!ready_ || pending_) {
    ESP_LOGW(TAG, "OTA snapshot skipped: not ready or sending");
    return;
  }
  OtaSnapshot s{OTA_MAGIC, uint8_t(powered_), uint8_t(temp_),
                uint8_t(mode_), uint8_t(fan_), 0};
  s.checksum = snapshot_checksum(s);
  ::Preferences p;
  if (!p.begin("bc7215_ota", false)) {
    ESP_LOGE(TAG, "OTA snapshot NVS open failed");
    return;
  }
  bool ok = p.putBytes("snapshot", &s, sizeof(s)) == sizeof(s);
  p.end();
  ESP_LOGI(TAG, "OTA snapshot %s: power=%u temp=%u mode=%u fan=%u",
           ok ? "saved" : "FAILED", s.powered, s.temp, s.mode, s.fan);
}
void BC7215Climate::restore_ota_state_() {
  ::Preferences p;
  if (!p.begin("bc7215_ota", false)) return;
  OtaSnapshot s{};
  bool valid = p.getBytesLength("snapshot") == sizeof(s);
  if (valid) valid = p.getBytes("snapshot", &s, sizeof(s)) == sizeof(s);
  p.remove("snapshot");  // one-shot, even if invalid or wrong reset type
  p.end();
  if (!valid || s.magic != OTA_MAGIC || s.checksum != snapshot_checksum(s) ||
      s.powered > 1 || s.temp < 16 || s.temp > 30 || s.mode > 4 || s.fan > 3) return;
  if (esp_reset_reason() != ESP_RST_SW) {
    ESP_LOGW(TAG, "Ignoring OTA snapshot: reset reason %d",
             int(esp_reset_reason()));
    return;
  }
  powered_ = s.powered;
  temp_ = s.temp;
  mode_ = s.mode;
  fan_ = s.fan;
  ota_restored_ = true;
  ESP_LOGI(TAG, "OTA state RESTORED: power=%u temp=%u mode=%u fan=%u",
           s.powered, s.temp, s.mode, s.fan);
}



static uint8_t whirlpool_crc_(const bc7215DataMaxPkt_t &pkt) {
  uint8_t crc = 0x85;
  for (unsigned i = 0; i < 13; ++i) crc ^= pkt.data[i];
  return crc;
}
bool BC7215Climate::whirlpool_frame_(const bc7215DataMaxPkt_t &pkt) const {
  // Match the observed SPIW 309L 168-bit frame, not arbitrary AC protocols.
  return pkt.bitLen == 168 && pkt.data[0] == 0x83 &&
         pkt.data[1] == 0x06 && (pkt.data[3] == 0x60 || pkt.data[3] == 0x70) &&
         pkt.data[13] == whirlpool_crc_(pkt);
}
void BC7215Climate::decode_around_u_() {
  if (!whirlpool_profile_) return;
  if (ac_.sampleCount != 1) return;
  const auto &pkt = ac_.sampleData[0];
  if (!whirlpool_frame_(pkt)) return;

  const bool enabled = (pkt.data[11] & 0x80) != 0;
  around_u_known_ = true;
  around_u_enabled_ = enabled;
  if (enabled && pkt.data[12] >= 5 && pkt.data[12] <= 45) {
    around_u_temperature_ = float(pkt.data[12]);
    around_u_last_rx_ms_ = millis();
  } else {
    around_u_temperature_ = NAN;
  }
  // Physical remote is authoritative; never estimate a state from TX.
  current_temperature = around_u_temperature();
  ESP_LOGI(TAG, "Around U RX: enabled=%d remote_temp=%.1f",
           enabled, around_u_temperature());
}


std::string BC7215Climate::pairing_identity() const {
  if (!paired_) return "Nincs parositva";
  char value[64];
  snprintf(value, sizeof(value), "SIG 0x%02X | %u bit | match %d",
           unsigned(paired_signature_), unsigned(paired_bit_len_), int(paired_match_));
  return std::string(value);
}

bool BC7215Climate::reload_pairing_() {
  bc7215FormatPkt_t fmt{};
  bc7215DataMaxPkt_t data{};
  int8_t match = 0;
  bool celsius = true;
  ::Preferences p;
  if (!p.begin("bc7215 init", true)) return false;
  const bool valid = p.getBytesLength("format") == sizeof(fmt) &&
                     p.getBytesLength("data") == sizeof(data);
  if (valid) {
    p.getBytes("format", &fmt, sizeof(fmt));
    p.getBytes("data", &data, sizeof(data));
    if (p.getBytesLength("matchIndex") == sizeof(match))
      p.getBytes("matchIndex", &match, sizeof(match));
    if (p.getBytesLength("tempUnit") == sizeof(celsius))
      p.getBytes("tempUnit", &celsius, sizeof(celsius));
  }
  p.end();
  if (!valid || !celsius || match < 0) return false;
  ac_.setCelsius();
  if (!ac_.init(data, fmt)) return false;
  for (int i = 0; i < match; ++i) {
    if (!ac_.matchNext()) {
      ac_.init(data, fmt);
      ESP_LOGW(TAG, "Saved match index invalid; using base");
      break;
    }
  }
  paired_bit_len_ = data.bitLen;
  paired_signature_ = fmt.signature.inByte;
  paired_match_ = match;
  return true;
}
void BC7215Climate::begin_learning() {
  if (!ready_ || pending_ || tx_fault_ || learning()) {
    ESP_LOGW(TAG, "Learning refused: busy or not ready"); return;
  }
  stop_receive_();
  learn_match_ = 0;
  learn_state_ = LearnState::CAPTURE;
  learn_started_ = millis();
  ac_.startCapture();
  ESP_LOGI(TAG, "IR LEARN: point remote at BC7215, COOL 25C, press FAN SPEED");
}
void BC7215Climate::handle_learn_capture_() {
  ac_.stopCapture();

  const bool accepted = ac_.sampleCount == 1 && ac_.init();
  ESP_LOGI(TAG, "IR LEARN: sample_count=%u result=%s",
           ac_.sampleCount, accepted ? "ACCEPTED" : "REJECTED");
  if (!accepted) {
    finish_learning_(false);
    return;
  }
  const auto *data = ac_.getDataPkt();
  const auto *fmt = ac_.getFormatPkt();
  if (data == nullptr || fmt == nullptr || data->bitLen == 0 ||
      (data->bitLen + 7) / 8 > sizeof(learned_data_.data)) {
    ESP_LOGW(TAG, "IR LEARN: invalid base frame");
    finish_learning_(false); return;
  }
  learned_data_ = {};
  learned_data_.bitLen = data->bitLen;
  memcpy(learned_data_.data, data->data, (data->bitLen + 7) / 8);
  learned_format_ = *fmt;
  if (ac_.sampleStatus[0] & 0x40) learned_format_.signature.inByte |= 0x40;
  learn_state_ = LearnState::REVIEW;
  ESP_LOGI(TAG, "IR LEARN: candidate accepted; SAVE / NEXT MATCH / CANCEL");
}
void BC7215Climate::next_learning_match() {
  if (learn_state_ != LearnState::REVIEW) return;
  if (learn_match_ >= 120 || !ac_.matchNext()) {
    ESP_LOGW(TAG, "IR LEARN: no next match; back to candidate");
    ac_.init(learned_data_, learned_format_);
    learn_match_ = 0; return;
  }
  ++learn_match_;
  ESP_LOGI(TAG, "IR LEARN: match index=%d", learn_match_);
}
void BC7215Climate::finish_learning_(bool save) {
  if (learn_state_ == LearnState::CAPTURE) ac_.stopCapture();

  bool saved_ok = false;
  bool backup_ok = false;
  bool original_exists = false;
  bc7215FormatPkt_t old_fmt{};
  bc7215DataMaxPkt_t old_data{};
  int8_t old_match = 0;
  bool old_celsius = true;

  if (save) {
    // Read the existing NVS profile before any write.
    ::Preferences old;
    if (old.begin("bc7215 init", true)) {
      original_exists = old.getBytesLength("format") == sizeof(old_fmt) &&
                        old.getBytesLength("data") == sizeof(old_data);
      if (original_exists) {
        original_exists &= old.getBytes("format", &old_fmt, sizeof(old_fmt)) == sizeof(old_fmt);
        original_exists &= old.getBytes("data", &old_data, sizeof(old_data)) == sizeof(old_data);
        if (old.getBytesLength("matchIndex") == sizeof(old_match))
          original_exists &= old.getBytes("matchIndex", &old_match, sizeof(old_match)) == sizeof(old_match);
        if (old.getBytesLength("tempUnit") == sizeof(old_celsius))
          original_exists &= old.getBytes("tempUnit", &old_celsius, sizeof(old_celsius)) == sizeof(old_celsius);
      }
      old.end();
    }

    // An existing, working profile must never be overwritten without backup.
    backup_ok = !original_exists;
    if (original_exists) {
      ::Preferences backup;
      if (backup.begin("bc7215_prev", false)) {
        backup_ok =
          backup.putBytes("format", &old_fmt, sizeof(old_fmt)) == sizeof(old_fmt) &&
          backup.putBytes("data", &old_data, sizeof(old_data)) == sizeof(old_data) &&
          backup.putBytes("matchIndex", &old_match, sizeof(old_match)) == sizeof(old_match) &&
          backup.putBytes("tempUnit", &old_celsius, sizeof(old_celsius)) == sizeof(old_celsius);
        backup.end();
      }
      ESP_LOGI(TAG, "IR LEARN: previous NVS profile backup %s",
               backup_ok ? "OK" : "FAILED");
    }

    if (backup_ok) {
      ::Preferences active;
      if (active.begin("bc7215 init", false)) {
        const bool celsius = true;
        // Write all keys. Do not skip later writes on earlier failure.
        const bool fmt_ok = active.putBytes("format", &learned_format_, sizeof(learned_format_)) == sizeof(learned_format_);
        const bool data_ok = active.putBytes("data", &learned_data_, sizeof(learned_data_)) == sizeof(learned_data_);
        const bool match_ok = active.putBytes("matchIndex", &learn_match_, sizeof(learn_match_)) == sizeof(learn_match_);
        const bool unit_ok = active.putBytes("tempUnit", &celsius, sizeof(celsius)) == sizeof(celsius);
        saved_ok = fmt_ok && data_ok && match_ok && unit_ok;
        active.end();
      }
    } else {
      ESP_LOGE(TAG, "IR LEARN: refusing to overwrite previous pairing");
    }

    // Verify the new NVS content, not just the number of bytes written.
    if (saved_ok) {
      ::Preferences check;
      saved_ok = false;
      if (check.begin("bc7215 init", true)) {
        bc7215FormatPkt_t check_fmt{};
        bc7215DataMaxPkt_t check_data{};
        int8_t check_match = -1;
        bool check_celsius = false;
        if (check.getBytesLength("format") == sizeof(check_fmt) &&
            check.getBytesLength("data") == sizeof(check_data) &&
            check.getBytes("format", &check_fmt, sizeof(check_fmt)) == sizeof(check_fmt) &&
            check.getBytes("data", &check_data, sizeof(check_data)) == sizeof(check_data) &&
            check.getBytes("matchIndex", &check_match, sizeof(check_match)) == sizeof(check_match) &&
            check.getBytes("tempUnit", &check_celsius, sizeof(check_celsius)) == sizeof(check_celsius)) {
          saved_ok = memcmp(&check_fmt, &learned_format_, sizeof(check_fmt)) == 0 &&
                     check_data.bitLen == learned_data_.bitLen &&
                     memcmp(check_data.data, learned_data_.data, sizeof(check_data.data)) == 0 &&
                     check_match == learn_match_ && check_celsius;
        }
        check.end();
      }
      ESP_LOGI(TAG, "IR LEARN: NVS readback %s", saved_ok ? "OK" : "FAILED");
    }

    if (saved_ok) {
      paired_ = reload_pairing_();
      if (!paired_) {
        ESP_LOGE(TAG, "IR LEARN: new pairing could not be initialized");
        saved_ok = false;
      }
    }

    if (!saved_ok && original_exists && backup_ok) {
      // Restore the original in-memory snapshot if any save or reload stage fails.
      ::Preferences restore;
      bool rollback_ok = false;
      if (restore.begin("bc7215 init", false)) {
        const bool fmt_ok = restore.putBytes("format", &old_fmt, sizeof(old_fmt)) == sizeof(old_fmt);
        const bool data_ok = restore.putBytes("data", &old_data, sizeof(old_data)) == sizeof(old_data);
        const bool match_ok = restore.putBytes("matchIndex", &old_match, sizeof(old_match)) == sizeof(old_match);
        const bool unit_ok = restore.putBytes("tempUnit", &old_celsius, sizeof(old_celsius)) == sizeof(old_celsius);
        rollback_ok = fmt_ok && data_ok && match_ok && unit_ok;
        restore.end();
      }
      ESP_LOGW(TAG, "IR LEARN: rollback to previous pairing %s",
               rollback_ok ? "OK" : "FAILED");
    }
  }

  if (!saved_ok) paired_ = reload_pairing_();
  learn_state_ = LearnState::IDLE;

  if (saved_ok && paired_) {
    // Reset only estimated state. Never transmit during learning or after save.
    powered_ = false;
    temp_ = 25;
    mode_ = 1;
    fan_ = 0;
    around_u_known_ = false;
    around_u_enabled_ = false;
    around_u_temperature_ = NAN;
    current_temperature = NAN;
    ESP_LOGI(TAG, "IR LEARN: SAVED AND VERIFIED, match=%d, no IR transmitted",
             int(learn_match_));
    publish_();
  } else {
    ESP_LOGI(TAG, "IR LEARN: CANCELLED / NOT SAVED; previous pairing reloaded");
  }
  start_receive_();
}
void BC7215Climate::cancel_learning() {
  if (!learning()) return;
  finish_learning_(false);
}
void BC7215Climate::save_learning() {
  if (learn_state_ != LearnState::REVIEW) return;
  finish_learning_(true);
}

void BC7215Climate::setup() {
  ESP_LOGCONFIG(TAG, "AC profile: %s", whirlpool_profile_ ? "whirlpool" : "generic");
  ESP_LOGCONFIG(TAG, "BC7215 pins: UART RX=GPIO%u TX=GPIO%u MOD=GPIO%u BUSY=GPIO%u",
                unsigned(uart_rx_pin_), unsigned(uart_tx_pin_),
                unsigned(mod_pin_), unsigned(busy_pin_));
  serial_.begin(19200, SERIAL_8N2, uart_rx_pin_, uart_tx_pin_);
  board_.setRx(); delay(50);
  board_.setTx(); delay(50);
  board_.setShutDown(); delay(100);
  if (board_.isBusy()) {
    ESP_LOGE(TAG, "BC7215 module did not respond");
    mark_failed(); return;
  }
  board_.setRx(); delay(50);
  board_.setTx();

  paired_ = reload_pairing_();
  ready_ = true;
  restore_ota_state_();
  current_temperature = NAN;
  if (!paired_) ESP_LOGW(TAG, "No valid saved IR pairing; use learning buttons");
  publish_();
  start_receive_();
  ESP_LOGI(TAG, "BC7215 ready (paired=%d); no IR sent during startup", paired_);
}

climate::ClimateTraits BC7215Climate::traits() {
  auto t = climate::ClimateTraits();
  t.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_AUTO,
                         climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_HEAT,
                         climate::CLIMATE_MODE_DRY, climate::CLIMATE_MODE_FAN_ONLY});
  t.set_supported_fan_modes({climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW,
                             climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH});
  t.set_visual_min_temperature(16);
  t.set_visual_max_temperature(30);
  t.set_visual_temperature_step(1);
  return t;
}

int BC7215Climate::mode_index_() const { return mode_; }

void BC7215Climate::publish_() {
  if (!powered_) mode = climate::CLIMATE_MODE_OFF;
  else {
    static constexpr climate::ClimateMode modes[] = {
      climate::CLIMATE_MODE_AUTO, climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_HEAT, climate::CLIMATE_MODE_DRY,
      climate::CLIMATE_MODE_FAN_ONLY};
    mode = modes[mode_];
  }
  static constexpr climate::ClimateFanMode fans[] = {
    climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW,
    climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH};
  fan_mode = fans[fan_];
  target_temperature = temp_;
  publish_state();
}

void BC7215Climate::transmit_() {
  if (!ready_ || !paired_ || learning() || pending_ || tx_fault_) return;
  stop_receive_();
  timed_out_ = false;
  if (powered_) ac_.setTo(temp_, mode_, fan_, last_button_);
  else ac_.off();
  pending_ = true;
  started_ = millis();
  // Setpoint is the commander's requested/estimated state, not AC feedback.
  publish_();
}

bool BC7215Climate::around_u_locked_() const {
  return whirlpool_profile_ && (!around_u_known_ || around_u_enabled_);
}

void BC7215Climate::control(const climate::ClimateCall &call) {
  if (!paired_ || learning()) {
    ESP_LOGW(TAG, "Climate command rejected: unpaired or learning");
    publish_(); return;
  }
  if (!ready_ || pending_ || tx_fault_) { ESP_LOGW(TAG, "Command ignored: not ready / IR busy"); return; }
  // When ownership is unknown or belongs to the remote, allow only
  // an explicit OFF request. This must never enable the AC by accident.
  if (around_u_locked_()) {
    if (call.get_mode().has_value() &&
        *call.get_mode() == climate::CLIMATE_MODE_OFF) {
      ESP_LOGI(TAG, "HA Climate OFF allowed under Around U lock");
      powered_ = false;
      transmit_();
    } else {
      ESP_LOGW(TAG, "HA Climate command rejected: Around U enabled or unknown");
      publish_();
    }
    return;
  }
  if (call.get_mode().has_value()) {
    switch (*call.get_mode()) {
      case climate::CLIMATE_MODE_OFF: powered_ = false; break;
      case climate::CLIMATE_MODE_AUTO: powered_ = true; mode_ = 0; last_button_ = 2; break;
      case climate::CLIMATE_MODE_COOL: powered_ = true; mode_ = 1; last_button_ = 2; break;
      case climate::CLIMATE_MODE_HEAT: powered_ = true; mode_ = 2; last_button_ = 2; break;
      case climate::CLIMATE_MODE_DRY: powered_ = true; mode_ = 3; last_button_ = 2; break;
      case climate::CLIMATE_MODE_FAN_ONLY: powered_ = true; mode_ = 4; last_button_ = 2; break;
      default: break;
    }
  }
  if (call.get_target_temperature().has_value()) {
    int wanted = (int) std::round(*call.get_target_temperature());
    if (wanted >= 16 && wanted <= 30) {
      last_button_ = wanted >= temp_ ? 1 : 0;
      temp_ = wanted;
    }
  }
  if (call.get_fan_mode().has_value()) {
    switch (*call.get_fan_mode()) {
      case climate::CLIMATE_FAN_AUTO: fan_ = 0; break;
      case climate::CLIMATE_FAN_LOW: fan_ = 1; break;
      case climate::CLIMATE_FAN_MEDIUM: fan_ = 2; break;
      case climate::CLIMATE_FAN_HIGH: fan_ = 3; break;
      default: break;
    }
    last_button_ = 3;
  }
  transmit_();
}

void BC7215Climate::temperature_delta(int delta) {
  if (!ready_ || !paired_ || learning() || pending_ || tx_fault_ || !powered_ || mode_ >= 3 ||
      around_u_locked_()) return;
  int next = temp_ + delta;
  if (next < 16 || next > 30) return;
  temp_ = next; last_button_ = delta > 0 ? 1 : 0; transmit_();
}
void BC7215Climate::next_mode() {
  if (!ready_ || !paired_ || learning() || pending_ || tx_fault_ || around_u_locked_()) return;
  powered_ = true; mode_ = (mode_ + 1) % 5; last_button_ = 2; transmit_();
}
void BC7215Climate::next_fan() {
  if (!ready_ || !paired_ || learning() || pending_ || tx_fault_ || (!around_u_known_ || around_u_enabled_)) return;
  fan_ = (fan_ + 1) % 4; last_button_ = 3; transmit_();
}
void BC7215Climate::toggle_power() {
  if (!ready_ || !paired_ || learning() || pending_ || tx_fault_) return;
  // While locked, the physical button may shut down the AC, but not power it on.
  if (around_u_locked_() && !powered_) {
    ESP_LOGW(TAG, "Power ON rejected: Around U enabled or unknown");
    return;
  }
  powered_ = !powered_;
  stop_receive_();
  if (powered_) ac_.on(); else ac_.off();
  pending_ = true; started_ = millis(); timed_out_ = false; publish_();
}
void BC7215Climate::start_receive_() {
  if (!ready_ || receiving_ || pending_ || learning()) return;
  ac_.startCapture();
  receiving_ = true;
}

void BC7215Climate::stop_receive_() {
  if (!receiving_) return;
  ac_.stopCapture();
  receiving_ = false;
}

void BC7215Climate::handle_received_() {
  int t = -1, m = -1, f = -1, p = -1;
  const bool parsed = ac_.parse(t, m, f, p);
  if (!parsed) {
    ++rx_error_count_;
    ESP_LOGW(TAG, "IR RX captured but parse failed (rx_errors=%u)", (unsigned) rx_error_count_);
    return;
  }
  ++rx_count_;
  const bool previous_power = powered_;
  ESP_LOGI(TAG, "IR RX raw: temp=%d mode=%d fan=%d power=%d previous_power=%d",
           t, m, f, p, previous_power ? 1 : 0);
  // Preserve original v0.2 semantics; diagnose the OFF mapping before changing behavior.
  if (p == 0) powered_ = false;
  else if (p == 1) powered_ = true;
  else if (p == 2) {
    // Whirlpool remote reports POWER as TOGGLE. This tracks the last observed state;
    // missed IR commands can still leave it out of sync with the physical unit.
    powered_ = !powered_;
    ESP_LOGI(TAG, "IR POWER TOGGLE -> %s (estimated)", powered_ ? "ON" : "OFF");
  } else {
    ESP_LOGW(TAG, "Unknown IR power value=%d; retaining previous state", p);
  }
  if (t >= 16 && t <= 30) temp_ = t;
  if (m >= 0 && m <= 4) mode_ = m;
  if (f >= 0 && f <= 3) fan_ = f;
  if (whirlpool_profile_) decode_around_u_();
  ESP_LOGI(TAG, "IR RX mapped: powered=%d temp=%d mode=%d fan=%d",
           powered_ ? 1 : 0, temp_, mode_, fan_);
  publish_();
}

void BC7215Climate::loop() {
  if (!ready_) return;
  if (learn_state_ == LearnState::CAPTURE) {
    if (ac_.signalCaptured()) handle_learn_capture_();
    else if (millis() - learn_started_ > 30000UL) {
      ESP_LOGW(TAG, "IR LEARN: capture timed out");
      finish_learning_(false);
    }
    return;
  }
  if (learn_state_ == LearnState::REVIEW) return;
  if (pending_) {
    if (!ac_.isBusy()) {
      pending_ = false;
      ++sent_count_;
      start_receive_();
    } else if ((uint32_t)(millis() - started_) > 3000) {
      pending_ = false;
      timed_out_ = true;
      ++error_count_;
      ESP_LOGW(TAG, "IR send timed out; physical AC state unknown");
      tx_fault_ = true;
    }
    return;
  }
  if (tx_fault_) return;
  if (receiving_ && ac_.signalCaptured()) {
    stop_receive_();
    handle_received_();
    start_receive_();
  }
}
void BC7215Climate::dump_config() {
  ESP_LOGCONFIG(TAG, "BC7215 IR climate: ready=%s", YESNO(ready_));
}
}  // namespace esphome::bc7215_climate
