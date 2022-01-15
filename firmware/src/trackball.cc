/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Jacek Fedorynski
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bsp/board.h>
#include <tusb.h>

#include <pico/bootrom.h>
#include <pico/stdlib.h>

#include <hardware/flash.h>
#include <hardware/gpio.h>

#include "crc.h"
#include "pmw3360.h"

// These IDs are bogus. If you want to distribute any hardware using this,
// you will have to get real ones.
#define USB_VID 0xCAFE
#define USB_PID 0xBADA

#define CONFIG_VERSION 1
#define CONFIG_SIZE 26

#define NSENSORS 2
#define NBUTTONS 4

#define PRESUMED_FLASH_SIZE 2097152
#define CONFIG_OFFSET_IN_FLASH (PRESUMED_FLASH_SIZE - FLASH_SECTOR_SIZE)
#define FLASH_CONFIG_IN_MEMORY (((uint8_t*) XIP_BASE) + CONFIG_OFFSET_IN_FLASH)

uint button_pins[NBUTTONS] = { 16, 17, 24, 26 };

#define SENSOR0_SPI spi0
#define SENSOR0_MISO 4
#define SENSOR0_MOSI 3
#define SENSOR0_SCK 2
#define SENSOR0_NCS 9
#define SENSOR1_SPI spi0
#define SENSOR1_MISO 20
#define SENSOR1_MOSI 23
#define SENSOR1_SCK 18
#define SENSOR1_NCS 25

PMW3360 sensors[NSENSORS] = {
    PMW3360(SENSOR0_SPI, SENSOR0_MISO, SENSOR0_MOSI, SENSOR0_SCK, SENSOR0_NCS),
    PMW3360(SENSOR1_SPI, SENSOR1_MISO, SENSOR1_MOSI, SENSOR1_SCK, SENSOR1_NCS),
};

tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x00,

    .bNumConfigurations = 0x01,
};

uint8_t const desc_hid_report[] = {
    0x05, 0x01,         // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,         // Usage (Mouse)
    0xA1, 0x01,         // Collection (Application)
    0x05, 0x01,         //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,         //   Usage (Mouse)
    0xA1, 0x02,         //   Collection (Logical)
    0x85, 0x01,         //     Report ID (1)
    0x09, 0x01,         //     Usage (Pointer)
    0xA1, 0x00,         //     Collection (Physical)
    0x05, 0x09,         //       Usage Page (Button)
    0x19, 0x01,         //       Usage Minimum (0x01)
    0x29, 0x08,         //       Usage Maximum (0x08)
    0x95, 0x08,         //       Report Count (8)
    0x75, 0x01,         //       Report Size (1)
    0x25, 0x01,         //       Logical Maximum (1)
    0x81, 0x02,         //       Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,         //       Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,         //       Usage (X)
    0x09, 0x31,         //       Usage (Y)
    0x95, 0x02,         //       Report Count (2)
    0x75, 0x10,         //       Report Size (16)
    0x16, 0x00, 0x80,   //       Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,   //       Logical Maximum (32767)
    0x81, 0x06,         //       Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
    0xA1, 0x02,         //       Collection (Logical)
    0x85, 0x02,         //         Report ID (2)
    0x09, 0x48,         //         Usage (Resolution Multiplier)
    0x95, 0x01,         //         Report Count (1)
    0x75, 0x02,         //         Report Size (2)
    0x15, 0x00,         //         Logical Minimum (0)
    0x25, 0x01,         //         Logical Maximum (1)
    0x35, 0x01,         //         Physical Minimum (1)
    0x45, 0x78,         //         Physical Maximum (120)
    0xB1, 0x02,         //         Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x01,         //         Report ID (1)
    0x09, 0x38,         //         Usage (Wheel)
    0x35, 0x00,         //         Physical Minimum (0)
    0x45, 0x00,         //         Physical Maximum (0)
    0x16, 0x00, 0x80,   //         Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,   //         Logical Maximum (32767)
    0x75, 0x10,         //         Report Size (16)
    0x81, 0x06,         //         Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,               //       End Collection
    0xA1, 0x02,         //       Collection (Logical)
    0x85, 0x02,         //         Report ID (2)
    0x09, 0x48,         //         Usage (Resolution Multiplier)
    0x75, 0x02,         //         Report Size (2)
    0x15, 0x00,         //         Logical Minimum (0)
    0x25, 0x01,         //         Logical Maximum (1)
    0x35, 0x01,         //         Physical Minimum (1)
    0x45, 0x78,         //         Physical Maximum (120)
    0xB1, 0x02,         //         Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x35, 0x00,         //         Physical Minimum (0)
    0x45, 0x00,         //         Physical Maximum (0)
    0x75, 0x04,         //         Report Size (4)
    0xB1, 0x03,         //         Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x01,         //         Report ID (1)
    0x05, 0x0C,         //         Usage Page (Consumer)
    0x16, 0x00, 0x80,   //         Logical Minimum (-32768)
    0x26, 0xFF, 0x7F,   //         Logical Maximum (32767)
    0x75, 0x10,         //         Report Size (16)
    0x0A, 0x38, 0x02,   //         Usage (AC Pan)
    0x81, 0x06,         //         Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,               //       End Collection
    0xC0,               //     End Collection
    0xC0,               //   End Collection
    0x06, 0x00, 0xFF,   //   Usage Page (Vendor Defined 0xFF00)
    0x09, 0x20,         //   Usage (0x20)
    0x85, 0x03,         //   Report ID (3)
    0x75, 0x08,         //   Report Size (8)
    0x95, CONFIG_SIZE,  //   Report Count (CONFIG_SIZE)
    0xB1, 0x02,         //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,               // End Collection
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
#define EPNUM_HID 0x81

uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, 0, 100),

    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 1)
};

