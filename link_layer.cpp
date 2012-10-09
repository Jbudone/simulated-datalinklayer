#include "link_layer.h"
#include "timeval_operators.h"

// TODO LIST
// * send() NOT set as boolean, but we're not allowed to change prototype ??
// * what are receive() and generate_ack_packet() for?
// * set p.send_time as now+timeout when pseudocode requests send_time = now
// * WHY are we receiving "0" each time?
// * Go-back-N protocol implemented?
// * TEST w/ settings
// * put on GIT

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned short checksum(struct Packet);

Link_layer::Link_layer(Physical_layer_interface* physical_layer_interface,
 unsigned int num_sequence_numbers,
 unsigned int max_send_window_size,unsigned int timeout)
{
	this->physical_layer_interface = physical_layer_interface;

	next_send_seq = 0;
	// next_send_ack = 0;
	next_receive_seq = 0;
	last_receive_ack = 0;

	this->num_sequence_numbers = num_sequence_numbers;
	this->max_send_window_size = max_send_window_size;
	this->timeout.tv_sec = timeout / 1000000;
	this->timeout.tv_usec = timeout % 1000000;
	receive_buffer_length = 0;

	send_queue = new Timed_packet[max_send_window_size];

	if (pthread_create(&thread,NULL,&Link_layer::loop,this) < 0) {
		throw Link_layer_exception();
	}
}

unsigned int Link_layer::send(unsigned char buffer[],unsigned int length)
{

	if (length==0 || length>Link_layer::MAXIMUM_DATA_LENGTH) {
		throw Link_layer_exception();
	}
	if (this->send_queue_length<this->max_send_window_size) {
		pthread_mutex_lock(&mutex);
		Timed_packet p;
		gettimeofday(&p.send_time, NULL);
		p.send_time = p.send_time + timeout;
		for (unsigned int i = 0; i < length; i++) {
			p.packet.data[i] = buffer[i];
		}
		p.packet.header.data_length = length;
		p.packet.header.seq = next_send_seq;
		p.packet.header.ack = next_send_seq; // TODO: is this accurate? (not stated)
		p.packet.header.checksum = (unsigned int)checksum(p.packet);
		this->send_queue_add_packet(p);


		unsigned char packet_buffer[Physical_layer_interface::MAXIMUM_BUFFER_LENGTH];
		unsigned int length = write_packet(p.packet, packet_buffer);
		unsigned int n = physical_layer_interface->send(packet_buffer,length);

		next_send_seq++;
		pthread_mutex_unlock(&mutex);
		return 1; // TODO: should be `return n;` ??
	} else {
		return 0;
	}
}

unsigned int Link_layer::receive(unsigned char buffer[])
{
	unsigned int length;

	pthread_mutex_lock(&mutex);
	length = receive_buffer_length;
	if (length > 0) {
		Packet p = read_packet(receive_buffer, length);
		length = p.header.data_length;
		for (unsigned int i = 0; i < length; i++) {
//std::cout << "buffer[" << i << "]: " << (int)p.data[i] << endl;
			buffer[i] = p.data[i];
		}
		receive_buffer_length = 0;
	}

	pthread_mutex_unlock(&mutex);
	return length;
}

void Link_layer::send_queue_add_packet(struct Timed_packet p)
{
	// Place packet p in position send_queue_length of send_queue
//std::cout << "adding packet: .seq: " << p.packet.header.seq << " .ack: " << p.packet.header.ack << endl;
	this->send_queue[this->send_queue_length]=p;
	this->send_queue_length++;
}


void Link_layer::append_num_to_buffer(unsigned char buffer[], unsigned int start_position, unsigned int num)
{
	buffer[start_position] = (num >> 24) & 0xFF;
	buffer[start_position+1] = (num >> 16) & 0xFF;
	buffer[start_position+2] = (num >> 8) & 0xFF;
	buffer[start_position+3] = (num) & 0xFF;
}

