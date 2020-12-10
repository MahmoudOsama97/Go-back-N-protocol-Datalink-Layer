
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

#define PORT 9002
#define MAX_PKT 1 /* determines packet size in bytes */
#define MAX_SEQ 7
typedef enum {frame_arrival, cksum_err, timeout, network_layer_ready} event_type;
typedef enum {FALSE, TRUE} boolean; /* boolean type */
typedef unsigned int seq_nr; /* sequence or ack numbers */
typedef struct {unsigned char data[MAX_PKT];} packet; /* packet definition */
typedef enum {data, ack, nak} frame_kind; /* frame_kind definition */
typedef struct { /* frames are transported in this layer */
frame_kind kind; /* what kind of frame is it? */
seq_nr seq; /* sequence number */
seq_nr ack; /* acknowledgement number */
packet info; /* the network layer packet */
} frame;

packet networkPackets[7];

int timer[MAX_SEQ];
bool timerflag[MAX_SEQ];
int networkCounter=0;
int flag=0;
event_type dummyEvent=network_layer_ready;

unsigned int server_socket;
unsigned int client_socket;


/* Wait for an event to happen; return its type in event. */
void wait_for_event(event_type *event);
/* Fetch a packet from the network layer for transmission on the channel. */
void from_network_layer(packet *p);
/* Deliver information from an inbound frame to the network layer. */
void to_network_layer(packet *p);
/* Go get an inbound frame from the physical_layer and copy it to r. */
void from_physical_layer(frame *r);
/* Pass the frame to the physical_layer for transmission. */
void to_physical_layer(frame *s);
/* Start the clock running and enable the timeout event. */
void start_timer(seq_nr k);
/* Stop the clock and disable the timeout event. */
void stop_timer(seq_nr k);
/* Allow the network layer to cause a network_layer_ready event. */
void enable_network_layer(void);
/* Forbid the network layer from causing a network_layer_ready event. */
void disable_network_layer(void);
/* Macro inc is expanded in-line: increment k circularly. */
#define inc(k) if (k < MAX_SEQ) k = k + 1; else k = 0
void Connect_Master(void);
void Connect_Slave(void);
bool networkLayer=true;