char const* string_desc_arr[] = {
    (const char[]){ 0x09, 0x04 },  // 0: is supported language is English (0x0409)
    "RP2040+PMW3360",              // 1: Manufacturer
    "Trackball",                   // 2: Product
};

struct __attribute__((packed)) hid_report_t {
    uint8_t buttons;
    int16_t dx;
    int16_t dy;
    int16_t vwheel;
    int16_t hwheel;
};

hid_report_t report;

enum class ButtonFunction : int8_t {
    NO_FUNCTION = 0,
    BUTTON1 = 1,
    BUTTON2 = 2,
    BUTTON3 = 3,
    BUTTON4 = 4,
    BUTTON5 = 5,
    BUTTON6 = 6,
    BUTTON7 = 7,
    BUTTON8 = 8,
    CLICK_DRAG = 9,
    SHIFT = 10,
};

enum class SensorFunction : int8_t {
    NO_FUNCTION = 0,
    CURSOR_X = 1,
    CURSOR_Y = 2,
    VERTICAL_SCROLL = 3,
    HORIZONTAL_SCROLL = 4,
    CURSOR_X_INVERTED = -1,
    CURSOR_Y_INVERTED = -2,
    VERTICAL_SCROLL_INVERTED = -3,
    HORIZONTAL_SCROLL_INVERTED = -4,
};

enum class ConfigCommand : int8_t {
    NO_COMMAND = 0,
    RESET_INTO_BOOTSEL = 1,
};

struct __attribute__((packed)) config_t {
    uint8_t version;
    ConfigCommand command;
    SensorFunction sensor_function[NSENSORS][2];
    SensorFunction sensor_shifted_function[NSENSORS][2];
    uint8_t sensor_cpi[NSENSORS];
    uint8_t sensor_shifted_cpi[NSENSORS];
    ButtonFunction button_function[NBUTTONS];
    ButtonFunction button_shifted_function[NBUTTONS];
    uint32_t crc32;
};

