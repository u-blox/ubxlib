/*
 * Copyright 2019-2023 u-blox
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
 * @brief Test data for the Geofence API.
 */

#ifdef U_CFG_GEOFENCE

# ifdef U_CFG_OVERRIDE
#  include "u_cfg_override.h" // For a customer's configuration override
# endif

#include "limits.h"    // INT_MAX, INT_MIN
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_linked_list.h"

#include "u_geofence.h"
#include "u_geofence_test_data.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Wot it says.
 */
#define U_GEOFENCE_TEST_DATA_DISTANCE_GREATER_THAN_ONE_DEGREE_AT_EQUATOR_METRES 180000UL

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES: MISC
 * -------------------------------------------------------------- */

/** A vertex at 0, 0.
 */
static const uGeofenceTestVertex_t gVertexOrigin = {0LL /* latX1e9 */,
                                                    0LL /* lonX1e9 */
                                                   };

/** An empty circle.
 */
static const uGeofenceTestCircle_t gCircleEmpty = {&gVertexOrigin, 0 /* radius mm */};

/* ----------------------------------------------------------------
 * VARIABLES: FENCE A, MINIMAL CIRCLE
 * -------------------------------------------------------------- */

/** A circle at the origin with the smallest possible radius.
 */
static const uGeofenceTestCircle_t gCircleOriginMinRadius = {&gVertexOrigin, 1 /* radius mm */};

/** Fence A: no altitude limits containing just the minimal circle at the origin.
 */
static const uGeofenceTestFence_t gFenceA = {
    "A: simple circle, 1 mm radius, at origin",
    INT_MAX /* altitude max mm */,
    INT_MIN /* altitude min mm */,
    1, {&gCircleOriginMinRadius},
    0
};

/** 1: boringly certain test point at the origin with the outcomes
 * for fence A, which is that "inside" is true and everything else
 * false.
 */
static const uGeofenceTestPoint_t gTestPointFenceAOrigin = {
    &gVertexOrigin,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST)
};

/** 2: slightly less boring test point at the origin, this time with
 * uncertainty for fence A: an optimistic test returns true for both
 * "inside" and "outside" because of the uncertainty and "transit"
 * happens for the pessimist because of the uncertainty.
 */
