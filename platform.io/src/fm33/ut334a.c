/*
 * Rad Pro
 * UNI-T UT334A driver
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 */

#if defined(UT334A)

#include "mcu-renderer-st7789.h"

#include "../fm33/device.h"
#include "../peripherals/comm.h"
#include "../peripherals/display.h"
#include "../peripherals/keyboard.h"
#include "../system/events.h"
#include "../system/system.h"

// System

void initGPIO(void)
{
    rcc_enable_pad();

    // Safe state before anything else (reset leaves every pin a disconnected input, so the HV
    // MOSFET gate on PC5 would float): HV converter stopped, HVbatt rail off, LCD supply off,
    // LED and speaker off. PD7/PD8 stay SWD.
    gpio_clear(TUBE_HV_PORT, TUBE_HV_PIN);
    gpio_setup_output(TUBE_HV_PORT, TUBE_HV_PIN, GPIO_OUTPUTTYPE_PUSHPULL);
    gpio_clear(PWR_HVBATT_PORT, PWR_HVBATT_PIN);
    gpio_setup_output(PWR_HVBATT_PORT, PWR_HVBATT_PIN, GPIO_OUTPUTTYPE_PUSHPULL);
    gpio_set(DISPLAY_POWER_PORT, DISPLAY_POWER_PIN);
    gpio_setup_output(DISPLAY_POWER_PORT, DISPLAY_POWER_PIN, GPIO_OUTPUTTYPE_PUSHPULL);
    gpio_clear(PULSE_LED_PORT, PULSE_LED_PIN);
    gpio_setup_output(PULSE_LED_PORT, PULSE_LED_PIN, GPIO_OUTPUTTYPE_PUSHPULL);
    gpio_clear(ALERT_LED_PORT, ALERT_LED_PIN);
    gpio_setup_output(ALERT_LED_PORT, ALERT_LED_PIN, GPIO_OUTPUTTYPE_PUSHPULL);
    gpio_clear(BUZZER_PORT, BUZZER_PIN);
    gpio_setup_output(BUZZER_PORT, BUZZER_PIN, GPIO_OUTPUTTYPE_PUSHPULL);

    // Keys (needed early: boot() checks the power key)
    gpio_setup_input(KEY_LEFT_PORT, KEY_LEFT_PIN, GPIO_PULL_PULLUP);
    gpio_setup_input(KEY_RIGHT_PORT, KEY_RIGHT_PIN, GPIO_PULL_PULLUP);
    gpio_setup_input(KEY_OK_PORT, KEY_OK_PIN, GPIO_PULL_PULLUP);
}

void initSystem(void)
{
    initSystemClock();

    rcc_enable_rtc();
}

// Comm

const char *const commId = "UNI-T UT334A;" FIRMWARE_NAME " " FIRMWARE_VERSION "/" LANGUAGE;

// Keyboard

void initKeyboardHardware(void)
{
    // GPIO (also set up in initGPIO, as boot() reads the power key early)
    gpio_setup_input(KEY_LEFT_PORT, KEY_LEFT_PIN, GPIO_PULL_PULLUP);
    gpio_setup_input(KEY_OK_PORT, KEY_OK_PIN, GPIO_PULL_PULLUP);
    gpio_setup_input(KEY_RIGHT_PORT, KEY_RIGHT_PIN, GPIO_PULL_PULLUP);
}

void onKeyboardTick(void)
{
    for (uint32_t i = 0; i < KEY_NUM; i++)
        keyboardKeyDown[i] <<= 1;

    keyboardKeyDown[KEY_LEFT] |= !gpio_get(KEY_LEFT_PORT, KEY_LEFT_PIN);
    keyboardKeyDown[KEY_OK] |= !gpio_get(KEY_OK_PORT, KEY_OK_PIN);
    keyboardKeyDown[KEY_RIGHT] |= !gpio_get(KEY_RIGHT_PORT, KEY_RIGHT_PIN);
}

void updateKeyboardState(void)
{
}

// Display

extern mr_t mr;

static bool displayEnabled;

static uint8_t displayTextbuffer[88 * 88];

