#include "M5Unified.h"
#include "board_pins.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <driver/gpio.h>
#include <driver/usb_serial_jtag.h>
#include <driver/usb_serial_jtag_vfs.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <fcntl.h>
#include <freertos/task.h>
#include <nvs.h>
#include <sys/time.h>
#include <unistd.h>
#define M5UNIFIED_IMPLEMENTATION
#include "Button_Class.inl"
#include "Touch_Class.inl"

M5Unified M5;
namespace {
constexpr const char *TAG = "ws-board";
i2c_master_bus_handle_t shared_bus = nullptr;
i2c_master_dev_handle_t pmu = nullptr, imu = nullptr;
bool imu_ready = false, power_ready = false;
uint32_t power_read_at = 0, key_until = 0, key_poll_at = 0;
int battery_mv = 0, battery_pct = -1, vbus_mv = 0;
m5::Power_Class::is_charging_t charging = m5::Power_Class::charge_unknown;
M5Unified::Action action = M5Unified::None;

bool read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *data, size_t count = 1) {
    return dev && i2c_master_transmit_receive(dev, &reg, 1, data, count, 50) == ESP_OK;
}
bool write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t value) {
    uint8_t data[] = {reg, value};
    return dev && i2c_master_transmit(dev, data, 2, 50) == ESP_OK;
}
bool change_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t clear, uint8_t set) {
    uint8_t value = 0;
    return read_reg(dev, reg, &value) && write_reg(dev, reg, (value & ~clear) | set);
}
i2c_master_dev_handle_t device(uint8_t address) {
    if (i2c_master_probe(shared_bus, address, 50) != ESP_OK)
        return nullptr;
    i2c_device_config_t cfg = {};
    cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    cfg.device_address = address;
    cfg.scl_speed_hz = 400000;
    i2c_master_dev_handle_t handle = nullptr;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(shared_bus, &cfg, &handle));
    return handle;
}
void refresh_power() {
    uint32_t now = M5.millis();
    if (power_read_at && now - power_read_at < 1000)
        return;
    power_read_at = now;
    uint8_t status[2] = {}, voltage[2] = {}, percent = 0;
    if (!read_reg(pmu, 0, status, 2)) {
        charging = m5::Power_Class::charge_unknown;
        return;
    }
    bool battery = status[0] & 8, usb = status[0] & 0x20;
    charging = battery && (status[1] & 0x60) == 0x20 ? m5::Power_Class::is_charging : m5::Power_Class::is_discharging;
    battery_mv = 0;
    battery_pct = -1;
    vbus_mv = 0;
    if (battery && read_reg(pmu, 0x34, voltage, 2)) {
        battery_mv = ((voltage[0] & 0x3f) << 8) | voltage[1];
        if (read_reg(pmu, 0xa4, &percent) && percent > 0 && percent <= 100)
            battery_pct = percent;
        else
            battery_pct =
                std::clamp((battery_mv - 3300) * 100 / 900, 0, 100); // approximate fallback, not a calibrated gauge
    }
    if (usb && read_reg(pmu, 0x38, voltage, 2))
        vbus_mv = ((voltage[0] & 0x3f) << 8) | voltage[1];
}
void save_clock() {
    nvs_handle_t nvs;
    if (nvs_open("ws_clock", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_i64(nvs, "epoch", time(nullptr));
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}
void init_clock() {
    setenv("TZ", "CST-8", 1);
    tzset();
    if (time(nullptr) >= 946684800)
        return;
    int64_t epoch = 0;
    nvs_handle_t nvs;
    if (nvs_open("ws_clock", NVS_READONLY, &nvs) == ESP_OK) {
        nvs_get_i64(nvs, "epoch", &epoch);
        nvs_close(nvs);
    }
    if (epoch < 946684800) {
        // Approximate first boot from the build timestamp; USB/manual setting is authoritative.
        struct tm tm_ = {};
        char month[4] = {};
        int year = 2000;
        sscanf(__DATE__, "%3s %d %d", month, &tm_.tm_mday, &year);
        tm_.tm_year = year - 1900;
        const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
        const char *found = strstr(months, month);
        tm_.tm_mon = found ? (found - months) / 3 : 0;
        sscanf(__TIME__, "%d:%d:%d", &tm_.tm_hour, &tm_.tm_min, &tm_.tm_sec);
        epoch = mktime(&tm_);
    }
    timeval tv = {.tv_sec = (time_t)epoch, .tv_usec = 0};
    settimeofday(&tv, nullptr);
}
void status() {
    refresh_power();
    float x = 0, y = 0, z = 0;
    M5.Imu.getAccel(&x, &y, &z);
    printf("@STATUS epoch=%lld battery_mv=%d battery_pct=%d vbus_mv=%d charging=%d imu=%d accel=%.3f,%.3f,%.3f "
           "touch=%d heap=%u psram=%u\n",
           (long long)time(nullptr), battery_mv, battery_pct, vbus_mv, int(charging), imu_ready, x, y, z,
           M5.Touch.isEnabled(), unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
           unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
}
void console() {
    static char line[80];
    static size_t count = 0;
    char c;
    while (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == '\r')
            continue;
        if (c != '\n') {
            if (count + 1 < sizeof(line))
                line[count++] = c;
            continue;
        }
        line[count] = 0;
        count = 0;
        if (!strcmp(line, "status"))
            status();
        else if (!strcmp(line, "screen"))
            M5.Display.dumpFrame();
        else if (!strcmp(line, "next"))
            action = M5Unified::Next;
        else if (!strcmp(line, "prev"))
            action = M5Unified::Previous;
        else if (!strcmp(line, "tap"))
            action = M5Unified::Tap;
        else if (!strcmp(line, "back"))
            action = M5Unified::Back;
        else if (!strcmp(line, "tone"))
            M5.Speaker.tone(880, 400);
        else if (!strncmp(line, "time ", 5)) {
            char *end = nullptr;
            int64_t epoch = strtoll(line + 5, &end, 10);
            if (end != line + 5 && !*end && epoch >= 1700000000 && epoch < 4102444800LL) {
                timeval tv = {.tv_sec = (time_t)epoch, .tv_usec = 0};
                settimeofday(&tv, nullptr);
                save_clock();
            } else {
                printf("@ERROR invalid epoch\n");
                continue;
            }
        } else if (line[0]) {
            printf("@ERROR unknown command\n");
            continue;
        }
        printf("@OK %s\n", line);
        fflush(stdout);
    }
}
} // namespace
namespace ws {
i2c_master_bus_handle_t i2c_bus() {
    return shared_bus;
}
} // namespace ws

m5::config_t M5Unified::config() {
    return m5::config_t();
}
void M5Unified::begin(const m5::config_t &) {
    i2c_master_bus_config_t cfg = {};
    cfg.i2c_port = I2C_NUM_0;
    cfg.sda_io_num = WS_I2C_SDA;
    cfg.scl_io_num = WS_I2C_SCL;
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.glitch_ignore_cnt = 7;
    cfg.flags.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &shared_bus));
    pmu = device(0x34);
    uint8_t id = 0;
    power_ready = read_reg(pmu, 3, &id) && id == 0x4a;
    if (!power_ready) {
        ESP_LOGE(TAG, "AXP2101 unavailable (id=%02x)", id);
        abort();
    }
    // Enable ADC reporting and the board's 3.3 V analog-audio rail. Preserve charge limits and all other rails.
    if (!change_reg(pmu, 0x30, 0, 5) || !change_reg(pmu, 0x92, 0x1f, 28) || !change_reg(pmu, 0x90, 0, 1))
        abort();
    if (!change_reg(pmu, 0x41, 0, 8) || !write_reg(pmu, 0x49, 8))
        abort();
    ESP_LOGI(TAG, "AXP2101 ready, ALDO1 3.3V enabled");
    init_clock();
    if (!Display.init()) {
        ESP_LOGE(TAG, "display init failed");
        abort();
    }
    Display.setBrightness(180);
    Display.fillScreen(TFT_BLACK);
    if (!Display.touchReady()) {
        ESP_LOGE(TAG, "touch init failed");
        abort();
    }
    Touch.begin(&Display);
    gpio_config_t btn = {};
    btn.pin_bit_mask = 1ULL << WS_BTN_BOOT;
    btn.mode = GPIO_MODE_INPUT;
    btn.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&btn));
    imu = device(0x6b);
    if (!imu)
        imu = device(0x6a);
    if (read_reg(imu, 0, &id) && id == 5) {
        write_reg(imu, 0x60, 0xb0);
        vTaskDelay(pdMS_TO_TICKS(25));
        imu_ready = change_reg(imu, 2, 0, 0x40) && write_reg(imu, 8, 0) && write_reg(imu, 3, 0x16) &&
                    change_reg(imu, 6, 0x0f, 1) && write_reg(imu, 8, 1);
    }
    ESP_LOGI(TAG, "QMI8658 %s (4g, 125Hz)", imu_ready ? "ready" : "unavailable");
    if (!ws::speaker_begin())
        ESP_LOGE(TAG, "speaker unavailable");
    // IDF 6's nonblocking VFS reads consult the driver's RX ring buffer.
    usb_serial_jtag_driver_config_t usb = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb));
    usb_serial_jtag_vfs_use_driver();
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    status();
}
void M5Unified::update() {
    uint32_t now = millis();
    BtnA.setRawState(now, gpio_get_level(WS_BTN_BOOT) == 0);
    if (now - key_poll_at >= 25) {
        key_poll_at = now;
        uint8_t irq = 0;
        if (read_reg(pmu, 0x49, &irq) && (irq & 8)) {
            write_reg(pmu, 0x49, 8);
            key_until = now + 70;
        }
    }
    BtnB.setRawState(now, int32_t(key_until - now) > 0);
    if (Touch.isEnabled())
        Touch.update(now);
    console();
}
M5Unified::Action M5Unified::takeAction() {
    auto result = action;
    action = None;
    return result;
}
uint32_t M5Unified::millis() {
    return esp_timer_get_time() / 1000;
}
void M5Unified::delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

