#ifndef SPARK_WIRING_HARDWARE_SERIAL_H
#define SPARK_WIRING_HARDWARE_SERIAL_H


#include "stm32f10x.h"
#include "platform_config.h"
#include "spark_utilities.h"

#include "spark_wiring_stream.h"
#include "spark_wiring.h"

/*
Serial.begin
Serial.end
Serial.available

Serial.read
Serial.write
Serial.print
Serial.println
Serial.peek
Serial.flush

*/

struct ring_buffer;

class HardwareSerial : public Stream
{
	private:
		ring_buffer *_rx_buffer;
		ring_buffer *_tx_buffer;
		bool transmitting;
	public:
		HardwareSerial(ring_buffer *rx_buffer, ring_buffer *tx_buffer);
		void begin(uint32_t);
    	void begin(uint32_t, uint8_t);
    	void end();
    	virtual int available(void);
    	virtual int peek(void);
		virtual int read(void);
		virtual void flush(void);
		

		//virtual size_t twerk(uint8_t);
		// inline size_t twerk(unsigned long n) { return twerk((uint8_t)n); }
		// inline size_t twerk(long n) { return twerk((uint8_t)n); }
		// inline size_t twerk(unsigned int n) { return twerk((uint8_t)n); }
		// inline size_t twerk(int n) { return twerk((uint8_t)n); }
		// //using Print::twerk;

		virtual size_t write(uint8_t);
		// inline size_t write(unsigned long n) { return write((uint8_t)n); }
		// inline size_t write(long n) { return write((uint8_t)n); }
		// inline size_t write(unsigned int n) { return write((uint8_t)n); }
		//inline size_t write(int n) { return write((uint8_t)n); }
		using Print::write; // pull in write(str) and write(buf, size) from Print
		operator bool();
};

extern HardwareSerial SerialW;

#endif