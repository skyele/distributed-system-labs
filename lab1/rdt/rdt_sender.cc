/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  1 byte  ->|<-             the rest            ->|
 *       | payload size |<-             payload             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <queue>
#include "rdt_struct.h"
#include "rdt_sender.h" 
using namespace std;
struct sender_buffer_packet{
  bool ack;
  struct packet* pkt;
  sender_buffer_packet(): ack(false), pkt(NULL){};
};
#define MAX_SEQ 9
#define NR_BUFS ((MAX_SEQ + 1)/2)
bool Sender_CheckChecksum(struct packet *pkt);
unsigned int Sender_GetACKNo(struct packet *pkt);
void Sender_AddChecksum(packet *pkt);
void Sender_MakePacket(struct packet* pkt, int size, char* data);
unsigned int Sender_Increase(unsigned int seq_no);
void Sender_SendByTrigger();
unsigned int Sender_GetSeq(packet *pkt);
bool Sender_NCKCheck(packet *pkt);
typedef unsigned int seq_nr; /* sequence or ack numbers */

typedef enum {frame_arrival, cksum_err, timeout, network_layer_ready, ack_timeout} even_type;
seq_nr oldest_frame = MAX_SEQ + 1;
seq_nr ack_expected = 0;
seq_nr next_frame_to_send = 0;
seq_nr next_seq_in_queue = 0;
int nbuffered = 0;
extern int HEADER_SIZE;
extern int SEQ_POS;
extern int ACK_POS;
extern int NCK_POS;
extern int CHK_POS;
double TIME_OUT = 0.6;
queue<packet *> packet_queue;

sender_buffer_packet sender_buffer[MAX_SEQ];

static bool between(seq_nr a, seq_nr b, seq_nr c){
    return ((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c ) && (c < a));
}

static unsigned short calculate_checksum(packet *pkt){
    unsigned short checksum = 0;
    unsigned short* ptr = (unsigned short*)pkt->data;
    for(int i = 0; i < RDT_PKTSIZE; i+=2){
        checksum += *(unsigned short*)ptr++;
    }
    while(checksum >> 16)
        checksum = (checksum & 0xffff) + (checksum >> 16);
    return ~checksum;
}

