
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
 * R_next (receiver->r_next) is the sequence number of the next in-order
 * frame the receiver expects. Frames are delivered to the upper layer in
 * order. Every ACK or NAK carries R_next, so it also tells the sender that
 * every frame below R_next has been received.
 *
 * Frames received in error are discarded with no response.
 *
 * Go-Back-N / Stop-and-Wait (Wr = 1):
 *   seq == R_next : deliver it, R_next++, send ACK(R_next).
 *   seq <  R_next : duplicate (its ACK was lost). Send ACK(R_next) again.
 *   seq >  R_next : out of order. Discard, no response.
 *
 * Selective-Repeat (Wr > 1), receive window [R_next, R_next + Wr):
 *   seq == R_next : deliver it and any buffered frames that follow it,
 *                   advance R_next past them, send ACK(R_next).
 *   seq >  R_next : out of order. Buffer it if it is inside the window.
 *                   The first out-of-order frame of a gap is answered with
 *                   NAK(R_next), asking the sender to resend frame R_next.
 *                   Later out-of-order frames, until R_next arrives, are
 *                   answered with ACK(R_next).
 *   seq <  R_next : duplicate. Send ACK(R_next).
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
 * Build an ACK (nak = 0) or NAK (nak = 1) carrying R_next and send it to
 * the sender over the reverse link.
 */

static void
send_ack(Simulation_Run_Ptr simulation_run, int nak, long int r_next)
{
  Simulation_Run_Data_Ptr data;
  Ack_Ptr ack;
  Time now;

  data = (Simulation_Run_Data_Ptr) simulation_run_data(simulation_run);
  now = simulation_run_get_time(simulation_run);

  ack = (Ack_Ptr) xmalloc(sizeof(Ack));
  ack->nak = nak;
  ack->r_next = r_next;
  ack->errored = channel_packet_errored(ACK_PACKET_ERROR_PROB);
  if(ack->errored) data->ack_errors++;

  if(nak) data->naks_sent++;
  else data->acks_sent++;

  TRACE(printf("%.3f: Send %s(%ld)\n", now, nak ? "NAK" : "ACK", r_next););

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
  long int seq;
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

  if(seq == receiver->r_next) {

    /* The expected frame. Deliver it, and (SR) every buffered frame that
       is now in order. */
    TRACE(printf("%.3f: Packet seq %ld accepted\n", now, seq););

    rslot = RECEIVER_SLOT(receiver, seq);
    rslot->received = 1;
    rslot->arrive_time = frame->arrive_time;

    while((rslot = RECEIVER_SLOT(receiver, receiver->r_next))->received) {
      rslot->received = 0;
      receiver->r_next++;

      data->packets_delivered++;
      data->accumulated_delay += now - rslot->arrive_time;

      output_blip_to_screen(simulation_run);
    }

    /* A new gap (if any) gets its own NAK. */
    receiver->nak_sent = 0;

    send_ack(simulation_run, 0, receiver->r_next);

  } else if(seq < receiver->r_next) {

    /* Already delivered. Its earlier ACK must have been lost, so ACK
       again or the sender would resend it forever. */
    TRACE(printf("%.3f: Duplicate packet seq %ld\n", now, seq););

    send_ack(simulation_run, 0, receiver->r_next);

  } else if(RECEIVE_WINDOW_SIZE == 1) {

    /* Go-Back-N: out of order. Discard with no response. */
    TRACE(printf("%.3f: Packet seq %ld out of order (R_next %ld), discarded\n",
		 now, seq, receiver->r_next););

  } else {

    /* Selective-Repeat: out of order. Buffer it if it fits in the
       receive window. */
    if(seq < receiver->r_next + RECEIVE_WINDOW_SIZE) {
      rslot = RECEIVER_SLOT(receiver, seq);
      if(!rslot->received) {
	rslot->received = 1;
	rslot->arrive_time = frame->arrive_time;
      }
      TRACE(printf("%.3f: Packet seq %ld out of order (R_next %ld), buffered\n",
		   now, seq, receiver->r_next););
    } else {
      TRACE(printf("%.3f: Packet seq %ld beyond receive window, discarded\n",
		   now, seq););
    }

    /* NAK the missing frame once; after that ACK(R_next) until it
       arrives. */
    if(!receiver->nak_sent) {
      receiver->nak_sent = 1;
      send_ack(simulation_run, 1, receiver->r_next);
    } else {
      send_ack(simulation_run, 0, receiver->r_next);
    }
  }

  xfree((void *) frame);
}

