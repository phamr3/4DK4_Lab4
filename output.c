
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

#include <stdio.h>
#include "simparameters.h"
#include "main.h"
#include "output.h"

/*******************************************************************************/

void
output_blip_to_screen(Simulation_Run_Ptr simulation_run)
{
  double percentagedone;
  Simulation_Run_Data_Ptr data;

  data = (Simulation_Run_Data_Ptr) simulation_run_data(simulation_run);

  data->blip_counter++;

  if((data->blip_counter >= BLIPRATE)
     ||
     (data->packets_delivered >= RUNLENGTH)) {

    data->blip_counter = 0;

    percentagedone =
      100 * (double) data->packets_delivered/RUNLENGTH;

    printf("%3.0f%% ", percentagedone);

    printf("Delivered Pkts = %ld (Transmissions = %ld) \r",
	   data->packets_delivered,
	   data->data_transmissions);

    fflush(stdout);
  }
}

/**********************************************************************/

static const char *
protocol_name(void)
{
  if(SEND_WINDOW_SIZE == 1 && RECEIVE_WINDOW_SIZE == 1)
    return "Stop-and-Wait";
  if(RECEIVE_WINDOW_SIZE == 1)
    return "Go-Back-N";
  return "Selective-Repeat";
}

/**********************************************************************/

void
output_results(Simulation_Run_Ptr this_simulation_run)
{
  Simulation_Run_Data_Ptr sim_data;
  Time sim_time;
  double throughput, success_prob, sw_cycle;

  sim_data = (Simulation_Run_Data_Ptr) simulation_run_data(this_simulation_run);
  sim_time = simulation_run_get_time(this_simulation_run);

  printf("\n");
  printf("Random Seed = %d \n", sim_data->random_seed);
  printf("Protocol = %s (Ws = %d, Wr = %d)\n", protocol_name(),
	 SEND_WINDOW_SIZE, RECEIVE_WINDOW_SIZE);
  printf("p_d = %.4f, p_a = %.4f\n",
	 (double) DATA_PACKET_ERROR_PROB, (double) ACK_PACKET_ERROR_PROB);
  printf("Pkt Length = %.0f bits, Bit Rate = %.0f bps, Pkt Tx Time = %.3f ms\n",
	 (double) PACKET_LENGTH_BITS, (double) LINK_BIT_RATE,
	 (double) DATA_PACKET_TX_TIME);
  printf("Prop. Delay = %.3f ms, ACK Tx Time = %.3f ms, Timeout = %.3f ms\n",
	 (double) PROPAGATION_DELAY, (double) ACK_PACKET_TX_TIME,
	 (double) TIMEOUT_DURATION);

  printf("Delivered Pkts = %ld, Simulated Time = %.1f ms\n",
	 sim_data->packets_delivered, sim_time);

  /* Fraction of the link capacity carrying successfully delivered data. */
  throughput = (double) sim_data->packets_delivered * DATA_PACKET_TX_TIME
    / sim_time;
  printf("Throughput = %.5f (%.2f kbps)\n", throughput,
	 throughput * LINK_BIT_RATE / 1000.0);

  printf("Data Transmissions = %ld (Retransmissions = %ld, Timeouts = %ld)\n",
	 sim_data->data_transmissions, sim_data->retransmissions,
	 sim_data->timeouts);

  printf("Mean Transmissions per Delivered Pkt = %.4f\n",
	 (double) sim_data->data_transmissions / sim_data->packets_delivered);

  printf("Data Pkts in Error = %ld, ACKs in Error = %ld\n",
	 sim_data->data_errors, sim_data->ack_errors);

  printf("Mean Delay = %.3f ms\n",
	 sim_data->accumulated_delay / sim_data->packets_delivered);

  if(!SATURATED_SOURCE) {
    printf("Pkt Arrivals = %ld (Service Fraction = %.5f)\n",
	   sim_data->arrival_count,
	   (double) sim_data->packets_delivered / sim_data->arrival_count);
  }

  /* For Stop-and-Wait with a saturated source there is a simple closed
     form, which is handy for checking the simulator. */
  if(SATURATED_SOURCE && SEND_WINDOW_SIZE == 1 && RECEIVE_WINDOW_SIZE == 1) {
    success_prob = (1.0 - DATA_PACKET_ERROR_PROB) *
      (1.0 - ACK_PACKET_ERROR_PROB);
    sw_cycle = (DATA_PACKET_TX_TIME + 2*PROPAGATION_DELAY + ACK_PACKET_TX_TIME)
      + (1.0/success_prob - 1.0) * (DATA_PACKET_TX_TIME + TIMEOUT_DURATION);
    printf("S&W Theoretical Throughput = %.5f\n",
	   DATA_PACKET_TX_TIME / sw_cycle);
  }

  printf("\n\n");
}

