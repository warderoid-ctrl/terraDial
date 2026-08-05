#pragma once

// LGFX panel definition for the CrowPanel 1.28" round display (GC9A01).
// Ported from Elecrow's factory example (RotaryScreen_1_28_new.ino) --
// pin values now come from pins.h instead of being repeated here.

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "pins.h"

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_GC9A01 _panel_instance;
    lgfx::Bus_SPI _bus_instance;

public:
    LGFX(void)
    {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 80000000;
            cfg.freq_read = 20000000;
            cfg.spi_3wire = true;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = PIN_LCD_SCLK;
            cfg.pin_mosi = PIN_LCD_MOSI;
            cfg.pin_miso = PIN_LCD_MISO;
            cfg.pin_dc = PIN_LCD_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = PIN_LCD_CS;
            cfg.pin_rst = PIN_LCD_RST;
            cfg.pin_busy = -1;
            cfg.memory_width = 240;
            cfg.memory_height = 240;
            cfg.panel_width = 240;
            cfg.panel_height = 240;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = false;
            cfg.invert = true;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            _panel_instance.config(cfg);
        }
        setPanel(&_panel_instance);
    }
};

inline void backlightInit()
{
    ledcSetup(BACKLIGHT_PWM_CHANNEL, BACKLIGHT_PWM_FREQ, BACKLIGHT_PWM_RESOLUTION);
    ledcAttachPin(PIN_LCD_BACKLIGHT, BACKLIGHT_PWM_CHANNEL);
    ledcWrite(BACKLIGHT_PWM_CHANNEL, 255); // full brightness by default
}

// percent: 0-100
inline void backlightSet(uint8_t percent)
{
    if (percent > 100) percent = 100;
    ledcWrite(BACKLIGHT_PWM_CHANNEL, (percent * 255) / 100);
}
