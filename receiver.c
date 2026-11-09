
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
 * The ARQ receiver (destination).
 *
 * The receiver accepts packets with sequence numbers in
 * [rcv_base, rcv_base + Wr). Packets are delivered to the upper layer in
 * order. Out-of-order packets inside the window are buffered until the
 * gap in front of them is filled.
 *
 * Every correctly received packet is answered with an ACK carrying:
 *   ack_seq       - the sequence number just received (selective ACK), or -1
 *                   if the packet was outside the window and discarded.
 *   next_expected - rcv_base after processing (cumulative ACK).
 * Packets received in error are dropped silently (no NAKs).
 */

/*******************************************************************************/

#include <stdio.h>
#include "trace.h"
#include "output.h"
#include "channel.h"
#include "sender.h"
#include "receiver.h"

/*******************************************************************************/

long int
schedule_data_reception_event(Simulation_Run_Ptr simulation_run,
			      Time event_time,
			      void * frame)
{
  Event event;

  event.description = "Data Packet Reception";
  event.function = data_reception_event;
  event.attachment = frame;

  return simulation_run_schedule_event(simulation_run, event, event_time);
}

/*******************************************************************************/

/*
 * Build an ACK and send it to the sender over the reverse link.
 */

static void
send_ack(Simulation_Run_Ptr simulation_run, long int ack_seq,
	 long int next_expected)
{
  Simulation_Run_Data_Ptr data;
  Ack_Ptr ack;
  Time now;

  data = (Simulation_Run_Data_Ptr) simulation_run_data(simulation_run);
  now = simulation_run_get_time(simulation_run);

  ack = (Ack_Ptr) xmalloc(sizeof(Ack));
  ack->ack_seq = ack_seq;
  ack->next_expected = next_expected;
  ack->errored = channel_packet_errored(ACK_PACKET_ERROR_PROB);
  if(ack->errored) data->ack_errors++;

  schedule_ack_reception_event(simulation_run,
			       channel_reverse_link_ack_arrival_time(data, now),
			       (void *) ack);
}

/*******************************************************************************/

/*
 * A data packet has been completely received at the destination.
 */

void
data_reception_event(Simulation_Run_Ptr simulation_run, void * ptr)
{
  Simulation_Run_Data_Ptr data;
  Receiver_Ptr receiver;
  Receiver_Slot_Ptr rslot;
  Frame_Ptr frame;
  long int seq, ack_seq;
  Time now;

  frame = (Frame_Ptr) ptr;
  data = (Simulation_Run_Data_Ptr) simulation_run_data(simulation_run);
  receiver = &(data->receiver);
  now = simulation_run_get_time(simulation_run);
  seq = frame->seq;

  /* A corrupted packet is discarded with no response. */
  if(frame->errored) {
    TRACE(printf("%.3f: Packet seq %ld received in error\n", now, seq););
    xfree((void *) frame);
    return;
  }

  if(seq >= receiver->rcv_base &&
     seq < receiver->rcv_base + RECEIVE_WINDOW_SIZE) {

    /* Inside the receive window. Buffer it (if it is not a duplicate). */
    rslot = RECEIVER_SLOT(receiver, seq);
    if(!rslot->received) {
      rslot->received = 1;
      rslot->arrive_time = frame->arrive_time;
    }
    ack_seq = seq;

    TRACE(printf("%.3f: Packet seq %ld accepted\n", now, seq););

    /* Deliver all packets that are now in order. */
    while((rslot = RECEIVER_SLOT(receiver, receiver->rcv_base))->received) {
      rslot->received = 0;
      receiver->rcv_base++;

      data->packets_delivered++;
      data->accumulated_delay += now - rslot->arrive_time;

      output_blip_to_screen(simulation_run);
    }

  } else if(seq < receiver->rcv_base) {

    /* Already delivered. Its earlier ACK must have been lost or delayed,
       so ACK it again. */
    ack_seq = seq;
    TRACE(printf("%.3f: Duplicate packet seq %ld\n", now, seq););

  } else {

    /* Beyond the receive window: discard. With Wr = 1 this happens to
       every packet after a lost one (Go-Back-N). */
    ack_seq = -1;
    TRACE(printf("%.3f: Packet seq %ld out of window, discarded\n",
		 now, seq););
  }

  send_ack(simulation_run, ack_seq, receiver->rcv_base);

  xfree((void *) frame);
}

