
/*
 * Simulation of ARQ Protocols (Stop-and-Wait, Go-Back-N, Selective-Repeat)
 *
 * Based on the simlib simulation framework,
 * Copyright (C) 2014 Terence D. Todd Hamilton, Ontario, CANADA
 * todd@mcmaster.ca
 *
 * Distributed under the GNU General Public License, version 3 or later.
 */

/**********************************************************************/

#ifndef _RECEIVER_H_
#define _RECEIVER_H_

/**********************************************************************/

#include "main.h"

/**********************************************************************/

/* The receiver window slot that holds sequence number s. */
#define RECEIVER_SLOT(receiver, s) \
  ((receiver)->slots + ((s) % RECEIVE_WINDOW_SIZE))

/**********************************************************************/

/*
 * Function prototypes
 */

long int
schedule_data_reception_event(Simulation_Run_Ptr, Time, void *);

void
data_reception_event(Simulation_Run_Ptr, void *);

/**********************************************************************/

#endif /* receiver.h */

