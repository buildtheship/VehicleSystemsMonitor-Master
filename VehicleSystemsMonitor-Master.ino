#include <Arduino.h>
#include <OneWire.h>

OneWire net(PD6);
byte addr_DS2431[8];
bool DS2431_ok = false;


#define LED D4 // GPIO number of connected LED, on NodeMCU the on-board led is D0

void ReadAndReportDS2431(OneWire* net, uint8_t* addr);
void WriteRow(OneWire* net, uint8_t* addr, byte row, byte* buffer);

void PrintBytes(uint8_t* addr, uint8_t count, bool newline = 0) {
  for (uint8_t i = 0; i < count; i++) {
    Serial.print(addr[i] >> 4, HEX);
    Serial.print(addr[i] & 0x0f, HEX);
  }
  if (newline)
    Serial.println();
}


void setup()
{
  Serial.begin(115200);

  Serial.println("Booting... ");

  if (!net.search(addr_DS2431)) {
    Serial.print("No more addresses.\n");
    net.reset_search();
    delay(1000);
  }
  else
  {
    if (OneWire::crc8(addr_DS2431, 7) != addr_DS2431[7]) {
      Serial.print("CRC is not valid!\n");
    }
    else
    {
      DS2431_ok = addr_DS2431[0] == 0x2D;
      if (DS2431_ok)
      {
        PrintBytes(addr_DS2431, 8);
        Serial.print(" is a DS2431.\n");
        ReadAndReportDS2431(&net, addr_DS2431);
      }
      else
      {
        PrintBytes(addr_DS2431, 8);
        Serial.print(" is an unknown device.\n");
      }
    }
  }
}


void loop() {
  // put your main code here, to run repeatedly:
  
//ReadDS2431(&net, uint8_t* addr_DS2431, uint16_t wordAddr)
  delay(500); // delay in ms
  ReadAndReportDS2431(&net, addr_DS2431);

}





////////////////////////////////

// EEPROM commands
const byte WRITE_SPAD = 0x0F;
const byte READ_SPAD = 0xAA;
const byte COPY_SPAD = 0x55;
const byte READ_MEMORY = 0xF0;

void ReadAndReportDS2431(OneWire* net, uint8_t* addr) {
  int i;
  net->reset();
  net->select(addr);
  net->write(READ_MEMORY, 1);  // Read Memory
  net->write(0x00, 1);  //Read Offset 0000h
  net->write(0x00, 1);

  for (i = 0; i < 128; i++) //whole mem is 144 
  {
    Serial.print(net->read(), HEX);
    Serial.print(" ");
  }
  Serial.println();
}

uint32_t ReadDS2431(OneWire* net, uint8_t* addr, uint16_t wordAddr) {
  int i;
  net->reset();
  net->select(addr);
  net->write(READ_MEMORY, 1);  // Read Memory
  net->write((wordAddr * 4) >> 8, 1);  //Read Offset 0000h
  net->write((byte)(wordAddr * 4), 1);

  int a = net->read();
  int b = net->read();
  int c = net->read();
  int d = net->read();

  uint32_t toReturn = 0;
  toReturn = (a << 24) | (b << 16) | (c << 8) | d;
  return toReturn;
}


void WriteReadScratchPad(OneWire* net, uint8_t* addr, byte TA1, byte TA2, byte* data)
{
  int i;
  net->reset();
  net->select(addr);
  net->write(WRITE_SPAD, 1);  // Write ScratchPad
  net->write(TA1, 1);
  net->write(TA2, 1);
  for (i = 0; i < 8; i++)
    net->write(data[i], 1);

  net->reset();
  net->select(addr);
  net->write(READ_SPAD);         // Read Scratchpad

  for (i = 0; i < 13; i++)
    data[i] = net->read();
}

void CopyScratchPad(OneWire* net, uint8_t* addr, byte* data)
{
  net->reset();
  net->select(addr);
  net->write(COPY_SPAD, 1);  // Copy ScratchPad
  net->write(data[0], 1);
  net->write(data[1], 1);  // Send TA1 TA2 and ES for copy authorization
  net->write(data[2], 1);
  delay(25); // Waiting for copy completion
         //Serial.print("Copy done!\n");
}

void WriteRow(OneWire* net, uint8_t* addr, byte row, byte* buffer)
{
  int i;
  if (row < 0 || row > 15) //There are 16 row of 8 bytes in the main memory
    return;                //The remaining are for the 64 bits register page

  WriteReadScratchPad(net, addr, row * 8, 0x00, buffer);

  /*  Print result of the ReadScratchPad
  for ( i = 0; i < 13; i++)
  {
  Serial.print(buffer[i], HEX);
  Serial.print(" ");
  }*/

  CopyScratchPad(net, addr, buffer);

}
