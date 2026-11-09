
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

#ifndef _OUTPUT_H_
#define _OUTPUT_H_

/**********************************************************************/

#include "main.h"

/**********************************************************************/

/*
 * Function prototypes
 */

void
output_blip_to_screen(Simulation_Run_Ptr);

void
output_results(Simulation_Run_Ptr);

/**********************************************************************/

#endif /* output.h */

