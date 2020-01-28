/**
 * MIT License
 * Copyright (c) 2019, 2018 - present OMRON Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
/* includes */
#include "opt3001.h"
#include <Wire.h>

/* defines */
#define GPIO_LED_R_PIN A12
#define GPIO_LED_G_PIN A11
#define GPIO_LED_B_PIN A1

#define conv8s_u16_be(b, n) \
    (uint16_t)(((uint16_t)b[n] << 8) | (uint16_t)b[n + 1])


/* I2C functions */
/** <!-- i2c_write_reg8 {{{1 --> I2C write function for bytes transfer.
 */
bool i2c_write_reg8(uint8_t slave_addr, uint8_t register_addr,
                       uint8_t *write_buff, uint8_t len) {
    Wire.beginTransmission(slave_addr);

    Wire.write(register_addr);
    if (len != 0) {
        for (uint8_t i = 0; i < len; i++) {
            Wire.write(write_buff[i]);
        }
    }
    Wire.endTransmission();
    return false;
}

/** <!-- i2c_read_reg8 {{{1 --> I2C read function for bytes transfer.
 */
bool i2c_read_reg8(uint8_t slave_addr, uint8_t register_addr,
                      uint8_t *read_buff, uint8_t len) {
    i2c_write_reg8(slave_addr, register_addr, NULL, 0);

    Wire.requestFrom(slave_addr, len);

    if (Wire.available() != len) {
        return true;
    }
    for (uint16_t i = 0; i < len; i++) {
        read_buff[i] = Wire.read();
    }
    return false;
}

/** <!-- opt3001_setup {{{1 --> setup for OPT3001
 * 1. sensor setup and start to measurements.
 */
void opt3001_setup(void) {
    uint8_t wbuf[2];

    wbuf[0] = OPT3001_CMD_CONFIG_MSB;
    wbuf[1] = OPT3001_CMD_CONFIG_LSB;

    i2c_write_reg8(OPT3001_ADDR, OPT3001_REG_CONFIG, wbuf, sizeof(wbuf));
}

/** <!-- opt3001_read {{{1 --> read sensor digit and convert to physical values
 */
int opt3001_read(uint16_t* light) {
    bool result;
    uint8_t rbuf[2];
    uint16_t raw_data;

    result = i2c_read_reg8(OPT3001_ADDR, OPT3001_REG_CONFIG,
                           rbuf, sizeof(rbuf));
    if (result) {
        return 1;
    }
    if ((rbuf[1] & 0x80) == 0) {
        return 2;  // sensor is working...
    }

    result = i2c_read_reg8(OPT3001_ADDR, OPT3001_REG_RESULT, rbuf, sizeof(rbuf));
    if (result) {
        return 100;
    }

    raw_data = conv8s_u16_be(rbuf, 0);
    *light = (uint16_t)(opt3001_convert_lux_value_x100(raw_data) / 100);
    return 0;
}

/** <!-- opt3001_convert_lux_value_x100 {{{1 --> convert sensors
 * raw output digits to [100lx]
 */
uint32_t opt3001_convert_lux_value_x100(uint16_t value_raw) {
    uint32_t value_converted = 0;
    uint32_t exp;
    uint32_t data;

    /* Convert the value to centi-percent RH */
    exp = (value_raw >> 12) & 0x0F;
    exp = 1 << exp;
    data = value_raw & 0x0FFF;
    value_converted = (uint32_t)(exp * data);

    return value_converted;
}

/** <!-- setup - illuminance sensor {{{1 -->
 * 1. setup LED gpio.
 * 2. setup sensor
 */
void setup() {
    Serial.begin(115200);
    Serial.println("peripherals: GPIO");
    pinMode(GPIO_LED_R_PIN, OUTPUT);
    pinMode(GPIO_LED_G_PIN, OUTPUT);
    pinMode(GPIO_LED_B_PIN, OUTPUT);

    digitalWrite(GPIO_LED_R_PIN, LOW);
    digitalWrite(GPIO_LED_G_PIN, LOW);
    digitalWrite(GPIO_LED_B_PIN, LOW);

    Serial.println("peripherals: I2C");
    Wire.begin();  // master

    Serial.println("sensor: illuminance");
    opt3001_setup();
    delay(32);
}

/** <!-- loop - illuminance sensor {{{1 -->
 * 1. blink LEDs
 * 2. read and convert sensor.
 * 3. output results, format is: x10[Pa], x100[degC],digit,digit
 */
void loop() {
    static bool blink = false;
    uint16_t illm;

    blink = !blink;
    digitalWrite(GPIO_LED_R_PIN, blink ? HIGH: LOW);
    digitalWrite(GPIO_LED_G_PIN, blink ? HIGH: LOW);
    digitalWrite(GPIO_LED_B_PIN, blink ? HIGH: LOW);
    delay(900);
    int ret = opt3001_read(&illm);
    Serial.print("sensor output:");
    Serial.print(illm);
    Serial.print(", return code:");
    Serial.println(ret);
}
// vi: ft=arduino:fdm=marker:et:sw=4:tw=80
