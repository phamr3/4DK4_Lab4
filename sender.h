/**********************************************************************/

#ifndef _SENDER_H_
#define _SENDER_H_

/**********************************************************************/

#include "main.h"

/**********************************************************************/

/* The sender window slot that holds sequence number s. */
#define SENDER_SLOT(sender, s) ((sender)->slots + ((s) % SEND_WINDOW_SIZE))

/**********************************************************************/

/*
 * Function prototypes
 */

void sender_try_transmit(Simulation_Run_Ptr);

long int schedule_transmission_end_event(Simulation_Run_Ptr, Time, void *);

void transmission_end_event(Simulation_Run_Ptr, void *);

long int schedule_timeout_event(Simulation_Run_Ptr, Time, void *);

void timeout_event(Simulation_Run_Ptr, void *);

long int schedule_ack_reception_event(Simulation_Run_Ptr, Time, void *);

void ack_reception_event(Simulation_Run_Ptr, void *);

/**********************************************************************/

#endif /* sender.h */

