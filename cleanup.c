
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

#include "simlib.h"
#include "main.h"
#include "simparameters.h"
#include "sender.h"
#include "receiver.h"
#include "cleanup.h"

/*******************************************************************************/

void
cleanup(Simulation_Run_Ptr simulation_run)
{
  Simulation_Run_Data_Ptr data;
  Event_Container_Ptr container;

  data = (Simulation_Run_Data_Ptr) simulation_run_data(simulation_run);

  /* Free the Frames and ACKs still travelling on the links. (Timeout events
     point into the sender window, which is freed below.) */
  for(container = simulation_run->eventlist->front_ptr;
      container != NULL;
      container = container->next_container) {
    if(container->event.function == transmission_end_event ||
       container->event.function == data_reception_event ||
       container->event.function == ack_reception_event) {
      xfree(container->event.attachment);
    }
  }

  /* Clean out the source buffer. */
  while(fifoqueue_size(data->sender.source_buffer) > 0) {
    xfree(fifoqueue_get(data->sender.source_buffer));
  }
  xfree(data->sender.source_buffer);

  /* Free the sender and receiver windows. */
  xfree(data->sender.slots);
  xfree(data->receiver.slots);

  /* Clean up the simulation_run. */
  simulation_run_free_memory(simulation_run);
}