config_t config = {
    .version = CONFIG_VERSION,
    .command = ConfigCommand::NO_COMMAND,
    .sensor_function = {
        { SensorFunction::CURSOR_X, SensorFunction::CURSOR_Y_INVERTED },
        { SensorFunction::VERTICAL_SCROLL, SensorFunction::NO_FUNCTION },
    },
    .sensor_shifted_function = {
        { SensorFunction::CURSOR_X, SensorFunction::CURSOR_Y_INVERTED },
        { SensorFunction::VERTICAL_SCROLL, SensorFunction::NO_FUNCTION },
    },
    .sensor_cpi = {
        600 / 100,
        800 / 100,
    },
    .sensor_shifted_cpi = {
        600 / 100,
        800 / 100,
    },
    .button_function = {
        ButtonFunction::BUTTON1,
        ButtonFunction::BUTTON1,
        ButtonFunction::BUTTON2,
        ButtonFunction::BUTTON3,
    },
    .button_shifted_function = {
        ButtonFunction::BUTTON1,
        ButtonFunction::BUTTON1,
        ButtonFunction::BUTTON2,
        ButtonFunction::BUTTON3,
    },
    .crc32 = 0,
};

uint8_t resolution_multiplier = 0;

int accumulated_scroll[NSENSORS][2] = { 0 };
uint64_t last_scroll_timestamp[NSENSORS][2] = { 0 };
uint32_t prev_pin_state = 0xffffffff;
bool click_drag = false;
uint8_t current_cpi[NSENSORS] = { 0 };
bool scroll_mode = false;
bool not_scroll_mode = false;
float running_avg_x = 0;
float running_avg_y = 0;
float running_avg_hscroll = 0;
float running_avg_vscroll = 0;

int16_t handle_scroll(int sensor, int axis, int16_t movement, uint8_t multiplier_mask, float* running_avg_scroll) {
    int16_t ret = 0;
    *running_avg_scroll += 0.1 * movement / current_cpi[sensor];
    if (resolution_multiplier & multiplier_mask) {
        ret = movement;
    } else {
        if (movement != 0) {
            last_scroll_timestamp[sensor][axis] = time_us_64();
            accumulated_scroll[sensor][axis] += movement;
            int ticks = accumulated_scroll[sensor][axis] / 120;
            accumulated_scroll[sensor][axis] -= ticks * 120;
            ret = ticks;
        } else {
            if ((accumulated_scroll[sensor][axis] != 0) &&
                (time_us_64() - last_scroll_timestamp[sensor][axis] > 1000000)) {
                accumulated_scroll[sensor][axis] = 0;
            }
        }
    }
    return ret;
}

/*
 * This function tries to decide whether we're scrolling or moving the cursor.
 * It then commits to one or the other until the ball stops moving.
 * If it decides that we're moving the cursor then it zeroes any scroll input.
 * It doesn't currently cancel cursor movement when it decides that we're
 * scrolling.
 * The sensor axes functions are configurable, but below logic probably only
 * makes sense if the default mapping is used. It might also slightly interfere
 * with a configuration where you have the ball set to scroll when "shift"
 * button is held, so keep that in mind.
 * False positives still happen occasionally, you can try to tweak the
 * thresholds to improve the situation.
 * In theory it is CPI agnostic, but I haven't done a lot of testing with
 * different CPI values.
 * Oh, and the running averages are sensitive to how many samples per second
 * we're getting from the sensors, at the time it was being written it was
 * around 500.
 */
void handle_twist_to_scroll() {
    if (fabs(running_avg_x) < 0.1 / 12 && fabs(running_avg_y) < 0.1 / 12) {
        not_scroll_mode = false;
    }

    if (fabs(running_avg_vscroll) < 0.1 / 16) {
        scroll_mode = false;
    }

    if (!scroll_mode && (running_avg_x * running_avg_x + running_avg_y * running_avg_y > 4.0 / (12 * 12))) {
        not_scroll_mode = true;
    }

    if (!not_scroll_mode && running_avg_vscroll * running_avg_vscroll > 4.0 / (16 * 16)) {
        scroll_mode = true;
    }

    if (!scroll_mode || not_scroll_mode) {
        report.vwheel = 0;
        for (int sensor = 0; sensor < NSENSORS; sensor++) {
            for (int axis = 0; axis < 2; axis++) {
                // ignoring the shifted function for now...
                if (config.sensor_function[sensor][axis] == SensorFunction::VERTICAL_SCROLL ||
                    config.sensor_function[sensor][axis] == SensorFunction::VERTICAL_SCROLL_INVERTED) {
                    accumulated_scroll[sensor][axis] = 0;
                }
            }
        }
    }
}

