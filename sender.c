/*
 * The ARQ sender (source).
 *
 * The sender may have up to SEND_WINDOW_SIZE unacknowledged packets
 * outstanding, i.e., sequence numbers in [base, base + Ws). Each packet has
 * its own retransmission timer, started at the end of each transmission.
 * When a timer expires, only that packet is retransmitted (selective
 * repeat). Pending retransmissions are always sent before new packets, lowest
 * sequence number first.
 *
 * With RECEIVE_WINDOW_SIZE = 1 the receiver discards every packet that
 * arrives after a lost one. Those packets are never ACKed, so their timers
 * expire one after another and they are all resent. This is Go-Back-N
 * behaviour. With Ws = Wr = 1 it is Stop-and-Wait.
 */

/*******************************************************************************/

#include <stdio.h>
#include "trace.h"
#include "channel.h"
#include "receiver.h"
#include "sender.h"

/*******************************************************************************/

/*
 * Start a data packet transmission if the forward link is idle and there is
 * something to send. This is called whenever something happens that might
 * allow a new transmission (link becomes free, ACK received, timeout,
 * packet arrival).
 */

void sender_try_transmit(Simulation_Run_Ptr simulation_run) {
  Simulation_Run_Data_Ptr data;
  Sender_Ptr sender;
  Sender_Slot_Ptr slot = NULL, candidate;
  Frame_Ptr frame;
  Time now, arrive_time;
  Time * arrive_time_ptr;
  long int s;

  data = (Simulation_Run_Data_Ptr) simulation_run_data(simulation_run);
  sender = &(data->sender);
  now = simulation_run_get_time(simulation_run);

  /* Only one data packet can be on the forward link at a time. */
  if(sender->link_busy) return;

  /* First priority: retransmissions, lowest sequence number first. */
  for(s = sender->base; s < sender->next_seq; s++) {
    candidate = SENDER_SLOT(sender, s);
    if(!candidate->acked && candidate->needs_retx) {
      slot = candidate;
      break;
    }
  }

  if(slot != NULL) {

    slot->needs_retx = 0;
    data->retransmissions++;

    TRACE(printf("%.3f: Retransmit seq %ld\n", now, slot->seq););

  } else {

    /* Second priority: a new packet, if the send window is not full. */
    if(sender->next_seq >= sender->base + SEND_WINDOW_SIZE) return;

    if(SATURATED_SOURCE) {
      /* The source always has a packet ready. */
      data->arrival_count++;
      arrive_time = now;
    } else {
      /* Take the oldest waiting packet, if there is one. */
      if(fifoqueue_size(sender->source_buffer) == 0) return;
      arrive_time_ptr = (Time *) fifoqueue_get(sender->source_buffer);
      arrive_time = *arrive_time_ptr;
      xfree((void *) arrive_time_ptr);
    }

    slot = SENDER_SLOT(sender, sender->next_seq);
    slot->seq = sender->next_seq++;
    slot->arrive_time = arrive_time;
    slot->tx_count = 0;
    slot->acked = 0;
    slot->needs_retx = 0;
    slot->timer_active = 0;

    TRACE(printf("%.3f: Transmit new seq %ld\n", now, slot->seq););
  }

  slot->tx_count++;
  data->data_transmissions++;

  /* Create the copy of the packet that travels over the link. */
  frame = (Frame_Ptr) xmalloc(sizeof(Frame));
  frame->seq = slot->seq;
  frame->arrive_time = slot->arrive_time;
  frame->errored = 0;

  sender->link_busy = 1;

  schedule_transmission_end_event(simulation_run,
				  now + DATA_PACKET_TX_TIME,
				  (void *) frame);
}

/*******************************************************************************/

long int schedule_transmission_end_event(Simulation_Run_Ptr simulation_run, Time event_time, void * frame) {
  Event event;

  event.description = "End of Data Packet Tx";
  event.function = transmission_end_event;
  event.attachment = frame;

  return simulation_run_schedule_event(simulation_run, event, event_time);
}

/*******************************************************************************/

/*
 * The last bit of a data packet has left the sender. Decide whether the
 * channel corrupts it, start its retransmission timer, send it on its way to
 * the receiver, and start the next transmission.
 */

