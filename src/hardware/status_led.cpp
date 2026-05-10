#include "hardware/status_led.h"

#include <Adafruit_NeoPixel.h>

#include "hardware/pins.h"

static Adafruit_NeoPixel g_strip(1, pins::kStatusLed, NEO_GRB + NEO_KHZ800);

void statusLedSetup()
{
    g_strip.begin();
    g_strip.setBrightness(32);
    g_strip.show();
}

void statusLedSetRgb(uint8_t r, uint8_t g, uint8_t b)
{
    g_strip.setPixelColor(0, g_strip.Color(r, g, b));
    g_strip.show();
}