void hid_task() {
    if (!tud_hid_ready()) {
        return;
    }

    memset(&report, 0, sizeof(report));

    uint32_t pin_state = gpio_get_all();

    bool shifted = false;

    // first pass to determine if we're in shifted state
    for (int i = 0; i < NBUTTONS; i++) {
        if (config.button_function[i] == ButtonFunction::SHIFT &&
            !(pin_state & (1 << button_pins[i]))) {
            shifted = true;
        }
    }

    // set CPI if not already correct
    for (int i = 0; i < NSENSORS; i++) {
        uint8_t wanted_cpi = shifted ? config.sensor_shifted_cpi[i] : config.sensor_cpi[i];
        if (current_cpi[i] != wanted_cpi && wanted_cpi >= 1 && wanted_cpi <= 120) {
            sensors[i].set_cpi(wanted_cpi * 100);
            current_cpi[i] = wanted_cpi;
        }
    }

    for (int i = 0; i < NBUTTONS; i++) {
        ButtonFunction button_function =
            shifted ? config.button_shifted_function[i] : config.button_function[i];
        if (config.button_function[i] == ButtonFunction::SHIFT) {
            button_function = ButtonFunction::NO_FUNCTION;
        }
        switch (button_function) {
            case ButtonFunction::NO_FUNCTION:
            case ButtonFunction::SHIFT:
                break;
            case ButtonFunction::BUTTON1:
            case ButtonFunction::BUTTON2:
            case ButtonFunction::BUTTON3:
            case ButtonFunction::BUTTON4:
            case ButtonFunction::BUTTON5:
            case ButtonFunction::BUTTON6:
            case ButtonFunction::BUTTON7:
            case ButtonFunction::BUTTON8: {
                int button = static_cast<int>(button_function) - 1;
                if (!(pin_state & (1 << button_pins[i]))) {
                    report.buttons |= 1 << button;
                }
                break;
            }
            case ButtonFunction::CLICK_DRAG:
                if ((prev_pin_state & (1 << button_pins[i])) &&
                    !(pin_state & (1 << button_pins[i]))) {
                    click_drag = !click_drag;
                }
                break;
        }
    }

    if (click_drag) {
        report.buttons |= 1 << 0;
    }

    prev_pin_state = pin_state;

    running_avg_x *= 0.9;
    running_avg_y *= 0.9;
    running_avg_vscroll *= 0.9;
    running_avg_hscroll *= 0.9;

    for (int sensor = 0; sensor < NSENSORS; sensor++) {
        sensors[sensor].update();
        for (int axis = 0; axis < 2; axis++) {
            int16_t movement = sensors[sensor].movement[axis];
            SensorFunction sensor_function =
                shifted ? config.sensor_shifted_function[sensor][axis] : config.sensor_function[sensor][axis];
            if (static_cast<int>(sensor_function) < 0) {
                movement *= -1;
            }
            switch (sensor_function) {
                case SensorFunction::NO_FUNCTION:
                    break;
                case SensorFunction::CURSOR_X:
                case SensorFunction::CURSOR_X_INVERTED:
                    report.dx += movement;
                    running_avg_x += 0.1 * movement / current_cpi[sensor];
                    break;
                case SensorFunction::CURSOR_Y:
                case SensorFunction::CURSOR_Y_INVERTED:
                    report.dy += movement;
                    running_avg_y += 0.1 * movement / current_cpi[sensor];
                    break;
                case SensorFunction::VERTICAL_SCROLL:
                case SensorFunction::VERTICAL_SCROLL_INVERTED:
                    report.vwheel += handle_scroll(sensor, axis, movement, 1 << 0, &running_avg_vscroll);
                    break;
                case SensorFunction::HORIZONTAL_SCROLL:
                case SensorFunction::HORIZONTAL_SCROLL_INVERTED:
                    report.hwheel += handle_scroll(sensor, axis, movement, 1 << 2, &running_avg_hscroll);
                    break;
            }
        }
    }

    // uncomment to have pressing all buttons reset into BOOTSEL
    // (convenient during development)
    // if (!(pin_state & (1 << button_pins[0]) ||
    //       pin_state & (1 << button_pins[1]) ||
    //       pin_state & (1 << button_pins[2]) ||
    //       pin_state & (1 << button_pins[3]))) {
    //     reset_usb_boot(0, 0);
    // }

    handle_twist_to_scroll();

    tud_hid_report(1, &report, sizeof(report));
}

