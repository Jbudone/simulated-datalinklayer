// #define TESTING_READWRITE_PACKET



#include <iostream>
#include <stdlib.h>
#include <stdio.h>

#include "link_layer.h"

#include "test_configuration.cpp"


using namespace std;

void send_n(Link_layer *send_link_layer,Link_layer *receive_link_layer,int n)
{
	unsigned char send_buffer
	 [Physical_layer_interface::MAXIMUM_BUFFER_LENGTH];
	unsigned char receive_buffer
	 [Physical_layer_interface::MAXIMUM_BUFFER_LENGTH];

	int send_count = 0;
	int receive_count = 0;

send_link_layer->isSender=true;
receive_link_layer->isSender=false;
	send_buffer[0] = 0;
	while (send_count < n || receive_count < n) {
		int n0 = send_link_layer->send(send_buffer,1);
		if (send_count < n && n0 == 1) {
			cout << "send:" << (int)send_buffer[0] << endl;
			send_buffer[0]++;
			send_count++;
		}

		n0 = receive_link_layer->receive(receive_buffer);
		if (n0 > 0) {
			cout << "\treceive:" << (int)receive_buffer[0] << endl;
			receive_count++;
		}
	}
}

Physical_layer physical_layer(a_impair,b_impair,NULL,NULL);

Link_layer* a_link_layer = new Link_layer
 (physical_layer.get_a_interface(),num_seq,max_win,timeout);

Link_layer* b_link_layer = new Link_layer
 (physical_layer.get_b_interface(),num_seq,max_win,timeout);

int main(int argc,char* argv[])
{

#ifdef TESTING_READWRITE_PACKET

	Packet p;
	p.header.seq=3;
	p.header.ack=2;
	p.header.data_length=5;
	p.data[0]='a';
	p.data[1]='b';
	p.data[2]='c';
	p.data[3]='d';
	p.data[4]='e';
	// unsigned int my_checksum = checksum(p);
	// p.header.checksum=my_checksum;
	std::cout << "Wrote packet p" << endl;

	unsigned char buffer[Link_layer::MAXIMUM_DATA_LENGTH];
	unsigned int length;
	length = b_link_layer->write_packet(p, buffer);

	std::cout << "Buffer[]: ";
	for (unsigned int i=0; i<length; i++) {
		std::cout << buffer[i];
	}
	std::cout << endl;

	p = b_link_layer->read_packet(buffer, length);
	std::cout << "Packet, " << endl;
	// std::cout << ".header.checksum = " << p.header.checksum << endl;
	std::cout << ".header.seq = " << p.header.seq << endl;
	std::cout << ".header.ack = " << p.header.ack << endl;
	std::cout << ".header.data_length = " << p.header.data_length << endl;
	std::cout << ".data = ";
	for (unsigned int i=0; i<p.header.data_length; i++) {
		std::cout << p.data[i];
	}
	std::cout << endl;

	// if (checksum(p)==my_checksum) {
	// 	std::cout << "Checksum matches!" << endl;
	// } else {
	// 	std::cout << "Checksums DONT match!" << endl;
	// }


	return -1;

#endif

	if (argc != 3) {
		cout << "Syntax: " << argv[0] <<
		 " a_to_b_count b_to_a_count" << endl;
		exit(1);
	}


	cout << "----- a to b..." << endl;
	send_n(a_link_layer,b_link_layer,atoi(argv[1]));

//a_link_layer->last_receive_ack=0;
//a_link_layer->received_ack_0=false;
//a_link_layer->next_receive_seq=0;
//a_link_layer->next_send_seq=0;
a_link_layer->receive_buffer_length=0;
a_link_layer->send_queue_length=0;
physical_layer.get_a_interface()->buffer_length=0;

//b_link_layer->last_receive_ack=0;
//b_link_layer->received_ack_0=false;
//b_link_layer->next_receive_seq=0;
//b_link_layer->next_send_seq=0;
b_link_layer->receive_buffer_length=0;
b_link_layer->send_queue_length=0;
physical_layer.get_b_interface()->buffer_length=0;

	cout << "----- b to a..." << endl;
	send_n(b_link_layer,a_link_layer,atoi(argv[2]));

	return 0;
}
