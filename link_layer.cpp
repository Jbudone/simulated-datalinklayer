#include "link_layer.h"
#include "timeval_operators.h"

// TODO LIST
// * send() NOT set as boolean, but we're not allowed to change prototype ??
// * set p.send_time as now+timeout when pseudocode requests send_time = now
// * what are these for? [receive(), generate_ack_packet(), num_sequence_numbers, next_send_ack]
// * Go-back-N protocol implemented?
// * WHY does receive() clear the receive_buffer BEFORE process_packet can process the receive!?
// * TEST w/ settings
// * BUG: last_receive_ack set to 0 which auto acknowledges 0th packet

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned short checksum(struct Packet);

unsigned char Link_layer::getSenderSymbol(){
	if (isSender) return ' ';
	else return '\t';
}


Link_layer::Link_layer(Physical_layer_interface* physical_layer_interface,
 unsigned int num_sequence_numbers,
 unsigned int max_send_window_size,unsigned int timeout)
{
	this->physical_layer_interface = physical_layer_interface;

	next_send_seq = 0;
	next_receive_seq = 0;
	last_receive_ack = 0;
	received_ack_0 = false;
	num_sequence_rollover = 0;

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
//cout << "length: " << (int)length << endl;
		throw Link_layer_exception();
	}
	if (send_queue_length<max_send_window_size) {
		pthread_mutex_lock(&mutex);
		Timed_packet p;
		gettimeofday(&p.send_time, NULL);
		p.send_time = p.send_time;// + timeout;
		for (unsigned int i = 0; i < length; i++) {
			p.packet.data[i] = (int)buffer[i];
//cout << getSenderSymbol() << "[" << (int)p.packet.data[i] << "->" << (int)buffer[i] << "] (send)" << endl;
		}
		p.packet.header.data_length = length;
		p.packet.header.seq = next_send_seq;
//		p.packet.header.ack = next_send_seq; // TODO: is this accurate? (not stated)
//		p.packet.header.checksum = (unsigned int)checksum(p.packet);
		send_queue_add_packet(p);


//		unsigned char packet_buffer[Physical_layer_interface::MAXIMUM_BUFFER_LENGTH];
//		unsigned int length = write_packet(p.packet, packet_buffer);
//		unsigned int n = physical_layer_interface->send(packet_buffer,length);

		next_send_seq = (next_send_seq+1)%num_sequence_numbers;
//cout << "next_send_seq: " << (int)next_send_seq << endl;
		pthread_mutex_unlock(&mutex);
		return 1;
	} else {
/*cout << getSenderSymbol() << "send_queue_length(" << (int)send_queue_length << "): ";
for (unsigned int i = 0; i < send_queue_length; i++) {
	cout << "{.seq(" << send_queue[i].packet.header.seq << ").ack(" << send_queue[i].packet.header.ack << ").time(" << send_queue[i].send_time << ")}";
}
cout << endl;*/
		return 0;
	}
}

// NOTE: Copies current receive_buffer out to &buffer[] for reader; then empties the receive_buffer
unsigned int Link_layer::receive(unsigned char buffer[])
{
	unsigned int length;

	pthread_mutex_lock(&mutex);
	length = receive_buffer_length;
	if (length > 0) {
		for (unsigned int i = 0; i < length; i++) {
			buffer[i] = (int)receive_buffer[i];
		}
		receive_buffer_length = 0;
	}

	pthread_mutex_unlock(&mutex);
	return length;
}
// Process a packet (from physical layer receive buffer); write packet into receive_buffer if .seq==next_receive_seq (for receive to see)
void Link_layer::process_received_packet(struct Packet p)
{
	if (p.header.seq == next_receive_seq) {
//cout << getSenderSymbol() << "prp:: p.seq: " << (int)p.header.seq << " == next_receive_seq: " << (int)next_receive_seq << " :: p.ack: " << (int)p.header.ack << " == last_receive_ack: " << (int)last_receive_ack << endl;
		if (p.header.data_length > 0) {
			//if (receive_buffer_length == 0) {
				for (unsigned int i=0; i<p.header.data_length; i++) {
					receive_buffer[i] = (int)p.data[i];
				}
				receive_buffer_length = p.header.data_length;
				//next_receive_seq++;
next_receive_seq = (next_receive_seq+1)%num_sequence_numbers;
//cout << getSenderSymbol() << "next_receive_seq: " << (int)next_receive_seq << endl;
if (next_receive_seq==0) num_sequence_rollover++;
			//} // NOTE: This check is pointless since process_received_packet is only called when receive_buffer_length==0
		} else {
//			next_receive_seq++;
next_receive_seq = (next_receive_seq+1)%num_sequence_numbers;
//cout <<  getSenderSymbol() << "next_receive_seq: " << (int)next_receive_seq << endl;
if (next_receive_seq==0) num_sequence_rollover++;
		}
		//p.header.ack = p.header.seq; // TODO: ADDED THIS BECAUSE OF BUGS!!!
//if (isSender) { receive_buffer_length=0; }
//std::cout << getSenderSymbol() << "process_received_packet::p.header.ack: " << p.header.ack << " .seq: " << p.header.seq << " next_receive_seq: " << (int)next_receive_seq << endl;
	} else {
//cout << getSenderSymbol() << "prp:: p.seq: " << (int)p.header.seq << " != next_receive_seq: " << (int)next_receive_seq << endl;
// TODO %%%%%%%%%
		receive_buffer_length=0; // ignore out-of sequence packets
	}
		last_receive_ack = p.header.ack; // TODO: do we really want this? (NOTE: moved to within if stmnt)
		if (!received_ack_0) received_ack_0=true;
//		if (received_ack_0 && last_receive_ack==0) received_ack_0=false;
//		else received_ack_0=true;
}