m5::Power_Class::is_charging_t m5::Power_Class::isCharging() {
    refresh_power();
    return charging;
}
int8_t m5::Power_Class::getBatteryLevel() {
    refresh_power();
    return battery_pct;
}
uint16_t m5::Power_Class::getBatteryVoltage() {
    refresh_power();
    return battery_mv;
}
int16_t m5::Power_Class::getVBUSVoltage() {
    refresh_power();
    return vbus_mv;
}
bool m5::Power_Class::powerOff() {
    save_clock();
    M5.Speaker.stop();
    uint8_t status = 0;
    if (!read_reg(pmu, 0, &status))
        return false;
    if (status & 0x20) {
        ESP_LOGI(TAG, "USB connected: use unplug/PWR for shutdown");
        return false;
    }
    ESP_LOGI(TAG, "AXP2101 power off");
    return change_reg(pmu, 0x10, 2, 1);
}
bool m5::Imu_Class::isEnabled() {
    return imu_ready;
}
bool m5::Imu_Class::getAccel(float *x, float *y, float *z) {
    uint8_t data[6];
    if (!imu_ready || !x || !y || !z || !read_reg(imu, 0x35, data, 6))
        return false;
    *x = int16_t(data[0] | data[1] << 8) / 8192.f;
    *y = int16_t(data[2] | data[3] << 8) / 8192.f;
    *z = int16_t(data[4] | data[5] << 8) / 8192.f;
    return true;
}
bool m5::Rtc_Class::isEnabled() {
    return true;
}
bool m5::Rtc_Class::getDateTime(rtc_datetime_t *out) {
    if (!out)
        return false;
    time_t now = time(nullptr);
    struct tm tm_;
    localtime_r(&now, &tm_);
    out->date = rtc_date_t(tm_.tm_year + 1900, tm_.tm_mon + 1, tm_.tm_mday, tm_.tm_wday);
    out->time = rtc_time_t(tm_.tm_hour, tm_.tm_min, tm_.tm_sec);
    return true;
}
bool m5::Rtc_Class::getDate(rtc_date_t *out) {
    rtc_datetime_t dt;
    if (!out || !getDateTime(&dt))
        return false;
    *out = dt.date;
    return true;
}
bool m5::Rtc_Class::setDateTime(const rtc_datetime_t *dt) {
    if (!dt)
        return false;
    struct tm tm_ = {};
    tm_.tm_year = dt->date.year - 1900;
    tm_.tm_mon = dt->date.month - 1;
    tm_.tm_mday = dt->date.date;
    tm_.tm_hour = dt->time.hours;
    tm_.tm_min = dt->time.minutes;
    tm_.tm_sec = dt->time.seconds;
    timeval tv = {.tv_sec = mktime(&tm_), .tv_usec = 0};
    if (settimeofday(&tv, nullptr) != 0)
        return false;
    save_clock();
    return true;
}
bool m5::Rtc_Class::setDateTime(const rtc_date_t *d, const rtc_time_t *t) {
    if (!d || !t)
        return false;
    rtc_datetime_t dt(*d, *t);
    return setDateTime(&dt);
}
