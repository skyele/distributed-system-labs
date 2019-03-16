/*
 * FILE: rdt_receiver.h
 * DESCRIPTION: The header file for reliable data transfer receiver.
 * NOTE: Do not touch this file!
 */


#ifndef _RDT_RECEIVER_H_
#define _RDT_RECEIVER_H_

#include "rdt_struct.h"

struct receiver_buffer_packet{
  bool rec;
  seq_nr seq_no;
  packet* pkt;
  receiver_buffer_packet(): rec(false){};
};

/*[]------------------------------------------------------------------------[]
  |  routines that you can call
  []------------------------------------------------------------------------[]*/

/* get simulation time (in seconds) */
double GetSimulationTime();

/* pass a packet to the lower layer at the receiver */
void Receiver_ToLowerLayer(struct packet *pkt);

/* deliver a message to the upper layer at the receiver */
void Receiver_ToUpperLayer(struct message *msg);

/*[]------------------------------------------------------------------------[]
  |  routines to be changed/enhanced by you
  []------------------------------------------------------------------------[]*/

/* receiver initialization, called once at the very beginning.
   this routine is here to help you.  leave it blank if you don't need it.*/
void Receiver_Init();

/* receiver finalization, called once at the very end.
   this routine is here to help you.  leave it blank if you don't need it.
   in certain cases, you might want to use this opportunity to release some 
   memory you allocated in Receiver_init(). */
void Receiver_Final();

/* event handler, called when a packet is passed from the lower layer at the 
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt);

void Receiver_AcktoLowerLayer(seq_nr seq_no);

void Receiver_AddChecksum(packet *pkt);
seq_nr Receiver_Increase(seq_nr seq_no);
void Receive_TrytoUpperLayer();
void Receiver_NcktoLowerLayer(seq_nr seq_no);
bool Receiver_CheckChecksum(struct packet *pkt);
#endif  /* _RDT_RECEIVER_H_ */
