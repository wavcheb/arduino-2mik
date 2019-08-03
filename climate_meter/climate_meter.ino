/* 
 * Temperature, Humidity and Pressure Meter 
 * 
 * Consists of
 * Arduino Nano,
 * BMP280   - 1 unit,
 * DHT11/22 - 1 unit,
 * DS18B20  - 3 units,
 * 
 * Supports Modbus RTU.
 * Holding Registers (4X), function 03H.
 * Register address 1 corresponds to the physical address 0.
 * 
 * Register  Type   Description
 * 01-02     Float  Pressure (BMP280)
 * 03-04     Float  Temperature (BMP280)
 * 05-06     Float  Humidity (DHT22)
 * 06-08     Float  Temperature (DHT22)
 * 07-10     Float  Temerature 0 (DS18B20)
 * 08-12     Float  Temerature 1 (DS18B20)
 * 11-14     Float  Temerature 2 (DS18B20)
 * 
 * (c) 2017-2019, Mikhail Shiryaev
 */

#include <Adafruit_BMP280.h>   // https://github.com/adafruit/Adafruit_BMP280_Library
#include <DHT.h>               // https://github.com/adafruit/DHT-sensor-library
#include <OneWire.h>           // http://www.pjrc.com/teensy/td_libs_OneWire.html
#include <DallasTemperature.h> // https://github.com/milesburton/Arduino-Temperature-Control-Library

// All the pins below are digital
#define DHT_PIN       2 // DHT22 data
#define ONE_WIRE_PIN0 5 // the 1st DS18B20 data
#define ONE_WIRE_PIN1 6 // the 2nd DS18B20 data
#define ONE_WIRE_PIN2 7 // the 3rd DS18B20 data

#define BMP_ADDR        0x76   // BMP280 I2C address
#define DHT_TYPE        DHT22  // DHT11 or DHT22
#define ONE_WIRE_CNT    3      // count of 1-wire connections
#define EMPTY_VAL       -100.0 // empty float value of measurement
#define MODBUS_BUF_LEN  33     // length of Modbus RTU output buffer

//#define DEBUG                // write detailed log to Serial port and disable Modbus

#ifdef DEBUG
 #define DEBUG_PRINT(val)       Serial.print(val)
 #define DEBUG_PRINTHEX(val)    Serial.print(val, HEX)
 #define DEBUG_PRINTLN(val)     Serial.println(val)
 #define DEBUG_WRITETIME()      writeTime()
#else
 #define DEBUG_PRINT(val)
 #define DEBUG_PRINTHEX(val)
 #define DEBUG_PRINTLN(val) 
 #define DEBUG_WRITETIME()
#endif

// BMP280 by I2C
Adafruit_BMP280 bmp;
bool bmpFound = false;
float bmpP = EMPTY_VAL;
float bmpT = EMPTY_VAL;

// DHT instance
DHT dht(DHT_PIN, DHT_TYPE);
float dhtH = EMPTY_VAL;
float dhtT = EMPTY_VAL;

// 1-wire and DS instances
OneWire oneWire0(ONE_WIRE_PIN0);
OneWire oneWire1(ONE_WIRE_PIN1);
OneWire oneWire2(ONE_WIRE_PIN2);
OneWire oneWires[] = {oneWire0, oneWire1, oneWire2};
DallasTemperature dsSensors[ONE_WIRE_CNT];
float dsT[ONE_WIRE_CNT] = { EMPTY_VAL, EMPTY_VAL, EMPTY_VAL };

// Modbus
// Table of CRC values for high–order byte
const byte crcHiTable[] = {
  0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
  0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
  0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
  0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
  0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81,
  0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
  0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
  0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
  0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
  0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
  0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
  0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
  0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
  0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
  0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
  0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
  0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
  0x40
};

// Table of CRC values for low–order byte
const byte crcLoTable[] = {
  0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4,
  0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
  0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD,
  0x1D, 0x1C, 0xDC, 0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
  0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7,
  0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
  0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE,
  0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
  0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2,
  0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
  0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB,
  0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
  0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0, 0x50, 0x90, 0x91,
  0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
  0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88,
  0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
  0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80,
  0x40
};

// Modbus RTU output buffer
byte modbusBuf[MODBUS_BUF_LEN] = { 
  0x00,       // Slave address
  0x03,       // Function code
  0x1C,       // Byte count (28)
  0x00, 0x00, 0x00, 0x00, // Register values
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00  // CRC
};

// Speed control
#ifdef DEBUG
unsigned long time;
#endif

// Initialize BMP280 sensor
void initBMP() {
  if (bmp.begin(BMP_ADDR)) {
    DEBUG_PRINTLN("BMP280 sensor initialized");
    bmpFound = true;
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                    Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                    Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                    Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
  } else {
    DEBUG_PRINTLN("BMP280 sensor not found");
  }  
}

