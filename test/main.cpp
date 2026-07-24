// DS18B20 "fake finder" bench tool, adapted from
// https://github.com/electrical-pro/Ds_Fake_Tester/blob/master/Ds_FakeFinder.ino
// for MobiFan's actual hardware:
//   - OneWire bus on GPIO4 (PIN_ONEWIRE in src/config.h)
//   - 72x40 SSD1306 OLED (I2C SDA=5/SCL=6) via U8g2, NOT Adafruit_SSD1306 --
//     bench-tested and confirmed the Adafruit driver renders this exact
//     panel garbled (wrong memory map); U8g2's SSD1306_72X40_ER is the
//     known-good driver for this glass, see /memories/repo/mobifan-hw-debug.md.
// Writes/reads alarm registers and cycles resolution to help distinguish a
// genuine DS18B20 (supports EEPROM copy + alarm regs) from common fakes
// that don't implement the full command set correctly.
#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>

// ---------- Pins (match src/config.h) ----------
constexpr int PIN_ONEWIRE  = 4; // PIN_ONEWIRE
constexpr int PIN_OLED_SDA = 5; // PIN_OLED_SDA
constexpr int PIN_OLED_SCL = 6; // PIN_OLED_SCL

OneWire oneWire(PIN_ONEWIRE);
DallasTemperature sensors(&oneWire);
DeviceAddress tempDeviceAddress;

static U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE,
                                           PIN_OLED_SCL, PIN_OLED_SDA);

uint8_t deviceCount = 0;
const uint8_t PARASITE = 0;
uint8_t highAlarmValue = 25;
uint8_t lowAlarmValue = 10;

uint32_t timeCon = 0;
uint8_t resol = 12;
uint32_t millChngRes = 0;

String setAlarms();          // was method2()
String readAlarms();         // was readDevicesMethod2()
String setResolution(uint8_t resSet);
void printAddress(DeviceAddress deviceAddress);

void setup(void) {
  Serial.begin(115200);
  delay(500);

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  u8g2.begin();
  u8g2.setBusClock(400000);
  u8g2.setContrast(255);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(2, 16, "DS18B20");
  u8g2.drawStr(2, 30, "FakeFinder");
  u8g2.sendBuffer();
  delay(1500);

  Serial.print("DallasTemperature library version: ");
  Serial.println(DALLASTEMPLIBVERSION);
}

void loop(void) {
  if (resol == 13) resol = 9;

  sensors.begin();
  deviceCount = sensors.getDeviceCount();

  Serial.println("");
  String resMsg = setResolution(resol);

  delay(50);
  uint32_t startMillis = millis();
  sensors.requestTemperatures();
  timeCon = millis() - startMillis;
  Serial.println("****************************************");
  Serial.print("TimeCon:");
  Serial.print(timeCon);
  Serial.println(" ms");
  Serial.println("****************************************");
  delay(50);

  String setMsg = setAlarms();
  String gotMsg = readAlarms();
  float tempC = sensors.getTempCByIndex(0);
  delay(50);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  char line[24];
  snprintf(line, sizeof(line), "N:%u R:%s", deviceCount, resMsg.c_str());
  u8g2.drawStr(0, 7, line);
  snprintf(line, sizeof(line), "SA:%s", setMsg.c_str());
  u8g2.drawStr(0, 15, line);
  snprintf(line, sizeof(line), "GA:%s", gotMsg.c_str());
  u8g2.drawStr(0, 23, line);
  snprintf(line, sizeof(line), "T:%.1fC", tempC);
  u8g2.drawStr(0, 31, line);
  snprintf(line, sizeof(line), "Cv:%ums", (unsigned)timeCon);
  u8g2.drawStr(0, 39, line);
  u8g2.sendBuffer();

  Serial.println("");
  delay(50);

  // change resolution every 5 sec, starting after 12 sec
  if (millis() > millChngRes + 5000 && millis() > 12000) {
    resol++;
    millChngRes = millis();
  }
}

