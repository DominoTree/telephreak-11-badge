#include <EEPROM.h>
#include <SPI.h>
#include <SPIFlash.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>
#include <gfxfont.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_ST77xx.h>

//Just doing a ghetto meshnet for now
#include <RHReliableDatagram.h>
#include <RH_RF69.h>

#define RADIO_INT 2 
#define RADIO_CS 6 // Chip select line for radio module
#define TFT_CS 10  // Chip select line for TFT display
#define TFT_RST 9  // Reset line for TFT (or see below...)
#define TFT_DC 8   // Data/command line for TFT
#define SPI_CS 7   // Chip select for SPI flash

#define SERIAL_BAUD 115200
#define RADIO_ADDRESS 2

#define MAX_MESSAGE_SIZE 60
#define BMP_DATA_OFFSET 54
#define IMAGE_SIZE 61496

#define BEACON_DELAY 10 // seconds

//125000 bytes flash storage
//61496 bytes per image

//122992 bytes for two images
//2008 bytes remaining

//60b max datagram size (61b nul-terminated)
//1025 datagrams per image

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

RH_RF69 radio(RADIO_CS, RADIO_INT);
RHReliableDatagram manager(radio, RADIO_ADDRESS);
//send datagram to RH_BROADCAST_ADDRESS for beacons (but these won't be guaranteed delivery obviously)

SPIFlash flash(SPI_CS);
uint32_t flash_offset;

unsigned int device_id;
unsigned long image_id;

int readByte()
{
  uint8_t b;
  b = flash.readByte(flash_offset);
  flash_offset++;
  return b;
}

void writeByte(uint8_t byte)
{
  flash.writeByte(flash_offset, byte);
  flash_offset++;
}

void sendBeacon() {
  char ts[13] = "";
  sprintf(ts, "BCN%010lu", image_id);
  Serial.print("Beaconing: ");
  Serial.println(ts);
  manager.sendtoWait((uint8_t *)ts, sizeof(ts) + 1, RH_BROADCAST_ADDRESS); //1b more to account for NUL terminator
}

void setup()
{
  Serial.begin(SERIAL_BAUD);
  Serial.println("I'm very hungry!");

  Serial.println("Give me the PACKET RADIO!");
  if (!manager.init()) {
    Serial.println("NO! I don't want that!");
  }
  radio.setTxPower(14, true);
  radio.setFrequency(915.0);
  radio.setModemConfig(RH_RF69::GFSK_Rb55555Fd50);

  Serial.println("Give me the DISPLAY!");
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.setAddrWindow(0, 0, 127, 159);

  Serial.println("Give me the SPI FLASH!");
  if (!flash.initialize()){
    Serial.println("NO! I don't want that!");
  }

  loadData();

  bmpDraw();
  Serial.println("Give me the PRIMARY LOOP!");
}

void clearTemp() {
  for (long i = 0; i<15; i++) {
    Serial.print("CLEARING TEMP PAGE ");
    Serial.println(i);
    flash.blockErase4K(IMAGE_SIZE+(i * 4096));
    while(flash.busy());
  }
}

void clearMain() {
  for (long i = 0; i<15; i++) {
    Serial.print("CLEARING MAIN PAGE ");
    Serial.println(i);
    flash.blockErase4K(i * 4096);
    while(flash.busy());
  }
}

void loadData() {
  randomSeed(analogRead(0));
  device_id = random(2,255);
  manager.setHeaderFrom(device_id);
  manager.setThisAddress(device_id);

  EEPROM.get(1, image_id);

  if (image_id >= 255) {
    image_id = 0;
  }

  Serial.print("Device ID ");
  Serial.print(device_id);
  Serial.print(" Image ID ");
  Serial.println(image_id);
  saveData();
}

void saveData() {
  EEPROM.put(0, device_id);
  EEPROM.put(1, image_id);
}

void loop()
{
  bool ok = true;
  if (manager.waitAvailableTimeout(1000))
  {
    ok = recv();
  }
  if(ok) {
    sendBeacon();
  }
}

void getImage(uint8_t src) {
  uint8_t get[] = "GET";
  Serial.print("GET -> ");
  Serial.println(src);
  manager.sendtoWait(get, sizeof(get), src); //1b more to account for NUL terminator
}

