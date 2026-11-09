
/*
 * Simulation of ARQ Protocols (Stop-and-Wait, Go-Back-N, Selective-Repeat)
 *
 * Based on the simlib simulation framework,
 * Copyright (C) 2014 Terence D. Todd Hamilton, Ontario, CANADA
 * todd@mcmaster.ca
 *
 * Distributed under the GNU General Public License, version 3 or later.
 */

/*******************************************************************************/

#include "simparameters.h"
#include "main.h"
#include "channel.h"

/*******************************************************************************/

/*
 * Link helper functions.
 *
 * The forward link (source -> destination) carries data packets and the
 * reverse link (destination -> source) carries ACKs. Each packet is received
 * in error independently with the given probability.
 */

int
channel_packet_errored(double error_probability)
{
  return (uniform_generator() < error_probability);
}

/*******************************************************************************/

/*
 * Reserve the reverse link for one ACK that is ready to be sent at time
 * "now". If a previous ACK is still being transmitted, this one waits behind
 * it. Returns the time that the ACK is completely received at the source.
 */

Time
channel_reverse_link_ack_arrival_time(Simulation_Run_Data_Ptr data, Time now)
{
  Time start;

  start = (data->reverse_link_free_time > now) ?
    data->reverse_link_free_time : now;

  data->reverse_link_free_time = start + ACK_PACKET_TX_TIME;

  return start + ACK_PACKET_TX_TIME + PROPAGATION_DELAY;
}

