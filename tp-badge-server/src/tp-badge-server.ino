#include <sstream>

//Just doing a ghetto meshnet for now
#include <RHReliableDatagram.h>
#include <RH_RF69.h>
#include "SdFat.h"

#define RADIO_ADDRESS 1
#define IMAGE_SIZE 61496

//RH_RF69 radio;
//cs, interrupt
RH_RF69 radio(10, 2);
RHReliableDatagram manager(radio, RADIO_ADDRESS);

SdFatSdio sd;

void setup() {
  Serial.begin(115200);

  if (!manager.init()) {
    Serial.println("radio failed");
    return;
  }
  radio.setTxPower(14, true);
  radio.setFrequency(915.0);
  radio.setModemConfig(RH_RF69::GFSK_Rb55555Fd50);
  manager.setRetries(100);

  if(sd.begin()) {
    loadData();
  }
}

String filename;
unsigned long img_num;
bool sd_last;
byte image[IMAGE_SIZE];

void loop() {
  if(manager.waitAvailableTimeout(1000)) {
    recv();
  }

  if(sd.begin()) {
    if (!sd_last) {
      Serial.println("SD card detected");
      sd_last = true;
    } 

    SdFile file;
    char newFile[20];

    while (file.openNext(sd.vwd(), O_READ)) {
      if (!file.isDir()) {
        file.getName(newFile, 20);
      }
      file.close();
    }
    if(filename != newFile) {
      loadFile(newFile);
    }
  } else {
    if (sd_last) {
      Serial.println("SD card removed");
      sd_last = false;
    }
  }

  sendBeacon();
  delay(1000);
}

void sendImage(uint8_t src) {
  Serial.print("INIT SEND TO ");
  Serial.print(src);

  char ts[13] = "";
  sprintf(ts, "IMG%010lu", img_num);
  manager.sendtoWait((uint8_t *)ts, sizeof(ts) + 1, src); //1b more to account for NUL terminator

  uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  for(long i=0; i<1043; i++) {
    for(int x=0; x<59; x++){
      Serial.print("Sending ");
      Serial.print((i * 59) + x);
      uint8_t a = image[(i*59)+x];
      Serial.print(" ");
      Serial.println(a);
      buf[x] = a;
    }
    manager.sendtoWait(buf, len, src);
    Serial.print(".");
  }
  Serial.print("COMPLETED SEND TO ");
  Serial.println(src);
}

void recv()
{
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
    Serial.println(radio.lastRssi(), DEC);

    if (op == "GET") {
      sendImage(src);
    }
  }
  else
  {
    Serial.println("Recv failed");
  }
}

void loadFile(char newFile[20]) {
  SdFile file;
  filename = newFile;
  img_num++;
  file.open(newFile, O_READ);
  Serial.print("Loading ");
  Serial.println(newFile);
  for (long i=0; i<IMAGE_SIZE; i++) {
    image[i] = file.read();
  }
  file.close();
  saveData();
}

void sendBeacon() {
  char ts[13] = "";
  sprintf(ts, "BCN%010lu", img_num);
  manager.sendtoWait((uint8_t *)ts, sizeof(ts) + 1, RH_BROADCAST_ADDRESS); //1b more to account for NUL terminator
}

void saveData() {
  File num = sd.open("0", FILE_WRITE);
  num.truncate(0);
  num.print(String(img_num));
  num.sync();
  num.close();
  Serial.println("Config saved");
}

void loadData() {
  File num = sd.open("0", FILE_READ);
  img_num = num.readString().toInt();
  num.close();
  Serial.println("Config loaded");
}
