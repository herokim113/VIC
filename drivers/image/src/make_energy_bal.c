/******************************************************************************
 * @section DESCRIPTION
 *
 * This routine makes an array of frozen soil data structures, one for each
 * vegetation type and bare soil.
 *
 * @section LICENSE
 *
 * The Variable Infiltration Capacity (VIC) macroscale hydrological model
 * Copyright (C) 2014 The Land Surface Hydrology Group, Department of Civil
 * and Environmental Engineering, University of Washington.
 *
 * The VIC model is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *****************************************************************************/

#include <vic_def.h>
#include <vic_run.h>
#include <vic_driver_image.h>

/******************************************************************************
 * @brief    This routine makes an array of frozen soil data structures, one
 *           for each vegetation type and bare soil.
 *****************************************************************************/
energy_bal_struct **
make_energy_bal(size_t nveg)
{
    extern option_struct options;

    size_t               i, j;
    energy_bal_struct  **temp = NULL;

    temp = (energy_bal_struct**) calloc(nveg,
                                        sizeof(energy_bal_struct*));
    if (temp == NULL) {
        nrerror("Memory allocation error in make_energy_bal().");
    }

    /** Initialize all records to unfrozen conditions */
    for (i = 0; i < nveg; i++) {
        temp[i] = (energy_bal_struct*) calloc(options.SNOW_BAND,
                                              sizeof(energy_bal_struct));
        if (temp[i] == NULL) {
            nrerror("Memory allocation error in make_energy_bal().");
        }
        for (j = 0; j < options.SNOW_BAND; j++) {
            temp[i][j].frozen = FALSE;
        }
    }

    return temp;
}