
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

#ifndef _CHANNEL_H_
#define _CHANNEL_H_

/**********************************************************************/

#include "main.h"

/**********************************************************************/

/*
 * Function prototypes
 */

int
channel_packet_errored(double error_probability);

Time
channel_reverse_link_ack_arrival_time(Simulation_Run_Data_Ptr data, Time now);

/**********************************************************************/

#endif /* channel.h */

