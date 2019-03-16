/*
 * FILE: rdt_sender.h
 * DESCRIPTION: The header file for reliable data transfer sender.
 * NOTE: Do not touch this file!
 */


#ifndef _RDT_SENDER_H_
#define _RDT_SENDER_H_

#include "rdt_struct.h"

struct sender_buffer_packet{
  bool ack;
  struct packet* pkt;
  sender_buffer_packet(): ack(false), pkt(NULL){};
};

/*[]------------------------------------------------------------------------[]
  |  routines that you can call
  []------------------------------------------------------------------------[]*/

/* get simulation time (in seconds) */
double GetSimulationTime();

/* start the sender timer with a specified timeout (in seconds).
   the timer is canceled with Sender_StopTimer() is called or a new 
   Sender_StartTimer() is called before the current timer expires.
   Sender_Timeout() will be called when the timer expires. */
void Sender_StartTimer(double timeout);

/* stop the sender timer */
void Sender_StopTimer();

/* check whether the sender timer is being set,
   return true if the timer is set, return false otherwise */
bool Sender_isTimerSet();

/* pass a packet to the lower layer at the sender */
void Sender_ToLowerLayer(struct packet *pkt);


/*[]------------------------------------------------------------------------[]
  |  routines to be changed/enhanced by you
  []------------------------------------------------------------------------[]*/

/* sender initialization, called once at the very beginning.
   this routine is here to help you.  leave it blank if you don't need it.*/
void Sender_Init();

/* sender finalization, called once at the very end.
   this routine is here to help you.  leave it blank if you don't need it.
   in certain cases, you might want to take this opportunity to release some 
   memory you allocated in Sender_init(). */
void Sender_Final();

/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg);

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt);

/* event handler, called when the timer expires */
void Sender_Timeout();

bool Sender_CheckChecksum(struct packet *pkt);
seq_nr Sender_GetACKNo(struct packet *pkt);
void Sender_AddChecksum(packet *pkt);
void Sender_MakePacket(struct packet* pkt, int size, char* data);
seq_nr Sender_Increase(seq_nr seq_no);
void Sender_SendByTrigger();
seq_nr Sender_GetSeq(packet *pkt);
bool Sender_NCKCheck(packet *pkt);
#endif  /* _RDT_SENDER_H_ */