unsigned int Link_layer::read_num_from_buffer(unsigned char buffer[], unsigned int start_position)
{
	unsigned int num=0;
	num = (unsigned int)buffer[start_position];
	num = (num << 8) + buffer[start_position+1];
	num = (num << 8) + buffer[start_position+2];
	num = (num << 8) + buffer[start_position+3];
	return num;
}

struct Packet Link_layer::read_packet(unsigned char buffer[], unsigned int length)
{
	// Read a buffer into a Packet
	struct Packet p;

	p.header.checksum=read_num_from_buffer(buffer, 0);
	p.header.seq=read_num_from_buffer(buffer, 4);
	p.header.ack=read_num_from_buffer(buffer, 8);
	p.header.data_length=read_num_from_buffer(buffer, 12);

	for (unsigned int i=16; i<length; i++) {
		p.data[i-16]=buffer[i];
	}

	return p;
}

unsigned int Link_layer::write_packet(struct Packet p, unsigned char buffer[])
{
	// Write a packet into a buffer
	unsigned int length = 0;

	p.header.checksum = (unsigned int)checksum(p);
	append_num_to_buffer(buffer, 0, p.header.checksum);
	length+=4;

	append_num_to_buffer(buffer, length, p.header.seq);
	length+=4;

	append_num_to_buffer(buffer, length, p.header.ack);
	length+=4;

	append_num_to_buffer(buffer, length, p.header.data_length);
	length+=4;

	for (unsigned int i=0; i<p.header.data_length; i++) {
		buffer[length+i]=p.data[i];
	}
	length+=p.header.data_length;
	return length;
}

void Link_layer::process_received_packet(struct Packet p)
{
cout << "prp:: p.seq: " << (int)p.header.seq << " == next_receive_seq: " << (int)next_receive_seq << endl;
	if (p.header.seq == next_receive_seq) {
		if (p.header.data_length > 0) {
			if (receive_buffer_length == 0) {
				for (unsigned int i=0; i<p.header.data_length; i++) {
					receive_buffer[i] = p.data[i];
//std::cout << "receive_buffer[" << i << "]: " << (int)receive_buffer[i] << endl;
				}
				receive_buffer_length = p.header.data_length;
				next_receive_seq++;
			}
		} else {
			next_receive_seq++;
		}
//std::cout << "process_received_packet::next_receive_seq: " << (int)next_receive_seq << endl;
		//p.header.ack = p.header.seq; // TODO: ADDED THIS BECAUSE OF BUGS!!!
		last_receive_ack = p.header.ack; // TODO: do we really want this? (NOTE: moved to within if stmnt)
//std::cout << "process_received_packet::p.header.ack: " << p.header.ack << " .seq: " << p.header.seq << endl;
	}
}

void Link_layer::remove_acked_packets()
{
	// TODO: Look over this (replaced (i-1) with i)
	bool remove_packets = false;
	unsigned int i = 0;
	for (; i < this->send_queue_length; i++) {
		if (send_queue[i].packet.header.ack <= last_receive_ack) remove_packets = true;
		if (send_queue[i].packet.header.ack > last_receive_ack) break;
	}

	if (remove_packets) {
		// Reorder the send_queue to shift the active packets over to the 0th position
		for (unsigned int j=i; j < this->send_queue_length; j++) {
std::cout << "removing packet w/  .seq: " << (int)send_queue[j-i].packet.header.seq << "  .ack: " << (int)send_queue[j-i].packet.header.ack << endl;
			send_queue[j-i]=send_queue[j];
		}
		this->send_queue_length-=(i);
/*std::cout << "removed " << (int)i << " packets" << endl;
std::cout << "remaining packets, " << endl;
for (unsigned int k=0; k<send_queue_length; k++) {
	std::cout << "  send_queue[" << (int)k << "]: .seq: " << send_queue[k].packet.header.seq << "  .ack: " << send_queue[k].packet.header.ack << endl;
}*/
	}
}