void Link_layer::remove_acked_packets()
{
	if (!received_ack_0) return;
	bool remove_packets = false;
	unsigned int i = 0;
	for (; i < this->send_queue_length; i++) {
		if (send_queue[i].packet.header.seq < last_receive_ack+(num_sequence_numbers*num_sequence_rollover)) { remove_packets = true;
//if (getSenderSymbol()==' ')cout << getSenderSymbol() << "removing packet [" << (int)i << "] .seq: " << send_queue[i].packet.header.seq << " .ack: " << send_queue[i].packet.header.ack << endl;
}
		if (send_queue[i].packet.header.seq == last_receive_ack) break;
	}

	if (remove_packets) {
		// Reorder the send_queue to shift the active packets over to the 0th position
		for (unsigned int j=i, k=0; j < this->send_queue_length; j++, k++) {
			send_queue[k]=send_queue[j];
		}
		this->send_queue_length-=(i);
//if (getSenderSymbol()==' ') cout << getSenderSymbol() << "send_queue_length: " << (int)send_queue_length << endl;
	}
}

void Link_layer::send_timed_out_packets()
{
//if (!isSender) cout << getSenderSymbol() << "send_timed_out_packets():: " << (int)send_queue_length << endl;
	timeval now;
	gettimeofday(&now, NULL);
	for (unsigned int i = 0; i < send_queue_length; i++) {
		if (send_queue[i].send_time < now) {
			send_queue[i].packet.header.ack = next_receive_seq; // TODO: Why is this expected!?
//std::cout << getSenderSymbol() << "send_timed_out_packets,  .seq: " << (int)send_queue[i].packet.header.seq << " (new).ack: " << (int)send_queue[i].packet.header.ack << endl;
			send_queue[i].packet.header.checksum = checksum(send_queue[i].packet);

			unsigned char packet_buffer[Physical_layer_interface::MAXIMUM_BUFFER_LENGTH];
			unsigned int length = write_packet(send_queue[i].packet, packet_buffer);
			if (physical_layer_interface->send(packet_buffer, length)) {
				send_queue[i].send_time = now + timeout;
			} else {
//cout << getSenderSymbol() << "failed sending timed out packets!" << endl;
			}
		} else {
//if (!isSender) cout << getSenderSymbol() << "NO TIMEOUT{now=" << now << "}" << endl;
}
	}
}

void Link_layer::generate_ack_packet()
{
	if (send_queue_length==0) {
		Timed_packet p;
		gettimeofday(&p.send_time, NULL);
		p.send_time = p.send_time;// + timeout;
		p.packet.header.seq = next_send_seq;
//		p.packet.header.ack = last_receive_ack;
// TODO %%%%%%%%%%
// give data length in order to allow loop:receive to accept this ack-packet
		p.packet.header.data_length = 0;
//		p.packet.data[0]='0';
		p.packet.header.checksum = (unsigned int)checksum(p.packet);

if (isSender && next_send_seq==0) {
//cout << getSenderSymbol() << "Sending out ack-packet of 0!!" << endl;
}	
		send_queue[0] = p;
		send_queue_length++;
// %%%%%%%%%%%%%%%%%%%%%%%
//next_send_seq++;
		next_send_seq = (next_send_seq+1)%10;
//cout <<  getSenderSymbol() << "next_send_seq: " << (int)next_send_seq << endl;


//		unsigned char packet_buffer[Physical_layer_interface::MAXIMUM_BUFFER_LENGTH];
//		unsigned int length = write_packet(p.packet, packet_buffer);
//		unsigned int n = physical_layer_interface->send(packet_buffer,length);
		//if (n>0) {
//if (getSenderSymbol()!=' ') cout << getSenderSymbol() << "gap::acknowledging [" << (int)p.packet.header.ack << endl;
//		} else {
//if (getSenderSymbol()!=' ') cout << getSenderSymbol() << "FAILED gap::acknowledge [" << (int)p.packet.header.ack << endl;
//		}
	}
}

