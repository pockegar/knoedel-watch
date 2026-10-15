// M5Unified-compatible type shims for the Waveshare 1.75C board.
//
// The rtc_date_t / rtc_time_t / rtc_datetime_t layouts and the
// Power_Class::is_charging_t enum are copied verbatim from M5Unified
// (MIT license, Copyright (c) M5Stack) so that application code keeps
// compiling unchanged: utility/rtc/RTC_Base.hpp and utility/Power_Class.hpp.
// The classes below adapt AXP2101 power, ES8311 audio and QMI8658 motion.
// This board has no external RTC, so Rtc_Class uses the ESP32 system clock.
#pragma once

#include <ctime>
#include <cstdint>

namespace m5
{
  struct __attribute__((packed)) rtc_time_t
  {
    std::int8_t hours;
    std::int8_t minutes;
    std::int8_t seconds;

    rtc_time_t(std::int8_t hours_ = -1, std::int8_t minutes_ = -1, std::int8_t seconds_ = -1)
    : hours   { hours_   }
    , minutes { minutes_ }
    , seconds { seconds_ }
    {}
  };

  struct __attribute__((packed)) rtc_date_t
  {
    std::int16_t year;    // 1900-2099
    std::int8_t  month;   // 1-12
    std::int8_t  date;    // 1-31
    std::int8_t  weekDay; // 0:sun .. 6:sat

    rtc_date_t(std::int16_t year_ = 2000, std::int8_t month_ = 1, std::int8_t date_ = -1, std::int8_t weekDay_ = -1)
    : year    { year_    }
    , month   { month_   }
    , date    { date_    }
    , weekDay { weekDay_ }
    {}
  };

  struct __attribute__((packed)) rtc_datetime_t
  {
    rtc_date_t date;
    rtc_time_t time;
    rtc_datetime_t() = default;
    rtc_datetime_t(const rtc_date_t& d, const rtc_time_t& t) : date { d }, time { t } {}
  };

  // AXP2101 battery / USB measurements and power-button support.
  class Power_Class
  {
  public:
    enum is_charging_t
    { is_discharging = 0
    , is_charging
    , charge_unknown
    };

    // AXP2101 battery power-off. Returns false while USB supplies power.
    bool powerOff(void);

    is_charging_t isCharging(void);
    int8_t getBatteryLevel(void);
    uint16_t getBatteryVoltage(void);
    int16_t getVBUSVoltage(void);
  };

  // ES8311 asynchronous playback with owned PCM buffers and cancellation.
  class Speaker_Class
  {
  public:
    void setVolume(uint8_t);
    bool playRaw(const int16_t*, size_t, uint32_t = 44100, bool = false, uint32_t = 1, int = -1, bool = false);
    void tone(uint32_t, uint32_t = ~0u, int = -1);
    void stop(void);
  };

  // QMI8658 acceleration in g, configured for a 4g range.
  class Imu_Class
  {
  public:
    void update(void) {}
    bool isEnabled(void);
    int getType(void) { return isEnabled() ? 8658 : 0; }
    bool getAccel(float*, float*, float*);
  };

  // System time while powered; NVS holds the last manually/USB-set time.
  // A full power loss cannot account for elapsed time without a real RTC.
  class Rtc_Class
  {
  public:
    bool isEnabled(void);
    bool getDateTime(rtc_datetime_t* dt);
    bool getDate(rtc_date_t* d);
    bool setDateTime(const rtc_datetime_t* dt);
    bool setDateTime(const rtc_date_t* d, const rtc_time_t* t);
  };

  struct config_t
  {
    bool internal_imu = true;
    bool internal_rtc = true;
    bool internal_mic = true;
    bool internal_spk = true;
  };
}
