
/*
 * Simulation of ARQ Protocols (Stop-and-Wait, Go-Back-N, Selective-Repeat)
 */

/*******************************************************************************/

#ifndef _SIMPARAMETERS_H_
#define _SIMPARAMETERS_H_

/*******************************************************************************/

/*
 * All times are in milliseconds (ms).
 *
 * Protocol selection:
 *   Stop-and-Wait    : SEND_WINDOW_SIZE = 1,  RECEIVE_WINDOW_SIZE = 1
 *   Go-Back-N        : SEND_WINDOW_SIZE = N,  RECEIVE_WINDOW_SIZE = 1
 *   Selective-Repeat : SEND_WINDOW_SIZE = Ws, RECEIVE_WINDOW_SIZE = Wr (> 1)
 */

#define SEND_WINDOW_SIZE 1          /* Ws */
#define RECEIVE_WINDOW_SIZE 1       /* Wr */

/* Link and packet. 1000 bits at 1 Mbps gives a 1 ms data packet Tx time. */
#define PACKET_LENGTH_BITS 1000.0   /* fixed data packet length (bits) */
#define LINK_BIT_RATE 1.0e6         /* link bit rate (bits/s) */
#define DATA_PACKET_TX_TIME (1000.0 * PACKET_LENGTH_BITS / LINK_BIT_RATE) /* ms */

/* ACK transmission time and processing time are neglected. */
#define ACK_PACKET_TX_TIME 0.0      /* ms */

/* One-way propagation delay. Round trip delay = 2 * 5 = 10 ms. */
#define PROPAGATION_DELAY 5.0       /* ms */

/*
 * Error model. Each data packet transmission is independently unsuccessful
 * with probability p_d. An unsuccessful packet is discarded at the receiver
 * and no ACK is returned. Each ACK (sent only for successfully received
 * packets) is independently unsuccessful with probability p_a, independent of
 * the data packet errors.
 */
#define DATA_PACKET_ERROR_PROB 0.1  /* p_d */
#define ACK_PACKET_ERROR_PROB 0.0   /* p_a */

/* Retransmission timeout (ms), started when a data packet transmission
   ends. It must be longer than the round trip delay
   (2*PROPAGATION_DELAY + ACK_PACKET_TX_TIME = 10 ms) or packets will be
   retransmitted needlessly. */
#define TIMEOUT_DURATION 15.0       /* ms */

/* Source model. If SATURATED_SOURCE is 1 the source always has a packet
   ready to send. If it is 0, packets arrive as a Poisson process with rate
   PACKET_ARRIVAL_RATE (packets per ms). */
#define SATURATED_SOURCE 1
#define PACKET_ARRIVAL_RATE 0.5

#define RUNLENGTH 1000000           /* packets delivered in order to the destination */
#define BLIPRATE 10000

/* Comma separated list of random seeds to run. */
#define RANDOM_SEED_LIST 400465631, 400459789, 400464608

/*******************************************************************************/

#endif /* simparameters.h */

