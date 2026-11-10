
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

/**********************************************************************/

#ifndef _MAIN_H_
#define _MAIN_H_

/**********************************************************************/

#include "simlib.h"
#include "simparameters.h"

/**********************************************************************/

typedef double Time;
typedef Fifoqueue_Ptr Buffer_Ptr;

/**********************************************************************/

/*
 * Sequence numbers are unbounded (long int), so there is no modulo
 * wrap-around to worry about. A packet with sequence number s is kept in
 * window slot (s % W).
 */

/*
 * One entry of the sender's window. It holds a packet that has been
 * transmitted at least once but not yet acknowledged.
 */

typedef struct _sender_slot_
{
  long int seq;
  Time arrive_time;        /* when the packet arrived at the source */
  int tx_count;            /* number of times this packet was transmitted */
  int acked;               /* 1 once an ACK covering this packet is received */
  int needs_retx;          /* 1 if the timer expired and a retransmission is due */
  int timer_active;        /* per-packet timer (Selective-Repeat only) */
  long int timer_id;       /* event id of the pending timeout event */
  Time last_tx_end;        /* end time of this packet's most recent transmission */
} Sender_Slot, * Sender_Slot_Ptr;

typedef struct _sender_
{
  Sender_Slot_Ptr slots;   /* SEND_WINDOW_SIZE slots */
  long int base;           /* oldest unacknowledged sequence number */
  long int next_seq;       /* next new sequence number to use */
  int link_busy;           /* 1 while a data packet is being transmitted */
  long int tx_seq;         /* sequence number on the link while link_busy */
  int gbn_timer_active;    /* Go-Back-N: single timer for the oldest */
  long int gbn_timer_id;   /*   unacknowledged packet (base) */
  Buffer_Ptr source_buffer;/* arrival times of waiting packets (Poisson source) */
} Sender, * Sender_Ptr;

/*
 * One entry of the receiver's window. It buffers a packet received
 * correctly but out of order.
 */

typedef struct _receiver_slot_
{
  int received;
  Time arrive_time;
} Receiver_Slot, * Receiver_Slot_Ptr;

typedef struct _receiver_
{
  Receiver_Slot_Ptr slots; /* RECEIVE_WINDOW_SIZE slots */
  long int r_next;         /* R_next: next in-order sequence number expected */
  int nak_sent;            /* SR: 1 once NAK(R_next) has been sent for the
			      current gap */
} Receiver, * Receiver_Ptr;

/*
 * A copy of a data packet on the forward link. A new Frame is created for
 * every (re)transmission and freed when it reaches the receiver.
 */

typedef struct _frame_
{
  long int seq;
  Time arrive_time;
  int errored;
} Frame, * Frame_Ptr;

/*
 * An ACK or NAK on the reverse link. Both carry R_next, the receiver's next
 * expected in-order sequence number, so both tell the sender that every
 * packet below R_next has been received. A NAK (Selective-Repeat only) also
 * asks the sender to resend packet R_next.
 */

typedef struct _ack_
{
  int nak;                 /* 0 = ACK, 1 = NAK */
  long int r_next;
  int errored;
} Ack, * Ack_Ptr;

/**********************************************************************/

typedef struct _simulation_run_data_
{
  Sender sender;
  Receiver receiver;
  Time reverse_link_free_time;     /* when the ACK channel is next free */

  long int blip_counter;
  long int arrival_count;          /* packet arrivals at the source */
  long int packets_delivered;      /* packets delivered in order at the destination */
  long int data_transmissions;     /* all data packet transmissions */
  long int retransmissions;        /* data transmissions that were retransmissions */
  long int timeouts;
  long int data_errors;            /* data packets received in error */
  long int ack_errors;             /* ACKs and NAKs received in error */
  long int acks_sent;              /* ACKs sent by the receiver */
  long int naks_sent;              /* NAKs sent by the receiver (SR only) */
  double accumulated_delay;        /* sum of (delivery time - arrival time) */
  unsigned random_seed;
} Simulation_Run_Data, * Simulation_Run_Data_Ptr;

/**********************************************************************/

/*
 * Function prototypes
 */

int
main(void);

/**********************************************************************/

#endif /* main.h */