void* Link_layer::loop(void* thread_creator)
{
	const unsigned int LOOP_INTERVAL = 10;
	Link_layer* link_layer = ((Link_layer*) thread_creator);
	unsigned int length;
	while (true) {
		length = 0;
			pthread_mutex_lock(&mutex);
//		if (link_layer->receive_buffer_length == 0) {
//if (link_layer->getSenderSymbol()==' ') cout << link_layer->getSenderSymbol() << "received " << (int)length << " bytes" << endl;
			length =
			 link_layer->physical_layer_interface->receive
			 (link_layer->receive_buffer);
			link_layer->receive_buffer_length = length;
//		}

			if (length > 0) {
				Packet p = link_layer->read_packet(link_layer->receive_buffer, length);
//cout << "receive packet: p.data_length: " << (int)p.header.data_length << " <= MAXIMUM_DATA_LENGTH: " << (int)Link_layer::MAXIMUM_DATA_LENGTH << endl;
				if (length >= sizeof(Packet_header)
					&& length <= sizeof(Packet_header) + Link_layer::MAXIMUM_DATA_LENGTH
					&& p.header.data_length <= Link_layer::MAXIMUM_DATA_LENGTH
					&& (unsigned int)checksum(p) == p.header.checksum) {

					link_layer->process_received_packet(p);
				} else {
					link_layer->receive_buffer_length=0;
//					if (p.header.checksum != (unsigned int)checksum(p)) {
//						cout << link_layer->getSenderSymbol() << "MISMATCH CHECKSUMS: " << (unsigned int)p.header.checksum << " != " << (unsigned int)checksum(p) << endl;
//					} else {
//					cout << "length: " << length << endl;
//					cout << "sizeof Packet_header: " << sizeof(Packet_header) << endl;
//					cout << "data_length: " << p.header.data_length << endl;
//					cout << "seq: " << p.header.seq << endl;
//					cout << "ack: " << p.header.ack << endl;
//					cout << "checksum: " << p.header.checksum << endl;
//					cout << "checksum(p): " << (unsigned int)checksum(p) << endl;
//					cout << "Buffer[]: ";
//					for (unsigned int i = 0; i < p.header.data_length; i++) {
//						cout << (unsigned int)p.data[i];
//					}
//					cout << endl;
//					cout << "ERROR HERE!" << endl;
//					}
				}
			}// else link_layer->receive_buffer_length=0;

		//}
// TODO %%%%%%%%%%%%%%
// moved these 2 OUTSIDE of the innerloop
		link_layer->remove_acked_packets();
		link_layer->send_timed_out_packets();
//}

			pthread_mutex_unlock(&mutex);

		usleep(LOOP_INTERVAL);

		// generate a pure acknowledgement
		link_layer->generate_ack_packet();
	}

	return NULL;
}






















void Link_layer::send_queue_add_packet(struct Timed_packet p)
{
	// Place packet p in position send_queue_length of send_queue
//std::cout << "adding packet: .seq: " << p.packet.header.seq << " .ack: " << p.packet.header.ack << endl;
	send_queue[send_queue_length]=p;
	send_queue_length++;
}


void Link_layer::append_num_to_buffer(unsigned char buffer[], unsigned int start_position, unsigned int num)
{
	buffer[start_position]   = num >> 24 & 0xFF;
	buffer[start_position+1] = num >> 16 & 0xFF;
	buffer[start_position+2] = num >> 8 & 0xFF;
	buffer[start_position+3] = num & 0xFF;
}

unsigned int Link_layer::read_num_from_buffer(unsigned char buffer[], unsigned int start_position)
{
	unsigned int num=0;
	num = buffer[start_position];
	num = (unsigned int)((num << 8) + (unsigned int)(buffer[start_position+1]));
	num = (unsigned int)((num << 8) + (unsigned int)(buffer[start_position+2]));
	num = (unsigned int)((num << 8) + (unsigned int)(buffer[start_position+3]));

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



// this is the standard Internet checksum algorithm
unsigned short checksum(struct Packet p)
{
	unsigned long sum = 0;
	struct Packet copy;
	unsigned short* shortbuf;
	unsigned int length;

	if (p.header.data_length > Link_layer::MAXIMUM_DATA_LENGTH) {
//cout << "p.header.data_length: " << (int)p.header.data_length << endl;
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