// ================================================
String setAlarms() { // using direct OneWire write commands
  String msg;
  Serial.println("========================================");
  Serial.print("Setting New Hi/Lo alarm values: ");
  highAlarmValue++;
  lowAlarmValue++;
  Serial.print(highAlarmValue);
  Serial.print("/");
  Serial.println(lowAlarmValue);

  Serial.println("Writing to devices");
  oneWire.reset_search();
  uint8_t addr[8];
  while (oneWire.search(addr)) {
    if (OneWire::crc8(addr, 7) == addr[7]) {
      Serial.print(".");
      oneWire.reset();
      oneWire.select(addr);
      oneWire.write(0x4E);           // Write to scratchpad
      oneWire.write(highAlarmValue); // Write high alarm value
      oneWire.write(lowAlarmValue);  // Write low alarm value
      oneWire.write(0x7F);           // Write configuration register, 12 bit temp res
      delay(30);                     // dallas temp lib doesn't delay

      oneWire.reset();
      oneWire.select(addr);
      oneWire.write(0x48, PARASITE); // copy scratchpad to eeprom
      delay(20);                     // need at least 10ms eeprom write delay
      if (PARASITE) delay(10);
      msg = String(highAlarmValue) + "/" + String(lowAlarmValue);
    } else {
      Serial.println("Bad device addr!");
      msg = "Err";
    }
  }
  Serial.println("Done");
  Serial.println("----------------------------------------");
  return msg;
}

// ================================================
String readAlarms() { // using direct OneWire write commands
  Serial.println("Reading...");
  uint8_t hAlarmValue = 0;
  uint8_t lAlarmValue = 0;
  uint8_t addr[8];
  uint8_t data[9];
  String msg;
  oneWire.reset_search();
  while (oneWire.search(addr)) {
    printAddress(addr);
    if (OneWire::crc8(addr, 7) == addr[7]) {
      oneWire.reset();
      oneWire.select(addr);
      oneWire.write(0xB8); // Copy eeprom to scratchpad cmd
      delay(50);
      oneWire.reset();
      oneWire.select(addr);
      oneWire.write(0xBE); // Read scratchpad cmd
      delay(50);
      for (uint8_t i = 0; i < 9; i++) data[i] = oneWire.read();
      hAlarmValue = data[2]; // byte 2 is high temp alarm
      lAlarmValue = data[3]; // byte 3 is low temp alarm
      Serial.print("Hi/Lo now is: ");
      Serial.print(hAlarmValue);
      Serial.print("/");
      Serial.println(lAlarmValue);
      msg = String(hAlarmValue) + "/" + String(lAlarmValue);
    } else {
      Serial.println("Bad device addr!");
      msg = "Err";
    }
  }
  Serial.println("========================================");
  return msg;
}

// ================================================
// Loop through each device, print out address
String setResolution(uint8_t resSet) {
  String msg;
  for (int i = 0; i < deviceCount; i++) {
    if (sensors.getAddress(tempDeviceAddress, i)) {
      Serial.println("");
      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(tempDeviceAddress);
      Serial.println();

      Serial.print("Setting resolution to ");
      Serial.println(resSet, DEC);
      sensors.setResolution(tempDeviceAddress, resSet);

      Serial.print("Resolution actually set to: ");
      Serial.print(sensors.getResolution(tempDeviceAddress), DEC);
      msg = String(resSet, DEC) + "/" + String(sensors.getResolution(tempDeviceAddress), DEC);
      Serial.println();
    } else {
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
      msg = "Err";
    }
  }
  return msg;
}

// ================================================
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0"); // zero pad the address
    Serial.print(deviceAddress[i], HEX);
    if (i < 7) Serial.print(":");
  }
  Serial.println(" | " + String(sensors.getTempC(deviceAddress)) + " C");
}
