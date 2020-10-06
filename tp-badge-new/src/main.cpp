#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_ST77xx.h>

#define TFT_CS 10 // Chip select line for TFT display
#define TFT_RST 9 // Reset line for TFT (or see below...)
#define TFT_DC 8  // Data/command line for TFT

#define WIDTH 128
#define HEIGHT 160

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

void rainbow(int count)
{
    for (int x = 0; x < count; x++)
    {
        tft.setRotation(random(0, 4));
        tft.fillScreen(random(0, 65535));
    }
}

void wiper(int count)
{
    for (int x = 0; x < count; x++)
    {
        tft.setRotation(random(0, 4));
        if(random(0,2) == 1) {
            tft.fillScreen(ST77XX_WHITE);
        } else {
            tft.fillScreen(ST77XX_BLACK);
        }
    }
}

void text(int count)
{
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(0, 0);
    for (int i = 0; i < count; i++)
    {
        tft.setTextColor(random(0, 65535));
        tft.print("TELEPHREAK");
    }
}

void randomize()
{
    tft.fillScreen(ST77XX_BLACK);
    //this is hacky but the display will just drop pixels it doesn't have lol
    for (int x = 0; x < HEIGHT; x++)
    {
        for (int y = 0; y < HEIGHT; y++)
        {
            tft.startWrite();
            tft.writePixel(x, y, random(0, 65535));
            tft.endWrite();
        }
    }
}

void rule30()
{
    uint16_t color = random(0, 65536);
    int prev_row[WIDTH];
    tft.setRotation(0);
    tft.fillScreen(ST77XX_BLACK);
    //initial seed
    for (int x = 0; x < WIDTH; x++)
    {
        tft.startWrite();
        if (random(0, 2) == 0)
        {
            prev_row[x] = 0;
            tft.writePixel(x, 0, ST77XX_BLACK);
        }
        else
        {
            prev_row[x] = 1;
            tft.writePixel(x, 0, color);
        }
        tft.endWrite();
    }

    //100 011 010 001
    for (int y = 1; y < HEIGHT; y++)
    {
        int a, b, c;
        int this_row[WIDTH];

        for (int x = 0; x < WIDTH; x++)
        {
            a = x - 1;
            b = x;
            c = x + 1;
            if (a == -1)
            {
                a = 127;
            }
            if (c == WIDTH)
            {
                c = 0;
            }

            tft.startWrite();
            if (
                (prev_row[a] == 1 && prev_row[b] == 0 && prev_row[c] == 0) ||
                (prev_row[a] == 0 && prev_row[b] == 1 && prev_row[c] == 1) ||
                (prev_row[a] == 0 && prev_row[b] == 1 && prev_row[c] == 0) ||
                (prev_row[a] == 0 && prev_row[b] == 0 && prev_row[c] == 1))
            {
                this_row[x] = 1;
                tft.writePixel(x, y, color);
            } else {
                this_row[x] = 0;
                tft.writePixel(x, y, ST77XX_BLACK);
            }

            tft.endWrite();
        }

        for (int i=0; i<WIDTH; i++) {
            prev_row[i] = this_row[i];
        }
    }
}

void setup()
{
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(0);
    tft.setAddrWindow(0, 0, 127, 159);
}

void flash(int count)
{
    for (int i = 0; i < count; i++)
    {
        tft.invertDisplay(true);
        delay(300);
        tft.invertDisplay(false);
        delay(300);
    }
}

void maybeInvert() {
    if(random(0,2) == 1) {
        tft.invertDisplay(true);
    } else {
        tft.invertDisplay(false);
    }
}

void loop()
{
    randomSeed(analogRead(0));
    maybeInvert();
    rule30();
    delay(2000);
    maybeInvert();
    tft.setRotation(random(0, 4));
    text(1000);
    flash(3);
    delay(200);
    tft.setRotation(random(0, 4));
    randomize();
    delay(200);
    rainbow(35);
    wiper(35);
}
