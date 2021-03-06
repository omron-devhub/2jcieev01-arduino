/*
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
#include "lis2dw.h"
#include <SPI.h>

/* defines */
#define LIS2DW_FIFO_SIZE   1
#define LIS2DW_VAL_DEVICEID 0x44
#define LIS2DW_CONV(x) ((double)(x) * 4000.0 / 32767.0)

#define conv8s_s16_le(b, n) ((int16_t)b[n] | ((int16_t)b[n + 1] << 8))

#define debg(x, ...) Serial.print(x, ##__VA_ARGS__)
#define debl(x, ...) Serial.println(x, ##__VA_ARGS__)

static uint8_t ram_acc[3 * 2 * LIS2DW_FIFO_SIZE] = {0};


/* defines */
#define GPIO_LED_R_PIN A12
#define GPIO_LED_G_PIN A11
#define GPIO_LED_B_PIN A1

#define PIN_SCLK    5
#define PIN_MOSI    18
#define PIN_MISO    19
#define PIN_CSB     4

#define SPI_CLK_SPEED   1000000
#define SPI_CS_HW

#if defined(SPI_CS_HW)
#define SOFT_CS_UP()
#define SOFT_CS_DOWN()
#else
#define SOFT_CS_DOWN()  digitalWrite(PIN_CSB, LOW)
#define SOFT_CS_UP()    digitalWrite(PIN_CSB, HIGH)
#endif

/* SPI functions */
void spi_setup() {
    SPI.begin(PIN_SCLK, PIN_MISO, PIN_MOSI, PIN_CSB);
    SPI.setFrequency(SPI_CLK_SPEED);
    SPI.setDataMode(SPI_MODE0);
    SPI.setBitOrder(MSBFIRST);
    #if defined(SPI_CS_HW)
    SPI.setHwCs(true);
    #else
    pinMode(PIN_CSB, OUTPUT);
    digitalWrite(PIN_CSB, HIGH);
    #endif
}

/** <!-- spi_read {{{1 --> read data from SPI-bus.
 */
void spi_read(uint8_t* tx, uint8_t tx_len, uint8_t* rx, uint8_t rx_len) {
    uint8_t rbuf[257] = {0};

    SOFT_CS_DOWN();
    SPI.transferBytes(tx, rbuf, (uint16_t)rx_len + 1);
    SOFT_CS_UP();
    for (uint16_t i = 0; i < rx_len; i++) {
        rx[i] = rbuf[i + 1];
    }
}

/** <!-- spi_write {{{1 --> write data to SPI-bus.
 */
void spi_write(uint8_t* pdata, uint8_t len) {
    // TODO(kuriyama): assert(len < 8)
    uint8_t dummy[8] = {0};
    SOFT_CS_DOWN();
    SPI.transferBytes(pdata, dummy, len);
    SOFT_CS_UP();
}

/** <!-- lis2dw_setup {{{1 --> setup a accerelometer sensor.
 */
void lis2dw_setup(void) {
    uint32_t retry = 100;
    uint8_t rbuf[8] = {0};
    /* Check connection */
    while ((rbuf[0] != LIS2DW_VAL_DEVICEID) && (retry > 0)) {
        lis2dw_readRegister(LIS2DW_REG_WHOAMI, rbuf, 1);
        delay(10);
        retry--;
    }
    debg("LIS2DW: WhoAmI: "); debg(rbuf[0], HEX);
    debg(", retry:");
    debl(retry);

    lis2dw_normalconfig();
}

/** <!-- lis2dw_normalconfig {{{1 --> set configuration to registers.
 */
void lis2dw_normalconfig(void) {
    uint8_t wbuf[8] = {0};

    wbuf[0] = 0x54;   // REG1: 100Hz, High-Performance
    wbuf[1] = 0x06;   // REG2:
    wbuf[2] = 0x00;   // REG3:
    wbuf[3] = 0x00;   // REG4: INT1
    wbuf[4] = 0x00;   // REG5: INT2
    wbuf[5] = 0x14;   // REG6: FS 4g

    lis2dw_writeRegister(LIS2DW_REG_CTRL1, wbuf, 6);
}

/** <!-- lis2dw_read_and_avg {{{1 --> get accerelo values from FIFO and
 * make average values.
 */
int lis2dw_read_and_avg(int16_t* accl) {
    uint8_t* accbuf = ram_acc;
    int32_t accsum[3] = {0, 0, 0};

    /* get accel data (x,y,z) x N */
    lis2dw_fifo_read(accbuf);
    for (uint8_t i = 0; i < LIS2DW_FIFO_SIZE; i++) {
        int n = i * 6;
        accsum[0] += (int32_t)conv8s_s16_le(accbuf, n + 0);
        accsum[1] += (int32_t)conv8s_s16_le(accbuf, n + 2);
        accsum[2] += (int32_t)conv8s_s16_le(accbuf, n + 4);
    }
    accl[0] = (int16_t)(accsum[0] / LIS2DW_FIFO_SIZE);
    accl[1] = (int16_t)(accsum[1] / LIS2DW_FIFO_SIZE);
    accl[2] = (int16_t)(accsum[2] / LIS2DW_FIFO_SIZE);
    return 0;
}

void lis2dw_fifo_read(uint8_t* pdata) {
    lis2dw_readRegister(LIS2DW_REG_OUT_X_L, pdata, 3*2*LIS2DW_FIFO_SIZE);
}

/** <!-- lis2dw_writeRegister {{{1 --> set registers
 */
void lis2dw_writeRegister(uint8_t reg, uint8_t* pbuf, uint8_t len) {
    uint8_t txbuf[256] = {0};

    txbuf[0] = reg & 0x7F;

    for (uint8_t i = 0; i < len; i++) {
        txbuf[i + 1] = pbuf[i];
    }
    spi_write(txbuf, (len + 1));
}

/** <!-- lis2dw_readRegister {{{1 --> get registers
 */
void lis2dw_readRegister(uint8_t reg, uint8_t* pbuf, uint8_t len) {
    uint8_t txbuf[256] = {0};

    txbuf[0] = reg | 0x80;

    spi_read(txbuf, 1, pbuf, len);
}

/** <!-- setup - accelerometer sensor {{{1 -->
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
    spi_setup();  // master

    Serial.println("sensor: accelerometer");
    lis2dw_setup();
    delay(32);
}

/** <!-- loop - accelerometer sensor {{{1 -->
 * 1. blink LEDs
 * 2. read and convert sensor.
 * 3. output results, format is: x[mg], y[mg], z[mg]
 */
void loop() {
    static bool blink = false;
    int16_t accl[3];

    blink = !blink;
    digitalWrite(GPIO_LED_R_PIN, blink ? HIGH: LOW);
    digitalWrite(GPIO_LED_G_PIN, blink ? HIGH: LOW);
    digitalWrite(GPIO_LED_B_PIN, blink ? HIGH: LOW);
    delay(900);
    int ret = lis2dw_read_and_avg(accl);
    Serial.print("sensor output:");
    Serial.print(LIS2DW_CONV(accl[0]));
    Serial.print(",");
    Serial.print(LIS2DW_CONV(accl[1]));
    Serial.print(",");
    Serial.print(LIS2DW_CONV(accl[2]));
    #if defined(OUTPUT_RAW)  // raw output
    Serial.print(","); Serial.print(accl[0]);
    Serial.print(","); Serial.print(accl[1]);
    Serial.print(","); Serial.print(accl[2]);
    #endif
    Serial.print(", return code:");
    Serial.println(ret);
}
// vi: ft=arduino:fdm=marker:et:sw=4:tw=80