void transmission_end_event(Simulation_Run_Ptr simulation_run, void * ptr) {
  Simulation_Run_Data_Ptr data;
  Sender_Ptr sender;
  Sender_Slot_Ptr slot;
  Frame_Ptr frame;
  Time now;

  frame = (Frame_Ptr) ptr;
  data = (Simulation_Run_Data_Ptr) simulation_run_data(simulation_run);
  sender = &(data->sender);
  now = simulation_run_get_time(simulation_run);

  sender->link_busy = 0;

  /* Is the packet received in error? */
  frame->errored = channel_packet_errored(DATA_PACKET_ERROR_PROB);
  if(frame->errored) data->data_errors++;

  /* Start the retransmission timer, unless the packet was ACKed (by an
     earlier copy) while it was being transmitted. */
  if(frame->seq >= sender->base && frame->seq < sender->next_seq) {
    slot = SENDER_SLOT(sender, frame->seq);
    if(slot->seq == frame->seq && !slot->acked) {
      if(slot->timer_active) {
	simulation_run_deschedule_event(simulation_run, slot->timer_id);
      }
      slot->timer_id = schedule_timeout_event(simulation_run,
					      now + TIMEOUT_DURATION,
					      (void *) slot);
      slot->timer_active = 1;
    }
  }

  /* The packet arrives at the receiver after the propagation delay. */
  schedule_data_reception_event(simulation_run,
				now + PROPAGATION_DELAY,
				(void *) frame);

  /* The link is free. Send the next packet if possible. */
  sender_try_transmit(simulation_run);
}

/*******************************************************************************/

long int schedule_timeout_event(Simulation_Run_Ptr simulation_run, Time event_time, void * slot) {
  Event event;

  event.description = "Retransmission Timeout";
  event.function = timeout_event;
  event.attachment = slot;

  return simulation_run_schedule_event(simulation_run, event, event_time);
}

/*******************************************************************************/

/*
 * A packet's timer expired before its ACK arrived. Mark it for
 * retransmission.
 */

void timeout_event(Simulation_Run_Ptr simulation_run, void * ptr) {
  Simulation_Run_Data_Ptr data;
  Sender_Slot_Ptr slot;

  slot = (Sender_Slot_Ptr) ptr;
  data = (Simulation_Run_Data_Ptr) simulation_run_data(simulation_run);

  slot->timer_active = 0;

  /* Timers are descheduled when a packet is ACKed, so this should not
     happen, but be safe. */
  if(slot->acked) return;

  TRACE(printf("%.3f: Timeout seq %ld\n",
	       simulation_run_get_time(simulation_run), slot->seq););

  data->timeouts++;
  slot->needs_retx = 1;

  sender_try_transmit(simulation_run);
}

/*******************************************************************************/

long int schedule_ack_reception_event(Simulation_Run_Ptr simulation_run, Time event_time, void * ack) {
  Event event;

  event.description = "ACK Reception";
  event.function = ack_reception_event;
  event.attachment = ack;

  return simulation_run_schedule_event(simulation_run, event, event_time);
}

/*******************************************************************************/

/*
 * Mark one packet in the send window as acknowledged and stop its timer.
 */

static void sender_mark_acked(Simulation_Run_Ptr simulation_run, Sender_Slot_Ptr slot) {
  if(slot->acked) return;

  slot->acked = 1;
  slot->needs_retx = 0;

  if(slot->timer_active) {
    simulation_run_deschedule_event(simulation_run, slot->timer_id);
    slot->timer_active = 0;
  }
}

/*******************************************************************************/

/*
 * An ACK has arrived at the sender. Mark the packets it covers as
 * acknowledged, slide the send window, and try to send more.
 */

void ack_reception_event(Simulation_Run_Ptr simulation_run, void * ptr) {
  Simulation_Run_Data_Ptr data;
  Sender_Ptr sender;
  Ack_Ptr ack;
  long int s, upper;

  ack = (Ack_Ptr) ptr;
  data = (Simulation_Run_Data_Ptr) simulation_run_data(simulation_run);
  sender = &(data->sender);

  /* A corrupted ACK is simply ignored. */
  if(ack->errored) {
    TRACE(printf("%.3f: ACK (seq %ld, next %ld) lost\n",
		 simulation_run_get_time(simulation_run),
		 ack->ack_seq, ack->next_expected););
    xfree((void *) ack);
    return;
  }

  TRACE(printf("%.3f: ACK received (seq %ld, next %ld)\n",
	       simulation_run_get_time(simulation_run),
	       ack->ack_seq, ack->next_expected););

  /* Selective ACK for one packet. */
  if(ack->ack_seq >= sender->base && ack->ack_seq < sender->next_seq) {
    sender_mark_acked(simulation_run, SENDER_SLOT(sender, ack->ack_seq));
  }

  /* Cumulative ACK: everything below next_expected was received. */
  upper = (ack->next_expected < sender->next_seq) ?
    ack->next_expected : sender->next_seq;

  for(s = sender->base; s < upper; s++) {
    sender_mark_acked(simulation_run, SENDER_SLOT(sender, s));
  }

  /* Slide the window past all ACKed packets at the front. */
  while(sender->base < sender->next_seq &&
	SENDER_SLOT(sender, sender->base)->acked) {
    sender->base++;
  }

  xfree((void *) ack);

  sender_try_transmit(simulation_run);
}

