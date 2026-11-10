/**********************************************************************/

#ifndef _SENDER_H_
#define _SENDER_H_

/**********************************************************************/

#include "main.h"

/**********************************************************************/

/* The sender window slot that holds sequence number s. */
#define SENDER_SLOT(sender, s) ((sender)->slots + ((s) % SEND_WINDOW_SIZE))

/* With Wr = 1 (Go-Back-N, and Stop-and-Wait as its Ws = 1 case) the sender
   uses a single timer for the oldest unacknowledged packet and goes back N
   on a timeout. With Wr > 1 (Selective-Repeat) each packet has its own
   timer. */
#define GO_BACK_N (RECEIVE_WINDOW_SIZE == 1)

/**********************************************************************/

/*
 * Function prototypes
 */

void sender_try_transmit(Simulation_Run_Ptr);

long int schedule_transmission_end_event(Simulation_Run_Ptr, Time, void *);

void transmission_end_event(Simulation_Run_Ptr, void *);

long int schedule_timeout_event(Simulation_Run_Ptr, Time, void *);

void timeout_event(Simulation_Run_Ptr, void *);

long int schedule_gbn_timeout_event(Simulation_Run_Ptr, Time);

void gbn_timeout_event(Simulation_Run_Ptr, void *);

long int schedule_ack_reception_event(Simulation_Run_Ptr, Time, void *);

void ack_reception_event(Simulation_Run_Ptr, void *);

/**********************************************************************/

#endif /* sender.h */

