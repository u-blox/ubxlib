/*
 * Copyright 2019-2022 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of the MNO profile database.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_at_client.h"

#include "u_port.h"
#include "u_port_os.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell.h"         // Order is
#include "u_cell_net.h"     // important here
#include "u_cell_private.h" // don't change it

#include "u_cell_mno_db.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** MNO database structure.
 */
typedef struct {
    int32_t mnoProfile;      /** the MNO profile. */
    uint64_t featuresBitmap; /** a bit-map of features to apply, taken
                                 from #uCellMnoDbFeature_t. */
} uCellMnoDb_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** List of features versus MNO profile.
 */
static const uCellMnoDb_t gMnoDb[] = {
    {
        // VZW
        3, ((1ULL << (int32_t) U_CELL_MNO_DB_FEATURE_NO_CGDCONT) |
            (1ULL << (int32_t) U_CELL_MNO_DB_FEATURE_IGNORE_APN)
           )
    }
};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Determine if the current MNO profile has the given feature.
bool uCellMnoDbProfileHas(const uCellPrivateInstance_t *pInstance,
                          uCellMnoDbFeature_t feature)
{
    bool hasFeature = false;

    if ((pInstance != NULL) && (pInstance->mnoProfile >= 0)) {
        for (size_t x = 0; (x < sizeof(gMnoDb) / sizeof(gMnoDb[0])) && !hasFeature; x++) {
            hasFeature = (((gMnoDb[x].mnoProfile == pInstance->mnoProfile) &&
                           (gMnoDb[x].featuresBitmap & (1ULL << (int32_t) feature)) != 0));
        }
    }

    return hasFeature;
}

// End of file