/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    //code here
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());

    for(int i = 0; i < MAX_SEQ; i++){
        sender_buffer[i].ack = false;
        sender_buffer[i].pkt = NULL;
    }
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some 
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    /* 10-byte header indicating the size of the payload */
    /* maximum payload size */
    int maxpayload_size = RDT_PKTSIZE - HEADER_SIZE;

    bool trigger = false;
    if(!packet_queue.size())
        trigger = true;
    /* split the message if it is too big */
    int cursor = 0;

    while (msg->size-cursor > maxpayload_size){
        packet* pkt = (packet*)malloc(sizeof(packet));
        Sender_MakePacket(pkt, maxpayload_size, msg->data + cursor);
        packet_queue.push(pkt);
        next_seq_in_queue = Sender_Increase(next_seq_in_queue);
        /* move the cursor */
        cursor += maxpayload_size;
    }

    /* send out the last packet */
    if (msg->size > cursor)
    {
        packet* pkt = (packet*)malloc(sizeof(packet));
        Sender_MakePacket(pkt, msg->size - cursor, msg->data + cursor);
        packet_queue.push(pkt);
        next_seq_in_queue = Sender_Increase(next_seq_in_queue);
    }

    if(trigger)
        Sender_SendByTrigger();
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    seq_nr seq_no;
    if(!Sender_CheckChecksum(pkt))
        return;
    seq_no = Sender_GetACKNo(pkt);
    if(Sender_NCKCheck(pkt)){   //nck
        if(between(ack_expected, Sender_Increase(seq_no), next_frame_to_send)){
            Sender_ToLowerLayer(sender_buffer[Sender_Increase(seq_no)%MAX_SEQ].pkt);
            Sender_StartTimer(TIME_OUT);
        }
        bool isMove = false;
        while(between(ack_expected, seq_no, next_frame_to_send)){
            nbuffered = nbuffered - 1;
            sender_buffer[ack_expected%MAX_SEQ].ack = false;
            free(sender_buffer[ack_expected%MAX_SEQ].pkt);
            sender_buffer[ack_expected%MAX_SEQ].pkt = NULL;
            ack_expected = Sender_Increase(ack_expected);
            isMove = true;
        }
        if(isMove){
            Sender_SendByTrigger();
        }
        if(!nbuffered && !packet_queue.size())
            Sender_StopTimer();
    }
    else{
        if(between(ack_expected, seq_no, next_frame_to_send)){
            packet *local_packet = (packet *)malloc(sizeof(packet));
            memcpy(local_packet->data, pkt->data, RDT_PKTSIZE);
            sender_buffer[seq_no%MAX_SEQ].ack = true;
            sender_buffer[seq_no%MAX_SEQ].pkt = local_packet;
            bool isMove = false;

            while(sender_buffer[ack_expected%MAX_SEQ].ack){
                nbuffered = nbuffered - 1;
                sender_buffer[ack_expected%MAX_SEQ].ack = false;
                free(sender_buffer[ack_expected%MAX_SEQ].pkt);
                sender_buffer[ack_expected%MAX_SEQ].pkt = NULL;
                ack_expected = Sender_Increase(ack_expected);
                isMove = true;
            }
            if(isMove)
                Sender_SendByTrigger();
            if(!nbuffered && !packet_queue.size())
                Sender_StopTimer();
        }
    }
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    seq_nr i = ack_expected;
    while(between(ack_expected, i, next_frame_to_send)){
        if(!sender_buffer[i%MAX_SEQ].ack){
            Sender_ToLowerLayer(sender_buffer[i%MAX_SEQ].pkt);
            Sender_StartTimer(TIME_OUT);
        }
        i = Sender_Increase(i);
    }
}

bool Sender_CheckChecksum(struct packet *pkt){
    ASSERT(pkt);
    unsigned short local_checksum = calculate_checksum(pkt);
    return local_checksum == 0;
}

seq_nr Sender_GetACKNo(struct packet *pkt){
    ASSERT(pkt);
    return *(seq_nr *)(pkt->data + ACK_POS);
}

seq_nr Sender_Increase(seq_nr seq_no){
    return (seq_no + 1);
}

void Sender_AddChecksum(packet *pkt){
    ASSERT(pkt);

    unsigned short checksum = calculate_checksum(pkt);
    *(unsigned short *)(pkt->data + CHK_POS) = checksum;
}

void Sender_MakePacket(struct packet* pkt, int size, char* data){
    memset(pkt->data, 0, RDT_PKTSIZE);
    pkt->data[0] = size;
    *(seq_nr *)(pkt->data + SEQ_POS) = next_seq_in_queue;   //seq_num
    *(seq_nr *)(pkt->data + ACK_POS) = (ack_expected - 1);    //ack
    *(char *)(pkt->data + NCK_POS) = 0;
    memcpy(pkt->data + HEADER_SIZE, data, size);    //data
    Sender_AddChecksum(pkt);
}

void Sender_SendByTrigger(){
    while(nbuffered < NR_BUFS - 1 && packet_queue.size()){
        packet *pkt = packet_queue.front();
        packet_queue.pop();
        sender_buffer[next_frame_to_send%MAX_SEQ].ack = false;
        sender_buffer[next_frame_to_send%MAX_SEQ].pkt = pkt;
        nbuffered++;
        next_frame_to_send = Sender_Increase(next_frame_to_send);
        Sender_StartTimer(TIME_OUT);
        /* send it out through the lower layer */
        Sender_ToLowerLayer(pkt);
    }
}

bool Sender_NCKCheck(packet *pkt){
    return *(char *)(pkt->data + NCK_POS);
}

seq_nr Sender_GetSeq(packet *pkt){
    return *(seq_nr *)(pkt->data + SEQ_POS);
}