// Panel settings of the stock firmware (0x5960), sent after mcu-renderer's reset. mcu-renderer sends
// COLMOD and MADCTL while the panel is still in sleep, and this panel ignores COLMOD there (it stays at
// the 18-bit default: text repeated 3x, striped greys), so as stock: SLPOUT, 120 ms, then MADCTL and
// COLMOD 0x05. The stock RAMWRC (0x3C) without data is left out.
static const uint8_t displayInitSequence[] = {
    MR_SEND_COMMAND(MR_ST7789_SLPOUT),
    MR_SLEEP(120),
    MR_SEND_COMMAND(MR_ST7789_MADCTL),
    MR_SEND_DATA(0x00),
    MR_SEND_COMMAND(MR_ST7789_COLMOD),
    MR_SEND_DATA(0x05),
    MR_SEND_COMMAND(MR_ST7789_IDMOFF),
    MR_SEND_COMMAND(MR_ST7789_WRDISBV),
    MR_SEND_DATA(0x00),
    MR_SEND_COMMAND(MR_ST7789_SPI2EN),
    MR_SEND_DATA(0x00),
    MR_SEND_COMMAND(MR_ST7789_PORCTRL),
    MR_SEND_DATA(0x0c),
    MR_SEND_DATA(0x0c),
    MR_SEND_DATA(0x00),
    MR_SEND_DATA(0x33),
    MR_SEND_DATA(0x33),
    MR_SEND_COMMAND(MR_ST7789_GCTRL),
    MR_SEND_DATA(0x35),
    MR_SEND_COMMAND(MR_ST7789_VCOMS),
    MR_SEND_DATA(0x32),
    MR_SEND_COMMAND(MR_ST7789_VDVVRHEN),
    MR_SEND_DATA(0x01),
    MR_SEND_COMMAND(MR_ST7789_VRHS),
    MR_SEND_DATA(0x15),
    MR_SEND_COMMAND(MR_ST7789_VDVS),
    MR_SEND_DATA(0x20),
    MR_SEND_COMMAND(MR_ST7789_FRCTRL2),
    MR_SEND_DATA(0x0f),
    MR_SEND_COMMAND(MR_ST7789_PWCTRL1),
    MR_SEND_DATA(0xa4),
    MR_SEND_DATA(0xa1),
    MR_SEND_COMMAND(MR_ST7789_PVGAMCTRL),
    MR_SEND_DATA(0xd0),
    MR_SEND_DATA(0x08),
    MR_SEND_DATA(0x0e),
    MR_SEND_DATA(0x09),
    MR_SEND_DATA(0x09),
    MR_SEND_DATA(0x05),
    MR_SEND_DATA(0x31),
    MR_SEND_DATA(0x33),
    MR_SEND_DATA(0x48),
    MR_SEND_DATA(0x17),
    MR_SEND_DATA(0x14),
    MR_SEND_DATA(0x15),
    MR_SEND_DATA(0x31),
    MR_SEND_DATA(0x34),
    MR_SEND_COMMAND(MR_ST7789_NVGAMCTRL),
    MR_SEND_DATA(0xd0),
    MR_SEND_DATA(0x08),
    MR_SEND_DATA(0x0e),
    MR_SEND_DATA(0x09),
    MR_SEND_DATA(0x09),
    MR_SEND_DATA(0x15),
    MR_SEND_DATA(0x31),
    MR_SEND_DATA(0x33),
    MR_SEND_DATA(0x48),
    MR_SEND_DATA(0x17),
    MR_SEND_DATA(0x14),
    MR_SEND_DATA(0x15),
    MR_SEND_DATA(0x31),
    MR_SEND_DATA(0x34),
    MR_SEND_COMMAND(MR_ST7789_INVON),
    MR_END(),
};

static void onDisplaySleep(uint32_t value)
{
    sleep(value);
}

static void onDisplaySetReset(bool value)
{
    gpio_modify(DISPLAY_RESX_PORT, DISPLAY_RESX_PIN, !value);
}

static void onDisplaySetChipselect(bool value)
{
    spi_wait_while_busy(DISPLAY_SPI);

    gpio_modify(DISPLAY_CSX_PORT, DISPLAY_CSX_PIN, !value);
}

static void onDisplaySetCommand(bool value)
{
    spi_wait_while_busy(DISPLAY_SPI);

    gpio_modify(DISPLAY_DCX_PORT, DISPLAY_DCX_PIN, !value);
}

static void onDisplaySend(uint16_t value)
{
    spi_send(DISPLAY_SPI, value);
}

static void onDisplaySend16(uint16_t value)
{
    spi_send(DISPLAY_SPI, (value >> 8) & 0xff);
    spi_send(DISPLAY_SPI, (value >> 0) & 0xff);
}

void initDisplay(void)
{
    // GPIO
    gpio_set(DISPLAY_RESX_PORT, DISPLAY_RESX_PIN);
    gpio_set(DISPLAY_CSX_PORT, DISPLAY_CSX_PIN);
    gpio_clear(DISPLAY_DCX_PORT, DISPLAY_DCX_PIN);

    gpio_setup_output(DISPLAY_RESX_PORT, DISPLAY_RESX_PIN, GPIO_OUTPUTTYPE_PUSHPULL);
    gpio_setup_output(DISPLAY_CSX_PORT, DISPLAY_CSX_PIN, GPIO_OUTPUTTYPE_PUSHPULL);
    gpio_setup_output(DISPLAY_DCX_PORT, DISPLAY_DCX_PIN, GPIO_OUTPUTTYPE_PUSHPULL);
    gpio_setup_af_dfs(DISPLAY_SCL_PORT, DISPLAY_SCL_PIN, GPIO_OUTPUTTYPE_PUSHPULL, GPIO_PULL_FLOATING, false);
    gpio_setup_af_dfs(DISPLAY_SDA_PORT, DISPLAY_SDA_PIN, GPIO_OUTPUTTYPE_PUSHPULL, GPIO_PULL_FLOATING, false);

    // LCD supply (P-MOSFET, low = on), then let it settle (stock: 1 ms)
#if defined(DISPLAY_POWER_ACTIVE_LOW)
    gpio_clear(DISPLAY_POWER_PORT, DISPLAY_POWER_PIN);
#else
    gpio_set(DISPLAY_POWER_PORT, DISPLAY_POWER_PIN);
#endif
    sleep(2);

    // SPI: 32 MHz (APBCLK / 2), as stock
    rcc_enable_spi1();
    spi_setup_master_tx(DISPLAY_SPI, SPI_BAUD_DIV2);

    // mcu-renderer
    mr_st7789_init(&mr,
                   240,
                   320,
                   MR_DISPLAY_ROTATION_0,
                   displayTextbuffer,
                   sizeof(displayTextbuffer),
                   onDisplaySleep,
                   onDisplaySetReset,
                   onDisplaySetChipselect,
                   onDisplaySetCommand,
                   onDisplaySend,
                   onDisplaySend16);

    mr_send_sequence(&mr, displayInitSequence);

    initBacklight();
}

void setDisplayEnabled(bool value)
{
    mr_st7789_set_display(&mr, value);
    mr_st7789_set_sleep(&mr, !value);

    displayEnabled = value;
}

bool isDisplayEnabled(void)
{
    return displayEnabled;
}

void refreshDisplay(void)
{
}

#endif