static const uGeofenceTestPoint_t gTestPointFenceAOriginUncertain = {
    &gVertexOrigin,
    {2 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** Fence A test data containing the fence and the boring test points.
 */
static const uGeofenceTestData_t gFenceATestData = {
    &gFenceA,
    4 /* star radius mm */,
    2, // Update this if you add a test point
    {
        &gTestPointFenceAOrigin, &gTestPointFenceAOriginUncertain
    }
};

/* ----------------------------------------------------------------
 * VARIABLES: FENCE B, POLYGON SQUARE
 * -------------------------------------------------------------- */

/** A vertex at 1.0, -1.0.
 */
static const uGeofenceTestVertex_t gVertexOneUpperLeft = {1000000000LL /* latX1e9 */,
                                                          -1000000000LL /* lonX1e9 */
                                                         };

/** A vertex at 1.0, 1.0.
 */
static const uGeofenceTestVertex_t gVertexOneUpperRight = {1000000000LL /* latX1e9 */,
                                                           1000000000LL /* lonX1e9 */
                                                          };

/** A vertex at -1.0, 1.0.
 */
static const uGeofenceTestVertex_t gVertexOneLowerRight = {-1000000000LL /* latX1e9 */,
                                                           1000000000LL /* lonX1e9 */
                                                          };

/** A vertex at -1.0, -1.0.
 */
static const uGeofenceTestVertex_t gVertexOneLowerLeft = {-1000000000LL /* latX1e9 */,
                                                          -1000000000LL /* lonX1e9 */
                                                         };

/** A polygon: a square with vertices at gVertexOne.
 */
static const uGeofenceTestPolygon_t gPolygonSquareOne = {4,
    {&gVertexOneUpperLeft, &gVertexOneUpperRight, &gVertexOneLowerRight, &gVertexOneLowerLeft}
};

/** Fence B: no altitude limits containing the polygon "square one".
 */
static const uGeofenceTestFence_t gFenceB = {
    "B: simple polygon (\"square one\") at origin",
    INT_MAX /* altitude max mm */,
    INT_MIN /* altitude min mm */,
    0, {&gCircleEmpty},
    1, {&gPolygonSquareOne}
};

/** 1: what was the slightly boring test, now properly boring again.
 */
static const uGeofenceTestPoint_t gTestPointFenceBOriginUncertain = {
    &gVertexOrigin,
    {10000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST)
};

/** A vertex just inside -1.0, -1.0:   |*
 *                                     +---
 */
static const uGeofenceTestVertex_t gVertexOneLowerLeftInside = {-1000000000LL + 1 /* latX1e9 */,
                                                                -1000000000LL + 1 /* lonX1e9 */
                                                               };

/** 2: a test point for gVertexOneLowerLeftInside with absolute certainty
 * in Fence B (i.e. with polygon "square one").
 */
static const uGeofenceTestPoint_t gTestPointFenceBVertexOneLowerLeftInside = {
    &gVertexOneLowerLeftInside,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST)
};

/** 3: a position just inside -1.0, -1.0, on the ground, with enough uncertainty
 * to breach polygon "square one" on two sides.
 */
static const uGeofenceTestPoint_t gTestPointFenceBVertexOneLowerLeftInsideUncertain = {
    &gVertexOneLowerLeftInside,
    {10000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // Uncertainty makes the pessimist think we could have escaped
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** A vertex just inside -1.0, -1.0 and to the right:   | *
 *                                                      +---
 */
static const uGeofenceTestVertex_t gVertexOneLowerLeftInsideRight = {-1000000000LL + 1 /* latX1e9 */,
                                                                     -1000000000LL + 2 /* lonX1e9 */
                                                                    };

/** 4: a position just inside -1.0, -1.0 and to the right, on the ground, with
 * absolute certainty.
 */
static const uGeofenceTestPoint_t gTestPointFenceBVertexOneLowerLeftInsideRight = {
    &gVertexOneLowerLeftInsideRight,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // For the pessimist, we will have transitted back again
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 5: a position just inside -1.0, -1.0 and to the right, on the ground, with
 * enough uncertainty to breach polygon "square one" on the lower side only.
 */
static const uGeofenceTestPoint_t gTestPointFenceBVertexOneLowerLeftInsideRightUncertain = {
    &gVertexOneLowerLeftInsideRight,
    {100 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** A vertex just inside -1.0, -1.0 and to the top:   |*
 *                                                    |
 *                                                    +---
 */
static const uGeofenceTestVertex_t gVertexOneLowerLeftInsideUpper = {-1000000000LL + 2 /* latX1e9 */,
                                                                     -1000000000LL + 1 /* lonX1e9 */
                                                                    };

/** 6: a position just inside -1.0, -1.0 and to the top, on the ground,
 * with absolute certainty.
 */
static const uGeofenceTestPoint_t gTestPointFenceBVertexOneLowerLeftInsideUpper = {
    &gVertexOneLowerLeftInsideUpper,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // For the pessimist, we will have transitted back again
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 7: a position just inside -1.0, -1.0 and to the top, on the ground, with
 * enough uncertainty to breach polygon "square one" on the left side only.
 */
static const uGeofenceTestPoint_t gTestPointFenceBVertexOneLowerLeftInsideUpperUncertain = {
    &gVertexOneLowerLeftInsideUpper,
    {100 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** A vertex just outside -1.0, -1.0:    |
 *                                       +---
 *                                      *
 */
static const uGeofenceTestVertex_t gVertexOneLowerLeftOutside = {-1000000000LL - 1 /* latX1e9 */,
                                                                 -1000000000LL - 1 /* lonX1e9 */
                                                                };

/** 8: a position just outside -1.0, -1.0, on the ground, with absolute certainty.
 */
static const uGeofenceTestPoint_t gTestPointFenceBVertexOneLowerLeftOutside = {
    &gVertexOneLowerLeftOutside,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // This is a transit for optimists (pessimists already saw it coming)
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 9: a position just outside -1.0, -1.0, on the ground, with enough uncertainty
 * to breach polygon "square one" on two sides.
 */
static const uGeofenceTestPoint_t gTestPointFenceBVertexOneLowerLeftOutsideUncertain = {
    &gVertexOneLowerLeftOutside,
    {100 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // A pessimist would see a transit back inside again
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** A vertex just outside -1.0, -1.0 and to the right:   |
 *                                                       +---
 *                                                         *
 */
static const uGeofenceTestVertex_t gVertexOneLowerLeftOutsideRight = {-1000000000LL - 1 /* latX1e9 */,
                                                                      -1000000000LL + 1 /* lonX1e9 */
                                                                     };

/** 10: a position just outside -1.0, -1.0 and to the right, on the ground,
 * with absolute certainty.
 */
static const uGeofenceTestPoint_t gTestPointFenceBVertexOneLowerLeftOutsideRight = {
    &gVertexOneLowerLeftOutsideRight,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // The certainty brings the pessimist into agreement with the optimist
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 11: a position just outside -1.0, -1.0 and to the right, on the ground,
 * with enough uncertainty to breach polygon "square one" on the bottom only.
 */
static const uGeofenceTestPoint_t gTestPointFenceBVertexOneLowerLeftOutsideRightUncertain = {
    &gVertexOneLowerLeftOutsideRight,
    {100 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // A pessimist would see a transit back inside again
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** A vertex just outside -1.0, -1.0 and to the top:  *|
 *                                                     |
 *                                                     +---
 */
static const uGeofenceTestVertex_t gVertexOneLowerLeftOutsideUpper = {-1000000000LL + 2 /* latX1e9 */,
                                                                      -1000000000LL - 1 /* lonX1e9 */
                                                                     };

/** 12: a position just outside -1.0, -1.0 and to the bottom, on the ground,
 * with absolute certainty.
 */
static const uGeofenceTestPoint_t gTestPointFenceBVertexOneLowerLeftOutsideUpper = {
    &gVertexOneLowerLeftOutsideUpper,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // The certainty brings the pessimist into agreement with the optimist
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 13: a position just outside -1.0, -1.0 and to the bottom, on the ground,
 * with enough uncertainty to breach polygon "square one" on the side only.
 */
static const uGeofenceTestPoint_t gTestPointFenceBVertexOneLowerLeftOutsideUpperUncertain = {
    &gVertexOneLowerLeftOutsideUpper,
    {100 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // A pessimist would see a transit back inside again
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** Fence B test data, containing the fence, the boring test point and
 * the test points surrounding the lower left corner.
 */
static const uGeofenceTestData_t gFenceBTestData = {
    &gFenceB,
    100000LL /* star radius mm */,
    13, // Update this if you add a test point
    {
        &gTestPointFenceBOriginUncertain,
        &gTestPointFenceBVertexOneLowerLeftInside, &gTestPointFenceBVertexOneLowerLeftInsideUncertain,
        &gTestPointFenceBVertexOneLowerLeftInsideRight, &gTestPointFenceBVertexOneLowerLeftInsideRightUncertain,
        &gTestPointFenceBVertexOneLowerLeftInsideUpper, &gTestPointFenceBVertexOneLowerLeftInsideUpperUncertain,
        &gTestPointFenceBVertexOneLowerLeftOutside, &gTestPointFenceBVertexOneLowerLeftOutsideUncertain,
        &gTestPointFenceBVertexOneLowerLeftOutsideRight, &gTestPointFenceBVertexOneLowerLeftOutsideRightUncertain,
        &gTestPointFenceBVertexOneLowerLeftOutsideUpper, &gTestPointFenceBVertexOneLowerLeftOutsideUpperUncertain
    }
};

/* ----------------------------------------------------------------
 * VARIABLES: FENCES C, D & E, LONGITUDE WRAP
 * -------------------------------------------------------------- */

/** A vertex at 0, -179.999999999.
 */
static const uGeofenceTestVertex_t gVertexLongitudeWrap0 = {0LL /* latX1e9 */,
                                                            -179999999999LL /* lonX1e9 */
                                                           };

/** A circle at the longitude wrap, centred to the right of it,
 * with radius large enough to cross it.
 * ```
 *                 +179 | -179
 *                      ...
 *               ------. x .------ 0
 *                      ...
 *                      |
 * ```
 */
static const uGeofenceTestCircle_t gCircleLongitudeWrap0 = {&gVertexLongitudeWrap0,
                                                            10000 /* radius mm */
                                                           };

/** Fence C: no altitude limits containing the centred-right circle
 * at the longitude wrap.
 */
static const uGeofenceTestFence_t gFenceC = {
    "C: longitude wrap, circle centred right",
    INT_MAX /* altitude max mm */,
    INT_MIN /* altitude min mm */,
    1, {&gCircleLongitudeWrap0},
    0
};

/** A vertex at 0, +179.999999999.
 */
static const uGeofenceTestVertex_t gVertexLongitudeWrap1 = {0LL /* latX1e9 */,
                                                            179999999999LL /* lonX1e9 */
                                                           };

/** A circle at the longitude wrap, centred to the left of it,
 * with radius large enough to cross it.
 * ```
 *                   +179 | -179
 *                      ...
 *               ------. x .------ 0
 *                      ...
 *                        |
 * ```
 */
static const uGeofenceTestCircle_t gCircleLongitudeWrap1 = {&gVertexLongitudeWrap1,
                                                            10000 /* radius mm */
                                                           };

/** Fence D no altitude limits containing the centred-left circle
 * at the longitude wrap.
 */
static const uGeofenceTestFence_t gFenceD = {
    "D: longitude wrap, circle centred left",
    INT_MAX /* altitude max mm */,
    INT_MIN /* altitude min mm */,
    1, {&gCircleLongitudeWrap1},
    0
};

/** A vertex at -0.0000001, +179.999999999, which puts it inside the
 * circle (and the polygon which is added in Fence D) when rounding
 * errors are taken into account.
 */
static const uGeofenceTestVertex_t gVertexLongitudeWrap0Inside = {-100LL /* latX1e9 */,
                                                                  -179999999999LL /* lonX1e9 */
                                                                 };

/** A vertex at -0.0000001, +179.999999999, which puts it
 * inside the circle when rounding errors are taken into account.
 */
static const uGeofenceTestVertex_t gVertexLongitudeWrap1Inside = {-100LL /* latX1e9 */,
                                                                  179999999999LL /* lonX1e9 */
                                                                 };

/** 1: a test point for gVertexLongitudeWrap0Inside with absolute certainty
 * in Fences C, D and E, i.e. the ones with the circle and the polygon
 * at the longitude wrap in them.
 */
static const uGeofenceTestPoint_t gTestPointFenceCDEVertexLongitudeWrap0 = {
    &gVertexLongitudeWrap0Inside,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST)
};

/** 2: a test point for gVertexLongitudeWrap0Inside in Fences C, D and E with
 * sufficient uncertainty that we might be outside the circles and the
 * polygon.
 */
static const uGeofenceTestPoint_t gTestPointFenceCDEVertexLongitudeWrap0Uncertain = {
    &gVertexLongitudeWrap0Inside,
    {20000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // The uncertainty causes the pessimist to see a transit
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 3: a test point for gVertexLongitudeWrap1Inside with absolute certainty
 * in Fences C, D and E, i.e. the ones with the circles and the polygon
 * at the longitude wrap in them.
 */
static const uGeofenceTestPoint_t gTestPointFenceCDEVertexLongitudeWrap1 = {
    &gVertexLongitudeWrap1Inside,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // Certainty brings the pessimist into line with the optimist: we're inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 4: a test point for gVertexLongitudeWrap1 in Fences C, D and E, with
 * sufficient uncertainty that we might be outside the circles and the polygon.
 */
static const uGeofenceTestPoint_t gTestPointFenceCDEVertexLongitudeWrap1Uncertain = {
    &gVertexLongitudeWrap1Inside,
    {20000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // The uncertainty causes the pessimist to see a transit back to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

// There are further test points common to fences C, D and E below.

/** A vertex at 0, -179.999000000.
 */
static const uGeofenceTestVertex_t gVertexLongitudeWrap2 = {0LL /* latX1e9 */,
                                                            -179999000000LL /* lonX1e9 */
                                                           };

/** A vertex at 0, +179.999000000.
 */
static const uGeofenceTestVertex_t gVertexLongitudeWrap3 = {0LL /* latX1e9 */,
                                                            179999000000LL /* lonX1e9 */
                                                           };

/** A vertex at -0.1, +179.999999999.
 */
static const uGeofenceTestVertex_t gVertexLongitudeWrap4 = {-100000000LL /* latX1e9 */,
                                                            179999999999LL /* lonX1e9 */
                                                           };

/** A polygon: a triangle that crosses the longitude wrap.
 * ```
 *                   +179 | -179
 *                ------.....------ 0
 *                       . .
 *                        .   -0.1
 *                        |
 * ```
 */
static const uGeofenceTestPolygon_t gPolygonTriangleLongitudeWrap = {3,
    {&gVertexLongitudeWrap2, &gVertexLongitudeWrap3, &gVertexLongitudeWrap4}
};

/** Fence E: no altitude limits containing the polygon at the longitude wrap.
 */
static const uGeofenceTestFence_t gFenceE = {
    "E: longitude wrap, polygon (triangle)",
    INT_MAX /* altitude max mm */,
    INT_MIN /* altitude min mm */,
    0, {&gCircleEmpty},
    1, {&gPolygonTriangleLongitudeWrap}
};

/** A vertex at 0, -179.
 */
static const uGeofenceTestVertex_t gVertexLongitudeWrap5 = {0LL /* latX1e9 */,
                                                            -179000000000LL /* lonX1e9 */
                                                           };

/** A vertex at 0, +179.
 */
static const uGeofenceTestVertex_t gVertexLongitudeWrap6 = {0LL /* latX1e9 */,
                                                            179000000000LL /* lonX1e9 */
                                                           };

/** A vertex at -1.0000001, +179.999999999.
 */
static const uGeofenceTestVertex_t gVertexLongitudeWrap7 = {-1000000100LL /* latX1e9 */,
                                                            179999999999LL /* lonX1e9 */
                                                           };

/** 5: a test point for gVertexLongitudeWrap5, to the right
 * of the circles in Fences C/D and the polygon in Fence E,
 * and the wrap point, with absolute certainty.
 */
static const uGeofenceTestPoint_t gTestPointFenceCDEVertexLongitudeWrap5 = {
    &gVertexLongitudeWrap5,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // This is a transit for optimists (pessimists already saw it coming)
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 6: a test point for gVertexLongitudeWrap5 with huge
 * uncertainty, so big that it crosses the circles in Fences C/D,
 * the polygon in Fence E, and the longitude wrap.
 */
static const uGeofenceTestPoint_t gTestPointFenceCDEVertexLongitudeWrap5Uncertain = {
    &gVertexLongitudeWrap5,
    {
        U_GEOFENCE_TEST_DATA_DISTANCE_GREATER_THAN_ONE_DEGREE_AT_EQUATOR_METRES * 1000 /* radius mm */,
        0 /* altitude mm */, 0 /* altitude uncertainty mm */
    },
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // Uncertainty causes the pessimist to see a transit
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 7: as (6) but for gVertexLongitudeWrap6, so in the opposite direction.
 */
static const uGeofenceTestPoint_t gTestPointFenceCDEVertexLongitudeWrap6 = {
    &gVertexLongitudeWrap6,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // Certainty brings the optimist and the pessimist back into line
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 8: as (7) but for gVertexLongitudeWrap6, so in the opposite direction
 */
static const uGeofenceTestPoint_t gTestPointFenceCDEVertexLongitudeWrap6Uncertain = {
    &gVertexLongitudeWrap6,
    {
        U_GEOFENCE_TEST_DATA_DISTANCE_GREATER_THAN_ONE_DEGREE_AT_EQUATOR_METRES * 1000 /* radius mm */,
        0 /* altitude mm */, 0 /* altitude uncertainty mm */
    },
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // Uncertainty causes the pessimist to see a transit
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 9: a test point for gVertexLongitudeWrap7, on the
 * longitude wrap but definitely below the equator and outside
 * the circles of Fences C/D and the polygon of Fence E.
 */
static const uGeofenceTestPoint_t gTestPointFenceCDEVertexLongitudeWrap7 = {
    &gVertexLongitudeWrap7,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // Certainty causes the pessimist to see a transit back outside again
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 10: a test point for gVertexLongitudeWrap7, with huge
 * uncertainty, so big that it crosses the circles of Fences C/D
 * and the polygon of Fence E.
 */
static const uGeofenceTestPoint_t gTestPointFenceCDEVertexLongitudeWrap7Uncertain = {
    &gVertexLongitudeWrap7,
    {
        U_GEOFENCE_TEST_DATA_DISTANCE_GREATER_THAN_ONE_DEGREE_AT_EQUATOR_METRES * 1000 /* radius mm */,
        0 /* altitude mm */, 0 /* altitude uncertainty mm */
    },
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // Uncertainty causes the pessimist to see a transit back inside again
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 11: a test point for gVertexLongitudeWrap0Inside with absolute certainty
 * in Fence E, just to the right of the wrap-point and inside the polygon.
 */
static const uGeofenceTestPoint_t gTestPointFenceEVertexLongitudeWrap0 = {
    &gVertexLongitudeWrap0Inside,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // This is a transit for optimists (pessimists already saw it coming)
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 12: a test point for gVertexLongitudeWrap0Inside in Fence E with sufficient
 * uncertainty that we might be outside the polygon.
 */
static const uGeofenceTestPoint_t gTestPointFenceEVertexLongitudeWrap0Uncertain = {
    &gVertexLongitudeWrap0Inside,
    {
        U_GEOFENCE_TEST_DATA_DISTANCE_GREATER_THAN_ONE_DEGREE_AT_EQUATOR_METRES * 1000 /* radius mm */,
        0 /* altitude mm */, 0 /* altitude uncertainty mm */
    },
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // Uncertainty causes the pessimist to see a transit back to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 13: a test point for gVertexLongitudeWrap1Inside with absolute certainty
 * in Fence E, this time just to the left of the wrap-point and inside the polygon.
 */
static const uGeofenceTestPoint_t gTestPointFenceEVertexLongitudeWrap1 = {
    &gVertexLongitudeWrap1Inside,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // Certainty brings the pessimist back into line with the optimist: we're inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 14: a test point for gVertexLongitudeWrap1Inside in Fence E with sufficient
 * uncertainty that we might be outside the polygon.
 */
static const uGeofenceTestPoint_t gTestPointFenceEVertexLongitudeWrap1Uncertain = {
    &gVertexLongitudeWrap1Inside,
    {
        U_GEOFENCE_TEST_DATA_DISTANCE_GREATER_THAN_ONE_DEGREE_AT_EQUATOR_METRES * 1000 /* radius mm */,
        0 /* altitude mm */, 0 /* altitude uncertainty mm */
    },
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // Uncertainty causes the pessimist to see a transit: we're outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

// There are further test points for fence E below.

/** A vertex at -0.05, +179.999999990.
 */
static const uGeofenceTestVertex_t gVertexLongitudeWrap8 = {-50000000LL /* latX1e9 */,
                                                            179999999990LL /* lonX1e9 */
                                                           };

/** A vertex at -0.05, +179.990000000.
 */
static const uGeofenceTestVertex_t gVertexLongitudeWrap9 = {-50000000LL /* latX1e9 */,
                                                            179990000000LL /* lonX1e9 */
                                                           };

/** A vertex at -0.05, -179.990000000.
 */
static const uGeofenceTestVertex_t gVertexLongitudeWrap10 = {-50000000LL /* latX1e9 */,
                                                             -179990000000LL /* lonX1e9 */
                                                            };

/** A vertex at -0.05, -179.999999990.
 */
static const uGeofenceTestVertex_t gVertexLongitudeWrap11 = {-50000000LL /* latX1e9 */,
                                                             -179999999990LL /* lonX1e9 */
                                                            };

/** 15: a test point for gVertexLongitudeWrap8, firmly inside the left-hand
 * side of the polygon, to check that the spherical maths works out correctly
 * for the case:
 *
 *    .
 *      .
 *        .  x <- inside
 *          .
 *            .
 *
 * ...in Fence E (not actually to do with longitude wrap, just convenient
 * to do the check here).
 */
static const uGeofenceTestPoint_t gTestPointFenceEVertexLongitudeWrap8 = {
    &gVertexLongitudeWrap8,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // The pessimist sees a transit back inside again
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 16: a test point for gVertexLongitudeWrap9, firmly outside the left-hand
 * side of the polygon, to check that the spherical maths works out correctly
 * for the case:
 *
 *    .
 *      .
 *        .
 *      x   .
 *      ^     .
 *      |       .
 *    outside     .
 *
 * ...in Fence E (not actually to do with longitude wrap, just convenient
 * to do the check here).
 */
static const uGeofenceTestPoint_t gTestPointFenceEVertexLongitudeWrap9 = {
    &gVertexLongitudeWrap9,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // Pessimist and optimist agree we're outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 17: a test point for gVertexLongitudeWrap10, firmly outside the left-hand
 * side of the polygon, to check that the spherical maths works out correctly
 * for the case:
 *
 *            .
 *          .
 *        .  x <- outside
 *      .
 *
 * ...in Fence E (not actually to do with longitude wrap, just convenient
 * to do the check here).
 */
static const uGeofenceTestPoint_t gTestPointFenceEVertexLongitudeWrap10 = {
    &gVertexLongitudeWrap10,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST)
};

/** 18: a test point for gVertexLongitudeWrap11, firmly inside the left-hand
 * side of the polygon, to check that the spherical maths works out correctly
 * for the case:
 *
 *    inside    .
 *      |     .
 *      v   .
 *      x .
 *      .
 *    .
 *  .
 *
 * ...in Fence E (not actually to do with longitude wrap, just convenient
 * to do the check here).
 */
static const uGeofenceTestPoint_t gTestPointFenceEVertexLongitudeWrap11 = {
    &gVertexLongitudeWrap11,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // Pessimist and optimist agree we're inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** Fence C test data, containing the fence and test points.
 */
static const uGeofenceTestData_t gFenceCTestData = {
    &gFenceC,
    40000LL /* star radius mm */,
    10, // Update this if you add a test point
    {
        &gTestPointFenceCDEVertexLongitudeWrap0, &gTestPointFenceCDEVertexLongitudeWrap0Uncertain,
        &gTestPointFenceCDEVertexLongitudeWrap1, &gTestPointFenceCDEVertexLongitudeWrap1Uncertain,
        &gTestPointFenceCDEVertexLongitudeWrap5, &gTestPointFenceCDEVertexLongitudeWrap5Uncertain,
        &gTestPointFenceCDEVertexLongitudeWrap6, &gTestPointFenceCDEVertexLongitudeWrap6Uncertain,
        &gTestPointFenceCDEVertexLongitudeWrap7, &gTestPointFenceCDEVertexLongitudeWrap7Uncertain
    }
};

/** Fence D test data, containing the fence and test points.
 */
static const uGeofenceTestData_t gFenceDTestData = {
    &gFenceD,
    40000LL /* star radius mm */,
    10, // Update this if you add a test point
    {
        &gTestPointFenceCDEVertexLongitudeWrap0, &gTestPointFenceCDEVertexLongitudeWrap0Uncertain,
        &gTestPointFenceCDEVertexLongitudeWrap1, &gTestPointFenceCDEVertexLongitudeWrap1Uncertain,
        &gTestPointFenceCDEVertexLongitudeWrap5, &gTestPointFenceCDEVertexLongitudeWrap5Uncertain,
        &gTestPointFenceCDEVertexLongitudeWrap6, &gTestPointFenceCDEVertexLongitudeWrap6Uncertain,
        &gTestPointFenceCDEVertexLongitudeWrap7, &gTestPointFenceCDEVertexLongitudeWrap7Uncertain
    }
};

/** Fence E test data, containing the fence and test points.
 */
static const uGeofenceTestData_t gFenceETestData = {
    &gFenceE,
    500000LL /* star radius mm */,
    18, // Update this if you add a test point
    {
        &gTestPointFenceCDEVertexLongitudeWrap0, &gTestPointFenceCDEVertexLongitudeWrap0Uncertain,
        &gTestPointFenceCDEVertexLongitudeWrap1, &gTestPointFenceCDEVertexLongitudeWrap1Uncertain,
        &gTestPointFenceCDEVertexLongitudeWrap5, &gTestPointFenceCDEVertexLongitudeWrap5Uncertain,
        &gTestPointFenceCDEVertexLongitudeWrap6, &gTestPointFenceCDEVertexLongitudeWrap6Uncertain,
        &gTestPointFenceCDEVertexLongitudeWrap7, &gTestPointFenceCDEVertexLongitudeWrap7Uncertain,
        &gTestPointFenceEVertexLongitudeWrap0, &gTestPointFenceEVertexLongitudeWrap0Uncertain,
        &gTestPointFenceEVertexLongitudeWrap1, &gTestPointFenceEVertexLongitudeWrap1Uncertain,
        &gTestPointFenceEVertexLongitudeWrap8, &gTestPointFenceEVertexLongitudeWrap9,
        &gTestPointFenceEVertexLongitudeWrap10, &gTestPointFenceEVertexLongitudeWrap11
    }
};

/* ----------------------------------------------------------------
 * VARIABLES: FENCES F, G AND H, WITH ALTITUDE
 * -------------------------------------------------------------- */

/** A vertex at the centre of the Eiffel tower.
 */
static const uGeofenceTestVertex_t gVertexEiffelTower = {48858184487LL /* latX1e9 */,
                                                         2294538652LL /* lonX1e9 */
                                                        };

/** A circle with the radius of the Eiffel Tower.
 */
static const uGeofenceTestCircle_t gCircleEiffelTower = {&gVertexEiffelTower,
                                                         90000 /* radius mm */
                                                        };

/** Fence F: the Eiffel tower viewing deck and above.
 */
static const uGeofenceTestFence_t gFenceF = {
    "F: altitude, Eiffel tower viewing floor",
    INT_MAX /* altitude max mm */,
    276000 /* altitude min mm */,
    1, {&gCircleEiffelTower},
    0
};

/** 1: on the ground, underneath the Eiffel tower, with a little
 * uncertainty for realism.
 */
static const uGeofenceTestPoint_t gTestPointFenceFGround = {
    &gVertexEiffelTower,
    {50000 /* radius mm */, 0 /* altitude mm */, 10000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST)
};

/** 2: in the lift on the way up, approaching the viewing deck.
 */
static const uGeofenceTestPoint_t gTestPointFenceFLift = {
    &gVertexEiffelTower,
    {50000 /* radius mm */, 265999 /* altitude mm */, 10000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST)
};

/** 3: as (2) but with greater uncertainty.
 */
static const uGeofenceTestPoint_t gTestPointFenceFLiftUncertain = {
    &gVertexEiffelTower,
    {50000 /* radius mm */, 265999 /* altitude mm */, 10001 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 4: on the viewing deck.
 */
static const uGeofenceTestPoint_t gTestPointFenceFViewing = {
    &gVertexEiffelTower,
    {50000 /* radius mm */, 276000 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // The optimist now sees the transit
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 5: as (4) but with uncertainty.
 */
static const uGeofenceTestPoint_t gTestPointFenceFViewingUncertain = {
    &gVertexEiffelTower,
    {50000 /* radius mm */, 276000 /* altitude mm */, 1 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit back
    // outside again
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 6: hanging from the antenna.
 */
static const uGeofenceTestPoint_t gTestPointFenceFAntenna = {
    &gVertexEiffelTower,
    {1000 /* radius mm */, 330000 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // Certainty brings the pessimist into agreement with the
    // optimist: we're inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 7: as (6) but with enough uncertainty that we may actually
 * be below the viewing deck.
 */
static const uGeofenceTestPoint_t gTestPointFenceFAntennaUncertain = {
    &gVertexEiffelTower,
    {1000 /* radius mm */, 330000 /* altitude mm */, 54001 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit back
    // outside again
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** Fence F test data containing the fence and our journey up the tower.
 */
static const uGeofenceTestData_t gFenceFTestData = {
    &gFenceF,
    200000LL /* star radius mm */,
    7, // Update this if you add a test point
    {
        &gTestPointFenceFGround,
        &gTestPointFenceFLift, &gTestPointFenceFLiftUncertain,
        &gTestPointFenceFViewing, &gTestPointFenceFViewingUncertain,
        &gTestPointFenceFAntenna, &gTestPointFenceFAntennaUncertain
    }
};

/** A vertex at the centre of Taipei 101.
 */
static const uGeofenceTestVertex_t gVertexTaipei101 = {25033669229LL /* latX1e9 */,
                                                       121564815473LL /* lonX1e9 */
                                                      };

/** A vertex at a corner of Taipei 101.
 */
static const uGeofenceTestVertex_t gVertexTaipei1010 = {25034093476LL /* latX1e9 */,
                                                        121564296212LL /* lonX1e9 */
                                                       };

/** A vertex at a corner of Taipei 101.
 */
static const uGeofenceTestVertex_t gVertexTaipei1011 = {25034134973LL /* latX1e9 */,
                                                        121565378020LL /* lonX1e9 */
                                                       };

/** A vertex at a corner of Taipei 101.
 */
static const uGeofenceTestVertex_t gVertexTaipei1012 = {25033087232LL /* latX1e9 */,
                                                        121565366797LL /* lonX1e9 */
                                                       };

/** A vertex at a corner of Taipei 101.
 */
static const uGeofenceTestVertex_t gVertexTaipei1013 = {25033111589LL /* latX1e9 */,
                                                        121564151685LL /* lonX1e9 */
                                                       };

/** A polygon that marks the footprint of Taipei 101.
 */
static const uGeofenceTestPolygon_t gPolygonTaipei101 = {4,
    {&gVertexTaipei1010, &gVertexTaipei1011, &gVertexTaipei1012, &gVertexTaipei1013}
};

/** Fence G: the restaurant on the 85th floor of Taipei 101.
 */
static const uGeofenceTestFence_t gFenceG = {
    "G: altitude, Taipei 101, restaurant on 85th floor",
    371000 /* altitude max mm */,
    365000 /* altitude min mm */,
    0, {&gCircleEmpty},
    1, {&gPolygonTaipei101}
};

/** 1: in the shopping centre down below.
 */
static const uGeofenceTestPoint_t gTestPointFenceGGround = {
    &gVertexTaipei101,
    {100000 /* radius mm */, 0 /* altitude mm */, 10000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST)
};

/** 2: on the 84th floor with reasonable certainty.
 */
static const uGeofenceTestPoint_t gTestPointFenceGFloor840 = {
    &gVertexTaipei101,
    {10000 /* radius mm */, 363999 /* altitude mm */, 1000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST)
};

/** 3: as (2) but with enough altitude uncertainty that we might
 * be in the restaurant.
 */
static const uGeofenceTestPoint_t gTestPointFenceGFloor841 = {
    &gVertexTaipei101,
    {10000 /* radius mm */, 363999 /* altitude mm */, 1001 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // The pessimist would see lunch
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 4: on the 85th floor, having rather a good lunch.
 */
static const uGeofenceTestPoint_t gTestPointFenceGFloor850 = {
    &gVertexTaipei101,
    {10000 /* radius mm */, 366000 /* altitude mm */, 1000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // The optimist now sees the transit, and lunch, finally
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 5: as (4) but with enough altitude uncertainty that we might
 * be on the floor below.
 */
static const uGeofenceTestPoint_t gTestPointFenceGFloor851 = {
    &gVertexTaipei101,
    {10000 /* radius mm */, 366000 /* altitude mm */, 1001 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit
    // back outside again
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 6: as (4) but with enough altitude uncertainty that we might
 * be on the floor above.
 */
static const uGeofenceTestPoint_t gTestPointFenceGFloor852 = {
    &gVertexTaipei101,
    {10000 /* radius mm */, 366000 /* altitude mm */, 5001 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit
    // back inside again
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 7: as (4) but with sufficient horizontal uncertainty that
 * we might be on a drone flying outside the building; with
 * our lunch of course.
 */
static const uGeofenceTestPoint_t gTestPointFenceGFloor853 = {
    &gVertexTaipei101,
    {150000 /* radius mm */, 366000 /* altitude mm */, 1000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit
    // back outside again
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 8: on the 86th floor now, must have got lost.
 */
static const uGeofenceTestPoint_t gTestPointFenceGFloor860 = {
    &gVertexTaipei101,
    {10000 /* radius mm */, 372000 /* altitude mm */, 999 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // The optimist has left their lunch behind, all agree we
    // are outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 9: as (8) but with enough altitude uncertainty that we might
 * actually be in the restaurant after all.
 */
static const uGeofenceTestPoint_t gTestPointFenceGFloor861 = {
    &gVertexTaipei101,
    {10000 /* radius mm */, 372000 /* altitude mm */, 1000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit
    // back to dessert
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** Fence G test data: going for lunch at the restaurant atop Taipei 101.
 */
static const uGeofenceTestData_t gFenceGTestData = {
    &gFenceG,
    100000LL /* star radius mm */,
    9, // Update this if you add a test point
    {
        &gTestPointFenceGGround,
        &gTestPointFenceGFloor840, &gTestPointFenceGFloor841,
        &gTestPointFenceGFloor850, &gTestPointFenceGFloor851, &gTestPointFenceGFloor852, &gTestPointFenceGFloor853,
        &gTestPointFenceGFloor860, &gTestPointFenceGFloor861
    }
};

/** A vertex at the centre of The Lowest Bar In The World.
 */
static const uGeofenceTestVertex_t gVertexTLBITW = {31762113083 /* latX1e9 */,
                                                    35503912404 /* lonX1e9 */
                                                   };

/** A circle with the approximate radius of The Lowest Bar In The World.
 */
static const uGeofenceTestCircle_t gCircleTLBITW = {&gVertexTLBITW,
                                                    15000 /* radius mm */
                                                   };

/** Fence G: The Lowest Bar In The World, on the shore of the
 * Dead Sea.
 */
static const uGeofenceTestFence_t gFenceH = {
    "H: altitude, The Lowest Bar In The World, Dead Sea",
    -393000 /* altitude max mm */,
    -408000 /* altitude min mm */,
    1, {&gCircleTLBITW},
    0
};

/** 1: sitting at the bar in The Lowest Bar In The World.
 */
static const uGeofenceTestPoint_t gTestPointFenceHTLBITW = {
    &gVertexTLBITW,
    {1000 /* radius mm */, -407000 /* altitude mm */, 1000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST)
};

/** 2: changing a barrel in the cellar of The Lowest Bar In The World.
 */
static const uGeofenceTestPoint_t gTestPointFenceHTLBITWCellar = {
    &gVertexTLBITW,
    {1000 /* radius mm */, -409000 /* altitude mm */, 999 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // Both optimist and pessimist see a transit outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 3: half way up the stairs from the cellar.
 */
static const uGeofenceTestPoint_t gTestPointFenceHTLBITWStairs = {
    &gVertexTLBITW,
    {1000 /* radius mm */, -408500 /* altitude mm */, 500 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit
    // back inside again
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 4: back in the bar again but with bad [horizontal] GNSS
 * reception.
 */
static const uGeofenceTestPoint_t gTestPointFenceHTLBITWBarProbably = {
    &gVertexTLBITW,
    {20000 /* radius mm */, -407000 /* altitude mm */, 1000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // The pessimist thinks we probably wandered outside,
    // the optimist's view is unchanged: we're still half
    // way up the stairs from the cellar as far as they are
    // concerned, no reason to think there's been a transt
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 5: walking on the roof of the bar, where the GNSS reception is better.
 */
static const uGeofenceTestPoint_t gTestPointFenceHTLBITWRoof = {
    &gVertexTLBITW,
    {1000 /* radius mm */, -391999 /* altitude mm */, 1000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST)
    // Note: the optimist had no evidence to think we'd made
    // a transit to inside before and still doesn't now, we're
    // still oustide, and since we can't be inside the pessimist
    // agrees
};

/** 6: falling through the roof, back to that seat at the bar.
 */
static const uGeofenceTestPoint_t gTestPointFenceHTLBITWCeiling = {
    &gVertexTLBITW,
    {1000 /* radius mm */, -392000 /* altitude mm */, 1000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 7: missed it: collapsed on the floor of The Lowest Bar In The World.
 */
static const uGeofenceTestPoint_t gTestPointFenceHTLBITWFloor = {
    &gVertexTLBITW,
    {1000 /* radius mm */, -408000 /* altitude mm */, 1000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit
    // back outside the bar, down to the cellar
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
    // Note: the optimist still has no reason to see a transit
    // since the uncertainy means we still could be outside
    // the bar; in the cellar, admittedly, but still oustide.

};

/** Fence H test data: a cocktail in The Lowest Bar In The World, on the
 * shore of the Dead Sea.
 */
static const uGeofenceTestData_t gFenceHTestData = {
    &gFenceH,
    50000LL /* star radius mm */,
    7, // Update this if you add a test point
    {
        &gTestPointFenceHTLBITW, &gTestPointFenceHTLBITWCellar, &gTestPointFenceHTLBITWStairs,
        &gTestPointFenceHTLBITWBarProbably, &gTestPointFenceHTLBITWRoof, &gTestPointFenceHTLBITWCeiling,
        &gTestPointFenceHTLBITWFloor
    }
};

/* ----------------------------------------------------------------
 * VARIABLES: FENCES I, J, K AND L, THE POLES
 * -------------------------------------------------------------- */

/** A vertex at the north pole.
 */
static const uGeofenceTestVertex_t gVertexNorthPole = {89999999999LL /* latX1e9 */,
                                                       0LL /* lonX1e9 */
                                                      };

/** A circle with radius less than 10 degrees longitude,
 * centred at the north pole, assuming one degree is 111 km.
 */
static const uGeofenceTestCircle_t gCircleNorthInsidePolarZone = {&gVertexNorthPole,
                                                                  1100000000UL /* radius mm */
                                                                 };

/** Fence I: a circle just inside the polar danger zone, north.
 */
static const uGeofenceTestFence_t gFenceI = {
    "I: polar, north, circle",
    0 /* altitude max mm */,
    0 /* altitude min mm */,
    1, {&gCircleNorthInsidePolarZone},
    0
};

/** A vertex just outside the polar danger zone, north.
 */
static const uGeofenceTestVertex_t gVertexNorthPoleOutside = {79999999999LL /* latX1e9 */,
                                                              0LL /* lonX1e9 */
                                                             };

/** A vertex just inside the polar danger zone, north.
 */
static const uGeofenceTestVertex_t gVertexNorthPoleInside = {81000000000LL /* latX1e9 */,
                                                             0LL /* lonX1e9 */
                                                            };

/** 1: just outside the northern polar danger zone, avoiding a square-extent
 * elimination, we want to test the whole-hog here.
 */
static const uGeofenceTestPoint_t gTestPointFenceIOutside = {
    &gVertexNorthPoleOutside,
    {
        U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES * 1000 /* radius mm */,
        0 /* altitude mm */, 0 /* altitude uncertainty mm */
    },
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST)
};

/** 2: as (1) but with sufficient uncertainty that we might be inside.
 */
static const uGeofenceTestPoint_t gTestPointFenceIOutsideUncertain = {
    &gVertexNorthPoleOutside,
    {120000000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 3: just inside the northern polar danger zone, with certainty,
 * but still avoiding a square-extent elimination.
 */
static const uGeofenceTestPoint_t gTestPointFenceIInside = {
    &gVertexNorthPoleInside,
    {
        U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES * 1000 /* radius mm */,
        0 /* altitude mm */, 0 /* altitude uncertainty mm */
    },
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // The optimist now sees the transit
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 4: inside the northern polar danger zone, but with sufficient
 * uncertainty that we might be outside it.
 */
static const uGeofenceTestPoint_t gTestPointFenceIInsideUncertain = {
    &gVertexNorthPoleInside,
    {120000000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** Fence I test data: on the edge of the danger zone at the north pole.
 */
static const uGeofenceTestData_t gFenceITestData = {
    &gFenceI,
    2000000000LL /* star radius mm */,
    4, // Update this if you add a test point
    {
        &gTestPointFenceIOutside, &gTestPointFenceIOutsideUncertain,
        &gTestPointFenceIInside, &gTestPointFenceIInsideUncertain,
    }
};

/** A vertex at the corner of a square centred on the south pole
 * of square extent less than 10 degrees.
 */
static const uGeofenceTestVertex_t gVertexSouth0 = {-80000000000LL /* latX1e9 */,
                                                    0LL /* lonX1e9 */
                                                   };

/** A vertex at the corner of a square centred on the south pole
 * of square extent less than 10 degrees.
 */
static const uGeofenceTestVertex_t gVertexSouth1 = {-80000000000LL /* latX1e9 */,
                                                    90000000000LL /* lonX1e9 */
                                                   };

/** A vertex at the corner of a square centred on the south pole
 * of square extent less than 10 degrees.
 */
static const uGeofenceTestVertex_t gVertexSouth2 = {-80000000000LL /* latX1e9 */,
                                                    179999999999LL /* lonX1e9 */
                                                   };

/** A vertex at the corner of a square centred on the south pole
 * of square extent less than 10 degrees.
 */
static const uGeofenceTestVertex_t gVertexSouth3 = {-80000000000LL /* latX1e9 */,
                                                    -90000000000LL /* lonX1e9 */
                                                   };

/** A polygon (square) who's vertices are less than 10 degrees
 * from the south pole.
 */
static const uGeofenceTestPolygon_t gPolygonSouthInsidePolarZone = {4,
    {&gVertexSouth0, &gVertexSouth1, &gVertexSouth2, &gVertexSouth3}
};

/** Fence J: a square that is inside the polar danger zone, south.
 */
static const uGeofenceTestFence_t gFenceJ = {
    "J: polar, south, square",
    0 /* altitude max mm */,
    0 /* altitude min mm */,
    0, {&gCircleEmpty},
    1, {&gPolygonSouthInsidePolarZone}
};

/** A vertex just outside the polar danger zone, south, leaving
 * room for a 100 m radius of position.
 */
static const uGeofenceTestVertex_t gVertexSouthPoleOutside = {-79500000000LL /* latX1e9 */,
                                                              0LL /* lonX1e9 */
                                                             };

/** A vertex just inside the polar danger zone, south, leaving
 * room for a 100 m radius of position.
 */
static const uGeofenceTestVertex_t gVertexSouthPoleInside = {-80500000000LL /* latX1e9 */,
                                                             0LL /* lonX1e9 */
                                                            };

/** 1: just outside the southern polar danger zone, avoiding a square-extent
 * elimination, we want to test the whole-hog here.
 */
static const uGeofenceTestPoint_t gTestPointFenceJOutside = {
    &gVertexSouthPoleOutside,
    {
        U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES * 1000 /* radius mm */,
        0 /* altitude mm */, 0 /* altitude uncertainty mm */
    },
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 2: as (1) but with sufficient uncertainty that we might be inside.
 */
static const uGeofenceTestPoint_t gTestPointFenceJOutsideUncertain = {
    &gVertexSouthPoleOutside,
    {100000000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 3: just inside the southern polar danger zone, with certainty,
 * but still avoiding a square-extent elimination.
 */
static const uGeofenceTestPoint_t gTestPointFenceJInside = {
    &gVertexSouthPoleInside,
    {
        U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES * 1000 /* radius mm */,
        0 /* altitude mm */, 0 /* altitude uncertainty mm */
    },
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // The optimist now sees the transit
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 4: inside the southern polar danger zone, but with sufficient
 * uncertainty that we might be outside it.
 */
static const uGeofenceTestPoint_t gTestPointFenceJInsideUncertain = {
    &gVertexSouthPoleInside,
    {100000000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** Fence J test data: in the edge of the danger zone at the south pole.
 */
static const uGeofenceTestData_t gFenceJTestData = {
    &gFenceJ,
    3000000000LL /* star radius mm */,
    4, // Update this if you add a test point
    {
        &gTestPointFenceJOutside, &gTestPointFenceJOutsideUncertain,
        &gTestPointFenceJInside, &gTestPointFenceJInsideUncertain,
    }
};

/** A vertex at the corner of a polygon that covers Rudolph Island,
 * in Arkhangelsk Oblast, Russia, near the north pole.
 */
static const uGeofenceTestVertex_t gVertexRudolphIsland0 = {81819361344LL /* latX1e9 */,
                                                            57885486016LL /* lonX1e9 */
                                                           };

/** Another vertex of the polygon that covers Rudolph Island, near
 * the north pole.
 */
static const uGeofenceTestVertex_t gVertexRudolphIsland1 = {81885183200LL /* latX1e9 */,
                                                            59399353500LL /* lonX1e9 */
                                                           };

/** Another vertex of the polygon that covers Rudolph Island, near
 * the north pole.
 */
static const uGeofenceTestVertex_t gVertexRudolphIsland2 = {81731809200LL /* latX1e9 */,
                                                            59377481713LL /* lonX1e9 */
                                                           };

/** Another vertex of the polygon that covers Rudolph Island, near
 * the north pole.
 */
static const uGeofenceTestVertex_t gVertexRudolphIsland3 = {81677494825LL /* latX1e9 */,
                                                            58160327329LL /* lonX1e9 */
                                                           };

/** Another vertex of the polygon that covers Rudolph Island, near
 * the north pole.
 */
static const uGeofenceTestVertex_t gVertexRudolphIsland4 = {81731809200LL /* latX1e9 */,
                                                            57857770926LL /* lonX1e9 */
                                                           };

/** Another vertex of the polygon that covers Rudolph Island, near
 * the north pole.
 */
static const uGeofenceTestVertex_t gVertexRudolphIsland5 = {81770247845LL /* latX1e9 */,
                                                            57996346377LL /* lonX1e9 */
                                                           };

/** The last vertex of the polygon that covers Rudolph Island, near
 * the north pole.
 */
static const uGeofenceTestVertex_t gVertexRudolphIsland6 = {81798128100LL /* latX1e9 */,
                                                            57880834909LL /* lonX1e9 */
                                                           };

/** A polygon that surrounds Rudolph Island, in Arkhangelsk Oblast,
 * Russia, near the north pole.
 */
static const uGeofenceTestPolygon_t gPolygonRudolphIsland = {7,
    {
        &gVertexRudolphIsland0, &gVertexRudolphIsland1, &gVertexRudolphIsland2,
        &gVertexRudolphIsland3, &gVertexRudolphIsland4, &gVertexRudolphIsland5,
        &gVertexRudolphIsland6
    }
};

/** Fence K: a polygon that surrounds Rudolph Island, in Arkhangelsk
 * Oblast, Russia, near the north pole.
 */
static const uGeofenceTestFence_t gFenceK = {
    "K: polar, north, Rudolph Island",
    0 /* altitude max mm */,
    0 /* altitude min mm */,
    0, {&gCircleEmpty},
    1, {&gPolygonRudolphIsland}
};

/** A vertex to the north of Rudolph Island, outside its surrounding polygon.
 */
static const uGeofenceTestVertex_t gVertexRudolphIslandNorthOutside = {81851536800LL /* latX1e9 */,
                                                                       58534731400LL /* lonX1e9 */
                                                                      };

/** A vertex to the north of Rudolph Island, inside its surrounding polygon.
 */
static const uGeofenceTestVertex_t gVertexRudolphIslandNorthInside = {81846281000LL /* latX1e9 */,
                                                                      58545427900LL /* lonX1e9 */
                                                                     };

/** A vertex to the east of Rudolph Island, outside its surrounding polygon.
 */
static const uGeofenceTestVertex_t gVertexRudolphIslandEastOutside = {81785238800LL /* latX1e9 */,
                                                                      59393870700LL /* lonX1e9 */
                                                                     };

/** A vertex to the east of Rudolph Island, inside its surrounding polygon.
 */
static const uGeofenceTestVertex_t gVertexRudolphIslandEastInside = {81784885400LL /* latX1e9 */,
                                                                     59369355500LL /* lonX1e9 */
                                                                    };

/** A vertex to the south of Rudolph Island, outside its surrounding polygon.
 */
static const uGeofenceTestVertex_t gVertexRudolphIslandSouthOutside = {81700988100LL /* latX1e9 */,
                                                                       58755746100LL /* lonX1e9 */
                                                                      };

/** A vertex to the south of Rudolph Island, inside its surrounding polygon.
 */
static const uGeofenceTestVertex_t gVertexRudolphIslandSouthInside = {81705808300LL /* latX1e9 */,
                                                                      58741164000LL /* lonX1e9 */
                                                                     };

/** A vertex to the west of Rudolph Island, outside its surrounding polygon.
 */
static const uGeofenceTestVertex_t gVertexRudolphIslandWestOutside = {81770178800LL /* latX1e9 */,
                                                                      57987787800LL /* lonX1e9 */
                                                                     };

/** A vertex to the west of Rudolph Island, inside its surrounding polygon.
 */
static const uGeofenceTestVertex_t gVertexRudolphIslandWestInside = {81770337700LL /* latX1e9 */,
                                                                     58005054800LL /* lonX1e9 */
                                                                    };

/** 1: just north of Rudolph Island's surrounding polygon, though with
 * sufficient uncertainty to avoid a square-extent elimination.
 */
static const uGeofenceTestPoint_t gTestPointFenceKNorthOutside = {
    &gVertexRudolphIslandNorthOutside,
    {
        U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES * 1000 /* radius mm */,
        0 /* altitude mm */, 0 /* altitude uncertainty mm */
    },
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 2: as (1) but with sufficient uncertainty that we might be inside.
 */
static const uGeofenceTestPoint_t gTestPointFenceKNorthOutsideUncertain = {
    &gVertexRudolphIslandNorthOutside,
    {500000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 3: just north of Rudolph Island but inside its surrounding polygon,
 * again with sufficient uncertainty to avoid a square-extent elimination.
 */
static const uGeofenceTestPoint_t gTestPointFenceKNorthInside = {
    &gVertexRudolphIslandNorthInside,
    {
        U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES * 1000 /* radius mm */,
        0 /* altitude mm */, 0 /* altitude uncertainty mm */
    },
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // Now the optimist sees that transition
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 4: as (3) but with sufficient uncertainty that we might be outside.
 */
static const uGeofenceTestPoint_t gTestPointFenceKNorthInsideUncertain = {
    &gVertexRudolphIslandNorthInside,
    {500000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 5: just east of Rudolph Island but inside its surrounding polygon,
 * again with sufficient uncertainty to avoid a square-extent elimination.
 */
static const uGeofenceTestPoint_t gTestPointFenceKEastInside = {
    &gVertexRudolphIslandEastInside,
    {
        U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES * 1000 /* radius mm */,
        0 /* altitude mm */, 0 /* altitude uncertainty mm */
    },
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // The pessimist see's the transit back inside again
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 6: as (5) but with sufficient uncertainty that we might be outside.
 */
static const uGeofenceTestPoint_t gTestPointFenceKEastInsideUncertain = {
    &gVertexRudolphIslandEastInside,
    {500000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 7: just east of Rudolph Island but this time outside its surrounding
 * polygon, again with sufficient uncertainty to avoid a square-extent
 * elimination.
 */
static const uGeofenceTestPoint_t gTestPointFenceKEastOutside = {
    &gVertexRudolphIslandEastOutside,
    {
        U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES * 1000 /* radius mm */,
        0 /* altitude mm */, 0 /* altitude uncertainty mm */
    },
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // The optimist agrees with the pessimist: we're outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 8: as (7) but with sufficient uncertainty that we might be inside.
 */
static const uGeofenceTestPoint_t gTestPointFenceKEastOutsideUncertain = {
    &gVertexRudolphIslandEastOutside,
    {500000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 9: just south of Rudolph Island and inside its surrounding polygon,
 * again with sufficient uncertainty to avoid a square-extent elimination.
 */
static const uGeofenceTestPoint_t gTestPointFenceKSouthInside = {
    &gVertexRudolphIslandSouthInside,
    {
        U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES * 1000 /* radius mm */,
        0 /* altitude mm */, 0 /* altitude uncertainty mm */
    },
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // Even the optimist agrees: we're inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 10: as (9) but with sufficient uncertainty that we might be outside.
 */
static const uGeofenceTestPoint_t gTestPointFenceKSouthInsideUncertain = {
    &gVertexRudolphIslandSouthInside,
    {500000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 11: just south of Rudolph Island but this time outside its surrounding
 * polygon, again with sufficient uncertainty to avoid a square-extent
 * elimination.
 */
static const uGeofenceTestPoint_t gTestPointFenceKSouthOutside = {
    &gVertexRudolphIslandSouthOutside,
    {
        U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES * 1000 /* radius mm */,
        0 /* altitude mm */, 0 /* altitude uncertainty mm */
    },
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // The optimist agrees with the pessimist: we're outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 12: as (11) but with sufficient uncertainty that we might be inside.
 */
static const uGeofenceTestPoint_t gTestPointFenceKSouthOutsideUncertain = {
    &gVertexRudolphIslandSouthOutside,
    {500000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 13: just west of Rudolph Island and inside its surrounding polygon,
 * again with sufficient uncertainty to avoid a square-extent elimination.
 */
static const uGeofenceTestPoint_t gTestPointFenceKWestInside = {
    &gVertexRudolphIslandWestInside,
    {
        U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES * 1000 /* radius mm */,
        0 /* altitude mm */, 0 /* altitude uncertainty mm */
    },
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // The optimist agrees: we're inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 14: as (13) but with sufficient uncertainty that we might be outside.
 */
static const uGeofenceTestPoint_t gTestPointFenceKWestInsideUncertain = {
    &gVertexRudolphIslandWestInside,
    {500000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 15: just west of Rudolph Island but this time outside its surrounding
 * polygon, again with sufficient uncertainty to avoid a square-extent
 * elimination.
 */
static const uGeofenceTestPoint_t gTestPointFenceKWestOutside = {
    &gVertexRudolphIslandWestOutside,
    {
        U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES * 1000 /* radius mm */,
        0 /* altitude mm */, 0 /* altitude uncertainty mm */
    },
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // The optimist agrees with the pessimist: we're outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 16: as (15) but with sufficient uncertainty that we might be inside.
 */
static const uGeofenceTestPoint_t gTestPointFenceKWestOutsideUncertain = {
    &gVertexRudolphIslandWestOutside,
    {500000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** Fence K test data: an exclusion zone around Rudolph Island in
 * Arkhangelsk Oblast, Russia, near the north pole.
 */
static const uGeofenceTestData_t gFenceKTestData = {
    &gFenceK,
    1000000LL /* star radius mm */,
    16, // Update this if you add a test point
    {
        &gTestPointFenceKNorthOutside, &gTestPointFenceKNorthOutsideUncertain,
        &gTestPointFenceKNorthInside, &gTestPointFenceKNorthInsideUncertain,
        &gTestPointFenceKEastInside, &gTestPointFenceKEastInsideUncertain,
        &gTestPointFenceKEastOutside, &gTestPointFenceKEastOutsideUncertain,
        &gTestPointFenceKSouthInside, &gTestPointFenceKSouthInsideUncertain,
        &gTestPointFenceKSouthOutside, &gTestPointFenceKSouthOutsideUncertain,
        &gTestPointFenceKWestInside, &gTestPointFenceKWestInsideUncertain,
        &gTestPointFenceKWestOutside, &gTestPointFenceKWestOutsideUncertain
    }
};

/** A vertex at Scott's hut, south pole.
 */
static const uGeofenceTestVertex_t gVertexScottsHut = {-77845769825LL /* latX1e9 */,
                                                       166641764614LL /* lonX1e9 */
                                                      };

/** A circle that surrounds Scott's hut at the south pole.
 */
static const uGeofenceTestCircle_t gCircleScottsHut = {&gVertexScottsHut,
                                                       9000UL /* radius mm */
                                                      };

/** Fence L: a circle just inside the polar danger zone, north.
 */
static const uGeofenceTestFence_t gFenceL = {
    "L: polar, south, Scott's hut",
    0 /* altitude max mm */,
    0 /* altitude min mm */,
    1, {&gCircleScottsHut},
    0
};

/** A vertex at McMurdo airport, south pole, 500 m south east
 * of Scott's hut.
 */
static const uGeofenceTestVertex_t gVertexMcMurdoAirport = {-77847526746LL /* latX1e9 */,
                                                            166663774552LL /* lonX1e9 */
                                                           };

/** A vertex on Hut Point Drive, about 50 metres from Scott's hut.
 */
static const uGeofenceTestVertex_t gVertexHutPointDrive = {-77845561081LL /* latX1e9 */,
                                                           166643068882LL /* lonX1e9 */
                                                          };

/** 1: at McMurdo airport, south pole, with enough uncertainty
 * to avoid a square-extent elimination.
 */
static const uGeofenceTestPoint_t gTestPointFenceLOutside = {
    &gVertexMcMurdoAirport,
    {
        U_GEOFENCE_SQUARE_EXTENT_CHECK_UNCERTAINTY_METRES * 1000 /* radius mm */,
        0 /* altitude mm */, 0 /* altitude uncertainty mm */
    },
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST)
};

/** 2: as (1) but with sufficient uncertainty that we might
 * actually be inside Scott's hut.
 */
static const uGeofenceTestPoint_t gTestPointFenceLOutsideUncertain = {
    &gVertexMcMurdoAirport,
    {1000000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 3: on Hut Point Drive, certainly not at the hut.
 */
static const uGeofenceTestPoint_t gTestPointFenceLApproaching = {
    &gVertexHutPointDrive,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit
    // back outside once more
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 4: as (3) but with enough uncertainty that we might be at the hut.
 */
static const uGeofenceTestPoint_t gTestPointFenceLApproachingUncertain = {
    &gVertexHutPointDrive,
    {100000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit
    // inside again
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 5: inside Scott's hut, with certainty.
 */
static const uGeofenceTestPoint_t gTestPointFenceLInside = {
    &gVertexScottsHut,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // The optimist now sees the transit
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 6: as (5) but with enough uncertainty that we might be outside
 * the hut.
 */
static const uGeofenceTestPoint_t gTestPointFenceLInsideUncertain = {
    &gVertexScottsHut,
    {10000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // With the uncertainty, a pessimist would see a transit
    // outside again
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** Fence L test data: at Scott's hut, south pole.
 */
static const uGeofenceTestData_t gFenceLTestData = {
    &gFenceL,
    50000LL /* star radius mm */,
    6, // Update this if you add a test point
    {
        &gTestPointFenceLOutside, &gTestPointFenceLOutsideUncertain,
        &gTestPointFenceLApproaching, &gTestPointFenceLApproachingUncertain,
        &gTestPointFenceLInside, &gTestPointFenceLInsideUncertain
    }
};

/* ----------------------------------------------------------------
 * VARIABLES: FENCES M, N AND O, OFFICE-BLOCK SIZED THINGS
 * -------------------------------------------------------------- */

/** A vertex at a corner of the u-blox office, Cambridge, UK.
 */
static const uGeofenceTestVertex_t gVertexUbloxCambridge0 = {52222776577LL /* latX1e9 */,
                                                             -74993565LL /* lonX1e9 */
                                                            };

/** A vertex at a corner of the u-blox office, Cambridge, UK.
 */
static const uGeofenceTestVertex_t gVertexUbloxCambridge1 = {52222573470LL /* latX1e9 */,
                                                             -73416999LL /* lonX1e9 */
                                                            };

/** A vertex at a corner of the u-blox office, Cambridge, UK.
 */
static const uGeofenceTestVertex_t gVertexUbloxCambridge2 = {52222362071LL /* latX1e9 */,
                                                             -73484663LL /* lonX1e9 */
                                                            };

/** A vertex at a corner of the u-blox office, Cambridge, UK.
 */
static const uGeofenceTestVertex_t gVertexUbloxCambridge3 = {52222567943LL /* latX1e9 */,
                                                             -75070251LL /* lonX1e9 */
                                                            };

/** A polygon (lozenge) whose vertices are the corners of the
 * u-blox office, Cambridge, UK.
 */
static const uGeofenceTestPolygon_t gPolygonUbloxCambridge = {4,
    {
        &gVertexUbloxCambridge0, &gVertexUbloxCambridge1,
        &gVertexUbloxCambridge2, &gVertexUbloxCambridge3
    }
};

/** Fence M: a lozenge containing the u-blox office, on the second floor
 * of building 2020, Cambourne Business Park, Cambridge, UK.
 */
static const uGeofenceTestFence_t gFenceM = {
    "M: u-blox Cambridge",
    90000 /* altitude max mm */,
    80000 /* altitude min mm */,
    0, {&gCircleEmpty},
    1, {&gPolygonUbloxCambridge}
};

/** Fence N: as Fence M but no longer taking altitude into account.
 */
static const uGeofenceTestFence_t gFenceN = {
    "N: u-blox Cambridge with altitude",
    0 /* altitude max mm */,
    0 /* altitude min mm */,
    0, {&gCircleEmpty},
    1, {&gPolygonUbloxCambridge}
};

/** A vertex just outside the front entrance of the u-blox Cambridge
 * office.
 */
static const uGeofenceTestVertex_t gVertexUbloxCambridgeEntrance = {52222426597LL /* latX1e9 */,
                                                                    -74241099LL /* lonX1e9 */
                                                                   };

/** A vertex at Rob's desk in the u-blox Cambridge office.
 */
static const uGeofenceTestVertex_t gVertexRobDesk = {52222565519LL /* latX1e9 */,
                                                     -74422444LL /* lonX1e9 */
                                                    };

/** A vertex in Procam, on the floor below the u-blox Cambridge office.
 */
static const uGeofenceTestVertex_t gVertexProcam = {52222682206LL /* latX1e9 */,
                                                    -74620418LL /* lonX1e9 */
                                                   };

/** A vertex at Mediatek Ltd, next door to the u-blox
 * Cambridge office.
 */
static const uGeofenceTestVertex_t gVertexMediatek = {52222231407LL /* latX1e9 */,
                                                      -72940036LL /* lonX1e9 */
                                                     };

/** A vertex in the car park behind and slightly to the left
 * of the u-blox Cambridge office.
 */
static const uGeofenceTestVertex_t gVertexUbloxCambridgeCarPark = {52222886691LL /* latX1e9 */,
                                                                   -74973189LL /* lonX1e9 */
                                                                  };

/** 1: at the entrance of the u-blox Cambridge office, outside on
 * the ground.
 */
static const uGeofenceTestPoint_t gTestPointFenceMAltitideEntrance = {
    &gVertexUbloxCambridgeEntrance,
    {1000 /* radius mm */, 63000 /* altitude mm */, 2000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST)
};

/** 2: at Rob's desk in the u-blox Cambridge office, pretty certainly,
 * with the right altitude.
 */
static const uGeofenceTestPoint_t gTestPointFenceMAltitideRobDesk = {
    &gVertexRobDesk,
    {1000 /* radius mm */, 82000 /* altitude mm */, 1000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // The pessimist and the optimist both see a transit
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 3: as (2) but with altitude uncertainty that might put us in
 * Procam.
 */
static const uGeofenceTestPoint_t gTestPointFenceMAltitideRobDeskUncertain = {
    &gVertexRobDesk,
    {1000 /* radius mm */, 82000 /* altitude mm */, 3000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // Uncertainty causes the pessimist to see a transit
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 4: back outside, in the car park, pretty certainly.
 */
static const uGeofenceTestPoint_t gTestPointFenceMAltitideCarPark = {
    &gVertexUbloxCambridgeCarPark,
    {1000 /* radius mm */, 63000 /* altitude mm */, 1000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // The optimist now sees the transit
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 5: in Procam, when altitude is taken into account.
 */
static const uGeofenceTestPoint_t gTestPointFenceMAltitideProcam = {
    &gVertexProcam,
    {1000 /* radius mm */, 75000 /* altitude mm */, 1000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST)
};

/** 6: in Mediatek.
 */
static const uGeofenceTestPoint_t gTestPointFenceMMAltitideMediatek = {
    &gVertexMediatek,
    {1000 /* radius mm */, 82000 /* altitude mm */, 1000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST)
};

/** Fence M test data: u-blox Cambridge office and vicinity,
 * taking altitude into account.
 */
static const uGeofenceTestData_t gFenceMTestData = {
    &gFenceM,
    100000LL /* star radius mm */,
    6, // Update this if you add a test point
    {
        &gTestPointFenceMAltitideEntrance, &gTestPointFenceMAltitideRobDesk,
        &gTestPointFenceMAltitideRobDeskUncertain, &gTestPointFenceMAltitideCarPark,
        &gTestPointFenceMAltitideProcam, &gTestPointFenceMMAltitideMediatek
    }
};

/** 1: at the entrance of the u-blox Cambridge office, outside.
 */
static const uGeofenceTestPoint_t gTestPointFenceNEntrance = {
    &gVertexUbloxCambridgeEntrance,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST)
};

/** 2: at Rob's desk in the u-blox Cambridge office, pretty certainly.
 */
static const uGeofenceTestPoint_t gTestPointFenceNRobDesk = {
    &gVertexRobDesk,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // The pessimist and the optimist both see a transit
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 3: in Procam, but we can't really tell because no altitude is employed.
 */
static const uGeofenceTestPoint_t gTestPointFenceNProcam = {
    &gVertexProcam,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST)
};

/** 4: outside, in the car park, pretty certainly.
 */
static const uGeofenceTestPoint_t gTestPointFenceNCarPark = {
    &gVertexUbloxCambridgeCarPark,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // The pessimist and the optimist both see a transit
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 5: in the Mediatek building.
 */
static const uGeofenceTestPoint_t gTestPointFenceNMediatek = {
    &gVertexMediatek,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST)
};

/** Fence N test data: as the Fence M test data but 2D.
 */
static const uGeofenceTestData_t gFenceNTestData = {
    &gFenceN,
    100000LL /* star radius mm */,
    5, // Update this if you add a test point
    {
        &gTestPointFenceNEntrance, &gTestPointFenceNRobDesk,
        &gTestPointFenceNProcam, &gTestPointFenceNCarPark,
        &gTestPointFenceNMediatek
    }
};

/** A vertex at the centre of the O2, London's docklands, UK.
 */
static const uGeofenceTestVertex_t gVertexO2Centre = {51503022839LL /* latX1e9 */,
                                                      3212829LL /* lonX1e9 */
                                                     };

/** A circle the diameter of the O2, London's docklands, UK.
 */
static const uGeofenceTestCircle_t gCircleO2 = {&gVertexO2Centre,
                                                193000UL /* radius mm */
                                               };

/** Fence O: containing the O2, London's docklands, UK.
 */
static const uGeofenceTestFence_t gFenceO = {
    "O: the O2, London's Docklands, UK",
    0 /* altitude max mm */,
    0 /* altitude min mm */,
    1, {&gCircleO2},
    0
};

/** A vertex at the exit of the Greenwich North underground station.
 */
static const uGeofenceTestVertex_t gVertexGreenwichNorthExit = {51500286701LL /* latX1e9 */,
                                                                3954927LL /* lonX1e9 */
                                                               };

/** A vertex at the Cutty Sark.
 */
static const uGeofenceTestVertex_t gVertexCuttySark = {51486537285LL /* latX1e9 */,
                                                       -515473LL /* lonX1e9 */
                                                      };

/** A vertex on the path that surrounds the O2.
 */
static const uGeofenceTestVertex_t gVertexO2Path = {51501421874LL /* latX1e9 */,
                                                    4820683LL /* lonX1e9 */
                                                   };

/** A vertex just inside the O2.
 */
static const uGeofenceTestVertex_t gVertexO2Inside = {51502174527LL /* latX1e9 */,
                                                      4273107LL /* lonX1e9 */
                                                     };

/** 1: at the exit of the Greenwich North underground station,
 * near the O2, just got out from under so accuracy is not good,
 * we might even think we have arrived.
 */
static const uGeofenceTestPoint_t gTestPointFenceOGreenwichNorth = {
    &gVertexGreenwichNorthExit,
    {200000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST)
};

/** 2: divert for a quick look at the Cutty Sark, crossing the meridian.
 */
static const uGeofenceTestPoint_t gTestPointFenceOCuttySark = {
    &gVertexCuttySark,
    {10000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST)
};

/** 3: outside the O2 now; not yet made it in.
 */
static const uGeofenceTestPoint_t gTestPointFenceOO2Path = {
    &gVertexO2Path,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST)
};

/** 4: inside the O2, but the GNSS signal is now weak.
 */
static const uGeofenceTestPoint_t gTestPointFenceOO2Inside = {
    &gVertexO2Inside,
    {100000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // The pessimist thinks we've breached the barrier
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 5: on stage.
 */
static const uGeofenceTestPoint_t gTestPointFenceOO2Stage = {
    &gVertexO2Centre,
    {100000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // Even the optimist now agrees that we're inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** Fence O test data: in the vicinity of the O2, London, UK.
 */
static const uGeofenceTestData_t gFenceOTestData = {
    &gFenceO,
    100000LL /* star radius mm */,
    5, // Update this if you add a test point
    {
        &gTestPointFenceOGreenwichNorth, &gTestPointFenceOCuttySark,
        &gTestPointFenceOO2Path, &gTestPointFenceOO2Inside,
        &gTestPointFenceOO2Stage
    }
};

/* ----------------------------------------------------------------
 * VARIABLES: FENCE P, LARGE GEOGRAPHIC AREAS, CHERNOBYL
 * -------------------------------------------------------------- */

/** A vertex at the top of a polygon encompassing Chernobyl.
 */
static const uGeofenceTestVertex_t gVertexChernobyl0 = {51290624184LL /* latX1e9 */,
                                                        30208070512LL /* lonX1e9 */
                                                       };

/** Another vertex, clockwise around a polygon encompassing
 * Chernobyl.
 */
static const uGeofenceTestVertex_t gVertexChernobyl1 = {51284578700LL /* latX1e9 */,
                                                        30228560600LL /* lonX1e9 */
                                                       };

/** The next vertex, clockwise around a polygon encompassing
 * Chernobyl.
 */
static const uGeofenceTestVertex_t gVertexChernobyl2 = {51279213300LL /* latX1e9 */,
                                                        30240952800LL /* lonX1e9 */
                                                       };

/** The next vertex, clockwise around a polygon encompassing
 * Chernobyl.
 */
static const uGeofenceTestVertex_t gVertexChernobyl3 = {51270002772LL /* latX1e9 */,
                                                        30251024871LL /* lonX1e9 */
                                                       };

/** The next vertex, clockwise around a polygon encompassing
 * Chernobyl.
 */
static const uGeofenceTestVertex_t gVertexChernobyl4 = {51257684437LL /* latX1e9 */,
                                                        30224344992LL /* lonX1e9 */
                                                       };

/** The next vertex, clockwise around a polygon encompassing
 * Chernobyl.
 */
static const uGeofenceTestVertex_t gVertexChernobyl5 = {51280229310LL /* latX1e9 */,
                                                        30195594382LL /* lonX1e9 */
                                                       };

/** The next vertex, clockwise around a polygon encompassing
 * Chernobyl.
 */
static const uGeofenceTestVertex_t gVertexChernobyl6 = {51281618488LL /* latX1e9 */,
                                                        30206236064LL /* lonX1e9 */
                                                       };

/** The last vertex of a polygon encompassing Chernobyl.
 */
static const uGeofenceTestVertex_t gVertexChernobyl7 = {51286087841LL /* latX1e9 */,
                                                        30201396847LL /* lonX1e9 */
                                                       };

/** A polygon whose vertices encompass the area of Chernobyl.
 */
static const uGeofenceTestPolygon_t gPolygonChernobyl = {8,
    {
        &gVertexChernobyl0, &gVertexChernobyl1, &gVertexChernobyl2,
        &gVertexChernobyl3, &gVertexChernobyl4, &gVertexChernobyl5,
        &gVertexChernobyl6, &gVertexChernobyl7
    }
};

/** Fence P: containing a polygon that encompasses Chernobyl.
 */
static const uGeofenceTestFence_t gFenceP = {
    "P: Chernobyl",
    0 /* altitude max mm */,
    0 /* altitude min mm */,
    0, {&gCircleEmpty},
    1, {&gPolygonChernobyl}
};

/** A vertex just oustide Chernobyl, on the approach road, just past
 * the Pripyat river.
 */
static const uGeofenceTestVertex_t gVertexChernobylApproachOutside = {51291117706LL /* latX1e9 */,
                                                                      30220817667LL /* lonX1e9 */
                                                                     };

/** A vertex on the road where the perimeter briefly extends
 * across it.
 */
static const uGeofenceTestVertex_t gVertexChernobylRoadInside = {51290365486LL /* latX1e9 */,
                                                                 30208259749LL /* lonX1e9 */
                                                                };

#ifdef U_CFG_GEOFENCE_USE_GEODESIC
/** A vertex at the junction with Kirova street, outside the
 * perimeter again, nice and tight so that only geodesic
 * calculations work.
 */
static const uGeofenceTestVertex_t gVertexChernobyKirovaStreetOutside = {51289145288LL /* latX1e9 */,
                                                                         30198073772LL /* lonX1e9 */
                                                                        };
#else
/** A vertex at the junction with Kirova street, outside the
 * perimeter again, with enough slack that spherical coordinates
 * work.
 */
static const uGeofenceTestVertex_t gVertexChernobyKirovaStreetOutside = {51289200100LL /* latX1e9 */,
                                                                         30197813937LL /* lonX1e9 */
                                                                        };
#endif

/** A vertex at Monument To Those Who Saved The World.
 */
static const uGeofenceTestVertex_t gVertexChernobyMonumentInside = {51280369419LL /* latX1e9 */,
                                                                    30208151736LL /* lonX1e9 */
                                                                   };

/** A vertex outside again, on the road to the WWII war monument.
 */
static const uGeofenceTestVertex_t gVertexChernobyMonumentOutside = {51262794671LL /* latX1e9 */,
                                                                     30203932648LL /* lonX1e9 */
                                                                    };

/** A vertex on the river Uzh, just south of the Chernobyl perimeter.
 */
static const uGeofenceTestVertex_t gVertexChernobyRiverUzhOutside = {51254917863LL /* latX1e9 */,
                                                                     30221913970LL /* lonX1e9 */
                                                                    };

/** A vertex on the river Richishche, inside the Chernobyl perimeter.
 */
static const uGeofenceTestVertex_t gVertexChernobyRiverRichishcheInside = {51263792400LL /* latX1e9 */,
                                                                           30236661000LL /* lonX1e9 */
                                                                          };

/** A vertex east of the Chernobyl perimeter on the Pripyat river.
 */
static const uGeofenceTestVertex_t gVertexChernobyRiverPripyatEastOutside = {51266416121LL /* latX1e9 */,
                                                                             30257787239LL /* lonX1e9 */
                                                                            };

/** A vertex to east of Chernobyl, very close to the perimeter, on the
 * Pripyat river.
 */
static const uGeofenceTestVertex_t gVertexChernobyRiverPripyatEastClose = {51279195564LL /* latX1e9 */,
                                                                           30241001589LL /* lonX1e9 */
                                                                          };

/** A vertex to the north of the Chernobyl perimeter, under the road bridge,
 * on the Pripyat river.
 */
static const uGeofenceTestVertex_t gVertexChernobyRiverPripyatNorthOutside = {51291438365LL /* latX1e9 */,
                                                                              30226027820LL /* lonX1e9 */
                                                                             };

/** 1: approaching Chernobyl on the road from the north-east, outside
 * the perimeter.
 */
static const uGeofenceTestPoint_t gTestPointFencePApproachOutside = {
    &gVertexChernobylApproachOutside,
    {10000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 2: briefly pass inside the perimeter while driving along the road.
 */
static const uGeofenceTestPoint_t gTestPointFencePRoadInside = {
    &gVertexChernobylRoadInside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // The pessimist and the optimist both see a transit to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 3: back outside again, about to turn down Kirova street.
 */
static const uGeofenceTestPoint_t gTestPointFencePKirovaStreetOutside = {
    &gVertexChernobyKirovaStreetOutside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // The pessimist and the optimist both see a transit to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 4: inside the perimeter proper now, looking at the Monument
 * To Those Who Saved The Wotld.
 */
static const uGeofenceTestPoint_t gTestPointFencePMonumentInside = {
    &gVertexChernobyMonumentInside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // The pessimist and the optimist both see a transit to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 5: outside again, going to visit the WWII memorial.
 */
static const uGeofenceTestPoint_t gTestPointFencePMonumentOutside = {
    &gVertexChernobyMonumentOutside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // The pessimist and the optimist both see a transit to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 6: abandon the car, decide to row back, first on the Uzh.
 */
static const uGeofenceTestPoint_t gTestPointFencePRiverUzhOutside = {
    &gVertexChernobyRiverUzhOutside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 7: lose our way and, checking GNSS, find ourselves on the
 * river Richishche and inside the perimeter again.
 */
static const uGeofenceTestPoint_t gTestPointFencePRiverRichishcheInside = {
    &gVertexChernobyRiverRichishcheInside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // The pessimist and the optimist both see a transit to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 8: safely out on the river Pripyat.
 */
static const uGeofenceTestPoint_t gTestPointFencePRiverPripyatEastOutside = {
    &gVertexChernobyRiverPripyatEastOutside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // The pessimist and the optimist both see a transit to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 9: rowing north up the Pripyat might briefly take us inside
 * the perimeter once more.
 */
static const uGeofenceTestPoint_t gTestPointFencePRiverPripyatEastUncertain = {
    &gVertexChernobyRiverPripyatEastClose,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // The pessimist sees a transit to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 10: back pretty much to where we started from, this time
 * under the road bridge on the Pripyat river.
 */
static const uGeofenceTestPoint_t gTestPointFencePRiverPripyatNorthOutside = {
    &gVertexChernobyRiverPripyatNorthOutside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // The the pessimist sees a transit back to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** Fence P test data: a tour around the perimeter of Chernobyl.
 */
static const uGeofenceTestData_t gFencePTestData = {
    &gFenceP,
    2000000LL /* star radius mm */,
    10, // Update this if you add a test point
    {
        &gTestPointFencePApproachOutside, &gTestPointFencePRoadInside,
        &gTestPointFencePKirovaStreetOutside, &gTestPointFencePMonumentInside,
        &gTestPointFencePMonumentOutside, &gTestPointFencePRiverUzhOutside,
        &gTestPointFencePRiverRichishcheInside, &gTestPointFencePRiverPripyatEastOutside,
        &gTestPointFencePRiverPripyatEastUncertain, &gTestPointFencePRiverPripyatNorthOutside
    }
};

/* ----------------------------------------------------------------
 * VARIABLES: FENCE Q, LARGE GEOGRAPHIC AREAS, UTAH
 * -------------------------------------------------------------- */

/** A vertex at the lower-right corner of the state of Utah, US of A.
 */
static const uGeofenceTestVertex_t gVertexUtah0 = {36998950191LL /* latX1e9 */,
                                                   -109045283306LL /* lonX1e9 */
                                                  };

/** A vertex at the next corner of Utah, clockwise.
 */
static const uGeofenceTestVertex_t gVertexUtah1 = {36998250900LL /* latX1e9 */,
                                                   -110175749300LL /* lonX1e9 */
                                                  };

/** A vertex at the next corner of Utah, clockwise.
 */
static const uGeofenceTestVertex_t gVertexUtah2 = {36997657400LL /* latX1e9 */,
                                                   -110469732700LL /* lonX1e9 */
                                                  };

/** A vertex at the next corner of Utah, clockwise.
 */
static const uGeofenceTestVertex_t gVertexUtah3 = {37003627600LL /* latX1e9 */,
                                                   -110490298800LL /* lonX1e9 */
                                                  };

/** A vertex at the next corner of Utah, clockwise.
 */
static const uGeofenceTestVertex_t gVertexUtah4 = {37000190422LL /* latX1e9 */,
                                                   -114050052350LL /* lonX1e9 */
                                                  };

/** A vertex at the next corner of Utah.
 */
static const uGeofenceTestVertex_t gVertexUtah5 = {38877836500LL /* latX1e9 */,
                                                   -114049586900LL /* lonX1e9 */
                                                  };

/** A vertex at the next corner of Utah.
 */
static const uGeofenceTestVertex_t gVertexUtah6 = {41993872228LL /* latX1e9 */,
                                                   -114041476351LL /* lonX1e9 */
                                                  };

/** A vertex at the next corner of Utah.
 */
static const uGeofenceTestVertex_t gVertexUtah7 = {41993095278LL /* latX1e9 */,
                                                   -113990090177LL /* lonX1e9 */
                                                  };

/** A vertex at the next corner of Utah.
 */
static const uGeofenceTestVertex_t gVertexUtah8 = {41988211854LL /* latX1e9 */,
                                                   -113866751339LL /* lonX1e9 */
                                                  };

/** A vertex at the next corner of Utah.
 */
static const uGeofenceTestVertex_t gVertexUtah9 = {42001701594LL /* latX1e9 */,
                                                   -111046714652LL /* lonX1e9 */
                                                  };

/** A vertex at the next corner of Utah.
 */
static const uGeofenceTestVertex_t gVertexUtah10 = {40997874291LL /* latX1e9 */,
                                                    -111046816176LL /* lonX1e9 */
                                                   };

/** A vertex at the next corner of Utah.
 */
static const uGeofenceTestVertex_t gVertexUtah11 = {40996267206LL /* latX1e9 */,
                                                    -110545271793LL /* lonX1e9 */
                                                   };

/** A vertex at the next corner of Utah.
 */
static const uGeofenceTestVertex_t gVertexUtah12 = {40994803645LL /* latX1e9 */,
                                                    -110505079447LL /* lonX1e9 */
                                                   };

/** A vertex at the next corner of Utah.
 */
static const uGeofenceTestVertex_t gVertexUtah13 = {41000690870LL /* latX1e9 */,
                                                    -109050026567LL /* lonX1e9 */
                                                   };

/** A vertex at the next corner of Utah.
 */
static const uGeofenceTestVertex_t gVertexUtah14 = {38275568872LL /* latX1e9 */,
                                                    -109060193883LL /* lonX1e9 */
                                                   };

/** A vertex at the last corner of Utah.
 */
static const uGeofenceTestVertex_t gVertexUtah15 = {38166265753LL /* latX1e9 */,
                                                    -109042835765LL /* lonX1e9 */
                                                   };

/** A polygon whose vertices are the corners of the state of Utah,
 * United States.
 */
static const uGeofenceTestPolygon_t gPolygonUtah = {15,
    {
        &gVertexUtah0, &gVertexUtah1, &gVertexUtah2, &gVertexUtah3,
        &gVertexUtah4, &gVertexUtah5, &gVertexUtah6, &gVertexUtah7,
        &gVertexUtah8, &gVertexUtah9, &gVertexUtah10, &gVertexUtah11,
        &gVertexUtah12, &gVertexUtah13, &gVertexUtah14, &gVertexUtah15
    }
};

/** Fence P: a polygon defining the edge of the state of Utah, USA.
 */
static const uGeofenceTestFence_t gFenceQ = {
    "Q: Utah, United States",
    0 /* altitude max mm */,
    0 /* altitude min mm */,
    0, {&gCircleEmpty},
    1, {&gPolygonUtah}
};

#ifdef U_CFG_GEOFENCE_USE_GEODESIC

/** A vertex at Four Corners monument, at the junction of the states
 * of Colorado, New Mexico, Arizona and Utah, on the side of the
 * monument furthest from Utah, not far from Grandma's Frybread Shack.
 */
static const uGeofenceTestVertex_t gVertexFourCornersMonumentOutside = {36998749227LL /* latX1e9 */,
                                                                        -109044969130LL /* lonX1e9 */
                                                                       };

/** A vertex on the "correct" side of Four Corners monument.
 */
static const uGeofenceTestVertex_t gVertexFourCornersMonumentInside = {36999190233LL /* latX1e9 */,
                                                                       -109045435502LL /* lonX1e9 */
                                                                      };

/** A vertex in the middle of Patrick Swayze loop.
 */
static const uGeofenceTestVertex_t gVertexPatrickSwayzeInside = {36998544286LL /* latX1e9 */,
                                                                 -110126919541LL /* lonX1e9 */
                                                                };

/** A vertex wild camping in Beaver Dam Wash, in Nevada about
 * 10 metres outside Utah.
 */
static const uGeofenceTestVertex_t gVertexBeaverDamWashOutside = {37000115485LL /* latX1e9 */,
                                                                  -114050125118LL /* lonX1e9 */
                                                                 };

/** A vertex on Burbank Back road, at the junction with the 1447,
 * 10 metres inside and half-way up the west side of Utah.
 */
static const uGeofenceTestVertex_t gVertexBurbankBackRoadInside = {38730427700LL /* latX1e9 */,
                                                                   -114049505200LL /* lonX1e9 */
                                                                  };

/** A vertex on the rather wiggly unnamed road that stops in the middle,
 * of nowhere, 10 metres outside the north-west corner of Utah.
 */
static const uGeofenceTestVertex_t gVertexUnnamedRoadOutside = {41993961648LL /* latX1e9 */,
                                                                -114041533887LL /* lonX1e9 */
                                                               };

/** A vertex on Birch Creek, 10 metres inside Utah.
 */
static const uGeofenceTestVertex_t gVertexBirchCreekInside = {41988343924LL /* latX1e9 */,
                                                              -113895635223LL /* lonX1e9 */
                                                             };

/** A vertex on Red Mountain, in Wyoming, about 10 metres outside
 * the north-east corner of Utah.
 */
static const uGeofenceTestVertex_t gVertexRedMountainOutside = {42001758764LL /* latX1e9 */,
                                                                -111046592057LL /* lonX1e9 */
                                                               };

/** A vertex on East Chalk Creek Road, at A V Richard's corner monument,
 * about 10 metres from the corner.
 */
static const uGeofenceTestVertex_t gVertexEastChalkCreekRoadInside = {40997775522LL /* latX1e9 */,
                                                                      -111046956050LL /* lonX1e9 */
                                                                     };

/** A vertex on the south-west side of Three Corners Triangle,
 * about 10 metres inside Utah.
 */
static const uGeofenceTestVertex_t gVertexThreeCornersTriangleInside = {41000642710LL /* latX1e9 */,
                                                                        -109050123239LL /* lonX1e9 */
                                                                       };

/** A vertex at the north-east corner of Three Corners Triangle,
 * about 15 metres into Wyoming.
 */
static const uGeofenceTestVertex_t gVertexThreeCornersTriangleOutside = {41000778679LL /* latX1e9 */,
                                                                         -109049860799LL /* lonX1e9 */
                                                                        };

/** A vertex on highway 46, about 15 metres inside Utah, staring at the
 * back of the "welcome to Utah: life elevated" sign.
 */
static const uGeofenceTestVertex_t gVertexUtahWelcomeSignInside = {38327596200LL /* latX1e9 */,
                                                                   -109060233100LL /* lonX1e9 */
                                                                  };

/** 1: at Four Corner's Monument, outside Utah, eating fried stuff.
 */
static const uGeofenceTestPoint_t gTestPointFenceQFourCornersMonumentOutside = {
    &gVertexFourCornersMonumentOutside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 2: at Four Corner's Monument, inside Utah, still eating fried stuff.
 */
static const uGeofenceTestPoint_t gTestPointFenceQFourCornersMonumentInside = {
    &gVertexFourCornersMonumentInside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // The pessimist and the optimist both see a transit to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 3: in the middle of Patrick Swayze loop, inside Utah but
 * with sufficent disinterest in Patrick Swayze that we might
 * be outside.
 */
static const uGeofenceTestPoint_t gTestPointFenceQPatrickSwayzeUncertain = {
    &gVertexPatrickSwayzeInside,
    {20000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // The pessimist sees a transit to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 4: enough of Swayze, we've now definitely left to watch beavers.
 */
static const uGeofenceTestPoint_t gTestPointFenceQBeaverDamWashOutside = {
    &gVertexBeaverDamWashOutside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // Even the optimist sees a transit to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 5: ...but the GNSS signal then gives out on us.
 */
static const uGeofenceTestPoint_t gTestPointFenceQBeaverDamWashUncertain = {
    &gVertexBeaverDamWashOutside,
    {20000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // The pessimist sees a transit to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 6: on Burbank Back road, having driven back inside Utah.
 */
static const uGeofenceTestPoint_t gTestPointFenceQBurbankBackRoadInside = {
    &gVertexBurbankBackRoadInside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // Even the optimist sees the transit back to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 7: now we're lost, on an unnamed road that goes nowhere; GNSS to
 * the rescue: we're not in Utah.
 */
static const uGeofenceTestPoint_t gTestPointFenceQUnnamedRoadOutside = {
    &gVertexUnnamedRoadOutside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // Both the optimist and the pessimist see a transit back to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 8: paddling down Birch Creek, we're inside Utah again.
 */
static const uGeofenceTestPoint_t gTestPointFenceQBirchCreekInside = {
    &gVertexBirchCreekInside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // Both the optimist and the pessimist see a transit back to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 9: a night on Red Mountain; not paying attention, and with GNSS
 * batteries running low, we _might_ have wandered outside again.
 */
static const uGeofenceTestPoint_t gTestPointFenceQRedMountainOutside = {
    &gVertexRedMountainOutside,
    {20000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // The pessimist sees a transit to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 10: driving down East Chalk Creek Road, pause to admire A V Richard's
 * efforts in marking state boundaries.
 */
static const uGeofenceTestPoint_t gTestPointFenceQEastChalkCreekRoadInside = {
    &gVertexEastChalkCreekRoadInside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // The pessimist see a transit back to inside (the
    // optimist never left)
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 11: pause at the somewhat underwhelming Three Corners triangle,
 * on the inside looking out.
 */
static const uGeofenceTestPoint_t gTestPointFenceQThreeCornersTriangleInside = {
    &gVertexThreeCornersTriangleInside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST)
};

/** 12: check to see if Three Corners triangle looks any better from the outside
 * looking in.
 */
static const uGeofenceTestPoint_t gTestPointFenceQThreeCornersTriangleOutside = {
    &gVertexThreeCornersTriangleOutside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // Both the optimist and the pessimist see a transit back to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 13: last check-point in Utah, on the 46 leaving for Colorado.
 */
static const uGeofenceTestPoint_t gTestPointFenceQUtahWelcomSignInside = {
    &gVertexUtahWelcomeSignInside,
    {20000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // The pessimist sees that we might have already left
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** Fence Q test data: a tour of the edge of the state of Utah, USA.
 */
static const uGeofenceTestData_t gFenceQTestData = {
    &gFenceQ,
    200000LL /* star radius mm */,
    13, // Update this if you add a test point
    {
        &gTestPointFenceQFourCornersMonumentOutside, &gTestPointFenceQFourCornersMonumentInside,
        &gTestPointFenceQPatrickSwayzeUncertain,
        &gTestPointFenceQBeaverDamWashOutside, &gTestPointFenceQBeaverDamWashUncertain,
        &gTestPointFenceQBurbankBackRoadInside, &gTestPointFenceQUnnamedRoadOutside,
        &gTestPointFenceQBirchCreekInside, &gTestPointFenceQRedMountainOutside,
        &gTestPointFenceQEastChalkCreekRoadInside,
        &gTestPointFenceQThreeCornersTriangleInside, &gTestPointFenceQThreeCornersTriangleOutside,
        &gTestPointFenceQUtahWelcomSignInside
    }
};

#else

/** Fence Q test data: keep the compiler happy.
 */
static const uGeofenceTestData_t gFenceQTestData = {&gFenceQ, 0, 0};

#endif // #ifdef U_CFG_GEOFENCE_USE_GEODESIC

/* ----------------------------------------------------------------
 * VARIABLES: FENCE R, LARGE GEOGRAPHIC AREAS, NORTH WEST TERRITORIES
 * -------------------------------------------------------------- */

/** A vertex at the lower-right corner of the North West Territories,
 * Canada.
 */
static const uGeofenceTestVertex_t gVertexNWT0 = {60000000000LL /* latX1e9 */,
                                                  -102000000000LL /* lonX1e9 */
                                                 };

/** A vertex next along, clockwise, on the edge of the North West
 * Territories.
 */
static const uGeofenceTestVertex_t gVertexNWT1 = {60000000000LL /* latX1e9 */,
                                                  -141001444000LL /* lonX1e9 */
                                                 };

/** The next vertex along, the edge of the North West Territories.
 */
static const uGeofenceTestVertex_t gVertexNWT2 = {69646614058LL /* latX1e9 */,
                                                  -141001444000LL /* lonX1e9 */
                                                 };

/** The next vertex along, the edge of the North West Territories.
 */
static const uGeofenceTestVertex_t gVertexNWT3 = {70666375231LL /* latX1e9 */,
                                                  -128208337591LL /* lonX1e9 */
                                                 };

/** The next vertex along, the edge of the North West Territories.
 */
static const uGeofenceTestVertex_t gVertexNWT4 = {69724729901LL /* latX1e9 */,
                                                  -120630784504LL /* lonX1e9 */
                                                 };

/** The next vertex along, the edge of the North West Territories.
 */
static const uGeofenceTestVertex_t gVertexNWT5 = {67779957871LL /* latX1e9 */,
                                                  -120630784504LL /* lonX1e9 */
                                                 };

/** The last vertex of the North West Territories.
 */
static const uGeofenceTestVertex_t gVertexNWT6 = {64189050578LL /* latX1e9 */,
                                                  -102000000000LL /* lonX1e9 */
                                                 };

/** A polygon whose vertices roughly contain the North West
 * Territories, Canada.
 */
static const uGeofenceTestPolygon_t gPolygonNWT = {7,
    {
        &gVertexNWT0, &gVertexNWT1, &gVertexNWT2, &gVertexNWT3,
        &gVertexNWT4, &gVertexNWT5, &gVertexNWT6
    }
};

/** Fence R: a polygon defining the edge of the North West
 * Territories, Canada.
 */
static const uGeofenceTestFence_t gFenceR = {
    "R: North West Territories, Canada",
    0 /* altitude max mm */,
    0 /* altitude min mm */,
    0, {&gCircleEmpty},
    1, {&gPolygonNWT}
};

#ifdef U_CFG_GEOFENCE_USE_GEODESIC

/** A vertex at Canadian Four Corners, at the junction of the states
 * of Nunavut, Manitoba, Saskatchewan and the North West Territories,
 * in Manitoba just outside the North West Territories.
 */
static const uGeofenceTestVertex_t gVertexFourCornersCanadaOutside = {59999381652LL /* latX1e9 */,
                                                                      -101999859017LL /* lonX1e9 */
                                                                     };

/** A vertex on the other side of Canadian Four Corners.
 */
static const uGeofenceTestVertex_t gVertexFourCornersCanadaInside = {60003159319LL /* latX1e9 */,
                                                                     -102000377711LL /* lonX1e9 */
                                                                    };

/** A vertex at the tripoint of Saskatchewan, Alberta and the North
 * West Territories, in Saskatchewan just outside the North West
 * Territories.
 */
static const uGeofenceTestVertex_t gVertexTripointOutside = {59999991846LL /* latX1e9 */,
                                                             -109999994038LL /* lonX1e9 */
                                                            };

/** A vertex on the North West Territories side of the tripoint.
 */
static const uGeofenceTestVertex_t gVertexTripointInside = {60000028051LL /* latX1e9 */,
                                                            -110000010784LL /* lonX1e9 */
                                                           };

/** A vertex at the Fort Smith Animal Shelter, just inside the North
 * West Territories.
 */
static const uGeofenceTestVertex_t gVertexFortSmithInside = {60000718111LL /* latX1e9 */,
                                                             -111903390638LL /* lonX1e9 */
                                                            };

/** A vertex at the "Northbrita" border, which is actually another
 * tripoint, this time between British Columbiam, Alberta and the
 * North West Territories, this vertex on the Alberta side.
 */
static const uGeofenceTestVertex_t gVertexNorthbritaOutside = {59999843596LL /* latX1e9 */,
                                                               -119999988384LL /* lonX1e9 */
                                                              };

/** A vertex on the North West Territories side of the "Northbrita"
 * border.
 */
static const uGeofenceTestVertex_t gVertexNorthbritaInside = {60000087397LL /* latX1e9 */,
                                                              -120000015267LL /* lonX1e9 */
                                                             };

/** A vertex on the "The Hump", just outside the south-west corner
 * of the North West Territories side.
 */
static const uGeofenceTestVertex_t gVertexTheHumpOutside = {60315788615LL /* latX1e9 */,
                                                            -141080125400LL /* lonX1e9 */
                                                           };

/** A vertex on Alaska highway, half way up the western edge of the
 * North West Territories, on the inside.
 */
static const uGeofenceTestVertex_t gVertexAlaskaHighwayInside = {62615197061LL /* latX1e9 */,
                                                                 -141001220500LL /* lonX1e9 */
                                                                };

/** A vertex at the Little Gold Creek border crossing, on Top Of
 * The World highway, further up the western edge of the North
 * West Territories, on the outside.
 */
static const uGeofenceTestVertex_t gVertexLittleGoldCreekOutside = {64085570983LL /* latX1e9 */,
                                                                    -141001902513LL /* lonX1e9 */
                                                                   };

/** A vertex on the beach in Gordon, just outside the top-left
 * corner of the North West Territories.
 */
static const uGeofenceTestVertex_t gVertexGordonOutside = {69681989617LL /* latX1e9 */,
                                                           -141207777556LL /* lonX1e9 */
                                                          };

/** A vertex on what looks like the edge of a glacier falling
 * into the say, on the north east corner of the North West
 * Territories, on the outside.
 */
static const uGeofenceTestVertex_t gVertexGlacierOutside = {69472381365LL /* latX1e9 */,
                                                            -120476777607LL /* lonX1e9 */
                                                           };

/** A vertex where the border does a dogs-leg to the right,
 * outside, on Nunavut side.
 */
static const uGeofenceTestVertex_t gVertexDogsLegOutside = {67779975806LL /* latX1e9 */,
                                                            -120629405846LL /* lonX1e9 */
                                                           };

/** A vertex half way down the eastern border, where it turns
 * south again, on the North West Territories side.
 */
static const uGeofenceTestVertex_t gVertexTurnsSouthInside = {64188350896LL /* latX1e9 */,
                                                              -102001947527LL /* lonX1e9 */
                                                             };

/** 1: at Canadian Four Corner's on the outside.
 */
static const uGeofenceTestPoint_t gTestPointFenceRFourCornersCanadaOutside = {
    &gVertexFourCornersCanadaOutside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 2: at Canadian Four Corner's on the inside.
 */
static const uGeofenceTestPoint_t gTestPointFenceRFourCornersCanadaInside = {
    &gVertexFourCornersCanadaInside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 3: at the Tripoint on the outside.
 */
static const uGeofenceTestPoint_t gTestPointFenceRTripointOutside = {
    &gVertexTripointOutside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 4: at the Tripoint on the inside.
 */
static const uGeofenceTestPoint_t gTestPointFenceRTripointInside = {
    &gVertexTripointInside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit back to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 5: a pause at the Fort Smith Animal Shelter, to shelter some
 * animals.
 */
static const uGeofenceTestPoint_t gTestPointFenceRFortSmithInside = {
    &gVertexFortSmithInside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST)
};

/** 6: at "Northbrita" on the outside.
 */
static const uGeofenceTestPoint_t gTestPointFenceRNorthbritaOutside = {
    &gVertexNorthbritaOutside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 7: at the "Northbrita" on the inside.
 */
static const uGeofenceTestPoint_t gTestPointFenceRNorthbritaInside = {
    &gVertexNorthbritaInside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit back to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 8: at the "The Hump", a snowy wasteland, on the outside again.
 */
static const uGeofenceTestPoint_t gTestPointFenceRTheHumpOutside = {
    &gVertexTheHumpOutside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 9: on the Alaska Highway, a road to nowhere, on the inside again.
 */
static const uGeofenceTestPoint_t gTestPointFenceRAlaskaHighwayInside = {
    &gVertexAlaskaHighwayInside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit back to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 10: pulled over by the border police at Little Gold Creek, on
 * the outside.
 */
static const uGeofenceTestPoint_t gTestPointFenceRLittleGoldCreekOutside = {
    &gVertexLittleGoldCreekOutside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 11: at Gordon, a sandy wasteland, on the outside.
 */
static const uGeofenceTestPoint_t gTestPointFenceRGordonOutside = {
    &gVertexGordonOutside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 12: on a glacier, maybe in the sea, still on the outside.
 */
static const uGeofenceTestPoint_t gTestPointFenceRGlacierOutside = {
    &gVertexGlacierOutside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 13: at the dog's leg, it doesn't seem to have a name, not
 * like a Four Corners or a Tripoint.  Anyway, on the outside.
 */
static const uGeofenceTestPoint_t gTestPointFenceRDogsLegOutside = {
    &gVertexDogsLegOutside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 14: where the border turns south again.  And we're done.
 */
static const uGeofenceTestPoint_t gTestPointFenceRTurnsSouthInside = {
    &gVertexTurnsSouthInside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit back to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** Fence R test data: a tour of the edge of the Noth West Territories,
 * Canada.
 */
static const uGeofenceTestData_t gFenceRTestData = {
    &gFenceR,
    20000000LL /* star radius mm */,
    14, // Update this if you add a test point
    {
        &gTestPointFenceRFourCornersCanadaOutside, &gTestPointFenceRFourCornersCanadaInside,
        &gTestPointFenceRTripointOutside, &gTestPointFenceRTripointInside,
        &gTestPointFenceRFortSmithInside,
        &gTestPointFenceRNorthbritaOutside, &gTestPointFenceRNorthbritaInside,
        &gTestPointFenceRTheHumpOutside, &gTestPointFenceRAlaskaHighwayInside,
        &gTestPointFenceRLittleGoldCreekOutside, &gTestPointFenceRGordonOutside,
        &gTestPointFenceRGlacierOutside, &gTestPointFenceRDogsLegOutside,
        &gTestPointFenceRTurnsSouthInside
    }
};

#else

/** Fence R test data: keep the compiler happy.
 */
static const uGeofenceTestData_t gFenceRTestData = {&gFenceR, 0, 0};

#endif // #ifdef U_CFG_GEOFENCE_USE_GEODESIC

/* ----------------------------------------------------------------
 * VARIABLES: FENCE S, MULTIPLE SHAPES, SPACESHIP IN THE RED SEA
 * -------------------------------------------------------------- */

/** A vertex on the southern side of the entrance to the Gulf of Aden.
 */
static const uGeofenceTestVertex_t gVertexSIRS0 = {11000000000LL /* latX1e9 */,
                                                   50700000000LL /* lonX1e9 */
                                                  };

/** Next vertex along, at the other end of the Gulf of Aden,
 * between Berbera and Borama.
 */
static const uGeofenceTestVertex_t gVertexSIRS1 = {9580025800LL /* latX1e9 */,
                                                   43874214700LL /* lonX1e9 */
                                                  };

/** Next vertex along, north west of Djibouti.
 */
static const uGeofenceTestVertex_t gVertexSIRS2 = {11666983600LL /* latX1e9 */,
                                                   42324166900LL /* lonX1e9 */
                                                  };

/** Next vertex along, at the pincers on the entrance to the
 * Red Sea, on the west side, a place called Fagal.
 */
static const uGeofenceTestVertex_t gVertexSIRS3 = {12475031900LL /* latX1e9 */,
                                                   43320382400LL /* lonX1e9 */
                                                  };

/** Next vertex along, a quarter of the way up the Red Sea
 * on the west side.
 */
static const uGeofenceTestVertex_t gVertexSIRS4 = {14913199400LL /* latX1e9 */,
                                                   39586584900LL /* lonX1e9 */
                                                  };

/** Next vertex along, half way up the Red Sea on the west side.
 */
static const uGeofenceTestVertex_t gVertexSIRS5 = {18709535800LL /* latX1e9 */,
                                                   37168425000LL /* lonX1e9 */
                                                  };

/** Next vertex along, beyond the north end of the Red Sea,
 * in Egypt, west of the Suez Canal.
 */
static const uGeofenceTestVertex_t gVertexSIRS6 = {29975415200LL /* latX1e9 */,
                                                   31985355400LL /* lonX1e9 */
                                                  };

/** Next vertex along, having crossed the Suez Canal
 * to be on the eastern side of it.
 */
static const uGeofenceTestVertex_t gVertexSIRS7 = {30250737200LL /* latX1e9 */,
                                                   32593618300LL /* lonX1e9 */
                                                  };

/** Next vertex along, south again, on the pointy bit
 * between the gulfs of Suez and Arabia, nearish
 * Sharm El Sheikh.
 */
static const uGeofenceTestVertex_t gVertexSIRS8 = {27984220200LL /* latX1e9 */,
                                                   34148805400LL /* lonX1e9 */
                                                  };

/** Next vertex along, north again, at the other end
 * of the Arabian Gulf, nearish somewhere called Be'er
 * Ora, which probably doesn't sell either of the things
 * it should.
 */
static const uGeofenceTestVertex_t gVertexSIRS9 = {29718207400LL /* latX1e9 */,
                                                   34796146900LL /* lonX1e9 */
                                                  };

/** Next vertex along, on the western side of the
 * Arabian Gulf still, but above it.
 */
static const uGeofenceTestVertex_t gVertexSIRS10 = {29685865800LL /* latX1e9 */,
                                                    35325447100LL /* lonX1e9 */
                                                   };

/** Next vertex along, south again and on the eastern
 * side of the Aradiab Gulf from the Sharm El Sheikh
 * pointy bit.
 */
static const uGeofenceTestVertex_t gVertexSIRS11 = {28237239000LL /* latX1e9 */,
                                                    34881482600LL /* lonX1e9 */
                                                   };

/** Next vertex along, east of the previous one, preparing
 * to turn south.
 */
static const uGeofenceTestVertex_t gVertexSIRS12 = {28411087200LL /* latX1e9 */,
                                                    35669871300LL /* lonX1e9 */
                                                   };

/** Next vertex along, half way down the Red Sea on the
 * east side, opposing point 5.
 */
static const uGeofenceTestVertex_t gVertexSIRS13 = {21436339400LL /* latX1e9 */,
                                                    40220837300LL /* lonX1e9 */
                                                   };

/** Next vertex along, three quarters of the way down the
 * Red Sea on the east side, near Qaza'a, opposing
 * point 4.
 */
static const uGeofenceTestVertex_t gVertexSIRS14 = {16710587300LL /* latX1e9 */,
                                                    43037034600LL /* lonX1e9 */
                                                   };

/** Next vertex along, at the bottom of the Red Sea,
 * on the other side of those pincers, opposing point 3.
 */
static const uGeofenceTestVertex_t gVertexSIRS15 = {12735532000LL /* latX1e9 */,
                                                    43535978400LL /* lonX1e9 */
                                                   };

/** Next vertex along, on the north side of the Gulf
 * of Aden, opposing point 0.
 */
static const uGeofenceTestVertex_t gVertexSIRS16 = {15111794500LL /* latX1e9 */,
                                                    50700000000LL /* lonX1e9 */
                                                   };

/** An outer bounding point to complete the polygon,
 * far north of point 16, in the Caspian Sea.
 */
static const uGeofenceTestVertex_t gVertexSIRS17 = {38000000000LL /* latX1e9 */,
                                                    50700000000LL /* lonX1e9 */
                                                   };

/** Another outer bounding point to complete the polygon,
 * far east of point 17, in the Mediterranean.
 */
static const uGeofenceTestVertex_t gVertexSIRS18 = {38000000000LL /* latX1e9 */,
                                                    10000000000LL /* lonX1e9 */
                                                   };

/** Another outer bounding point to complete the polygon,
 * far south of point 18, on the equator in Equatorial
 * Guinea.
 */
static const uGeofenceTestVertex_t gVertexSIRS19 = {0LL /* latX1e9 */,
                                                    10000000000LL /* lonX1e9 */
                                                   };

/** Last outer bounding point to complete the polygon,
 * south of point 0, on the equator in the Arabian Sea
 * between Ethiopia and the Seychelles.
 */
static const uGeofenceTestVertex_t gVertexSIRS20 = {0LL /* latX1e9 */,
                                                    50700000000LL /* lonX1e9 */
                                                   };

/** A polygonal exclusion zone surrounding the Red Sea,
 * i.e. with the Red Sea _outside_ it, kind of like this:
 *```
 *    ....................
 *    .............. .....
 *    .............  .....
 *    .............   ....
 *    .............   ....
 *    ............... ....
 *    ...............
 *    ....................
 *```
 * ...with the entrance at the bottom-right.
 */
static const uGeofenceTestPolygon_t gExclusionZoneSIRS = {21,
    {
        &gVertexSIRS0, &gVertexSIRS1, &gVertexSIRS2, &gVertexSIRS3,
        &gVertexSIRS4, &gVertexSIRS5, &gVertexSIRS6, &gVertexSIRS7,
        &gVertexSIRS8, &gVertexSIRS9, &gVertexSIRS10, &gVertexSIRS11,
        &gVertexSIRS12, &gVertexSIRS13, &gVertexSIRS14, &gVertexSIRS15,
        &gVertexSIRS16, &gVertexSIRS17, &gVertexSIRS18, &gVertexSIRS19,
        &gVertexSIRS20
    }
};

/** A vertex about a third of the way up the middle of the Red Sea.
 */
static const uGeofenceTestVertex_t gVertexSIRS21 = {17848344100LL /* latX1e9 */,
                                                    40150912300LL /* lonX1e9 */
                                                   };

/** The alien vehicle, fully 120 km in radius.
 */
static const uGeofenceTestCircle_t gSpaceshipSIRS = {&gVertexSIRS21,
                                                     120000000UL /* radius mm */
                                                    };

/** Fence S: a huge polygonal exclusion zone which surrounds the
 * Red Sea to the south, west and north, leaving just the
 * narrow entrance via the Gulf of Aden from the south-east, and,
 * in the middle of the Red Sea, a saucer-shaped alien vehicle.
 */
static const uGeofenceTestFence_t gFenceS = {
    "S: first contact",
    10000000 /* altitude max mm */,
    0 /* altitude min mm */,
    1, {&gSpaceshipSIRS},
    1, {&gExclusionZoneSIRS}
};

#ifdef U_CFG_GEOFENCE_USE_GEODESIC

/** A vertex at the entrance to the Gulf of Aden.
 */
static const uGeofenceTestVertex_t gVertexAdenEntranceOutside = {13000000000LL /* latX1e9 */,
                                                                 51042582200LL /* lonX1e9 */
                                                                };

/** A vertex between the pincers at the entrance to the Red Sea.
 */
static const uGeofenceTestVertex_t gVertexRedSeaPincersOutside = {12550315000LL /* latX1e9 */,
                                                                  43349242100LL /* lonX1e9 */
                                                                 };

/** A vertex at Fagal, on-shore at the pincers.
 */
static const uGeofenceTestVertex_t gVertexFagalInside = {12461992200LL /* latX1e9 */,
                                                         43297027200LL /* lonX1e9 */
                                                        };

/** A vertex immediately in front of the spaceship on the south side.
 */
static const uGeofenceTestVertex_t gVertexSpaceshipSouthOutside = {16908123200LL /* latX1e9 */,
                                                                   40768272500LL /* lonX1e9 */
                                                                  };

/** A vertex on the western side of the spaceship, still in the water.
 */
static const uGeofenceTestVertex_t gVertexSpaceshipWestOutside = {17459263600LL /* latX1e9 */,
                                                                  38904684300LL /* lonX1e9 */
                                                                 };

/** A vertex at the old government building, Suez.
 */
static const uGeofenceTestVertex_t gVertexSuezOutside = {29963787500LL /* latX1e9 */,
                                                         32551537700LL /* lonX1e9 */
                                                        };

/** A vertex at the north end of the Suez Canal, in the exclusion zone.
 */
static const uGeofenceTestVertex_t gVertexSuezCanalInside = {30262277400LL /* latX1e9 */,
                                                             32502519800LL /* lonX1e9 */
                                                            };

/** A vertex in the harbour at Aqaba, at the top of the Arabian Gulf.
 */
static const uGeofenceTestVertex_t gVertexAqabaOutside = {29547231800LL /* latX1e9 */,
                                                          34988385400LL /* lonX1e9 */
                                                         };

/** A vertex at the Ilan and Asaf Ramon Internetional Airport,
 * in the exclusion zone.
 */
static const uGeofenceTestVertex_t gVertexAirportInside = {29724895700LL /* latX1e9 */,
                                                           35005606700LL /* lonX1e9 */
                                                          };

/** A vertex at the entrance to the Arabian Gulf.
 */
static const uGeofenceTestVertex_t gVertexArabianGulfOutside = {27984213100LL /* latX1e9 */,
                                                                34444908600LL /* lonX1e9 */
                                                               };

/** A vertex on the Red Sea on the eastern side of the spaceship.
 */
static const uGeofenceTestVertex_t gVertexSpaceshipEastOutside = {18413545500LL /* latX1e9 */,
                                                                  41299237200LL /* lonX1e9 */
                                                                 };

/** A vertex just inside the spaceship on the eastern side.
 */
static const uGeofenceTestVertex_t gVertexSpaceshipInside = {18345033800LL /* latX1e9 */,
                                                             41147582400LL /* lonX1e9 */
                                                            };

/** A vertex in Istanbul.
 */
static const uGeofenceTestVertex_t gVertexIstanbulOutside = {41001729500LL /* latX1e9 */,
                                                             28973405300LL /* lonX1e9 */
                                                            };

/** A vertex in Palermo, in the exclusion zone.
 */
static const uGeofenceTestVertex_t gVertexPalermoInside = {37984495000LL /* latX1e9 */,
                                                           13706611800LL /* lonX1e9 */
                                                          };

/** A vertex on the shores of Lamu on the eastern edge of Kenya.
 */
static const uGeofenceTestVertex_t gVertexKenyaOutside = {-2301540000LL /* latX1e9 */,
                                                          40872179300LL /* lonX1e9 */
                                                         };

/** 1: entering the Gulf of Aden, on our way to make history.
 */
static const uGeofenceTestPoint_t gTestPointFenceSAdenEntranceOutside = {
    &gVertexAdenEntranceOutside,
    {100000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 2: entering the Red Sea.
 */
static const uGeofenceTestPoint_t gTestPointFenceSRedSeaPincersOutside = {
    &gVertexRedSeaPincersOutside,
    {100000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 3: called to a conference at Fagal, inside the exclusion zone.
 */
static const uGeofenceTestPoint_t gTestPointFenceSFagalInside = {
    &gVertexFagalInside,
    {10000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 4: directly in front of the spaceship on the south side, taking
 * a very close look indeed.
 */
static const uGeofenceTestPoint_t gTestPointFenceSSpaceshipSouthOutside = {
    &gVertexSpaceshipSouthOutside,
    {10000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit back to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 5: skirting around the western side of the spaceship, still on
 * the water.
 */
static const uGeofenceTestPoint_t gTestPointFenceSSpaceshipWestOutside = {
    &gVertexSpaceshipWestOutside,
    {10000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 6: in Suez, at the old government building, for another conference.
 */
static const uGeofenceTestPoint_t gTestPointFenceSSuezOutside = {
    &gVertexSuezOutside,
    {10000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 7: checking that the Suez Canal is clear, entering the exclusion
 * zone as a result.
 */
static const uGeofenceTestPoint_t gTestPointFenceSSuezCanalInside = {
    &gVertexSuezCanalInside,
    {10000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 8: moored at Aqaba, at the top of the Arabian Gulf, outside again.
 */
static const uGeofenceTestPoint_t gTestPointFenceSAqabaOutside = {
    &gVertexAqabaOutside,
    {10000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit back to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 9: stop to pick up scientific experts at Ilan and Asaf Ramon
 * Internetional Airport, in the exclusion zone, north of Aqaba.
 */
static const uGeofenceTestPoint_t gTestPointFenceSAirportInside = {
    &gVertexAirportInside,
    {5000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit back to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 10: back at the mouth of the Arabian Gulf, now on a mission.
 */
static const uGeofenceTestPoint_t gTestPointFenceSArabianGulfOutside = {
    &gVertexArabianGulfOutside,
    {10000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit back to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 11: squeezing past the eastern side of the spaceship.
 */
static const uGeofenceTestPoint_t gTestPointFenceSSpaceshipEastOutside = {
    &gVertexSpaceshipEastOutside,
    {10000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 12: was that a door, a way in?  We're inside, my god it's full
 * of... crackle... crackle... silence.
 */
static const uGeofenceTestPoint_t gTestPointFenceSSpaceshipInside = {
    &gVertexSpaceshipInside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit back to
    // inside, for a moment anyway...
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 13: the view from a check-point in Istanbul, outside the
 * exclusion zone.
 */
static const uGeofenceTestPoint_t gTestPointFenceSIstanbulOutside = {
    &gVertexIstanbulOutside,
    {10000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit back to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 14: the view from a holiday in Palermo, but in the danger zone.
 */
static const uGeofenceTestPoint_t gTestPointFenceSPalermoInside = {
    &gVertexPalermoInside,
    {10000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit back to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 15: on the beach at Lamu, far south of the action, in Kenya.
 */
static const uGeofenceTestPoint_t gTestPointFenceSKenyaOutside = {
    &gVertexKenyaOutside,
    {100000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // Both the optimist and the pessimist see a transit back to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 16: in a spy plane, directly over the alien vehicle.
 */
static const uGeofenceTestPoint_t gTestPointFenceSAbove = {
    &gVertexSIRS21,
    {10000 /* radius mm */, 11000000 /* altitude mm */, 10000 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** Fence S test data: first contact.
 */
static const uGeofenceTestData_t gFenceSTestData = {
    &gFenceS,
    500000000LL /* star radius mm */,
    16, // Update this if you add a test point
    {
        &gTestPointFenceSAdenEntranceOutside, &gTestPointFenceSRedSeaPincersOutside,
        &gTestPointFenceSFagalInside, &gTestPointFenceSSpaceshipSouthOutside,
        &gTestPointFenceSSpaceshipWestOutside, &gTestPointFenceSSuezOutside,
        &gTestPointFenceSSuezCanalInside, &gTestPointFenceSAqabaOutside,
        &gTestPointFenceSAirportInside, &gTestPointFenceSArabianGulfOutside,
        &gTestPointFenceSSpaceshipEastOutside, &gTestPointFenceSSpaceshipInside,
        &gTestPointFenceSIstanbulOutside, &gTestPointFenceSPalermoInside,
        &gTestPointFenceSKenyaOutside, &gTestPointFenceSAbove
    }
};

#else

/** Fence S test data: keep the compiler happy.
 */
static const uGeofenceTestData_t gFenceSTestData = {&gFenceS, 0, 0};

#endif // #ifdef U_CFG_GEOFENCE_USE_GEODESIC

/* ----------------------------------------------------------------
 * VARIABLES: FENCE T, HEMISPHERES
 * -------------------------------------------------------------- */

/** A circle representing the northern hemisphere which ends just
 * north of the equator (9,900,000 km in radius).
 */
static const uGeofenceTestCircle_t gCircleHemispherNorth = {&gVertexNorthPole,
                                                            9900000000UL /* radius mm */
                                                           };

/** A vertex at the south pole.
 */
static const uGeofenceTestVertex_t gVertexSouthPole = {-89999999999LL /* latX1e9 */,
                                                       0LL /* lonX1e9 */
                                                      };

/** A circle representing the southern hemisphere which ends just
 * south of the equator (9,900,000 km in radius).
 */
static const uGeofenceTestCircle_t gCircleHemispherSouth = {&gVertexSouthPole,
                                                            9900000000UL /* radius mm */
                                                           };

/** Fence T: containing both hemispheres but with a gap
 * left at the equator sufficiently large that we can tell
 * which is which in both the geodesic and spherical cases.
 */
static const uGeofenceTestFence_t gFenceT = {
    "T: hemispheres",
    INT_MAX /* altitude max mm */,
    INT_MIN /* altitude min mm */,
    2, {&gCircleHemispherNorth, &gCircleHemispherSouth},
    0
};

/** A vertex on the equator at 90 longitude.
 */
static const uGeofenceTestVertex_t gVertexEquator90 = {0LL /* latX1e9 */,
                                                       90000000000LL /* lonX1e9 */
                                                      };

/** A vertex on the equator at +179.999999999 longitude.
 */
static const uGeofenceTestVertex_t gVertexEquator180 = {0LL /* latX1e9 */,
                                                        179999999999LL /* lonX1e9 */
                                                       };

/** A vertex on the equator at -90 longitude.
 */
static const uGeofenceTestVertex_t gVertexEquator270 = {0LL /* latX1e9 */,
                                                        -90000000000LL /* lonX1e9 */
                                                       };

/** 1: on the equator at 0 longitude.
 */
static const uGeofenceTestPoint_t gTestPointFenceTEquatorZeroLongitude = {
    &gVertexOrigin,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 2: on the equator at 0 longitude but with sufficient
 * uncertainty that we might be in either hemisphere.
 */
static const uGeofenceTestPoint_t gTestPointFenceTEquatorZeroLongitudeUncertain = {
    &gVertexOrigin,
    {1000000000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // The pessimist see a transit to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 3: as (1) but at 90 longitude.
 */
static const uGeofenceTestPoint_t gTestPointFenceTEquator90Longitude = {
    &gVertexEquator90,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // The pessimist see a transit back to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 4: as (2) but at 90 longitude.
 */
static const uGeofenceTestPoint_t gTestPointFenceTEquator90LongitudeUncertain = {
    &gVertexOrigin,
    {1000000000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // The pessimist see a transit to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 5: as (1) but at 179.999999999 longitude.
 */
static const uGeofenceTestPoint_t gTestPointFenceTEquator180Longitude = {
    &gVertexEquator180,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // The pessimist see a transit back to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 6: as (2) but at 179.999999999 longitude.
 */
static const uGeofenceTestPoint_t gTestPointFenceTEquator180LongitudeUncertain = {
    &gVertexEquator180,
    {1000000000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // The pessimist see a transit to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 7: as (1) but at -90 longitude.
 */
static const uGeofenceTestPoint_t gTestPointFenceTEquator270Longitude = {
    &gVertexEquator270,
    {0 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // The pessimist see a transit back to outside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 8: as (2) but at -90 longitude.
 */
static const uGeofenceTestPoint_t gTestPointFenceTEquator270LongitudeUncertain = {
    &gVertexEquator270,
    {1000000000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    // The pessimist see a transit to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 9: at the north pole, still with large uncertainty but
 * not enough to make a difference.
 */
static const uGeofenceTestPoint_t gTestPointFenceTNorthPole = {
    &gVertexNorthPole,
    {1000000000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // Even the optimist now agrees, we're inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 10: at the south pole, still with large uncertainty but
 * not enough to make a difference.
 */
static const uGeofenceTestPoint_t gTestPointFenceTSouthPole = {
    &gVertexSouthPole,
    {1000000000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST)
};

/** Fence T test data: points along the equator and at the
 * poles plus one existing "inside" test point that is known
 * to fall into the northern hemisphere and happens to have
 * the right transit outcome.
 */
static const uGeofenceTestData_t gFenceTTestData = {
    &gFenceT,
    1000000000LL /* star radius mm */,
    11, // Update this if you add a test point
    {
        &gTestPointFenceTEquatorZeroLongitude, &gTestPointFenceTEquatorZeroLongitudeUncertain,
        &gTestPointFenceTEquator90Longitude, &gTestPointFenceTEquator90LongitudeUncertain,
        &gTestPointFenceTEquator180Longitude, &gTestPointFenceTEquator180LongitudeUncertain,
        &gTestPointFenceTEquator270Longitude, &gTestPointFenceTEquator270LongitudeUncertain,
        &gTestPointFenceTNorthPole, &gTestPointFenceTSouthPole,
        &gTestPointFenceHTLBITW
    }
};

/* ----------------------------------------------------------------
 * VARIABLES: FENCE U, MULTIPLE SMALL SHAPES, A CROP "CIRCLE"
 * -------------------------------------------------------------- */

/** Vertex 0 of the crop circle, top left.
 */
static const uGeofenceTestVertex_t gVertexCC0 = {51355161800LL /* latX1e9 */,
                                                 -1855959900LL /* lonX1e9 */
                                                };
/** Vertex 1 of the crop circle, bottom left.
 */
static const uGeofenceTestVertex_t gVertexCC1 = {51354441600LL /* latX1e9 */,
                                                 -1855970600LL /* lonX1e9 */
                                                };

/** Vertex 2 of the crop circle, left of the entrance.
 */
static const uGeofenceTestVertex_t gVertexCC2 = {51354444900LL /* latX1e9 */,
                                                 -1854884300LL /* lonX1e9 */
                                                };

/** Vertex 3 of the crop circle, above point (2).
 */
static const uGeofenceTestVertex_t gVertexCC3 = {51354527000LL /* latX1e9 */,
                                                 -1854884300LL /* lonX1e9 */
                                                };

/** Vertex 4 of the crop circle, inside lower left.
 */
static const uGeofenceTestVertex_t gVertexCC4 = {51354521100LL /* latX1e9 */,
                                                 -1855768100LL /* lonX1e9 */
                                                };

/** Vertex 5 of the crop circle, inside top left.
 */
static const uGeofenceTestVertex_t gVertexCC5 = {51355044600LL /* latX1e9 */,
                                                 -1855713100LL /* lonX1e9 */
                                                };

/** Vertex 6 of the crop circle, the first "tooth".
 */
static const uGeofenceTestVertex_t gVertexCC6 = {51354597400LL /* latX1e9 */,
                                                 -1855251800LL /* lonX1e9 */
                                                };

/** Vertex 7 of the crop circle, between the first
 * tooth and the middle tooth.
 */
static const uGeofenceTestVertex_t gVertexCC7 = {51355047900LL /* latX1e9 */,
                                                 -1854978200LL /* lonX1e9 */
                                                };

/** Vertex 8 of the crop circle, middle tooth.
 */
static const uGeofenceTestVertex_t gVertexCC8 = {51354585600LL /* latX1e9 */,
                                                 -1854846800LL /* lonX1e9 */
                                                };

/** Vertex 9 of the crop circle, between the middle
 * tooth and the third tooth.
 */
static const uGeofenceTestVertex_t gVertexCC9 = {51355051300LL /* latX1e9 */,
                                                 -1854699300LL /* lonX1e9 */
                                                };

/** Vertex 10 of the crop circle, third tooth.
 */
static const uGeofenceTestVertex_t gVertexCC10 = {51354600700LL /* latX1e9 */,
                                                  -1854436400LL /* lonX1e9 */
                                                 };

/** Vertex 11 of the crop circle, inside top right.
 */
static const uGeofenceTestVertex_t gVertexCC11 = {51355041200LL /* latX1e9 */,
                                                  -1854060900LL /* lonX1e9 */
                                                 };

/** Vertex 12 of the crop circle, inside lower right.
 */
static const uGeofenceTestVertex_t gVertexCC12 = {51354516100LL /* latX1e9 */,
                                                  -1854008600LL /* lonX1e9 */
                                                 };

/** Vertex 13 of the crop circle, top of the entrance
 * on the right.
 */
static const uGeofenceTestVertex_t gVertexCC13 = {51354527800LL /* latX1e9 */,
                                                  -1854805200LL /* lonX1e9 */
                                                 };

/** Vertex 14 of the crop circle, below (13).
 */
static const uGeofenceTestVertex_t gVertexCC14 = {51354447400LL /* latX1e9 */,
                                                  -1854805200LL /* lonX1e9 */
                                                 };

/** Vertex 15 of the crop circle, bottom right.
 */
static const uGeofenceTestVertex_t gVertexCC15 = {51354438200LL /* latX1e9 */,
                                                  -1853873100LL /* lonX1e9 */
                                                 };

/** Vertex 16 of the crop circle, top right.
 */
static const uGeofenceTestVertex_t gVertexCC16 = {51355148400LL /* latX1e9 */,
                                                  -1853862400LL /* lonX1e9 */
                                                 };

/** A polygon representing the outer portion of a "crop circle",
 * something like this:
 *```
 *    .....................
 *    .. .... ..... .... ..
 *    ..  ...  ...  ...  ..
 *    ..   ..   .   ..   ..
 *    ..  x . x . x .  x ..
 *    ..                 ..
 *    ........... .........
 *```
 * ...with the narrow entrance at the bottom.  The x's mark the
 * centres of the four circles that follow, each of which
 * are sized so that they don't touch the bounding polygon.
 */
static const uGeofenceTestPolygon_t gPolygonCC = {17,
    {
        &gVertexCC0, &gVertexCC1, &gVertexCC2, &gVertexCC3,
        &gVertexCC4, &gVertexCC5, &gVertexCC6, &gVertexCC7,
        &gVertexCC8, &gVertexCC9, &gVertexCC10, &gVertexCC11,
        &gVertexCC12, &gVertexCC13, &gVertexCC14, &gVertexCC15,
        &gVertexCC16
    }
};

/** A vertex between the inner left hand side of the crop circle
 * polygon and the first tooth.
 */
static const uGeofenceTestVertex_t gVertexCC17 = {51354627000LL /* latX1e9 */,
                                                  -1855549500LL /* lonX1e9 */
                                                 };

/** A circle at vertex 17, small enough not to touch any part
 * of the outer polygon, but only just (10 metre radius).
 */
static const uGeofenceTestCircle_t gCircleCCOne = {&gVertexCC17,
                                                   10000UL /* radius mm */
                                                  };

/** A vertex between the first two teeth of the crop circle
 * polygon.
 */
static const uGeofenceTestVertex_t gVertexCC18 = {51354625300LL /* latX1e9 */,
                                                  -1855069300LL /* lonX1e9 */
                                                 };

/** A circle at vertex 18, small enough not to touch any part
 * of the outer polygon, 10 metre radius.
 */
static const uGeofenceTestCircle_t gCircleCCTwo = {&gVertexCC18,
                                                   10000UL /* radius mm */
                                                  };

/** A vertex between the second and third teeth of the crop
 * circle polygon.
 */
static const uGeofenceTestVertex_t gVertexCC19 = {51354613400LL /* latX1e9 */,
                                                  -1854651900LL /* lonX1e9 */
                                                 };

/** A circle at vertex 19, small enough not to touch any part
 * of the outer polygon, 10 metre radius.
 */
static const uGeofenceTestCircle_t gCircleCCThree = {&gVertexCC19,
                                                     10000UL /* radius mm */
                                                    };

/** A vertex between the third tooth of the crop circle
 * and the edge side of the bounding polygon.
 */
static const uGeofenceTestVertex_t gVertexCC20 = {51354637000LL /* latX1e9 */,
                                                  -1854192300LL /* lonX1e9 */
                                                 };

/** A circle at vertex 20, small enough not to touch any part
 * of the outer polygon, 10 metre radius.
 */
static const uGeofenceTestCircle_t gCircleCCFour = {&gVertexCC20,
                                                    10000UL /* radius mm */
                                                   };

/** Fence U: a "crop circle", though not really a circle
 * in this case but a somewhat ugly outer polygon with
 * "teeth" pointing south, between each of which is a
 * 10 metre radius circle.
 */
static const uGeofenceTestFence_t gFenceU = {
    "U: crop \"circle\"",
    INT_MAX /* altitude max mm */,
    INT_MIN /* altitude min mm */,
    4, {&gCircleCCOne, &gCircleCCTwo, &gCircleCCThree, &gCircleCCFour},
    1, {&gPolygonCC}
};

/** A vertex entirely outside the crop circle, below circle one.
 */
static const uGeofenceTestVertex_t gVertexCCLeftOuterOutside = {51354387500LL /* latX1e9 */,
                                                                -1855549400LL /* lonX1e9 */
                                                               };

/** A vertex entirely outside the crop circle, below the entrance.
 */
static const uGeofenceTestVertex_t gVertexCCMiddleOuterOutside = {51354397600LL /* latX1e9 */,
                                                                  -1854846700LL /* lonX1e9 */
                                                                 };

/** A vertex entirely outside the crop circle, below circle four.
 */
static const uGeofenceTestVertex_t gVertexCCRightOuterOutside = {51354342300LL /* latX1e9 */,
                                                                 -1854028600LL /* lonX1e9 */
                                                                };

/** A vertex within the bounding polygon below circle one but still
 * outside of either shape.
 */
static const uGeofenceTestVertex_t gVertexCCLeftLowerInnerOutside = {51354531600LL /* latX1e9 */,
                                                                     -1855603000LL /* lonX1e9 */
                                                                    };

/** A vertex within the bounding polygon above circle one and still
 * outside of either shape.
 */
static const uGeofenceTestVertex_t gVertexCCLeftUpperInnerOutside = {51354838100LL /* latX1e9 */,
                                                                     -1855624500LL /* lonX1e9 */
                                                                    };

/** A vertex within the bounding polygon above circle two and still
 * outside of either shape.
 */
static const uGeofenceTestVertex_t gVertexCCMiddleLeftUpperInnerOutside = {51354819700LL /* latX1e9 */,
                                                                           -1855021000LL /* lonX1e9 */
                                                                          };

/** A vertex right in the middle of the entrance-way.
 */
static const uGeofenceTestVertex_t gVertexCCMiddleEntranceOutside = {51354480500LL /* latX1e9 */,
                                                                     -1854841300LL /* lonX1e9 */
                                                                    };

/** A vertex within the bounding polygon above circle three and still
 * outside of either shape.
 */
static const uGeofenceTestVertex_t gVertexCCMiddleRightUpperInnerOutside = {51354480500LL /* latX1e9 */,
                                                                            -1854841300LL /* lonX1e9 */
                                                                           };

/** A vertex within the bounding polygon below circle four and still
 * outside of either shape.
 */
static const uGeofenceTestVertex_t gVertexCCRightLowerInnerOutside = {51354539100LL /* latX1e9 */,
                                                                      -1854117100LL /* lonX1e9 */
                                                                     };

/** A vertex within the bounding polygon above circle four and outside
 * of either shape.
 */
static const uGeofenceTestVertex_t gVertexCCRightUpperInnerOutside = {51354833100LL /* latX1e9 */,
                                                                      -1854152000LL /* lonX1e9 */
                                                                     };

/** A vertex within the wall of the polygon on the lower left.
 */
static const uGeofenceTestVertex_t gVertexCCLeftLowerInside = {51354478700LL /* latX1e9 */,
                                                               -1855476500LL /* lonX1e9 */
                                                              };

/** A vertex within the first tooth.
 */
static const uGeofenceTestVertex_t gVertexCCToothOneInside = {51354669600LL /* latX1e9 */,
                                                              -1855288700LL /* lonX1e9 */
                                                             };

/** A vertex within the middle tooth.
 */
static const uGeofenceTestVertex_t gVertexCCToothTwoInside = {51354676300LL /* latX1e9 */,
                                                              -1854843500LL /* lonX1e9 */
                                                             };

/** A vertex within the third tooth.
 */
static const uGeofenceTestVertex_t gVertexCCToothThreeInside = {51354669600LL /* latX1e9 */,
                                                                -1854430400LL /* lonX1e9 */
                                                               };

/** A vertex within the wall of the polygon on the lower right.
 */
static const uGeofenceTestVertex_t gVertexCCRightLowerInside = {51354483700LL /* latX1e9 */,
                                                                -1854556500LL /* lonX1e9 */
                                                               };

/** A vertex within circle one.
 */
static const uGeofenceTestVertex_t gVertexCCCircleOneInside = {51354632800LL /* latX1e9 */,
                                                               -1855465800LL /* lonX1e9 */
                                                              };

/** A vertex within circle two.
 */
static const uGeofenceTestVertex_t gVertexCCCircleTwoInside = {51354631100LL /* latX1e9 */,
                                                               -1854996400LL /* lonX1e9 */
                                                              };

/** A vertex within circle three.
 */
static const uGeofenceTestVertex_t gVertexCCCircleThreeInside = {51354617700LL /* latX1e9 */,
                                                                 -1854714800LL /* lonX1e9 */
                                                                };

/** A vertex within circle four.
 */
static const uGeofenceTestVertex_t gVertexCCCircleFourInside = {51354594200LL /* latX1e9 */,
                                                                -1854105900LL /* lonX1e9 */
                                                               };

/** 1: entirely outside the crop circle on the left.
 */
static const uGeofenceTestPoint_t gTestPointFenceULeftOuterOutside = {
    &gVertexCCLeftOuterOutside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 2: entirely outside the crop circle in the middle, below the entrance.
 */
static const uGeofenceTestPoint_t gTestPointFenceUMiddleOuterOutside = {
    &gVertexCCMiddleOuterOutside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 3: entirely outside the crop circle on the right.
 */
static const uGeofenceTestPoint_t gTestPointFenceURightOuterOutside = {
    &gVertexCCRightOuterOutside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 4: within the bounding polygon, below circle one but outside both
 * shapes.
 */
static const uGeofenceTestPoint_t gTestPointFenceULeftLowerInnerOutside = {
    &gVertexCCLeftLowerInnerOutside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 5: within the bounding polygon, above circle one and still outside both
 * shapes.
 */
static const uGeofenceTestPoint_t gTestPointFenceULeftUpperInnerOutside = {
    &gVertexCCLeftUpperInnerOutside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 6: within the bounding polygon, above circle two and still outside both
 * shapes.
 */
static const uGeofenceTestPoint_t gTestPointFenceUMiddleLeftUpperInnerOutside = {
    &gVertexCCMiddleLeftUpperInnerOutside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 7: right in the middle of the entrance to the crop circle.
 */
static const uGeofenceTestPoint_t gTestPointFenceUMiddleEntranceOutside = {
    &gVertexCCMiddleEntranceOutside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 8: as (7) but with sufficient uncertainty that we might be within
 * the walls of the bounding polygon.
 */
static const uGeofenceTestPoint_t gTestPointFenceUMiddleEntranceOutsideUncertain = {
    &gVertexCCMiddleEntranceOutside,
    {3000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    // The pessimist sees a transit to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 9: within the bounding polygon, above circle three and still outside
 * both shapes.
 */
static const uGeofenceTestPoint_t gTestPointFenceUMiddleRightUpperInnerOutside = {
    &gVertexCCMiddleRightUpperInnerOutside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST) |
    // The pessimist sees a transit to outside again
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST)
};

/** 10: within the bounding polygon, below circle four and still outside
 * both shapes.
 */
static const uGeofenceTestPoint_t gTestPointFenceURightLowerInnerOutside = {
    &gVertexCCRightLowerInnerOutside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 11: finally for the outsides, within the bounding polygon and above
 * circle four and still outside both shapes.
 */
static const uGeofenceTestPoint_t gTestPointFenceURightUpperInnerOutside = {
    &gVertexCCRightUpperInnerOutside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_OUTSIDE_PESSIMIST)
};

/** 12: inside the bounding polygon wall on the lower left.
 */
static const uGeofenceTestPoint_t gTestPointFenceULeftLowerInside = {
    &gVertexCCLeftLowerInside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST) |
    // Both the pessimist and the optimist see a transit to inside
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_PESSIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_TRANSIT_OPTIMIST)
};

/** 13: inside the first tooth.
 */
static const uGeofenceTestPoint_t gTestPointFenceUToothOneInside = {
    &gVertexCCToothOneInside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST)
};

/** 14: inside the second tooth.
 */
static const uGeofenceTestPoint_t gTestPointFenceUToothTwoInside = {
    &gVertexCCToothTwoInside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST)
};

/** 15: inside the third tooth.
 */
static const uGeofenceTestPoint_t gTestPointFenceUToothThreeInside = {
    &gVertexCCToothThreeInside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST)
};

/** 16: inside the bounding polygon wall on the lower right.
 */
static const uGeofenceTestPoint_t gTestPointFenceURightLowerInside = {
    &gVertexCCRightLowerInside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST)
};

/** 17: inside circle one.
 */
static const uGeofenceTestPoint_t gTestPointFenceUCircleOneInside = {
    &gVertexCCCircleOneInside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST)
};

/** 18: inside circle two.
 */
static const uGeofenceTestPoint_t gTestPointFenceUCircleTwoInside = {
    &gVertexCCCircleTwoInside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST)
};

/** 19: inside circle three.
 */
static const uGeofenceTestPoint_t gTestPointFenceUCircleThreeInside = {
    &gVertexCCCircleThreeInside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST)
};

/** 20: inside circle four.
 */
static const uGeofenceTestPoint_t gTestPointFenceUCircleFourInside = {
    &gVertexCCCircleFourInside,
    {1000 /* radius mm */, 0 /* altitude mm */, 0 /* altitude uncertainty mm */},
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_OPTIMIST) |
    (1U << U_GEOFENCE_TEST_PARAMETERS_INSIDE_PESSIMIST)
};

/** Fence U test data: points scattered around inside and outside
 * a crop "circle" consisting of a non-closed outer polygon and
 * three circles within it.
 */
static const uGeofenceTestData_t gFenceUTestData = {
    &gFenceU,
    100000LL /* star radius mm */,
    20, // Update this if you add a test point
    {
        &gTestPointFenceULeftOuterOutside, &gTestPointFenceUMiddleOuterOutside,
        &gTestPointFenceURightOuterOutside,
        &gTestPointFenceULeftLowerInnerOutside, &gTestPointFenceULeftUpperInnerOutside,
        &gTestPointFenceUMiddleLeftUpperInnerOutside,
        &gTestPointFenceUMiddleEntranceOutside, &gTestPointFenceUMiddleEntranceOutsideUncertain,
        &gTestPointFenceUMiddleRightUpperInnerOutside, &gTestPointFenceURightLowerInnerOutside,
        &gTestPointFenceURightUpperInnerOutside,
        &gTestPointFenceULeftLowerInside,
        &gTestPointFenceUToothOneInside, &gTestPointFenceUToothTwoInside, &gTestPointFenceUToothThreeInside,
        &gTestPointFenceURightLowerInside,
        &gTestPointFenceUCircleOneInside, &gTestPointFenceUCircleTwoInside,
        &gTestPointFenceUCircleThreeInside, &gTestPointFenceUCircleFourInside
    }
};

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** The fence test data.
 */
const uGeofenceTestData_t *gpUGeofenceTestData[] = {
    &gFenceATestData, &gFenceBTestData, &gFenceCTestData, &gFenceDTestData,
    &gFenceETestData, &gFenceFTestData, &gFenceGTestData, &gFenceHTestData,
    &gFenceITestData, &gFenceJTestData, &gFenceKTestData, &gFenceLTestData,
    &gFenceMTestData, &gFenceNTestData, &gFenceOTestData, &gFencePTestData,
    &gFenceQTestData, &gFenceRTestData, &gFenceSTestData, &gFenceTTestData,
    &gFenceUTestData
};

/** Number of items in the gpUGeofenceTestData array,
 * has to be done in this file and externed or GCC complains about
 * asking for the size of a partially defined type.
 */
const size_t gpUGeofenceTestDataSize = sizeof(gpUGeofenceTestData) /
                                       sizeof(gpUGeofenceTestData[0]);

#endif // #ifdef U_CFG_GEOFENCE

// End of file