void pin_init(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

void pins_init() {
    for (int i = 0; i < NBUTTONS; i++) {
        pin_init(button_pins[i]);
    }
}

void sensors_init() {
    for (int i = 0; i < NSENSORS; i++) {
        sensors[i].init();
    }
}

void run_config_command() {
    // we probably shouldn't do this for config read from flash
    // or let's just not write any non-null command to flash
    if (config.command == ConfigCommand::RESET_INTO_BOOTSEL) {
        reset_usb_boot(0, 0);
    }
}

bool checksum_ok(const uint8_t* buffer) {
    return crc32(buffer, CONFIG_SIZE - 4) == ((config_t*) buffer)->crc32;
}

bool version_ok(const uint8_t* buffer) {
    return ((config_t*) buffer)->version == CONFIG_VERSION;
}

void load_config() {
    if (checksum_ok(FLASH_CONFIG_IN_MEMORY) && version_ok(FLASH_CONFIG_IN_MEMORY)) {
        memcpy(&config, FLASH_CONFIG_IN_MEMORY, CONFIG_SIZE);
    }
}

void persist_config() {
    uint8_t buffer[FLASH_PAGE_SIZE];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, &config, CONFIG_SIZE);
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(CONFIG_OFFSET_IN_FLASH, FLASH_SECTOR_SIZE);
    flash_range_program(CONFIG_OFFSET_IN_FLASH, buffer, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
}

int main() {
    stdio_init_all();
    board_init();
    load_config();
    pins_init();
    sensors_init();
    tusb_init();

    while (true) {
        tud_task();  // tinyusb device task
        hid_task();
    }

    return 0;
}

// Invoked when device is mounted
void tud_mount_cb() {
    // reset hi-res scroll for when we reboot from Windows into Linux
    resolution_multiplier = 0;
}

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const* tud_descriptor_device_cb() {
    return (uint8_t const*) &desc_device;
}

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const* tud_hid_descriptor_report_cb(uint8_t itf) {
    return desc_hid_report;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    if (report_id == 2 && reqlen >= 1) {
        memcpy(buffer, &resolution_multiplier, 1);
        return 1;
    }
    if (report_id == 3 && reqlen >= CONFIG_SIZE) {
        config.crc32 = crc32((uint8_t*) &config, CONFIG_SIZE - 4);
        memcpy(buffer, &config, CONFIG_SIZE);
        return CONFIG_SIZE;
    }

    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    if (report_id == 2 && bufsize >= 1) {
        memcpy(&resolution_multiplier, buffer, 1);
    }
    if (report_id == 3 && bufsize >= CONFIG_SIZE) {
        if (checksum_ok(buffer) && version_ok(buffer)) {
            memcpy(&config, buffer, CONFIG_SIZE);
            run_config_command();
            persist_config();
        }
    }
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    return desc_configuration;
}

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
        // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

        if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])))
            return NULL;

        const char* str = string_desc_arr[index];

        // Cap at max char
        chr_count = strlen(str);
        if (chr_count > 31)
            chr_count = 31;

        // Convert ASCII string into UTF-16
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

    return _desc_str;
}