void sendImage(uint8_t src) {
  Serial.print("INIT SEND TO ");
  Serial.println(src);
  Serial.print("Sending");

  char ts[13] = "";
  sprintf(ts, "IMG%010lu", image_id);
  manager.sendtoWait((uint8_t *)ts, sizeof(ts) + 1, src); //1b more to account for NUL terminator

  uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  for(long i=0; i<1043; i++) {
    for(int x=0; x<59; x++){
      uint8_t a = flash.readByte((i*59)+x);
      buf[x] = a;
    }
    manager.sendtoWait(buf, len, src);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("COMPLETED SEND TO ");
  Serial.println(src);
}

void recvImage(unsigned long ts, uint8_t from) {
  uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  uint8_t src;
  uint8_t dst;
  uint8_t id;
  uint8_t flags;

  long n;
  
  Serial.print("Receiving");

  clearTemp();
  Serial.println(flash.readStatus());
  flash_offset = IMAGE_SIZE;
  Serial.print("Receive");
  while (true) {
    manager.waitAvailableTimeout(1000);
    if(manager.recvfromAck(buf, &len, &src, &dst, &id, &flags)){
      if (src == from) {
        Serial.print(".");
        long off = (59 * n) + IMAGE_SIZE;
        for (int i = 0; i < 59; i++){
          flash.writeByte(off + i, buf[i]);
        }
        if(n==1043) {
          break;
        }
        n++;
      }
    }
  }
  Serial.println();

  //copy data to permanent spot, save token, redraw
  image_id = ts;
  clearMain();
  Serial.print("Copy");
  for(long x=0;x<IMAGE_SIZE;x++) {
    Serial.print(".");
    uint8_t b = flash.readByte(x + IMAGE_SIZE);
    flash.writeByte(x, b);
    while(flash.busy());
  }
  Serial.println();
  saveData();
  bmpDraw();
}

bool recv()
{
  bool ok = false;

  uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  uint8_t src;
  uint8_t dst;
  uint8_t id;
  uint8_t flags;

  if (manager.recvfromAck(buf, &len, &src, &dst, &id, &flags))
  {
    String op;

    char* str;
    str = (char *)buf;

    for (int i=0; i<3; i++) {
      op += str[i];
    }

    Serial.print(op);
    Serial.print(" FROM ");
    Serial.print(src);
    Serial.print(", RSSI: ");
    Serial.print(radio.lastRssi(), DEC);

    if (op == "GET") {
      ok = true;
      sendImage(src);
    }

    if (op == "IMG") {
      ok = true;
      String ts_str;
      for (int i=3; i<len; i++) {
        ts_str += str[i];
      }
      unsigned long timestamp = ts_str.toInt();
      recvImage(timestamp, src);
    }

    if (op == "BCN") {
      ok = true;
      String ts_str;
      for (int i=3; i<len; i++) {
        ts_str += str[i];
      }

      unsigned long timestamp = ts_str.toInt();
      Serial.print(", VAL: ");
      Serial.print(timestamp);
      if(timestamp > image_id) {
        getImage(src);
      }
      delay(1000);
    }
    Serial.println();
  }
  else
  {
    Serial.println("recv failed");
  }
  return ok;
}

void bmpDraw()
{
  flash_offset = BMP_DATA_OFFSET;
  uint32_t rowSize; // Not always = bmpWidth; may have padding
  int w, h, row, col;
  uint8_t r, g, b;
  uint32_t pos = 0; 

  //ALL SAFETY CHECKS ARE OFF
  //THIS SHOULD BE INTERESTING

  // BMP rows are padded (if needed) to 4-byte boundary
  rowSize = (128 * 3 + 3) & ~3;

  for (row = 0; row < 160; row++)
  { // For each scanline...
    pos = BMP_DATA_OFFSET + (160 - 1 - row) * rowSize;

    if (flash_offset != pos)
    { // Need seek?
      flash_offset = pos;
    }

    //Buffering the data here will improve performance
    for (col = 0; col < 128; col++)
    { // For each pixel...
      b = readByte();
      g = readByte();
      r = readByte();
      tft.pushColor(tft.color565(r, g, b));
    } // end pixel
  }   // end scanline
}
