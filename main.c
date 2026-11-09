
/*
 * Simulation of ARQ Protocols (Stop-and-Wait, Go-Back-N, Selective-Repeat)
 *
 * Based on the simlib simulation framework,
 * Copyright (C) 2014 Terence D. Todd Hamilton, Ontario, CANADA
 * todd@mcmaster.ca
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*******************************************************************************/

/*
 * A single source sends packets to a single destination over one
 * point-to-point link using a general Selective-Repeat ARQ protocol with
 * send window Ws and receive window Wr (see simparameters.h):
 *
 *   Ws = Wr = 1  -> Stop-and-Wait
 *   Wr = 1       -> Go-Back-N
 *   otherwise    -> Selective-Repeat
 */

/*******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "output.h"
#include "trace.h"
#include "simparameters.h"
#include "cleanup.h"
#include "packet_arrival.h"
#include "sender.h"
#include "receiver.h"
#include "main.h"

/*******************************************************************************/

int
main(void)
{
  /* Get the list of random number generator seeds defined in simparameters.h */
  unsigned random_seed;
  unsigned RANDOM_SEEDS[] = {RANDOM_SEED_LIST, 0};

  Simulation_Run_Ptr simulation_run;
  Simulation_Run_Data data;
  int j=0;

  /* Check the parameters. */
  if(SEND_WINDOW_SIZE < 1 || RECEIVE_WINDOW_SIZE < 1) {
    printf("Error: window sizes must be at least 1.\n");
    exit(1);
  }

  if(RECEIVE_WINDOW_SIZE > SEND_WINDOW_SIZE) {
    printf("Warning: Wr > Ws. Receive buffer beyond Ws is never used.\n");
  }

  if(TIMEOUT_DURATION < 2*PROPAGATION_DELAY + ACK_PACKET_TX_TIME) {
    printf("Warning: timeout is shorter than the round trip time.\n");
  }

  /* Do a new simulation_run for each random number generator seed. */
  while ((random_seed = RANDOM_SEEDS[j++]) != 0) {

    /* Set the random generator seed. */
    random_generator_initialize(random_seed);

    /* Create a new simulation_run. This gives a clock and
       eventlist. Clock time is set to zero. */
    simulation_run = (Simulation_Run_Ptr) simulation_run_new();

    /* Add our data definitions to the simulation_run. */
    simulation_run_set_data(simulation_run, (void *) & data);

    /* Initialize various simulation_run variables. */
    data.blip_counter = 0;
    data.arrival_count = 0;
    data.packets_delivered = 0;
    data.data_transmissions = 0;
    data.retransmissions = 0;
    data.timeouts = 0;
    data.data_errors = 0;
    data.ack_errors = 0;
    data.accumulated_delay = 0.0;
    data.reverse_link_free_time = 0.0;
    data.random_seed = random_seed;

    /* Create and initialize the sender. xcalloc zeroes the window slots. */
    data.sender.slots = (Sender_Slot_Ptr)
      xcalloc((unsigned int) SEND_WINDOW_SIZE, sizeof(Sender_Slot));
    data.sender.base = 0;
    data.sender.next_seq = 0;
    data.sender.link_busy = 0;
    data.sender.source_buffer = fifoqueue_new();

    /* Create and initialize the receiver. */
    data.receiver.slots = (Receiver_Slot_Ptr)
      xcalloc((unsigned int) RECEIVE_WINDOW_SIZE, sizeof(Receiver_Slot));
    data.receiver.rcv_base = 0;

    /* Get the source going. */
    if(SATURATED_SOURCE) {
      sender_try_transmit(simulation_run);
    } else {
      schedule_packet_arrival_event(simulation_run,
		    simulation_run_get_time(simulation_run) +
		    exponential_generator((double) 1/PACKET_ARRIVAL_RATE));
    }

    /* Execute events until we are finished. */
    while(data.packets_delivered < RUNLENGTH) {
      simulation_run_execute_event(simulation_run);
    }

    /* Print out some results. */
    output_results(simulation_run);

    /* Clean up memory. */
    cleanup(simulation_run);
  }

  /* Pause before finishing. */
  getchar();

  return 0;
}

