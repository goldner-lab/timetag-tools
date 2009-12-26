#ifndef _TIMETAGGER_H
#define _TIMETAGGER_H

#include <libusb.h>

class timetagger {
public:
	struct data_cb_t {
		virtual void operator()(const uint8_t* buffer, size_t length)=0;
	};

private:
	libusb_device_handle* dev;
	libusb_transfer* transfer;
	uint8_t* data_buffer;
	
	void send_simple_command(uint8_t mask, uint8_t* data, size_t length);

public:
	data_cb_t& data_cb;

	timetagger(libusb_device_handle* dev, data_cb_t& data_cb) :
		dev(dev),
		data_cb(data_cb)
	{
		libusb_claim_interface(dev, 0);
	}

	~timetagger()
	{
		stop_readout();
		libusb_release_interface(dev, 0);
	}
	
	void start_readout();
	void stop_readout();

	void get_status();

	void start_capture();
	void stop_capture();
	void reset_counter();

	void pulseseq_start();
	void pulseseq_stop();
	void pulseseq_set_low_count(char seq_mask, uint32_t count);
	void pulseseq_set_high_count(char seq_mask, uint32_t count);
	void pulseseq_set_initial_count(char seq_mask, uint32_t count);
	void pulseseq_set_initial_state(char seq_mask, bool state);
};

#endif