static bool between(seq_nr a, seq_nr b, seq_nr c)
{
/* Return true if a <= b < c circularly; false otherwise. */
if (((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a)))
return true;
else
return false;
}
static void send_data(seq_nr frame_nr, seq_nr frame_expected, packet buffer[ ])
{
/* Construct and send a data frame. */
frame s; /* scratch variable */
s.info = buffer[frame_nr]; /* insert packet into frame */
s.seq = frame_nr; /* insert sequence number into frame */
s.ack = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1); /* piggyback ack */
to_physical_layer(&s); /* transmit the frame */
start_timer(frame_nr); /* start the timer running */
printf("Data inside send fn: %d\n", s.info.data[0]);
}
int main(void)
{
    Connect_Master();
    networkPackets[0].data[0]=1;
    networkPackets[1].data[0]=2;
    networkPackets[2].data[0]=3;
    networkPackets[3].data[0]=4;
    networkPackets[4].data[0]=5;
    networkPackets[5].data[0]=6;
    networkPackets[6].data[0]=7;
    networkPackets[7].data[0]=8;
    seq_nr next_frame_to_send; /* MAX_SEQ > 1; used for outbound stream */
    seq_nr ack_expected; /* oldest frame as yet unacknowledged */
    seq_nr frame_expected; /* next frame_expected on inbound stream */
    frame r; /* scratch variable */
    packet buffer[MAX_SEQ + 1]; /* buffers for the outbound stream */
    seq_nr nbuffered; /* number of output buffers currently in use */
    seq_nr i; /* used to index into the buffer array */
    event_type event;
    enable_network_layer(); /* allow network_layer_ready events */
    ack_expected = 0; /* next ack_expected inbound */
    next_frame_to_send = 0; /* next frame going out */
    frame_expected = 0; /* number of frame_expected inbound */
    nbuffered = 0; /* initially no packets are buffered */
    while (true) {
        wait_for_event(&event); /* four possibilities: see event_type above */
        //printf("event received");
        switch(event) {
            case network_layer_ready: /* the network layer has a packet to send */
                /* Accept, save, and transmit a new frame. */
                printf("network is ready\n");
                from_network_layer(&buffer[next_frame_to_send]); /* fetch new packet */
                nbuffered = nbuffered + 1; /* expand the sender’s window */
                send_data(next_frame_to_send, frame_expected, buffer);/* transmit the frame */
                inc(next_frame_to_send); /* advance sender’s upper window edge */
                
                break;
            case frame_arrival: /* a data or control frame has arrived */
                printf("frame arrival\n");
                from_physical_layer(&r); /* get incoming frame from_physical_layer */
                printf("Arrived data %d\n",r.info.data[0]);
                if (r.seq == frame_expected) {
                /* Frames are accepted only in order. */
                to_network_layer(&r.info); /* pass packet to_network_layer */
                inc(frame_expected); /* advance lower edge of receiver’s window */
                }
                /* Ack n implies n − 1, n − 2, etc. Check for this. */
                while (between(ack_expected, r.ack, next_frame_to_send)) {
                /* Handle piggybacked ack. */
                nbuffered = nbuffered - 1; /* one frame fewer buffered */
                stop_timer(ack_expected); /* frame arrived intact; stop_timer */
                inc(ack_expected); /* contract sender’s window */
                }
            break;
            case cksum_err: break; /* just ignore bad frames */
            case timeout: /* trouble; retransmit all outstanding frames */
                printf("timeout\n");
                next_frame_to_send = ack_expected; /* start retransmitting here */
                for (i = 1; i <= nbuffered; i++) {
                send_data(next_frame_to_send, frame_expected, buffer);/* resend frame */
                inc(next_frame_to_send); /* prepare to send the next one */
                }
        }
        if (nbuffered < MAX_SEQ)
        enable_network_layer();
        else
        disable_network_layer();
        
        if(flag==1){
            dummyEvent=frame_arrival;
            flag=0;
            }
        else {
            dummyEvent=network_layer_ready;
            flag=1;
            }
        for(int i=0;i<MAX_SEQ;i++){
            if(timerflag[i]==true){
                
                timer[i]++;
                if (timer[i]==10000) dummyEvent=timeout;
            }
            
        }
        for(int i=0; i<100000000;i++);
    }
}

void enable_network_layer(void){
    networkLayer=true;
}


void disable_network_layer(void){
    networkLayer=false;
}


void start_timer(seq_nr k){
    
    timerflag[k]=true;
}
/* Stop the clock and disable the timeout event. */
void stop_timer(seq_nr k){
    
    timerflag[k]=false;
    
}
/* Allow the network layer to cause a network_layer_ready event. */

void wait_for_event(event_type *event){
    
    *event=dummyEvent;
    
}


void from_network_layer(packet *p){
    
    *p=networkPackets[(networkCounter++)%8];
    
}
/* Deliver information from an inbound frame to the network layer. */
void to_network_layer(packet *p){
    
    
    
}
/* Go get an inbound frame from the physical_layer and copy it to r. */
void from_physical_layer(frame *r){
    
    
    read(client_socket, r, sizeof(frame));
    
}
/* Pass the frame to the physical_layer for transmission. */
void to_physical_layer(frame *s){
    
    
     write(client_socket, s, sizeof(frame));
    
}
/* Start the clock running and enable the timeout event. */



void Connect_Slave(void)
{
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("192.168.1.4");
    address.sin_port = htons(PORT);
    printf("Waiting for Connection...\n");
    while (connect(client_socket, (struct sockaddr *)&address, sizeof(address)));
    printf("Slave Connected\n");
}

void Connect_Master(void){
            printf("Master Waiting\n");
            server_socket = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in address;
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = inet_addr("192.168.1.4");
            address.sin_port = htons(PORT);
            bind(server_socket, (struct sockaddr *)&address, sizeof(address));
            listen(server_socket, 1);
            client_socket = accept(server_socket, NULL, NULL);
            printf("Slave connected\n");
}