// Initialize DHT sensor
void initDHT() {
  dht.begin();
  DEBUG_PRINTLN("DHT sensor initialized");
}

// Initialize DS sensors
void initDS() {
  DeviceAddress deviceAddress;
  for (int i = 0; i < ONE_WIRE_CNT; i++) {
    dsSensors[i].setOneWire(&oneWires[i]);
    dsSensors[i].begin();
    if (dsSensors[i].getAddress(deviceAddress, 0)) 
      dsSensors[i].setResolution(deviceAddress, 12);
  }
  DEBUG_PRINTLN("DS sensors initialized");
}

#ifdef DEBUG
void writeTime() {
  Serial.print("Time elapsed = ");
  Serial.println(millis() - time);
}
#endif

// Read data from BMP280 sensor
void readBMP() {
  DEBUG_WRITETIME();
  DEBUG_PRINTLN("Reading from BMP280 sensor");

  if (bmpFound) {
    bmpP = bmp.readPressure();    // Pa
    bmpT = bmp.readTemperature(); // *C
  }
  
  DEBUG_WRITETIME();
  DEBUG_PRINT("t = ");
  DEBUG_PRINTLN(bmpT);
  DEBUG_PRINT("p = ");
  DEBUG_PRINTLN(bmpP);
  
  DEBUG_WRITETIME();
  DEBUG_PRINTLN();
}

// Read data from DHT sensor
void readDHT() {
  DEBUG_WRITETIME();
  DEBUG_PRINTLN("Reading from DHT sensor");
  
  dhtH = dht.readHumidity();
  dhtT = dht.readTemperature(); // *C
  
  DEBUG_WRITETIME();

  if (isnan(dhtH) || isnan(dhtT)) {
    dhtH = EMPTY_VAL;
    dhtT = EMPTY_VAL;
    DEBUG_PRINTLN("Error reading from DHT sensor");
  } else {
    DEBUG_PRINT("h = ");
    DEBUG_PRINTLN(dhtH);
    DEBUG_PRINT("t = ");
    DEBUG_PRINTLN(dhtT);
  }
  
  DEBUG_WRITETIME();
  DEBUG_PRINTLN();
}

// Read data from DS sensor
void readDS(int index) {
  DEBUG_WRITETIME();
  DEBUG_PRINT("Requesting DS sensor ");
  DEBUG_PRINTLN(index);

  dsSensors[index].requestTemperatures();
  
  DEBUG_WRITETIME();
  
  dsT[index] = dsSensors[index].getTempCByIndex(0);

  DEBUG_PRINT("t = ");
  DEBUG_PRINTLN(dsT[index]);
  DEBUG_WRITETIME();
  DEBUG_PRINTLN();
}

// Process serial communication using Modbus protocol
void communicate() {
#ifndef DEBUG
  // wait for the request 00 03 00 00 00 0E C5 DF
  boolean sendResponse = false;
  while (Serial.available() > 0) {
    byte inByte = (byte)Serial.read();
    
    if (inByte == 0xDF) {
      sendResponse = true;
    }
  }
  
  // send response
  if (sendResponse) {
    memcpy(modbusBuf + 3, (byte*)&bmpP, 4);
    memcpy(modbusBuf + 7, (byte*)&bmpT, 4);
    memcpy(modbusBuf + 11, (byte*)&dhtH, 4);
    memcpy(modbusBuf + 15, (byte*)&dhtT, 4);
    memcpy(modbusBuf + 19, (byte*)&dsT, 12);
    unsigned short crc = CRC16(modbusBuf, MODBUS_BUF_LEN - 2);
    memcpy(modbusBuf + 31, (byte*)&crc, 2);
    Serial.write(modbusBuf, MODBUS_BUF_LEN);
  }
#endif
}

// The function returns the CRC as a unsigned int type (2 bytes)
unsigned int CRC16(byte* buf, unsigned int len)
{
  byte crcHi = 0xFF;
  byte crcLo = 0xFF;
  unsigned int index;
  while (len--)
  {
    index = crcLo ^ *buf++;
    crcLo = crcHi ^ crcHiTable[index];
    crcHi = crcLoTable[index];
  }
  return (crcHi << 8 | crcLo);
}

void setup() {
  Serial.begin(9600);
  DEBUG_PRINTLN();
  DEBUG_PRINTLN("SERIAL TH meter started");
  initBMP();
  initDHT();
  initDS();
  DEBUG_PRINTLN();
}

void loop() {
#ifdef DEBUG
  time = millis();  
  delay(500);
#endif

  readBMP(); // around 3 ms
  communicate();
  readDHT(); // around 50 ms
  communicate();
  readDS(0); // around 700 ms
  communicate();
  readDS(1); // around 700 ms
  communicate();
  readDS(2); // around 700 ms
  communicate();
}