void Link_layer::send_timed_out_packets()
{
	timeval now;
	gettimeofday(&now, NULL);
	for (unsigned int i = 0; i < send_queue_length; i++) {
		if (send_queue[i].send_time < now) {
			send_queue[i].packet.header.ack = next_receive_seq; // TODO: Why is this expected!?
//std::cout << "send_timed_out_packets,  .seq: " << (int)send_queue[i].packet.header.seq << " (new).ack: " << (int)send_queue[i].packet.header.ack << endl;
			send_queue[i].packet.header.checksum = checksum(send_queue[i].packet);

			unsigned char packet_buffer[Physical_layer_interface::MAXIMUM_BUFFER_LENGTH];
			unsigned int length = write_packet(send_queue[i].packet, packet_buffer);
			if (physical_layer_interface->send(packet_buffer, length)) {
				send_queue[i].send_time = now + timeout;
			}
		}
	}
}

void Link_layer::generate_ack_packet()
{
	if (send_queue_length==0) {
		Timed_packet p;
		gettimeofday(&p.send_time, NULL);
		p.packet.header.seq = next_send_seq;
		p.packet.header.ack = next_send_seq;
		p.packet.header.data_length = 0;

		send_queue[0] = p;
		send_queue_length++;
	}
}

void* Link_layer::loop(void* thread_creator)
{
	const unsigned int LOOP_INTERVAL = 10;
	Link_layer* link_layer = ((Link_layer*) thread_creator);

	while (true) {
		if (link_layer->receive_buffer_length == 0) {
			pthread_mutex_lock(&mutex);
			unsigned int length =
			 link_layer->physical_layer_interface->receive
			 (link_layer->receive_buffer);

			if (length > 0) {
				Packet p = link_layer->read_packet(link_layer->receive_buffer, length);
				if (length >= sizeof(Packet_header)
					&& length <= sizeof(Packet_header) + Link_layer::MAXIMUM_DATA_LENGTH
					&& p.header.data_length <= Link_layer::MAXIMUM_DATA_LENGTH
					&& (unsigned int)checksum(p) == p.header.checksum) {

					link_layer->process_received_packet(p);
				} else {
					cout << "length: " << length << endl;
					cout << "sizeof Packet_header: " << sizeof(Packet_header) << endl;
					cout << "data_length: " << p.header.data_length << endl;
					cout << "seq: " << p.header.seq << endl;
					cout << "ack: " << p.header.ack << endl;
					cout << "checksum: " << p.header.checksum << endl;
					cout << "checksum(p): " << (unsigned int)checksum(p) << endl;
					cout << "Buffer[]: ";
					for (unsigned int i = 0; i < p.header.data_length; i++) {
						cout << (unsigned int)p.data[i];
					}
					cout << endl;
					cout << "ERROR HERE!" << endl;
				}
			}

			link_layer->receive_buffer_length = length;
			link_layer->remove_acked_packets();
			link_layer->send_timed_out_packets();



			pthread_mutex_unlock(&mutex);
		}

		usleep(LOOP_INTERVAL);

		// generate a pure acknowledgement
		link_layer->generate_ack_packet();
	}

	return NULL;
}


// this is the standard Internet checksum algorithm
unsigned short checksum(struct Packet p)
{
	unsigned long sum = 0;
	struct Packet copy;
	unsigned short* shortbuf;
	unsigned int length;

	if (p.header.data_length > Link_layer::MAXIMUM_DATA_LENGTH) {
		throw Link_layer_exception();
	}

	copy = p;
	copy.header.checksum = 0;
	length = sizeof(Packet_header)+copy.header.data_length;
	shortbuf = (unsigned short*) &copy;

	while (length > 1) {
		sum += *shortbuf++;
		length -= 2;
	}
	// handle the trailing byte, if present
	if (length == 1) {
		sum += *(unsigned char*) shortbuf;
	}

	sum = (sum >> 16)+(sum & 0xffff);
	sum = (~(sum+(sum >> 16)) & 0xffff);
	return (unsigned short) sum;
}

