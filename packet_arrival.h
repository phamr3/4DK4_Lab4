
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

#ifndef _PACKET_ARRIVAL_H_
#define _PACKET_ARRIVAL_H_

/**********************************************************************/

#include "main.h"

/**********************************************************************/

/*
 * Function prototypes
 */

long int
schedule_packet_arrival_event(Simulation_Run_Ptr, Time);

void
packet_arrival_event(Simulation_Run_Ptr, void *);

/**********************************************************************/

#endif /* packet_arrival.h */

