The Problem

* Sending buffer[i]=0 sometimes casts 0 as '0' and hence buffer[i]=48
* generate_ack_packet() is not defined to ONLY work for receiver
* receiver continuously sends acknowledgement (and hence the program never stops)
* receive is reading either 244 or 12 each time
* send is never displayed
* setup on loop is PROBABLY very wrong (deleting receive_buffer if set; this was implemented because we need to somehow clear the SENDER's receive_buffer)
* other settings have not even been tested yet (sender window, sequence numbers?, corruption, delay)
