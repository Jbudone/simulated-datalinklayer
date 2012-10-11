The Problem

* Sending buffer[i]=0 sometimes casts 0 as '0' and hence buffer[i]=48
* generate_ack_packet() is not defined to ONLY work for receiver
* other settings have not even been tested yet ( sequence numbers?, corruption, delay, drop)
* implement num_sequence_number (including issues such as window_size > num_sequence)
* issue with swapping a/b (2nd call -- initialization?)
* CLEAN
