/* timetag-tools - Tools for UMass FPGA timetagger
 *
 * Copyright © 2010 Ben Gamari
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/ .
 *
 * Author: Ben Gamari <bgamari@physics.umass.edu>
 */


#include <cstdio>
#include <cstring>
#include "timetagger.h"
#include "record_format.h"

#define CMD_ENDP	0x02
#define DATA_ENDP	0x86
#define REPLY_ENDP	0x88

#define TIMEOUT 100
#define DATA_TIMEOUT 5000


#define REQ_TYPE_TO_DEV		(0x0 << 0)
#define REQ_TYPE_TO_IFACE	(0x1 << 0)
#define REQ_TYPE_TO_ENDP	(0x2 << 0)

#define REQ_TYPE_VENDOR 	(0x2 << 5)

#define REQ_TYPE_HOST2DEV 	(0x0 << 7)
#define REQ_TYPE_DEV2HOST 	(0x1 << 7)


void timetagger::reset()
{
	stop_capture();
	flush_fx2_fifo();
	needs_flush = true;
}

void timetagger::send_simple_command(uint8_t mask, cmd_data data)
{
	std::vector<uint8_t> buffer = {
		0xAA,			// Magic number
		(uint8_t) data.size(),	// Command length
		mask			// Command mask
	};
	for (unsigned int i=0; i<data.size(); i++)
		buffer.push_back(data[i]);

	int ret, transferred;
	if ( (ret = libusb_bulk_transfer(dev, CMD_ENDP, &buffer[0], buffer.size(), &transferred, TIMEOUT)) )
		fprintf(stderr, "Failed sending request: %d\n", ret);

#if 0
	if (ret = libusb_bulk_transfer(dev, REPLY_ENDP, buffer, 128, &transferred, TIMEOUT) )
		fprintf(stderr, "failed receiving reply: %d\n", ret);

	if (transferred > 4)
		fprintf(stderr, "Length: %d\n", ((int*) buffer)[0]);
	else
		fprintf(stderr, "No response\n");
#endif
}

void timetagger::start_capture()
{
	// Don't start capture until flush has finished
	while (needs_flush)
		sleep(1);
	cmd_data data = { 0x1 };
	send_simple_command(0x01, data);
}

void timetagger::stop_capture()
{
	cmd_data data = { 0x2 };
	send_simple_command(0x01, data);
}

void timetagger::reset_counter()
{
	cmd_data data = { 0x4 };
	send_simple_command(0x01, data);
}

void timetagger::set_send_window(unsigned int records)
{
	unsigned int bytes = RECORD_LENGTH*records;
	if (bytes > 512) {
		fprintf(stderr, "Error: Send window too large\n");
		return;
	}

	int res = libusb_control_transfer(dev,
		REQ_TYPE_TO_DEV | REQ_TYPE_VENDOR | REQ_TYPE_HOST2DEV,
		0x01, bytes, 0, NULL, 0, 1);
	if (res != 0)
		fprintf(stderr, "Error setting send window size: %d\n", res);

	send_window = records;
}

void timetagger::flush_fx2_fifo() {
	// Request FIFO flush
	int res = libusb_control_transfer(dev, REQ_TYPE_VENDOR, 0x02, 0, 0, NULL, 0, 0);
	if (res != 0)
		fprintf(stderr, "Error requesting FX2 FIFO flush: %d\n", res);
}

void timetagger::readout_handler::do_flush() {
	int transferred = 0;
	do {
		uint8_t buffer[510];
		usleep(5000); // Give plenty of time for FPGA to fill FIFOs
		libusb_bulk_transfer(dev, DATA_ENDP, buffer, 510, &transferred, 10);
		//fprintf(stderr, "Flushed %d bytes\n", transferred);
	} while (transferred > 0);
	needs_flush = false;
}

void timetagger::readout_handler::operator()()
{
	uint8_t* buffer = new uint8_t[510];

	do {
		if (needs_flush)
			do_flush();

		int transferred, res;
		res = libusb_bulk_transfer(dev, DATA_ENDP,
			buffer, 510,
			&transferred, data_timeout);
		//fprintf(stderr, "Read %d bytes\n", transferred);
		if (!res || res == LIBUSB_ERROR_OVERFLOW) {
			if (transferred % RECORD_LENGTH != 0)
				fprintf(stderr, "Warning: Received partial record.");
			cb(buffer, transferred);
		} else if (res == LIBUSB_ERROR_TIMEOUT) {
			// Ignore timeouts, we just need to fall through to the
			// needs_flush check every once in a while
		} else
			fprintf(stderr, "Transfer failed: %d\n", res);

		try {
			boost::this_thread::interruption_point();
		} catch (...) {
			break;
		}
	} while (true);
	delete [] buffer;
}

void timetagger::start_readout() 
{
	assert(readout_thread == 0);
	readout_handler readout(dev, data_cb, needs_flush, send_window);
	readout_thread = boost::shared_ptr<boost::thread>(new boost::thread(readout));
}

void timetagger::stop_readout() {
	assert(readout_thread != 0);
	readout_thread->interrupt();
	readout_thread->join();
}

