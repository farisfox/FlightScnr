#include "hardware/buzzer.h"

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>

#include <cctype>
#include <cmath>
#include <cstdlib>

#include "hardware/pin_config.h"
#include "services/settings_state.h"

#if FLIGHTSCNR_HAS_HAPTIC
#include "SensorDRV2605.hpp"
#endif

#if FLIGHTSCNR_HAS_SPEAKER
#include <driver/i2s.h>
#endif

namespace hardware {

namespace {

constexpr char kStoreNs[] = "flightscnr";
constexpr char kEnabledKey[] = "beep_en";
constexpr char kToneKey[] = "beep_vol";

constexpr int kLedcChannel = 2;
constexpr int kLedcResolution = 8;
constexpr uint16_t kToneHzDefault = 2000;

constexpr char kToneLetters[] = {'A', 'B', 'C', 'D', 'E'};
constexpr size_t kToneLevelCount = sizeof(kToneLetters) / sizeof(kToneLetters[0]);
constexpr uint8_t kDefaultToneIndex = 0;  // A

// LilyGO T-Encoder-Pro factory (Lvgl_CIT): ledcAttach @ 2000 Hz, duty 127 (~50%),
// ledcChangeFrequency per step (1500/2000/2500 Hz). Never use duty 255 (DC, no tone).
struct BeepProfile {
  uint16_t freq_hz;
  uint8_t duty;
  uint8_t duration_ms;
};

constexpr BeepProfile kBeepProfiles[] = {
    {1500, 10, 5},    // A
    {1750, 28, 12},   // B
    {2000, 64, 22},   // C
    {2250, 96, 28},   // D
    {2500, 127, 32},  // E
};
static_assert(sizeof(kBeepProfiles) / sizeof(kBeepProfiles[0]) == kToneLevelCount,
              "beep profiles must match tone levels");

#if FLIGHTSCNR_HAS_HAPTIC
constexpr uint8_t kHapticClickEffect = 1;  // Strong Click - 100% (UI only)
#endif

constexpr uint8_t kLegacyTonePercents[] = {20, 40, 60, 80, 100};

bool s_enabled = true;
uint8_t s_tone_index = kDefaultToneIndex;
bool s_playing = false;
unsigned long s_stop_at_ms = 0;

constexpr uint16_t kAlertFreqHz = 2500;
constexpr uint8_t kAlertDuty = 127;
constexpr uint8_t kAlertBeepMs = 60;
constexpr uint16_t kAlertIntervalMs = 140;
constexpr uint8_t kAlertBeepCount = 3;

uint8_t s_alert_beeps_remaining = 0;
unsigned long s_alert_next_ms = 0;

#if FLIGHTSCNR_HAS_HAPTIC
SensorDRV2605 s_drv;
bool s_haptic_ready = false;

void hapticFireClick() {
  s_drv.stop();
  s_drv.setMode(DRV2605_MODE_INTTRIG);
  s_drv.selectLibrary(1);
  s_drv.setWaveform(0, kHapticClickEffect);
  s_drv.setWaveform(1, 0);
  s_drv.run();
}

bool hapticInit() {
  if (!s_drv.init(Wire, IIC_SDA, IIC_SCL, DRV2605_SLAVE_ADDRESS)) {
    Serial.println("Haptic: DRV2605 not found");
    return false;
  }
  s_drv.useERM();
  s_drv.selectLibrary(1);
  s_drv.setMode(DRV2605_MODE_INTTRIG);
  Serial.printf("Haptic: DRV2605 ready (UI click effect %u)\n", kHapticClickEffect);
  return true;
}
#endif

#if FLIGHTSCNR_HAS_SPEAKER
// ---- ES8311 speaker (Waveshare AMOLED 1.75C) ----
// Slave-mode codec, 16 kHz / 16-bit stereo, MCLK = 256 x fs. Register values
// follow Espressif's es8311 driver for that clock ratio. Tones are synthesised
// into a small buffer and handed to the I2S DMA, so playback never blocks.
constexpr i2s_port_t kI2sPort = I2S_NUM_0;
constexpr uint32_t kSpeakerRate = 16000;
constexpr uint32_t kSpeakerMclk = kSpeakerRate * 256;
constexpr uint8_t kSpeakerClickMs = 25;
constexpr uint8_t kSpeakerAlertMs = 60;
constexpr size_t kSpeakerMaxFrames = (kSpeakerRate * 80) / 1000;
// Peak amplitude per tone level A..E (of 32767).
constexpr int16_t kSpeakerAmplitude[] = {1500, 3500, 7000, 12000, 20000};

bool s_speaker_ready = false;
int16_t s_speaker_buf[kSpeakerMaxFrames * 2];

bool es8311Write(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ES8311_I2C_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool es8311Init() {
  Wire.beginTransmission(ES8311_I2C_ADDRESS);
  if (Wire.endTransmission() != 0) {
    return false;
  }
  bool ok = true;
  ok &= es8311Write(0x00, 0x1F);  // reset
  delay(20);
  ok &= es8311Write(0x00, 0x00);
  ok &= es8311Write(0x00, 0x80);  // power on, slave mode
  ok &= es8311Write(0x01, 0x3F);  // all clocks on, MCLK from pin
  ok &= es8311Write(0x02, 0x00);  // pre-divider 1, pre-multiplier 1x
  ok &= es8311Write(0x03, 0x10);  // ADC OSR
  ok &= es8311Write(0x04, 0x20);  // DAC OSR
  ok &= es8311Write(0x05, 0x00);  // ADC / DAC dividers 1
  ok &= es8311Write(0x06, 0x03);  // BCLK divider
  ok &= es8311Write(0x07, 0x00);  // LRCK divider high (256 - 1)
  ok &= es8311Write(0x08, 0xFF);  // LRCK divider low
  ok &= es8311Write(0x09, 0x0C);  // SDP in: I2S, 16 bit
  ok &= es8311Write(0x0A, 0x0C);  // SDP out: I2S, 16 bit
  ok &= es8311Write(0x0D, 0x01);  // power up analog circuitry
  ok &= es8311Write(0x0E, 0x02);  // enable analog PGA and ADC modulator
  ok &= es8311Write(0x12, 0x00);  // power up DAC
  ok &= es8311Write(0x13, 0x10);  // enable headphone / speaker drive
  ok &= es8311Write(0x1C, 0x6A);  // ADC equaliser bypass, DC offset cancel
  ok &= es8311Write(0x37, 0x08);  // DAC equaliser bypass
  ok &= es8311Write(0x31, 0x00);  // unmute DAC
  ok &= es8311Write(0x32, 0xBF);  // DAC volume 0 dB
  return ok;
}

bool speakerInit() {
#if SPEAKER_PA_EN >= 0
  pinMode(SPEAKER_PA_EN, OUTPUT);
  digitalWrite(SPEAKER_PA_EN, HIGH);
#endif
  i2s_config_t cfg = {};
  cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = kSpeakerRate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;  // play silence when no tone is queued
  cfg.fixed_mclk = kSpeakerMclk;
  cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  if (i2s_driver_install(kI2sPort, &cfg, 0, nullptr) != ESP_OK) {
    return false;
  }
  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_MCLK;
  pins.bck_io_num = I2S_BCLK;
  pins.ws_io_num = I2S_WS;
  pins.data_out_num = I2S_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  if (i2s_set_pin(kI2sPort, &pins) != ESP_OK) {
    return false;
  }
  return es8311Init();
}

void speakerPlay(uint16_t freq_hz, int16_t amplitude, uint8_t duration_ms) {
  if (!s_speaker_ready) {
    return;
  }
  size_t frames = (kSpeakerRate * duration_ms) / 1000;
  if (frames > kSpeakerMaxFrames) {
    frames = kSpeakerMaxFrames;
  }
  const size_t fade = frames / 6 + 1;  // short ramps avoid clicks
  const float step = 2.0f * 3.14159265f * freq_hz / kSpeakerRate;
  for (size_t i = 0; i < frames; ++i) {
    float env = 1.0f;
    if (i < fade) {
      env = static_cast<float>(i) / fade;
    } else if (i + fade > frames) {
      env = static_cast<float>(frames - i) / fade;
    }
    const int16_t v = static_cast<int16_t>(sinf(step * i) * amplitude * env);
    s_speaker_buf[2 * i] = v;
    s_speaker_buf[2 * i + 1] = v;
  }
  size_t written = 0;
  // Short timeout: if the DMA queue is still busy with a previous beep, drop this one.
  i2s_write(kI2sPort, s_speaker_buf, frames * 2 * sizeof(int16_t), &written,
            pdMS_TO_TICKS(10));
}
#endif

size_t toneIndexFromLegacyPercent(uint8_t pct) {
  for (size_t i = 0; i < kToneLevelCount; ++i) {
    if (kLegacyTonePercents[i] == pct) {
      return i;
    }
  }
  size_t best = 0;
  uint8_t best_delta = 255;
  for (size_t i = 0; i < kToneLevelCount; ++i) {
    const uint8_t level = kLegacyTonePercents[i];
    const uint8_t delta =
        (pct > level) ? static_cast<uint8_t>(pct - level) : static_cast<uint8_t>(level - pct);
    if (delta < best_delta) {
      best_delta = delta;
      best = i;
    }
  }
  return best;
}

size_t toneIndexFromStored(uint8_t stored) {
  if (stored < kToneLevelCount) {
    return stored;
  }
  return toneIndexFromLegacyPercent(stored);
}

size_t toneIndexFromForm(const char* value) {
  if (value == nullptr || value[0] == '\0') {
    return kDefaultToneIndex;
  }

  if (value[1] == '\0') {
    char letter = static_cast<char>(toupper(static_cast<unsigned char>(value[0])));
    if (letter >= 'A' && letter <= 'E') {
      return static_cast<size_t>(letter - 'A');
    }
  }

  const int pct = static_cast<int>(lroundf(strtof(value, nullptr)));
  if (pct <= 0) {
    return 0;
  }
  if (pct >= static_cast<int>(kLegacyTonePercents[kToneLevelCount - 1])) {
    return kToneLevelCount - 1;
  }
  return toneIndexFromLegacyPercent(static_cast<uint8_t>(pct));
}

void persistSettings() {
  Preferences prefs;
  if (prefs.begin(kStoreNs, false)) {
    prefs.putBool(kEnabledKey, s_enabled);
#if !FLIGHTSCNR_HAS_HAPTIC
    prefs.putUChar(kToneKey, s_tone_index);
#endif
    prefs.end();
  }
  settingsStateBump();
}

const BeepProfile& beepProfileForIndex(size_t index) {
  return kBeepProfiles[index];
}

void startTone() {
  if (!s_enabled) {
    return;
  }

#if FLIGHTSCNR_HAS_HAPTIC
  if (s_haptic_ready) {
    hapticFireClick();
    return;
  }
#endif

#if FLIGHTSCNR_HAS_BUZZER
  const BeepProfile& profile = beepProfileForIndex(s_tone_index);
  ledcChangeFrequency(kLedcChannel, profile.freq_hz, kLedcResolution);
  ledcWrite(kLedcChannel, profile.duty);
  s_playing = true;
  s_stop_at_ms = millis() + profile.duration_ms;
#elif FLIGHTSCNR_HAS_SPEAKER
  speakerPlay(beepProfileForIndex(s_tone_index).freq_hz, kSpeakerAmplitude[s_tone_index],
              kSpeakerClickMs);
#else
  (void)beepProfileForIndex;
#endif
}

}  // namespace

void buzzerInit() {
#if FLIGHTSCNR_HAS_BUZZER
  ledcSetup(kLedcChannel, kToneHzDefault, kLedcResolution);
  ledcAttachPin(BUZZER_DATA, kLedcChannel);
  ledcWrite(kLedcChannel, 0);
#endif
#if FLIGHTSCNR_HAS_SPEAKER
  s_speaker_ready = speakerInit();
  Serial.printf("Speaker: ES8311 %s\n", s_speaker_ready ? "ready" : "init failed");
#endif
#if FLIGHTSCNR_HAS_HAPTIC
  s_haptic_ready = hapticInit();
#endif
}

void buzzerBootLoad() {
  Preferences prefs;
  if (!prefs.begin(kStoreNs, true)) {
    return;
  }
  s_enabled = prefs.getBool(kEnabledKey, true);
#if !FLIGHTSCNR_HAS_HAPTIC
  const uint8_t stored = prefs.getUChar(kToneKey, kDefaultToneIndex);
  s_tone_index = static_cast<uint8_t>(toneIndexFromStored(stored));
#endif
  prefs.end();
#if FLIGHTSCNR_HAS_HAPTIC
  Serial.printf("Haptic: UI click %s\n", s_enabled ? "on" : "off");
  if (s_haptic_ready) {
    hapticFireClick();
  }
#endif
}

bool buzzerEnabled() { return s_enabled; }

char buzzerToneLetter() { return kToneLetters[s_tone_index]; }

void buzzerSetEnabled(bool enabled) {
  if (s_enabled == enabled) {
    return;
  }
  s_enabled = enabled;
  persistSettings();
#if FLIGHTSCNR_HAS_HAPTIC
  Serial.printf("Vibration: %s\n", s_enabled ? "on" : "off");
#else
  Serial.printf("UI beep: %s\n", s_enabled ? "on" : "off");
#endif
}

void buzzerToneStep(int8_t delta) {
#if FLIGHTSCNR_HAS_HAPTIC
  (void)delta;
  return;
#else
  if (delta == 0) {
    return;
  }

  size_t idx = s_tone_index;
  if (delta > 0) {
    idx = (idx + 1) % kToneLevelCount;
  } else {
    idx = (idx == 0) ? kToneLevelCount - 1 : idx - 1;
  }
  s_tone_index = static_cast<uint8_t>(idx);
  persistSettings();
  Serial.printf("Beep tone: %c\n", buzzerToneLetter());
  buzzerClick();
#endif
}

void buzzerClick() {
  startTone();
}

void buzzerPoll() {
#if FLIGHTSCNR_HAS_BUZZER
  if (s_playing && millis() >= s_stop_at_ms) {
    ledcWrite(kLedcChannel, 0);
    s_playing = false;
  }
#endif

  if (s_alert_beeps_remaining == 0) {
    return;
  }
  if (millis() < s_alert_next_ms) {
    return;
  }

#if FLIGHTSCNR_HAS_BUZZER
  ledcChangeFrequency(kLedcChannel, kAlertFreqHz, kLedcResolution);
  ledcWrite(kLedcChannel, kAlertDuty);
  s_playing = true;
  s_stop_at_ms = millis() + kAlertBeepMs;
  --s_alert_beeps_remaining;
  s_alert_next_ms = millis() + kAlertIntervalMs;
#elif FLIGHTSCNR_HAS_SPEAKER
  speakerPlay(kAlertFreqHz, kSpeakerAmplitude[s_tone_index], kSpeakerAlertMs);
  --s_alert_beeps_remaining;
  s_alert_next_ms = millis() + kAlertIntervalMs;
#else
  s_alert_beeps_remaining = 0;
#endif
}

void buzzerAlert() {
#if FLIGHTSCNR_HAS_HAPTIC
  // Aircraft alerts use a red screen flash, not the vibration motor.
  return;
#endif
  if (!s_enabled) {
    return;
  }
  s_alert_beeps_remaining = kAlertBeepCount;
  s_alert_next_ms = 0;
}

void saveBeepEnabledFromForm(const char* checkbox_value) {
  const bool enabled = checkbox_value != nullptr && checkbox_value[0] == 'T';
  s_enabled = enabled;
  Preferences prefs;
  if (prefs.begin(kStoreNs, false)) {
    prefs.putBool(kEnabledKey, s_enabled);
    prefs.end();
  }
}

void saveBeepToneFromForm(const char* value) {
#if FLIGHTSCNR_HAS_HAPTIC
  (void)value;
#else
  s_tone_index = static_cast<uint8_t>(toneIndexFromForm(value));
  Preferences prefs;
  if (prefs.begin(kStoreNs, false)) {
    prefs.putUChar(kToneKey, s_tone_index);
    prefs.end();
  }
#endif
}

}  // namespace hardware
