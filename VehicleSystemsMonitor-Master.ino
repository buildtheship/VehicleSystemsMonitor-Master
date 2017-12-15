#include <Arduino.h>
#include <OneWire.h>
#include <SPI.h>
#include "epd2in7b.h"
#include "imagedata.h"
#include "epdpaint.h"

#define COLORED     1
#define UNCOLORED   0


OneWire net(PD6);
byte addr_DS2431[8];
bool DS2431_ok = false;
Epd epd;
String str("ECE 471");

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

	//Epd epd;

	/* check if epaper device is found */
	if (epd.Init() != 0) {
		Serial.print("e-Paper init failed");
		return;
	}

	/* This clears the SRAM of the e-paper display */
	epd.ClearFrame();

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
	uint32_t freq_reading = 0;  // frequency read from one-wire slave
	float freq_float = 0.0;
	unsigned char image[1024];    // image buffer
	Paint paint(image, 176, 24);    //width should be the multiple of 8 



	ReadAndReportDS2431(&net, addr_DS2431);

	freq_reading = (uint32_t)(ReadDS2431(&net, addr_DS2431, 0));
	freq_float = ((float)freq_reading) / 100; // cast to float and adjust fixed-point
	Serial.print("Read: ");
	Serial.println(freq_float);
	str = String(freq_float);
	str = String(str + " Hz");
	Serial.println(str);

	float bus_speed = freq_float*(52.0 / 14.2);
	String str_speed = String(bus_speed);
	str_speed = String(str_speed + "mi/hr");

	paint.Clear(UNCOLORED);
	paint.DrawStringAt(0, 0, str.c_str(), &Font20, COLORED);
	epd.TransmitPartialBlack(paint.GetImage(), 16, 32, paint.GetWidth(), paint.GetHeight());

	paint.Clear(COLORED);
	paint.DrawStringAt(2, 2, str_speed.c_str(), &Font20, UNCOLORED);
	epd.TransmitPartialRed(paint.GetImage(), 0, 64, paint.GetWidth(), paint.GetHeight());

	/* This displays the data from the SRAM in e-Paper module */
	epd.DisplayFrame();

	/* This clears the SRAM of the e-paper display */
	epd.ClearFrame();

	/* Deep sleep */
	//epd.Sleep();

	//ReadDS2431(&net, addr_DS2431, uint16_t wordAddr)
	delay(5000); // delay in ms

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

	int a = net->read(); // read occurs for succesive bytes for each call (EEPROM)
	int b = net->read();
	int c = net->read();
	int d = net->read();
	Serial.println(a, HEX);
	uint32_t toReturn = 0;
	toReturn = a | (b << 8) | (d << 16) | (c << 24); // big-endian
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