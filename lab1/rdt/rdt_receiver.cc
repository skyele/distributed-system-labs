/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
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

#include "rdt_struct.h"
#include "rdt_receiver.h"
#define MAX_SEQ 3
#define NR_BUFS ((MAX_SEQ + 1)/2)
//static int CHECKSUM_SIZE = 1;
seq_nr frame_expected = 0;
seq_nr too_far = NR_BUFS - 1;
int HEADER_SIZE = 12;
int SEQ_POS = 1;
int ACK_POS = 5;
int NCK_POS = 9;
int CHK_POS = 10;
int IMPOSSIBLE_SEQ_NO = 4096;
int no_nak = true;
int nak_no = -1;
seq_nr next_surrend = 0;
//static int lun = 0;

receiver_buffer_packet receiver_buffer[MAX_SEQ];

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

static bool between(seq_nr a, seq_nr b, seq_nr c){
    return ((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c ) && (c < a));
}

/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    //code here

    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
    for(int i = 0; i < MAX_SEQ; i++){
        receiver_buffer[i].pkt = NULL;
        receiver_buffer[i].rec = false;
        receiver_buffer[i].seq_no = IMPOSSIBLE_SEQ_NO;
    }
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some 
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a packet is passed from the lower layer at the 
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    ASSERT(pkt);
    seq_nr seq_no = *(seq_nr *)(pkt->data + SEQ_POS);
    // fprintf(stdout, "Re -- from low layer -- seq_nr -- %u -- frame_expected -- %u -- too_far -- %u \n", seq_no, frame_expected, too_far);
    if(!Receiver_CheckChecksum(pkt)){
        if(no_nak || !no_nak){
            Receiver_NcktoLowerLayer(0);
        }
    }
    //if(seq_no != frame_expected && no_nak){
    if(seq_no != frame_expected){
        //fprintf(stdout, "Re -- %d -- nak-caused\n", seq_no);
        Receiver_NcktoLowerLayer(seq_no);
    }
    
    if(between(frame_expected, seq_no, too_far) && receiver_buffer[seq_no%MAX_SEQ].rec == false){
        //fprintf(stdout, "Re -- %d -- ack\n", seq_no);
        packet *new_packet = (packet *)malloc(sizeof(packet));
        memcpy(new_packet->data, pkt->data, RDT_PKTSIZE);
        receiver_buffer[seq_no%MAX_SEQ].rec = true;
        receiver_buffer[seq_no%MAX_SEQ].seq_no = seq_no;
        receiver_buffer[seq_no%MAX_SEQ].pkt = new_packet;
        Receiver_AcktoLowerLayer(seq_no);
        while(receiver_buffer[frame_expected%MAX_SEQ].rec){
            // fprintf(stdout, "Re -- try to upper layer -- %u\n", frame_expected);
            Receive_TrytoUpperLayer();
            no_nak = true;
            receiver_buffer[frame_expected%MAX_SEQ].rec = false;
            receiver_buffer[frame_expected%MAX_SEQ].pkt = NULL;
            receiver_buffer[frame_expected%MAX_SEQ].seq_no = IMPOSSIBLE_SEQ_NO;
            frame_expected = Receiver_Increase(frame_expected);
            // fprintf(stdout, "Receiver -- after upper layer -- frame_expected -- %d\n", frame_expected);
            too_far = Receiver_Increase(too_far);
        }
    }
}

void Receiver_AcktoLowerLayer(seq_nr seq_no){
    /* 10-byte header indicating the size of the payload */
    struct packet *pkt = (packet *)malloc(sizeof(packet));
    /* maximum payload size */
    int maxpayload_size = RDT_PKTSIZE - HEADER_SIZE;

    ASSERT(pkt);
    memset(pkt->data, 0, RDT_PKTSIZE);
    pkt->data[0] = 0;
    *(seq_nr *)(pkt->data + SEQ_POS) = 0;         //seq_nr
    *(seq_nr *)(pkt->data + ACK_POS) = seq_no;    //ack
    *(seq_nr *)(pkt->data + NCK_POS) = 0;         //not nak
    memset(pkt->data + HEADER_SIZE, 0, maxpayload_size);
    Receiver_AddChecksum(pkt);
    Receiver_ToLowerLayer(pkt);
}

void Receiver_AddChecksum(packet *pkt){
    ASSERT(pkt);
    unsigned short checksum = calculate_checksum(pkt);
    *(unsigned short *)(pkt->data + CHK_POS) = checksum;
}

void Receive_TrytoUpperLayer(){
    struct message *msg = (struct message*) malloc(sizeof(struct message));
    ASSERT(msg!=NULL);
    packet *pkt = receiver_buffer[frame_expected%MAX_SEQ].pkt;
    msg->size = pkt->data[0];
    // fprintf(stdout, "R -- before surrender -- frame_expected -- %u\n", frame_expected);
    // fprintf(stdout, "R surrender %d -- size -- %d -- content --- %*s\n", *(unsigned char *)((unsigned char*)pkt->data+SEQ_POS), pkt->data[0], pkt->data[0], pkt->data + HEADER_SIZE);
    /* sanity check in case the packet is corrupted */
    if (msg->size<0) msg->size=0;
    if (msg->size>RDT_PKTSIZE-HEADER_SIZE) msg->size=RDT_PKTSIZE-HEADER_SIZE;

    msg->data = (char*) malloc(msg->size);
    ASSERT(msg->data!=NULL);
    memcpy(msg->data, pkt->data+HEADER_SIZE, msg->size);
    Receiver_ToUpperLayer(msg);
    seq_nr tmp =  *(seq_nr *)(pkt->data+SEQ_POS);
    // fprintf(stdout, "tmp -- %u -- next_surrend -- %u\n", tmp, next_surrend);
    ASSERT(tmp == next_surrend);
    next_surrend = Receiver_Increase(next_surrend);
    /* don't forget to free the space */
    if (msg->data!=NULL) free(msg->data);
    if (msg!=NULL) free(msg);
}

seq_nr Receiver_Increase(seq_nr seq_no){
    return (seq_no + 1) ;//% (MAX_SEQ);
}

void Receiver_NcktoLowerLayer(seq_nr seq_no){
    /* 10-byte header indicating the size of the payload */
    struct packet *pkt = (packet *)malloc(sizeof(packet));
    /* maximum payload size */
    int maxpayload_size = RDT_PKTSIZE - HEADER_SIZE;
    memset(pkt->data, 0, RDT_PKTSIZE);
    //fprintf(stdout, "Re -- %d -- nak-expect\n", frame_expected);
    ASSERT(pkt);
    pkt->data[0] = 0;   //size
    *(seq_nr *)(pkt->data + SEQ_POS) = 0;         //seq_nr
    *(seq_nr *)(pkt->data + ACK_POS) = (frame_expected - 1) ;    //ack
    *(char *)((char *)pkt->data + NCK_POS) = 1;         //seq_nr
    no_nak = false;
    nak_no = (frame_expected - 1);
    memset(pkt->data + HEADER_SIZE, 0, maxpayload_size);
    Receiver_AddChecksum(pkt);
    Receiver_ToLowerLayer(pkt);
}

bool Receiver_CheckChecksum(struct packet *pkt){
    ASSERT(pkt);
    unsigned short local_checksum = calculate_checksum(pkt);
    return local_checksum == 0;
}