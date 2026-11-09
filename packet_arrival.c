
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

/*
 * Poisson packet arrivals at the source. Only used when SATURATED_SOURCE is
 * 0. Each arrival time is stored in the sender's source buffer until the
 * send window has room for the packet.
 */

/*******************************************************************************/

#include <stdio.h>
#include "trace.h"
#include "sender.h"
#include "packet_arrival.h"

/*******************************************************************************/

long int
schedule_packet_arrival_event(Simulation_Run_Ptr simulation_run,
			      Time event_time)
{
  Event event;

  event.description = "Packet Arrival";
  event.function = packet_arrival_event;
  event.attachment = NULL;

  return simulation_run_schedule_event(simulation_run, event, event_time);
}

/*******************************************************************************/

void
packet_arrival_event(Simulation_Run_Ptr simulation_run, void * dummy_ptr)
{
  Simulation_Run_Data_Ptr data;
  Time now;
  Time * arrive_time;

  now = simulation_run_get_time(simulation_run);
  data = (Simulation_Run_Data_Ptr) simulation_run_data(simulation_run);
  data->arrival_count++;

  TRACE(printf("%.3f: Packet arrival\n", now););

  /* Put the packet (its arrival time) in the source buffer. */
  arrive_time = (Time *) xmalloc(sizeof(Time));
  *arrive_time = now;
  fifoqueue_put(data->sender.source_buffer, (void *) arrive_time);

  /* Send it now if the link is idle and the window is open. */
  sender_try_transmit(simulation_run);

  /* Schedule the next packet arrival. */
  schedule_packet_arrival_event(simulation_run,
	now + exponential_generator((double) 1/PACKET_ARRIVAL_RATE));
}

