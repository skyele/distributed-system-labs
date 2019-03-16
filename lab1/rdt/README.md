# lab1 - selective repeat

## packet structure 
    header size: 12 bytes
        size: 1 byte
        seq number: 4 bytes
        ack number: 4 bytes
        nak flag:   1 bytes
        checksum:   2 bytes
## packet queue
    buffer the packets passed from the upper layer of the sender
## sender_buffer
    buffer the packets on the fly that are not commit by sender
## receiver_buffer
    buffer the packets that receiver may get out of order packets
## some implementation
    1.nak
        When receiver gets the packets that does not match the number of 'frame_expected', receiver sends a nak packet to sender, to mention it to resend the particular packet.

    2.ack
        When receiver gets a packets in its sliding window, it sends back a 'ack' packet to notify the sender to turn off the packet's timer.

    3.checksum
        I browered the implementation of internet checksum, and integreted it in my code. There are 2 bytes(a short word) in the packet's header, which to check whether the packet is valid or not.

    4.selective repeat
        To implement selective repeat, I used a sliding window and a sender-buffer to record which packet need to resend.