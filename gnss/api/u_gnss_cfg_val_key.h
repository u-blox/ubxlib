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

#ifndef _U_GNSS_CFG_VAL_KEY_H_
#define _U_GNSS_CFG_VAL_KEY_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _GNSS
 *  @{
 */

/** @file
 * @brief This header file defines macros and types that may be used
 * with the VALSET/VALGET/VALDEL generic configuration API.  In
 * particular, note the full set of U_GNSS_CFG_VAL_KEY_ID_XXX items, right
 * at the end of this file, which can be used as keyId when calling the
 * uGnssCfgValGet()/uGnssCfgValSet()/uGnssCfgValDel() functions and in
 * the #uGnssCfgVal_t structure when calling the uGnssCfgValSetList()/
 * uGnssCfgValDelList() functions.
 */

/* NOTE TO MAINTAINERS: when updating this file modify/add enumerations
 * ONLY and follow the existing naming patterns.  Do NOT edit the area
 * that is marked for automatic update, instead run the
 * u_gnss_cfg_val_key.py Python script when you have finished editing
 * the enumerations and it will update that part automagically.
 *
 * Also please do update the version in U_GNSS_CFG_VAL_VERSION as required.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS: KEY EXAMINATION AND CREATION
 * -------------------------------------------------------------- */

/** The protocol version for the CFG VAL entities.
 */
#define U_GNSS_CFG_VAL_VERSION 34.00

/** A key group ID which means "all groups", the wildcard.
 */
#define U_GNSS_CFG_VAL_KEY_GROUP_ID_ALL 0xFFF

/** A key item ID which means "all items", the wildcard.
 */
#define U_GNSS_CFG_VAL_KEY_ITEM_ID_ALL 0xFFFF

/** Macro to get the item ID from a key ID.
 * Note that the reserved part is included since that's how "all" is indicated.
 */
#define U_GNSS_CFG_VAL_KEY_GET_ITEM_ID(keyId)  (((uint32_t) (keyId)) & 0xFFFF)

/** Macro to get the group ID (#uGnssCfgValKeyGroupId_t) from a key ID.
 * Note that the reserved part is included since that's how "all" is indicated.
 */
#define U_GNSS_CFG_VAL_KEY_GET_GROUP_ID(keyId)  ((((uint32_t) (keyId)) >> 16) & 0xFFF)

/** Macro to get the storage size (#uGnssCfgValKeySize_t) from a key ID.
 */
#define U_GNSS_CFG_VAL_KEY_GET_SIZE(keyId) ((uGnssCfgValKeySize_t) ((((uint32_t) (keyId)) >> 28) & 0x07))

/** Macro to create a key ID given the group ID, item ID and size.
 */
#define U_GNSS_CFG_VAL_KEY(groupId, itemId, size) ((((uint32_t) (size) & 0x07) << 28)     | \
                                                   (((uint32_t) (groupId) & 0xFFF) << 16) | \
                                                   (((uint32_t) (itemId)) & 0xFFFF))

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* The name of this enum MUST be uGnssCfgValKeySize_t, every
 * entry must begin with U_GNSS_CFG_VAL_KEY_SIZE_ and all entries
 * must have a hard-coded value, otherwise the u_gnss_cfg_val_key.py
 * script that updates the macros at the end of the file will not work.
*/
/** The storage sizes for the VALSET/VALGET/VALDEL API.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_SIZE_NONE        = 0x00,
    U_GNSS_CFG_VAL_KEY_SIZE_ONE_BIT     = 0x01,
    U_GNSS_CFG_VAL_KEY_SIZE_ONE_BYTE    = 0x02,
    U_GNSS_CFG_VAL_KEY_SIZE_TWO_BYTES   = 0x03,
    U_GNSS_CFG_VAL_KEY_SIZE_FOUR_BYTES  = 0x04,
    U_GNSS_CFG_VAL_KEY_SIZE_EIGHT_BYTES = 0x05
} uGnssCfgValKeySize_t;

/* The name of this enum MUST be uGnssCfgValKeyGroupId_t, every
 * entry must begin with U_GNSS_CFG_VAL_KEY_GROUP_ID_ and all entries
 * must have a hard-coded value, otherwise the u_gnss_cfg_val_key.py
 * script that updates the macros at the end of the file will not work.
 * In addition, the bit after U_GNSS_CFG_VAL_KEY_GROUP_ID_ must
 * have a matching enumeration elsewhere in this file: for
 * instance U_GNSS_CFG_VAL_KEY_GROUP_ID_ANA matches the enum
 * named uGnssCfgValKeyItemAna_t; the entries in those enums must
 * also have hard-coded values.
 */
/** The group IDs for the VALSET/VALGET/VALDEL API.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_GROUP_ID_ANA           = 0x23, /**< AssistNow Autonomous and Offline configuration;
                                                           for items in this group see #uGnssCfgValKeyItemAna_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_BATCH         = 0x26, /**< batched output configuration; for items in this
                                                           group see #uGnssCfgValKeyItemBatch_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_BDS           = 0x34, /**< BeiDou system configuration, see #uGnssCfgValKeyItemBds_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_GEOFENCE      = 0x24, /**< geofencing configuration; for items in this
                                                           group see #uGnssCfgValKeyItemGeofence_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_HW            = 0xa3, /**< hardware configuration; for items in this
                                                           group see #uGnssCfgValKeyItemHw_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_I2C           = 0x51, /**< configuration of the I2C interface; for items in this
                                                           group see #uGnssCfgValKeyItemI2c_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_I2CINPROT     = 0x71, /**< input protocol configuration of the I2C interface;
                                                           for items in this group see #uGnssCfgValKeyItemI2cinprot_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_I2COUTPROT    = 0x72, /**< output protocol configuration of the I2C interface;
                                                           for items in this group see #uGnssCfgValKeyItemI2coutprot_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_INFMSG        = 0x92, /**< information message configuration; for items in this
                                                           group see #uGnssCfgValKeyItemInfmsg_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_ITFM          = 0x41, /**< jamming and interference monitor configuration; for
                                                           items in this group see #uGnssCfgValKeyItemItfm_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_LOGFILTER     = 0xde, /**< data logger configuration; for items in this
                                                           group see #uGnssCfgValKeyItemLogfilter_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_MOT           = 0x25, /**< motion detector configuration; for items in this group
                                                           see #uGnssCfgValKeyItemMot_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_MSGOUT        = 0x91, /**< message output configuration; for items in this group
                                                           see #uGnssCfgValKeyItemMsgout_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_NAV2          = 0x17, /**< secondary output configuration; for items in
                                                           this group see #uGnssCfgValKeyItemNav2_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_NAVHPG        = 0x14, /**< high precision navigation configuration; for items in
                                                           this group see #uGnssCfgValKeyItemNavhpg_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_NAVSPG        = 0x11, /**< standard precision navigation configuration; for items in
                                                           this group see #uGnssCfgValKeyItemNavspg_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_NMEA          = 0x93, /**< NMEA protocol configuration; for items in
                                                           this group see #uGnssCfgValKeyItemNmea_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_ODO           = 0x22, /**< odometer and low-speed course over ground filter configuration;
                                                           for items in this group see #uGnssCfgValKeyItemOdo_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_PM            = 0xd0, /**< configuration for receiver power management; for items in
                                                           this group see #uGnssCfgValKeyItemPm_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_PMP           = 0xb1, /**< configuration for L-band point to multipoint (PMP)
                                                           receiver; for items in this group see
                                                           #uGnssCfgValKeyItemPmp_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_QZSS          = 0x37, /**< QZSS system configuration; for items in this group
                                                           see #uGnssCfgValKeyItemQzss_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_RATE          = 0x21, /**< navigation and measurement rate configuration; for items in
                                                           this group see #uGnssCfgValKeyItemRate_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_RINV          = 0xc7, /**< remote inventory; for items in this group see
                                                           #uGnssCfgValKeyItemRinv_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_RTCM          = 0x09, /**< RTCM protocol configuration; for items in
                                                           this group see #uGnssCfgValKeyItemRtcm_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_SBAS          = 0x36, /**< SBAS configuration; for items in this group see
                                                           #uGnssCfgValKeyItemSbas_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_SEC           = 0xf6, /**< security configuration; for items in this group see
                                                           #uGnssCfgValKeyItemSec_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_SFCORE        = 0x08, /**< sensor fusion core configuration for dead-reckoning
                                                           products; for items in this group see
                                                           #uGnssCfgValKeyItemSfcore_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_SFIMU         = 0x06, /**< inertial measurement unit configuration for
                                                           dead-reckoning products; for items in this group see
                                                           #uGnssCfgValKeyItemSfimu_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_SFODO         = 0x07, /**< odometer configuration for dead-reckoning products;
                                                           for items in this group see #uGnssCfgValKeyItemSfodo_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_SIGNAL        = 0x31, /**< satellite systems (GNSS) signal configuration; for items in
                                                           this group see #uGnssCfgValKeyItemSignal_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_SPARTN        = 0xa7, /**< SPARTNconfiguration; for items in this group see
                                                           #uGnssCfgValKeyItemSpartn_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_SPI           = 0x64, /**< configuration of the SPI interface; for items in this group
                                                           see #uGnssCfgValKeyItemSpi_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_SPIINPROT     = 0x79, /**< input protocol configuration of the SPI interface; for items in
                                                           this group see #uGnssCfgValKeyItemSpiinprot_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_SPIOUTPROT    = 0x7a, /**< output protocol configuration of the SPI interface; for items in
                                                           this group see #uGnssCfgValKeyItemSpioutprot_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_TMODE         = 0x03, /**< time mode configuration; for items in this group see
                                                           #uGnssCfgValKeyItemTmode_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_TP            = 0x05, /**< time-pulse configuration; for items in this group see
                                                           #uGnssCfgValKeyItemTp_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_TXREADY       = 0xa2, /**< TX ready configuration; for items in this group see
                                                           #uGnssCfgValKeyItemTxready_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_UART1         = 0x52, /**< configuration of the UART1 interface; for items in this group
                                                           see #uGnssCfgValKeyItemUart1_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_UART1INPROT   = 0x73, /**< input protocol configuration of the UART1 interface; for items
                                                           in this group see #uGnssCfgValKeyItemUart1inprot_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_UART1OUTPROT  = 0x74, /**< output protocol configuration of the UART1 interface; for items
                                                           in this group see #uGnssCfgValKeyItemUart1outprot_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_UART2         = 0x53, /**< configuration of the UART2 interface; for items in this group
                                                           see #uGnssCfgValKeyItemUart2_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_UART2INPROT   = 0x75, /**< input protocol configuration of the UART2 interface; for items
                                                           in this group see #uGnssCfgValKeyItemUart2inprot_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_UART2OUTPROT  = 0x76, /**< output protocol configuration of the UART2 interface; for items
                                                           in this group see #uGnssCfgValKeyItemUart2outprot_t.*/
    U_GNSS_CFG_VAL_KEY_GROUP_ID_USB           = 0x65, /**< configuration of the USB interface; for items in this group
                                                           see #uGnssCfgValKeyItemUsb_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_USBINPROT     = 0x77, /**< input protocol configuration of the USB interface; for items
                                                           in this group see #uGnssCfgValKeyItemUsbinprot_t. */
    U_GNSS_CFG_VAL_KEY_GROUP_ID_USBOUTPROT    = 0x78  /**< output protocol configuration of the USB interface; for items
                                                           in this group see #uGnssCfgValKeyItemUsboutprot_t. */
} uGnssCfgValKeyGroupId_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_ANA.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_ANA_USE_ANA_L    = 0x01, /**< use AssistNow Autonomous. */
    U_GNSS_CFG_VAL_KEY_ITEM_ANA_ORBMAXERR_U2 = 0x02  /**< maximum acceptable (modeled) orbit error. */
} uGnssCfgValKeyItemAna_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_BATCH.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_BATCH_ENABLE_L       = 0x13,  /**< enable data batching; will do nothing unless
                                                               a positive value is set for
                                                               #U_GNSS_CFG_VAL_KEY_ITEM_BATCH_MAXENTRIES_U2. */
    U_GNSS_CFG_VAL_KEY_ITEM_BATCH_PIOENABLE_L    = 0x14,  /**< enable PIO notification when the buffer fill
                                                               level exceeds #U_GNSS_CFG_VAL_KEY_ITEM_BATCH_WARNTHRS_U2. */
    U_GNSS_CFG_VAL_KEY_ITEM_BATCH_MAXENTRIES_U2  = 0x15,  /**< size of buffer in number of epochs to store. */
    U_GNSS_CFG_VAL_KEY_ITEM_BATCH_WARNTHRS_U2    = 0x16,  /**< buffer fill level that triggers PIO notification,
                                                               in number of epochs stored. */
    U_GNSS_CFG_VAL_KEY_ITEM_BATCH_PIOACTIVELOW_L = 0x18,  /**< if this is set the PIO selected with
                                                               #U_GNSS_CFG_VAL_KEY_ITEM_BATCH_PIOID_U1 will be driven low when
                                                               the buffer fill level reaches
                                                               #U_GNSS_CFG_VAL_KEY_ITEM_BATCH_WARNTHRS_U2. */
    U_GNSS_CFG_VAL_KEY_ITEM_BATCH_PIOID_U1       = 0x19,  /**< PIO that is used for buffer fill level notification. */
    U_GNSS_CFG_VAL_KEY_ITEM_BATCH_EXTRAPVT_L     = 0x1a,  /**< set this to include the fields iTOW, tAcc, numSV, hMSL,
                                                               vAcc, velN, velE, velD, sAcc, headAcc and pDOP in
                                                               UBX-LOG-BATCH messages. */
    U_GNSS_CFG_VAL_KEY_ITEM_BATCH_EXTRAODO_L     = 0x1b   /**< set this to include the fields distance, totalDistance
                                                               and distanceStd in UBX-LOG-BATCH messages. */
} uGnssCfgValKeyItemBatch_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_BDS.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_BDS_USE_GEO_PRN_L   = 0x14    /**< use BeiDou geostationary satellites (PRN 1-5 and 59-63). */
} uGnssCfgValKeyItemBds_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_GEOFENCE.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_CONFLVL_E1    = 0x11, /**< this value times the position's standard deviation
                                                                (sigma) defines the confidence band for state
                                                                evaluation, see #uGnssCfgValKeyItemValueGeofenceConflvl_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_USE_PIO_L     = 0x12, /**< use PIO combined fence state output. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_PINPOL_E1     = 0x13, /**< use 0 for PIO low to mean inside geofence, 1 for
                                                                PIO low to mean outside geofence. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_PIN_U1        = 0x14, /**< PIO pin number. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_USE_FENCE1_L  = 0x20, /**< use first geofence. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_FENCE1_LAT_I4 = 0x21, /**< latitude of the first geofence circle centre in 10 millionths of a degree. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_FENCE1_LON_I4 = 0x22, /**< longitude of the first geofence circle centre in 10 millionths of a degree. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_FENCE1_RAD_U4 = 0x23, /**< radius of the first geofence circle centre in centimetres. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_USE_FENCE2_L  = 0x30, /**< use second geofence. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_FENCE2_LAT_I4 = 0x31, /**< latitude of the second geofence circle centre in 10 millionths of a degree. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_FENCE2_LON_I4 = 0x32, /**< longitude of the second geofence circle centre in 10 millionths of a degree. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_FENCE2_RAD_U4 = 0x33, /**< radius of the second geofence circle centre in centimetres. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_USE_FENCE3_L  = 0x40, /**< use third geofence. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_FENCE3_LAT_I4 = 0x41, /**< latitude of the third geofence circle centre in 10 millionths of a degree. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_FENCE3_LON_I4 = 0x42, /**< longitude of the third geofence circle centre in 10 millionths of a degree. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_FENCE3_RAD_U4 = 0x43, /**< radius of the third geofence circle centre in centimetres. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_USE_FENCE4_L  = 0x50, /**< use fourth geofence. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_FENCE4_LAT_I4 = 0x51, /**< atitude of the fourth geofence circle centre in 10 millionths of a degree. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_FENCE4_LON_I4 = 0x52, /**< longitude of the fourth geofence circle centre in 10 millionths of a degree. */
    U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_FENCE4_RAD_U4 = 0x53  /**< radius of the fourth geofence circle centre in centimetres. */
} uGnssCfgValKeyItemGeofence_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_GEOFENCE_CONFLVL_E1, see
 * #uGnssCfgValKeyItemGeofence_t.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_GEOFENCE_CONFLVL_L680    = 1, /**< 68%. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_GEOFENCE_CONFLVL_L950    = 2, /**< 95%. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_GEOFENCE_CONFLVL_L997    = 3, /**< 99.7%. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_GEOFENCE_CONFLVL_L9999   = 4, /**< 99.99%. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_GEOFENCE_CONFLVL_L999999 = 5  /**< 99.9999%. */
} uGnssCfgValKeyItemValueGeofenceConflvl_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_HW.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_HW_ANT_CFG_VOLTCTRL_L      = 0x2e, /**< enable active antenna voltage control; used
                                                                    by EXT and MADC engines. */
    U_GNSS_CFG_VAL_KEY_ITEM_HW_ANT_CFG_SHORTDET_L      = 0x2f, /**< enable antenna short detection; used by EXT and
                                                                    MADC engines. */
    U_GNSS_CFG_VAL_KEY_ITEM_HW_ANT_CFG_SHORTDET_POL_L  = 0x30, /**< set to true if the polarity of the antenna short
                                                                    detection is active low; used by EXT engine. */
    U_GNSS_CFG_VAL_KEY_ITEM_HW_ANT_CFG_OPENDET_L       = 0x31, /**< enable antenna open detection; used by EXT and
                                                                    MADC engines. */
    U_GNSS_CFG_VAL_KEY_ITEM_HW_ANT_CFG_OPENDET_POL_L   = 0x32, /**< set to true if the polarity of the antenna open
                                                                    detection is active low; used by EXT engine. */
    U_GNSS_CFG_VAL_KEY_ITEM_HW_ANT_CFG_PWRDOWN_L       = 0x33, /**< enable powering down of the antenna in the event
                                                                    of an antenna short circuit;
                                                                    #U_GNSS_CFG_VAL_KEY_ITEM_HW_ANT_CFG_SHORTDET_L must
                                                                    also be enabled. */
    U_GNSS_CFG_VAL_KEY_ITEM_HW_ANT_CFG_PWRDOWN_POL_L   = 0x34, /**< set to true if the polarity of the antenna open
                                                                    detection is active high; used by EXT and MADC
                                                                    engines. */
    U_GNSS_CFG_VAL_KEY_ITEM_HW_ANT_CFG_RECOVER_L       = 0x35, /**< enable automatic recovery from antenna short state;
                                                                    used by EXT and MADC engines. */
    U_GNSS_CFG_VAL_KEY_ITEM_HW_ANT_SUP_SWITCH_PIN_U1   = 0x36, /**< antenna switch (ANT1) PIO number; used by EXT and
                                                                    MADC engines. */
    U_GNSS_CFG_VAL_KEY_ITEM_HW_ANT_SUP_SHORT_PIN_U1    = 0x37, /**< antenna short (ANT0) PIO number; used by EXT engine. */
    U_GNSS_CFG_VAL_KEY_ITEM_HW_ANT_SUP_OPEN_PIN_U1     = 0x38, /**< antenna open (ANT2) PIO number; used by EXT engine. */
    U_GNSS_CFG_VAL_KEY_ITEM_HW_ANT_SUP_ENGINE_E1       = 0x54, /**< 0 means EXT (use external comparators for current
                                                                    measurement), 1 means MADC (use the built-in ADC and
                                                                    a shunt for current measurement). */
    U_GNSS_CFG_VAL_KEY_ITEM_HW_ANT_SUP_SHORT_THR_U1    = 0x55, /**< antenna supervisor MADC engine short detection
                                                                    threshold in milliVolts. */
    U_GNSS_CFG_VAL_KEY_ITEM_HW_ANT_SUP_OPEN_THR_U1     = 0x56  /**< antenna supervisor MADC engine open/disconnect
                                                                    detection threshold in milliVolts. */
} uGnssCfgValKeyItemHw_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_I2C.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_I2C_ADDRESS_U1         = 0x01, /**< set the I2C slave address of the receiver (7 bits),
                                                                #U_GNSS_I2C_ADDRESS by default. */
    U_GNSS_CFG_VAL_KEY_ITEM_I2C_EXTENDEDTIMEOUT_L  = 0x02, /**< set this to disable timing-out of the I2C interface
                                                                after 1.5 seconds. */
    U_GNSS_CFG_VAL_KEY_ITEM_I2C_ENABLED_L          = 0x03  /**< enable or disable I2C. */
} uGnssCfgValKeyItemI2c_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_I2CINPROT.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_I2CINPROT_UBX_L    = 0x01, /**< set this flag to allow UBX protocol as input on I2C. */
    U_GNSS_CFG_VAL_KEY_ITEM_I2CINPROT_NMEA_L   = 0x02, /**< set this flag to allow NMEA protocol as input on I2C. */
    U_GNSS_CFG_VAL_KEY_ITEM_I2CINPROT_RTCM3X_L = 0x04, /**< set this flag to allow RTCM3X protocol as input on I2C. */
    U_GNSS_CFG_VAL_KEY_ITEM_I2CINPROT_SPARTN_L = 0x05  /**< set this flag to allow SPARTN protocol as input on I2C. */
} uGnssCfgValKeyItemI2cinprot_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_I2COUTPROT.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_I2COUTPROT_UBX_L    = 0x01, /**< set this flag to use UBX protocol on the output of I2C. */
    U_GNSS_CFG_VAL_KEY_ITEM_I2COUTPROT_NMEA_L   = 0x02, /**< set this flag to use NMEA protocol on the output of I2C. */
    U_GNSS_CFG_VAL_KEY_ITEM_I2COUTPROT_RTCM3X_L = 0x04  /**< set this flag to use RTCM3X protocol on the output of I2C. */
} uGnssCfgValKeyItemI2coutprot_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_INFMSG.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_INFMSG_UBX_I2C_X1    = 0x01, /**< enable flags for information on the I2C interface with
                                                              UBX protocol, see #uGnssCfgValKeyItemValueInfmsg_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_INFMSG_UBX_UART1_X1  = 0x02, /**< enable flags for information on the UART1 interface with
                                                              UBX protocol, see #uGnssCfgValKeyItemValueInfmsg_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_INFMSG_UBX_UART2_X1  = 0x03, /**< enable flags for information on the UART2 interface with
                                                              UBX protocol, see #uGnssCfgValKeyItemValueInfmsg_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_INFMSG_UBX_USB_X1    = 0x04, /**< enable flags for information on the USB interface with
                                                              UBX protocol, see #uGnssCfgValKeyItemValueInfmsg_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_INFMSG_UBX_SPI_X1    = 0x05, /**< enable flags for information on the SPI interface with
                                                              UBX protocol, see #uGnssCfgValKeyItemValueInfmsg_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_INFMSG_NMEA_I2C_X1   = 0x06, /**< enable flags for information on the I2C interface with
                                                              NMEA protocol, see #uGnssCfgValKeyItemValueInfmsg_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_INFMSG_NMEA_UART1_X1 = 0x07, /**< enable flags for information on the UART1 interface with
                                                              NMEA protocol, see #uGnssCfgValKeyItemValueInfmsg_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_INFMSG_NMEA_UART2_X1 = 0x08, /**< enable flags for information on the UART2 interface with
                                                              NMEA protocol, see #uGnssCfgValKeyItemValueInfmsg_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_INFMSG_NMEA_USB_X1   = 0x09, /**< enable flags for information on the USB interface with
                                                              NMEA protocol, see #uGnssCfgValKeyItemValueInfmsg_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_INFMSG_NMEA_SPI_X1   = 0x0a  /**< enable flags for information on the SPI interface with
                                                              NMEA protocol, see #uGnssCfgValKeyItemValueInfmsg_t. */
} uGnssCfgValKeyItemInfmsg_t;

/** Flags for #uGnssCfgValKeyItemInfmsg_t.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_INFMSG_ERROR   = 0x01,
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_INFMSG_WARNING = 0x02,
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_INFMSG_NOTICE  = 0x04,
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_INFMSG_TEST    = 0x08,
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_INFMSG_DEBUG   = 0x10
} uGnssCfgValKeyItemValueInfmsg_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_ITFM.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_ITFM_BBTHRESHOLD_U1   = 0x01, /**< broadband jamming detection threshold. */
    U_GNSS_CFG_VAL_KEY_ITEM_ITFM_CWTHRESHOLD_U1   = 0x02, /**< CW jamming detection threshold. */
    U_GNSS_CFG_VAL_KEY_ITEM_ITFM_ENABLE_L         = 0x0d, /**< enable interference detection. */
    U_GNSS_CFG_VAL_KEY_ITEM_ITFM_ANTSETTING_E1    = 0x10, /**< antenna setting, see #uGnssCfgValKeyItemValueItfmAntsetting_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_ITFM_ENABLE_AUX_L     = 0x13  /**< enable scanning of auxiliary bands (M8 only). */
} uGnssCfgValKeyItemItfm_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_ITFM_ANTSETTING_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_ITFM_ANTSETTING_UNKNOWN  = 0,
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_ITFM_ANTSETTING_PASSIVE  = 1,
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_ITFM_ANTSETTING_ACTIVE   = 2
} uGnssCfgValKeyItemValueItfmAntsetting_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_LOGFILTER.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_LOGFILTER_RECORD_ENA_L            = 0x02, /**< set to true to enable recording. */
    U_GNSS_CFG_VAL_KEY_ITEM_LOGFILTER_ONCE_PER_WAKE_UP_ENA_L  = 0x03, /**< set to true to record only one single
                                                                           position per PSM on/off mode wake-up;
                                                                           the value set here does not take effect
                                                                           unless #U_GNSS_CFG_VAL_KEY_ITEM_LOGFILTER_APPLY_ALL_FILTERS_L
                                                                           is set. */
    U_GNSS_CFG_VAL_KEY_ITEM_LOGFILTER_APPLY_ALL_FILTERS_L     = 0x04, /**< set to true to apply all filter settings,
                                                                           not just recording enable/disable. */
    U_GNSS_CFG_VAL_KEY_ITEM_LOGFILTER_MIN_INTERVAL_U2         = 0x05, /**< the minimum time interval between logged
                                                                           positions in seconds (0 = not set).  This
                                                                           is only applied in combination with the
                                                                           speed and/or position thresholds.  If
                                                                           both #U_GNSS_CFG_VAL_KEY_ITEM_LOGFILTER_MIN_INTERVAL_U2
                                                                           and #U_GNSS_CFG_VAL_KEY_ITEM_LOGFILTER_TIME_THRS_U2
                                                                           are set #U_GNSS_CFG_VAL_KEY_ITEM_LOGFILTER_MIN_INTERVAL_U2
                                                                           must be less than or equal to
                                                                           #U_GNSS_CFG_VAL_KEY_ITEM_LOGFILTER_TIME_THRS_U2.
                                                                           The value set here does not take effect
                                                                           unless #U_GNSS_CFG_VAL_KEY_ITEM_LOGFILTER_APPLY_ALL_FILTERS_L
                                                                           is set. */
    U_GNSS_CFG_VAL_KEY_ITEM_LOGFILTER_TIME_THRS_U2            = 0x06, /**< if the time difference is greater than this number
                                                                           of seconds then the position is logged (0 = not set);
                                                                           the value set here does not take effect
                                                                           unless #U_GNSS_CFG_VAL_KEY_ITEM_LOGFILTER_APPLY_ALL_FILTERS_L
                                                                           is set. */
    U_GNSS_CFG_VAL_KEY_ITEM_LOGFILTER_SPEED_THRS_U2           = 0x07, /**< if the current speed is greater than this (in metres/second)
                                                                           then the position is logged (0 = not set);
                                                                           #U_GNSS_CFG_VAL_KEY_ITEM_LOGFILTER_MIN_INTERVAL_U2 also applies.
                                                                           The value set here does not take effect
                                                                           unless #U_GNSS_CFG_VAL_KEY_ITEM_LOGFILTER_APPLY_ALL_FILTERS_L
                                                                           is set. */
    U_GNSS_CFG_VAL_KEY_ITEM_LOGFILTER_POSITION_THRS_U4        = 0x08  /**< if the 3D position is greater than this (in metres)
                                                                           then the position is logged (0 = not set);
                                                                           #U_GNSS_CFG_VAL_KEY_ITEM_LOGFILTER_MIN_INTERVAL_U2 also applies.
                                                                           The value set here does not take effect
                                                                           unless #U_GNSS_CFG_VAL_KEY_ITEM_LOGFILTER_APPLY_ALL_FILTERS_L
                                                                           is set. */
} uGnssCfgValKeyItemLogfilter_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_MOT.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_MOT_GNSSSPEED_THRS_U1 = 0x38, /**< the speed (in centimetres/second) below which the device
                                                               is considered stationary (AKA the static hold threshold);
                                                               use 0 for firmware default value/behaviour. */
    U_GNSS_CFG_VAL_KEY_ITEM_MOT_GNSSDIST_THRS_U2  = 0x3b  /**< the distance above which the device is no longer stationary
                                                               (AKA the static hold distance threshold); use 0 for firmware
                                                               default value/behaviour. */
} uGnssCfgValKeyItemMot_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_MSGOUT.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_DTM_I2C_U1          = 0x0a6, /**< output rate of the NMEA-GX-DTM message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_DTM_SPI_U1          = 0x0aa, /**< output rate of the NMEA-GX-DTM message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_DTM_UART1_U1        = 0x0a7, /**< output rate of the NMEA-GX-DTM message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_DTM_UART2_U1        = 0x0a8, /**< output rate of the NMEA-GX-DTM message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_DTM_USB_U1          = 0x0a9, /**< output rate of the NMEA-GX-DTM message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GBS_I2C_U1          = 0x0dd, /**< output rate of the NMEA-GX-GBS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GBS_SPI_U1          = 0x0e1, /**< output rate of the NMEA-GX-GBS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GBS_UART1_U1        = 0x0de, /**< output rate of the NMEA-GX-GBS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GBS_UART2_U1        = 0x0df, /**< output rate of the NMEA-GX-GBS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GBS_USB_U1          = 0x0e0, /**< output rate of the NMEA-GX-GBS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GGA_I2C_U1          = 0x0ba, /**< output rate of the NMEA-GX-GGA message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GGA_SPI_U1          = 0x0be, /**< output rate of the NMEA-GX-GGA message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GGA_UART1_U1        = 0x0bb, /**< output rate of the NMEA-GX-GGA message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GGA_UART2_U1        = 0x0bc, /**< output rate of the NMEA-GX-GGA message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GGA_USB_U1          = 0x0bd, /**< output rate of the NMEA-GX-GGA message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GLL_I2C_U1          = 0x0c9, /**< output rate of the NMEA-GX-GLL message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GLL_SPI_U1          = 0x0cd, /**< output rate of the NMEA-GX-GLL message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GLL_UART1_U1        = 0x0ca, /**< output rate of the NMEA-GX-GLL message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GLL_UART2_U1        = 0x0cb, /**< output rate of the NMEA-GX-GLL message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GLL_USB_U1          = 0x0cc, /**< output rate of the NMEA-GX-GLL message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GNS_I2C_U1          = 0x0b5, /**< output rate of the NMEA-GX-GNS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GNS_SPI_U1          = 0x0b9, /**< output rate of the NMEA-GX-GNS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GNS_UART1_U1        = 0x0b6, /**< output rate of the NMEA-GX-GNS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GNS_UART2_U1        = 0x0b7, /**< output rate of the NMEA-GX-GNS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GNS_USB_U1          = 0x0b8, /**< output rate of the NMEA-GX-GNS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GRS_I2C_U1          = 0x0ce, /**< output rate of the NMEA-GX-GRS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GRS_SPI_U1          = 0x0d2, /**< output rate of the NMEA-GX-GRS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GRS_UART1_U1        = 0x0cf, /**< output rate of the NMEA-GX-GRS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GRS_UART2_U1        = 0x0d0, /**< output rate of the NMEA-GX-GRS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GRS_USB_U1          = 0x0d1, /**< output rate of the NMEA-GX-GRS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GSA_I2C_U1          = 0x0bf, /**< output rate of the NMEA-GX-GSA message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GSA_SPI_U1          = 0x0c3, /**< output rate of the NMEA-GX-GSA message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GSA_UART1_U1        = 0x0c0, /**< output rate of the NMEA-GX-GSA message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GSA_UART2_U1        = 0x0c1, /**< output rate of the NMEA-GX-GSA message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GSA_USB_U1          = 0x0c2, /**< output rate of the NMEA-GX-GSA message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GST_I2C_U1          = 0x0d3, /**< output rate of the NMEA-GX-GST message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GST_SPI_U1          = 0x0d7, /**< output rate of the NMEA-GX-GST message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GST_UART1_U1        = 0x0d4, /**< output rate of the NMEA-GX-GST message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GST_UART2_U1        = 0x0d5, /**< output rate of the NMEA-GX-GST message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GST_USB_U1          = 0x0d6, /**< output rate of the NMEA-GX-GST message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GSV_I2C_U1          = 0x0c4, /**< output rate of the NMEA-GX-GSV message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GSV_SPI_U1          = 0x0c8, /**< output rate of the NMEA-GX-GSV message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GSV_UART1_U1        = 0x0c5, /**< output rate of the NMEA-GX-GSV message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GSV_UART2_U1        = 0x0c6, /**< output rate of the NMEA-GX-GSV message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_GSV_USB_U1          = 0x0c7, /**< output rate of the NMEA-GX-GSV message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_RLM_I2C_U1          = 0x400, /**< output rate of the NMEA-GX-RLM message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_RLM_SPI_U1          = 0x404, /**< output rate of the NMEA-GX-RLM message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_RLM_UART1_U1        = 0x401, /**< output rate of the NMEA-GX-RLM message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_RLM_UART2_U1        = 0x402, /**< output rate of the NMEA-GX-RLM message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_RLM_USB_U1          = 0x403, /**< output rate of the NMEA-GX-RLM message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_RMC_I2C_U1          = 0x0ab, /**< output rate of the NMEA-GX-RMC message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_RMC_SPI_U1          = 0x0af, /**< output rate of the NMEA-GX-RMC message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_RMC_UART1_U1        = 0x0ac, /**< output rate of the NMEA-GX-RMC message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_RMC_UART2_U1        = 0x0ad, /**< output rate of the NMEA-GX-RMC message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_RMC_USB_U1          = 0x0ae, /**< output rate of the NMEA-GX-RMC message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_VLW_I2C_U1          = 0x0e7, /**< output rate of the NMEA-GX-VLW message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_VLW_SPI_U1          = 0x0eb, /**< output rate of the NMEA-GX-VLW message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_VLW_UART1_U1        = 0x0e8, /**< output rate of the NMEA-GX-VLW message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_VLW_UART2_U1        = 0x0e9, /**< output rate of the NMEA-GX-VLW message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_VLW_USB_U1          = 0x0ea, /**< output rate of the NMEA-GX-VLW message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_VTG_I2C_U1          = 0x0b0, /**< output rate of the NMEA-GX-VTG message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_VTG_SPI_U1          = 0x0b4, /**< output rate of the NMEA-GX-VTG message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_VTG_UART1_U1        = 0x0b1, /**< output rate of the NMEA-GX-VTG message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_VTG_UART2_U1        = 0x0b2, /**< output rate of the NMEA-GX-VTG message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_VTG_USB_U1          = 0x0b3, /**< output rate of the NMEA-GX-VTG message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_ZDA_I2C_U1          = 0x0d8, /**< output rate of the NMEA-GX-ZDA message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_ZDA_SPI_U1          = 0x0dc, /**< output rate of the NMEA-GX-ZDA message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_ZDA_UART1_U1        = 0x0d9, /**< output rate of the NMEA-GX-ZDA message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_ZDA_UART2_U1        = 0x0da, /**< output rate of the NMEA-GX-ZDA message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_ID_ZDA_USB_U1          = 0x0db, /**< output rate of the NMEA-GX-ZDA message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GGA_I2C_U1     = 0x661, /**< output rate of the NMEA-NAV2-GX-GGA message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GGA_SPI_U1     = 0x665, /**< output rate of the NMEA-NAV2-GX-GGA message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GGA_UART1_U1   = 0x662, /**< output rate of the NMEA-NAV2-GX-GGA message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GGA_UART2_U1   = 0x663, /**< output rate of the NMEA-NAV2-GX-GGA message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GGA_USB_U1     = 0x664, /**< output rate of the NMEA-NAV2-GX-GGA message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GLL_I2C_U1     = 0x670, /**< output rate of the NMEA-NAV2-GX-GLL message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GLL_SPI_U1     = 0x674, /**< output rate of the NMEA-NAV2-GX-GLL message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GLL_UART1_U1   = 0x671, /**< output rate of the NMEA-NAV2-GX-GLL message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GLL_UART2_U1   = 0x672, /**< output rate of the NMEA-NAV2-GX-GLL message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GLL_USB_U1     = 0x673, /**< output rate of the NMEA-NAV2-GX-GLL message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GNS_I2C_U1     = 0x65c, /**< output rate of the NMEA-NAV2-GX-GNS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GNS_SPI_U1     = 0x660, /**< output rate of the NMEA-NAV2-GX-GNS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GNS_UART1_U1   = 0x65d, /**< output rate of the NMEA-NAV2-GX-GNS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GNS_UART2_U1   = 0x65e, /**< output rate of the NMEA-NAV2-GX-GNS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GNS_USB_U1     = 0x65f, /**< output rate of the NMEA-NAV2-GX-GNS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GSA_I2C_U1     = 0x666, /**< output rate of the NMEA-NAV2-GX-GSA message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GSA_SPI_U1     = 0x66a, /**< output rate of the NMEA-NAV2-GX-GSA message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GSA_UART1_U1   = 0x667, /**< output rate of the NMEA-NAV2-GX-GSA message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GSA_UART2_U1   = 0x668, /**< output rate of the NMEA-NAV2-GX-GSA message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_GSA_USB_U1     = 0x669, /**< output rate of the NMEA-NAV2-GX-GSA message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_RMC_I2C_U1     = 0x652, /**< output rate of the NMEA-NAV2-GX-RMC message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_RMC_SPI_U1     = 0x656, /**< output rate of the NMEA-NAV2-GX-RMC message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_RMC_UART1_U1   = 0x653, /**< output rate of the NMEA-NAV2-GX-RMC message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_RMC_UART2_U1   = 0x654, /**< output rate of the NMEA-NAV2-GX-RMC message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_RMC_USB_U1     = 0x655, /**< output rate of the NMEA-NAV2-GX-RMC message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_VTG_I2C_U1     = 0x657, /**< output rate of the NMEA-NAV2-GX-VTG message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_VTG_SPI_U1     = 0x65b, /**< output rate of the NMEA-NAV2-GX-VTG message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_VTG_UART1_U1   = 0x658, /**< output rate of the NMEA-NAV2-GX-VTG message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_VTG_UART2_U1   = 0x649, /**< output rate of the NMEA-NAV2-GX-VTG message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_VTG_USB_U1     = 0x65a, /**< output rate of the NMEA-NAV2-GX-VTG message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_ZDA_I2C_U1     = 0x67f, /**< output rate of the NMEA-NAV2-GX-ZDA message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_ZDA_SPI_U1     = 0x683, /**< output rate of the NMEA-NAV2-GX-ZDA message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_ZDA_UART1_U1   = 0x680, /**< output rate of the NMEA-NAV2-GX-ZDA message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_ZDA_UART2_U1   = 0x681, /**< output rate of the NMEA-NAV2-GX-ZDA message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_NMEA_NAV2_ID_ZDA_USB_U1     = 0x682, /**< output rate of the NMEA-NAV2-GX-ZDA message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_PUBX_ID_POLYP_I2C_U1        = 0x0ec, /**< output rate of the NMEA-GX-PUBX00 message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_PUBX_ID_POLYP_SPI_U1        = 0x0f0, /**< output rate of the NMEA-GX-PUBX00 message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_PUBX_ID_POLYP_UART1_U1      = 0x0ed, /**< output rate of the NMEA-GX-PUBX00 message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_PUBX_ID_POLYP_UART2_U1      = 0x0ee, /**< output rate of the NMEA-GX-PUBX00 message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_PUBX_ID_POLYP_USB_U1        = 0x0ef, /**< output rate of the NMEA-GX-PUBX00 message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_PUBX_ID_POLYS_I2C_U1        = 0x0f1, /**< output rate of the NMEA-GX-PUBX03 message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_PUBX_ID_POLYS_SPI_U1        = 0x0f5, /**< output rate of the NMEA-GX-PUBX03 message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_PUBX_ID_POLYS_UART1_U1      = 0x0f2, /**< output rate of the NMEA-GX-PUBX03 message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_PUBX_ID_POLYS_UART2_U1      = 0x0f3, /**< output rate of the NMEA-GX-PUBX03 message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_PUBX_ID_POLYS_USB_U1        = 0x0f4, /**< output rate of the NMEA-GX-PUBX03 message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_PUBX_ID_POLYT_I2C_U1        = 0x0f6, /**< output rate of the NMEA-GX-PUBX04 message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_PUBX_ID_POLYT_SPI_U1        = 0x0fa, /**< output rate of the NMEA-GX-PUBX04 message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_PUBX_ID_POLYT_UART1_U1      = 0x0f7, /**< output rate of the NMEA-GX-PUBX04 message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_PUBX_ID_POLYT_UART2_U1      = 0x0f8, /**< output rate of the NMEA-GX-PUBX04 message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_PUBX_ID_POLYT_USB_U1        = 0x0f9, /**< output rate of the NMEA-GX-PUBX04 message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1005_I2C_U1     = 0x2bd, /**< output rate of the RTCM-3X-TYPE1005 message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1005_SPI_U1     = 0x2c1, /**< output rate of the RTCM-3X-TYPE1005 message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1005_UART1_U1   = 0x2be, /**< output rate of the RTCM-3X-TYPE1005 message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1005_UART2_U1   = 0x2bf, /**< output rate of the RTCM-3X-TYPE1005message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1005_USB_U1     = 0x2c0, /**< output rate of the RTCM-3X-TYPE1005 message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1074_I2C_U1     = 0x35e, /**< output rate of the RTCM-3X-TYPE1074 message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1074_SPI_U1     = 0x362, /**< output rate of the RTCM-3X-TYPE1074 message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1074_UART1_U1   = 0x35f, /**< output rate of the RTCM-3X-TYPE1074 message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1074_UART2_U1   = 0x360, /**< output rate of the RTCM-3X-TYPE1074 message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1074_USB_U1     = 0x361, /**< output rate of the RTCM-3X-TYPE1074 message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1077_I2C_U1     = 0x2cc, /**< output rate of the RTCM-3X-TYPE1077 message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1077_SPI_U1     = 0x2d0, /**< output rate of the RTCM-3X-TYPE1077 message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1077_UART1_U1   = 0x2cd, /**< output rate of the RTCM-3X-TYPE1077 message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1077_UART2_U1   = 0x2ce, /**< output rate of the RTCM-3X-TYPE1077 message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1077_USB_U1     = 0x2cf, /**< output rate of the RTCM-3X-TYPE1077 message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1084_I2C_U1     = 0x363, /**< output rate of the RTCM-3X-TYPE1084 message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1084_SPI_U1     = 0x367, /**< output rate of the RTCM-3X-TYPE1084 message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1084_UART1_U1   = 0x364, /**< output rate of the RTCM-3X-TYPE1084 message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1084_UART2_U1   = 0x365, /**< output rate of the RTCM-3X-TYPE1084 message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1084_USB_U1     = 0x366, /**< output rate of the RTCM-3X-TYPE1084 message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1087_I2C_U1     = 0x2d1, /**< output rate of the RTCM-3X-TYPE1087 message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1087_SPI_U1     = 0x2d5, /**< output rate of the RTCM-3X-TYPE1087 message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1087_UART1_U1   = 0x2d2, /**< output rate of the RTCM-3X-TYPE1087 message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1087_UART2_U1   = 0x2d3, /**< output rate of the RTCM-3X-TYPE1087 message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1087_USB_U1     = 0x2d4, /**< output rate of the RTCM-3X-TYPE1087 message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1094_I2C_U1     = 0x368, /**< output rate of the RTCM-3X-TYPE1094 message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1094_SPI_U1     = 0x36c, /**< output rate of the RTCM-3X-TYPE1094 message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1094_UART1_U1   = 0x369, /**< output rate of the RTCM-3X-TYPE1094 message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1094_UART2_U1   = 0x36a, /**< output rate of the RTCM-3X-TYPE1094 message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1094_USB_U1     = 0x36b, /**< output rate of the RTCM-3X-TYPE1094 message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1097_I2C_U1     = 0x318, /**< output rate of the RTCM-3X-TYPE1097 message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1097_SPI_U1     = 0x31c, /**< output rate of the RTCM-3X-TYPE1097 message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1097_UART1_U1   = 0x319, /**< output rate of the RTCM-3X-TYPE1097 message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1097_UART2_U1   = 0x31a, /**< output rate of the RTCM-3X-TYPE1097 message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1097_USB_U1     = 0x31b, /**< output rate of the RTCM-3X-TYPE1097 message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1124_I2C_U1     = 0x36d, /**< output rate of the RTCM-3X-TYPE1124 message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1124_SPI_U1     = 0x371, /**< output rate of the RTCM-3X-TYPE1124 message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1124_UART1_U1   = 0x36e, /**< output rate of the RTCM-3X-TYPE1124 message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1124_UART2_U1   = 0x36f, /**< output rate of the RTCM-3X-TYPE1124 message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1124_USB_U1     = 0x370, /**< output rate of the RTCM-3X-TYPE1124 message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1127_I2C_U1     = 0x2d6, /**< output rate of the RTCM-3X-TYPE1127 message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1127_SPI_U1     = 0x2da, /**< output rate of the RTCM-3X-TYPE1127 message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1127_UART1_U1   = 0x2d7, /**< output rate of the RTCM-3X-TYPE1127 message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1127_UART2_U1   = 0x2d8, /**< output rate of the RTCM-3X-TYPE1127 message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1127_USB_U1     = 0x2d9, /**< output rate of the RTCM-3X-TYPE1127 message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1230_I2C_U1     = 0x303, /**< output rate of the RTCM-3X-TYPE1230 message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1230_SPI_U1     = 0x307, /**< output rate of the RTCM-3X-TYPE1230 message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1230_UART1_U1   = 0x304, /**< output rate of the RTCM-3X-TYPE1230 message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1230_UART2_U1   = 0x305, /**< output rate of the RTCM-3X-TYPE1230 message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE1230_USB_U1     = 0x306, /**< output rate of the RTCM-3X-TYPE1230 message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE4072_0_I2C_U1   = 0x2fe, /**< output rate of the RTCM-3X-TYPE4072_0 message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE4072_0_SPI_U1   = 0x302, /**< output rate of the RTCM-3X-TYPE4072_0 message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE4072_0_UART1_U1 = 0x2ff, /**< output rate of the RTCM-3X-TYPE4072_0 message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_RTCM_3X_TYPE4072_0_UART2_U1 = 0x300, /**< output rate of the RTCM-3X-TYPE4072_0 message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_ALG_I2C_U1          = 0x10f, /**< output rate of the UBX-ESF-ALG message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_ALG_SPI_U1          = 0x113, /**< output rate of the UBX-ESF-ALG message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_ALG_UART1_U1        = 0x110, /**< output rate of the UBX-ESF-ALG message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_ALG_UART2_U1        = 0x111, /**< output rate of the UBX-ESF-ALG message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_ALG_USB_U1          = 0x112, /**< output rate of the UBX-ESF-ALG message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_INS_I2C_U1          = 0x114, /**< output rate of the UBX-ESF-INS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_INS_SPI_U1          = 0x118, /**< output rate of the UBX-ESF-INS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_INS_UART1_U1        = 0x115, /**< output rate of the UBX-ESF-INS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_INS_UART2_U1        = 0x116, /**< output rate of the UBX-ESF-INS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_INS_USB_U1          = 0x117, /**< output rate of the UBX-ESF-INS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_MEAS_I2C_U1         = 0x277, /**< output rate of the UBX-ESF-MEAS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_MEAS_SPI_U1         = 0x27b, /**< output rate of the UBX-ESF-MEAS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_MEAS_UART1_U1       = 0x278, /**< output rate of the UBX-ESF-MEAS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_MEAS_UART2_U1       = 0x279, /**< output rate of the UBX-ESF-MEAS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_MEAS_USB_U1         = 0x27a, /**< output rate of the UBX-ESF-MEAS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_RAW_I2C_U1          = 0x29f, /**< output rate of the UBX-ESF-RAW message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_RAW_SPI_U1          = 0x2a3, /**< output rate of the UBX-ESF-RAW message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_RAW_UART1_U1        = 0x2a0, /**< output rate of the UBX-ESF-RAW message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_RAW_UART2_U1        = 0x2a1, /**< output rate of the UBX-ESF-RAW message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_RAW_USB_U1          = 0x2a2, /**< output rate of the UBX-ESF-RAW message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_STATUS_I2C_U1       = 0x105, /**< output rate of the UBX-ESF-STATUS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_STATUS_SPI_U1       = 0x109, /**< output rate of the UBX-ESF-STATUS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_STATUS_UART1_U1     = 0x106, /**< output rate of the UBX-ESF-STATUS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_STATUS_UART2_U1     = 0x107, /**< output rate of the UBX-ESF-STATUS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_ESF_STATUS_USB_U1       = 0x108, /**< output rate of the UBX-ESF-STATUS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_LOG_INFO_I2C_U1         = 0x259, /**< output rate of the UBX-LOG-INFO message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_LOG_INFO_SPI_U1         = 0x25d, /**< output rate of the UBX-LOG-INFO message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_LOG_INFO_UART1_U1       = 0x25a, /**< output rate of the UBX-LOG-INFO message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_LOG_INFO_UART2_U1       = 0x25b, /**< output rate of the UBX-LOG-INFO message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_LOG_INFO_USB_U1         = 0x25c, /**< output rate of the UBX-LOG-INFO message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_COMMS_I2C_U1        = 0x34f, /**< output rate of the UBX-MON-COMMS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_COMMS_SPI_U1        = 0x353, /**< output rate of the UBX-MON-COMMS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_COMMS_UART1_U1      = 0x350, /**< output rate of the UBX-MON-COMMS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_COMMS_UART2_U1      = 0x351, /**< output rate of the UBX-MON-COMMS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_COMMS_USB_U1        = 0x352, /**< output rate of the UBX-MON-COMMS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_HW2_I2C_U1          = 0x1b9, /**< output rate of the UBX-MON-HW2 message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_HW2_SPI_U1          = 0x1bd, /**< output rate of the UBX-MON-HW2 message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_HW2_UART1_U1        = 0x1ba, /**< output rate of the UBX-MON-HW2 message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_HW2_UART2_U1        = 0x1bb, /**< output rate of the UBX-MON-HW2 message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_HW2_USB_U1          = 0x1bc, /**< output rate of the UBX-MON-HW2 message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_HW3_I2C_U1          = 0x354, /**< output rate of the UBX-MON-HW3 message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_HW3_SPI_U1          = 0x358, /**< output rate of the UBX-MON-HW3 message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_HW3_UART1_U1        = 0x355, /**< output rate of the UBX-MON-HW3 message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_HW3_UART2_U1        = 0x356, /**< output rate of the UBX-MON-HW3 message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_HW3_USB_U1          = 0x357, /**< output rate of the UBX-MON-HW3 message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_HW_I2C_U1           = 0x1b4, /**< output rate of the UBX-MON-HW message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_HW_SPI_U1           = 0x1b8, /**< output rate of the UBX-MON-HW message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_HW_UART1_U1         = 0x1b5, /**< output rate of the UBX-MON-HW message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_HW_UART2_U1         = 0x1b6, /**< output rate of the UBX-MON-HW message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_HW_USB_U1           = 0x1b7, /**< output rate of the UBX-MON-HW message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_IO_I2C_U1           = 0x1a5, /**< output rate of the UBX-MON-IO message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_IO_SPI_U1           = 0x1a9, /**< output rate of the UBX-MON-IO message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_IO_UART1_U1         = 0x1a6, /**< output rate of the UBX-MON-IO message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_IO_UART2_U1         = 0x1a7, /**< output rate of the UBX-MON-IO message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_IO_USB_U1           = 0x1a8, /**< output rate of the UBX-MON-IO message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_MSGPP_I2C_U1        = 0x196, /**< output rate of the UBX-MON-MSGPP message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_MSGPP_SPI_U1        = 0x19a, /**< output rate of the UBX-MON-MSGPP message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_MSGPP_UART1_U1      = 0x197, /**< output rate of the UBX-MON-MSGPP message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_MSGPP_UART2_U1      = 0x198, /**< output rate of the UBX-MON-MSGPP message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_MSGPP_USB_U1        = 0x199, /**< output rate of the UBX-MON-MSGPP message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_RF_I2C_U1           = 0x359, /**< output rate of the UBX-MON-RF message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_RF_SPI_U1           = 0x35d, /**< output rate of the UBX-MON-RF message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_RF_UART1_U1         = 0x35a, /**< output rate of the UBX-MON-RF message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_RF_UART2_U1         = 0x35b, /**< output rate of the UBX-MON-RF message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_RF_USB_U1           = 0x35c, /**< output rate of the UBX-MON-RF message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_RXBUF_I2C_U1        = 0x1a0, /**< output rate of the UBX-MON-RXBUF message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_RXBUF_SPI_U1        = 0x1a4, /**< output rate of the UBX-MON-RXBUF message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_RXBUF_UART1_U1      = 0x1a1, /**< output rate of the UBX-MON-RXBUF message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_RXBUF_UART2_U1      = 0x1a2, /**< output rate of the UBX-MON-RXBUF message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_RXBUF_USB_U1        = 0x1a3, /**< output rate of the UBX-MON-RXBUF message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_RXR_I2C_U1          = 0x187, /**< output rate of the UBX-MON-RXR message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_RXR_SPI_U1          = 0x18b, /**< output rate of the UBX-MON-RXR message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_RXR_UART1_U1        = 0x188, /**< output rate of the UBX-MON-RXR message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_RXR_UART2_U1        = 0x189, /**< output rate of the UBX-MON-RXR message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_RXR_USB_U1          = 0x18a, /**< output rate of the UBX-MON-RXR message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_SPAN_I2C_U1         = 0x38b, /**< output rate of the UBX-MON-SPAN message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_SPAN_SPI_U1         = 0x38f, /**< output rate of the UBX-MON-SPAN message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_SPAN_UART1_U1       = 0x38c, /**< output rate of the UBX-MON-SPAN message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_SPAN_UART2_U1       = 0x38d, /**< output rate of the UBX-MON-SPAN message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_SPAN_USB_U1         = 0x38e, /**< output rate of the UBX-MON-SPAN message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_SYS_I2C_U1          = 0x69d, /**< output rate of the UBX-MON-SYS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_SYS_SPI_U1          = 0x6a1, /**< output rate of the UBX-MON-SYS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_SYS_UART1_U1        = 0x69e, /**< output rate of the UBX-MON-SYS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_SYS_UART2_U1        = 0x69f, /**< output rate of the UBX-MON-SYS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_SYS_USB_U1          = 0x6a0, /**< output rate of the UBX-MON-SYS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_TXBUF_I2C_U1        = 0x19b, /**< output rate of the UBX-MON-TXBUF message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_TXBUF_SPI_U1        = 0x19f, /**< output rate of the UBX-MON-TXBUF message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_TXBUF_UART1_U1      = 0x19c, /**< output rate of the UBX-MON-TXBUF message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_TXBUF_UART2_U1      = 0x19d, /**< output rate of the UBX-MON-TXBUF message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_MON_TXBUF_USB_U1        = 0x19e, /**< output rate of the UBX-MON-TXBUF message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_CLOCK_I2C_U1       = 0x430, /**< output rate of the UBX-NAV2-CLOCK message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_CLOCK_SPI_U1       = 0x434, /**< output rate of the UBX-NAV2-CLOCK message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_CLOCK_UART1_U1     = 0x431, /**< output rate of the UBX-NAV2-CLOCK message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_CLOCK_UART2_U1     = 0x432, /**< output rate of the UBX-NAV2-CLOCK message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_CLOCK_USB_U1       = 0x433, /**< output rate of the UBX-NAV2-CLOCK message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_COV_I2C_U1         = 0x435, /**< output rate of the UBX-NAV2-COV message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_COV_SPI_U1         = 0x439, /**< output rate of the UBX-NAV2-COV message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_COV_UART1_U1       = 0x436, /**< output rate of the UBX-NAV2-COV message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_COV_UART2_U1       = 0x437, /**< output rate of the UBX-NAV2-COV message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_COV_USB_U1         = 0x438, /**< output rate of the UBX-NAV2-COV message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_DOP_I2C_U1         = 0x465, /**< output rate of the UBX-NAV2-DOP message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_DOP_SPI_U1         = 0x469, /**< output rate of the UBX-NAV2-DOP message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_DOP_UART1_U1       = 0x466, /**< output rate of the UBX-NAV2-DOP message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_DOP_UART2_U1       = 0x467, /**< output rate of the UBX-NAV2-DOP message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_DOP_USB_U1         = 0x468, /**< output rate of the UBX-NAV2-DOP message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_EOE_I2C_U1         = 0x565, /**< output rate of the UBX-NAV2-EOE message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_EOE_SPI_U1         = 0x569, /**< output rate of the UBX-NAV2-EOE message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_EOE_UART1_U1       = 0x566, /**< output rate of the UBX-NAV2-EOE message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_EOE_UART2_U1       = 0x567, /**< output rate of the UBX-NAV2-EOE message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_EOE_USB_U1          = 0x568, /**< output rate of the UBX-NAV2-EOE message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_ODO_I2C_U1         = 0x475, /**< output rate of the UBX-NAV2-ODO message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_ODO_SPI_U1         = 0x479, /**< output rate of the UBX-NAV2-ODO message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_ODO_UART1_U1       = 0x476, /**< output rate of the UBX-NAV2-ODO message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_ODO_UART2_U1       = 0x477, /**< output rate of the UBX-NAV2-ODO message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_ODO_USB_U1         = 0x478, /**< output rate of the UBX-NAV2-ODO message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_POSECEF_I2C_U1     = 0x480, /**< output rate of the UBX-NAV2-POSECEF message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_POSECEF_SPI_U1     = 0x484, /**< output rate of the UBX-NAV2-POSECEF message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_POSECEF_UART1_U1   = 0x481, /**< output rate of the UBX-NAV2-POSECEF message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_POSECEF_UART2_U1   = 0x482, /**< output rate of the UBX-NAV2-POSECEF message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_POSECEF_USB_U1     = 0x483, /**< output rate of the UBX-NAV2-POSECEF message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_POSLLH_I2C_U1      = 0x485, /**< output rate of the UBX-NAV2-POSLLH message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_POSLLH_SPI_U1      = 0x489, /**< output rate of the UBX-NAV2-POSLLH message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_POSLLH_UART1_U1    = 0x486, /**< output rate of the UBX-NAV2-POSLLH message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_POSLLH_UART2_U1    = 0x487, /**< output rate of the UBX-NAV2-POSLLH message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_POSLLH_USB_U1      = 0x488, /**< output rate of the UBX-NAV2-POSLLH message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_PVT_I2C_U1         = 0x490, /**< output rate of the UBX-NAV2-PVT message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_PVT_SPI_U1         = 0x494, /**< output rate of the UBX-NAV2-PVT message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_PVT_UART1_U1       = 0x491, /**< output rate of the UBX-NAV2-PVT message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_PVT_UART2_U1       = 0x492, /**< output rate of the UBX-NAV2-PVT message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_PVT_USB_U1         = 0x493, /**< output rate of the UBX-NAV2-PVT message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SAT_I2C_U1         = 0x495, /**< output rate of the UBX-NAV2-SAT message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SAT_SPI_U1         = 0x499, /**< output rate of the UBX-NAV2-SAT message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SAT_UART1_U1       = 0x496, /**< output rate of the UBX-NAV2-SAT message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SAT_UART2_U1       = 0x497, /**< output rate of the UBX-NAV2-SAT message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SAT_USB_U1         = 0x498, /**< output rate of the UBX-NAV2-SAT message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SBAS_I2C_U1        = 0x500, /**< output rate of the UBX-NAV2-SBAS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SBAS_SPI_U1        = 0x504, /**< output rate of the UBX-NAV2-SBAS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SBAS_UART1_U1      = 0x501, /**< output rate of the UBX-NAV2-SBAS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SBAS_UART2_U1      = 0x502, /**< output rate of the UBX-NAV2-SBAS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SBAS_USB_U1        = 0x503, /**< output rate of the UBX-NAV2-SBAS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SIG_I2C_U1         = 0x505, /**< output rate of the UBX-NAV2-SIG message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SIG_SPI_U1         = 0x509, /**< output rate of the UBX-NAV2-SIG message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SIG_UART1_U1       = 0x506, /**< output rate of the UBX-NAV2-SIG message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SIG_UART2_U1       = 0x507, /**< output rate of the UBX-NAV2-SIG message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SIG_USB_U1         = 0x508, /**< output rate of the UBX-NAV2-SIG message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SLAS_I2C_U1        = 0x510, /**< output rate of the UBX-NAV2-SLAS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SLAS_SPI_U1        = 0x514, /**< output rate of the UBX-NAV2-SLAS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SLAS_UART1_U1      = 0x511, /**< output rate of the UBX-NAV2-SLAS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SLAS_UART2_U1      = 0x512, /**< output rate of the UBX-NAV2-SLAS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SLAS_USB_U1        = 0x513, /**< output rate of the UBX-NAV2-SLAS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_STATUS_I2C_U1      = 0x515, /**< output rate of the UBX-NAV2-STATUS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_STATUS_SPI_U1      = 0x519, /**< output rate of the UBX-NAV2-STATUS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_STATUS_UART1_U1    = 0x516, /**< output rate of the UBX-NAV2-STATUS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_STATUS_UART2_U1    = 0x517, /**< output rate of the UBX-NAV2-STATUS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_STATUS_USB_U1      = 0x518, /**< output rate of the UBX-NAV2-STATUS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SVIN_I2C_U1        = 0x520, /**< output rate of the UBX-NAV2-SVIN message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SVIN_SPI_U1        = 0x524, /**< output rate of the UBX-NAV2-SVIN message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SVIN_UART1_U1      = 0x521, /**< output rate of the UBX-NAV2-SVIN message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SVIN_UART2_U1      = 0x522, /**< output rate of the UBX-NAV2-SVIN message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_SVIN_USB_U1        = 0x523, /**< output rate of the UBX-NAV2-SVIN message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEBDS_I2C_U1     = 0x525, /**< output rate of the UBX-NAV2-TIMEBDS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEBDS_SPI_U1     = 0x529, /**< output rate of the UBX-NAV2-TIMEBDS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEBDS_UART1_U1   = 0x526, /**< output rate of the UBX-NAV2-TIMEBDS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEBDS_UART2_U1   = 0x527, /**< output rate of the UBX-NAV2-TIMEBDS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEBDS_USB_U1     = 0x528, /**< output rate of the UBX-NAV2-TIMEBDS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEGAL_I2C_U1     = 0x530, /**< output rate of the UBX-NAV2-TIMEGAL message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEGAL_SPI_U1     = 0x534, /**< output rate of the UBX-NAV2-TIMEGAL message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEGAL_UART1_U1   = 0x531, /**< output rate of the UBX-NAV2-TIMEGAL message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEGAL_UART2_U1   = 0x532, /**< output rate of the UBX-NAV2-TIMEGAL message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEGAL_USB_U1     = 0x533, /**< output rate of the UBX-NAV2-TIMEGAL message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEGLO_I2C_U1     = 0x535, /**< output rate of the UBX-NAV2-TIMEGLO message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEGLO_SPI_U1     = 0x539, /**< output rate of the UBX-NAV2-TIMEGLO message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEGLO_UART1_U1   = 0x536, /**< output rate of the UBX-NAV2-TIMEGLO message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEGLO_UART2_U1   = 0x537, /**< output rate of the UBX-NAV2-TIMEGLO message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEGLO_USB_U1     = 0x538, /**< output rate of the UBX-NAV2-TIMEGLO message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEGPS_I2C_U1     = 0x540, /**< output rate of the UBX-NAV2-TIMEGPS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEGPS_SPI_U1     = 0x544, /**< output rate of the UBX-NAV2-TIMEGPS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEGPS_UART1_U1   = 0x541, /**< output rate of the UBX-NAV2-TIMEGPS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEGPS_UART2_U1   = 0x542, /**< output rate of the UBX-NAV2-TIMEGPS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEGPS_USB_U1     = 0x543, /**< output rate of the UBX-NAV2-TIMEGPS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMELS_I2C_U1      = 0x545, /**< output rate of the UBX-NAV2-TIMELS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMELS_SPI_U1      = 0x549, /**< output rate of the UBX-NAV2-TIMELS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMELS_UART1_U1    = 0x546, /**< output rate of the UBX-NAV2-TIMELS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMELS_UART2_U1    = 0x547, /**< output rate of the UBX-NAV2-TIMELS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMELS_USB_U1      = 0x548, /**< output rate of the UBX-NAV2-TIMELS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEQZSS_I2C_U1    = 0x575, /**< output rate of the UBX-NAV2-TIMEQZSS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEQZSS_SPI_U1    = 0x579, /**< output rate of the UBX-NAV2-TIMEQZSS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEQZSS_UART1_U1  = 0x576, /**< output rate of the UBX-NAV2-TIMEQZSS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEQZSS_UART2_U1  = 0x577, /**< output rate of the UBX-NAV2-TIMEQZSS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEQZSS_USB_U1    = 0x578, /**< output rate of the UBX-NAV2-TIMEQZSS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEUTC_I2C_U1     = 0x550, /**< output rate of the UBX-NAV2-TIMEUTC message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEUTC_SPI_U1     = 0x554, /**< output rate of the UBX-NAV2-TIMEUTC message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEUTC_UART1_U1   = 0x551, /**< output rate of the UBX-NAV2-TIMEUTC message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEUTC_UART2_U1   = 0x552, /**< output rate of the UBX-NAV2-TIMEUTC message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_TIMEUTC_USB_U1     = 0x553, /**< output rate of the UBX-NAV2-TIMEUTC message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_VELECEF_I2C_U1     = 0x555, /**< output rate of the UBX-NAV2-VELECEF message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_VELECEF_SPI_U1     = 0x559, /**< output rate of the UBX-NAV2-VELECEF message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_VELECEF_UART1_U1   = 0x556, /**< output rate of the UBX-NAV2-VELECEF message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_VELECEF_UART2_U1   = 0x557, /**< output rate of the UBX-NAV2-VELECEF message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_VELECEF_USB_U1     = 0x558, /**< output rate of the UBX-NAV2-VELECEF message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_VELNED_I2C_U1      = 0x560, /**< output rate of the UBX-NAV2-VELNED message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_VELNED_SPI_U1      = 0x564, /**< output rate of the UBX-NAV2-VELNED message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_VELNED_UART1_U1    = 0x561, /**< output rate of the UBX-NAV2-VELNED message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_VELNED_UART2_U1     = 0x562, /**< output rate of the UBX-NAV2-VELNED message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV2_VELNED_USB_U1      = 0x563, /**< output rate of the UBX-NAV2-VELNED message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_AOPSTATUS_I2C_U1    = 0x079, /**< output rate of the UBX-NAV-AOPSTATUS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_AOPSTATUS_SPI_U1    = 0x07d, /**< output rate of the UBX-NAV-AOPSTATUS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_AOPSTATUS_UART1_U1  = 0x07a, /**< output rate of the UBX-NAV-AOPSTATUS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_AOPSTATUS_UART2_U1  = 0x07b, /**< output rate of the UBX-NAV-AOPSTATUS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_AOPSTATUS_USB_U1    = 0x07c, /**< output rate of the UBX-NAV-AOPSTATUS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_CLOCK_I2C_U1        = 0x065, /**< output rate of the UBX-NAV-CLOCK message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_CLOCK_SPI_U1        = 0x069, /**< output rate of the UBX-NAV-CLOCK message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_CLOCK_UART1_U1      = 0x066, /**< output rate of the UBX-NAV-CLOCK message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_CLOCK_UART2_U1      = 0x067, /**< output rate of the UBX-NAV-CLOCK message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_CLOCK_USB_U1        = 0x068, /**< output rate of the UBX-NAV-CLOCK message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_COV_I2C_U1          = 0x083, /**< output rate of the UBX-NAV-COV message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_COV_SPI_U1          = 0x087, /**< output rate of the UBX-NAV-COV message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_COV_UART1_U1        = 0x084, /**< output rate of the UBX-NAV-COV message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_COV_UART2_U1        = 0x085, /**< output rate of the UBX-NAV-COV message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_COV_USB_U1          = 0x086, /**< output rate of the UBX-NAV-COV message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_DOP_I2C_U1          = 0x038, /**< output rate of the UBX-NAV-DOP message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_DOP_SPI_U1          = 0x03c, /**< output rate of the UBX-NAV-DOP message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_DOP_UART1_U1        = 0x039, /**< output rate of the UBX-NAV-DOP message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_DOP_UART2_U1        = 0x03a, /**< output rate of the UBX-NAV-DOP message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_DOP_USB_U1          = 0x03b, /**< output rate of the UBX-NAV-DOP message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_EOE_I2C_U1          = 0x15f, /**< output rate of the UBX-NAV-EOE message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_EOE_SPI_U1          = 0x163, /**< output rate of the UBX-NAV-EOE message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_EOE_UART1_U1        = 0x160, /**< output rate of the UBX-NAV-EOE message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_EOE_UART2_U1        = 0x161, /**< output rate of the UBX-NAV-EOE message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_EOE_USB_U1          = 0x162, /**< output rate of the UBX-NAV-EOE message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_GEOFENCE_I2C_U1     = 0x0a1, /**< output rate of the UBX-NAV-GEOFENCE message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_GEOFENCE_SPI_U1     = 0x0a5, /**< output rate of the UBX-NAV-GEOFENCE message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_GEOFENCE_UART1_U1   = 0x0a2, /**< output rate of the UBX-NAV-GEOFENCE message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_GEOFENCE_UART2_U1   = 0x0a3, /**< output rate of the UBX-NAV-GEOFENCE message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_GEOFENCE_USB_U1     = 0x0a4, /**< output rate of the UBX-NAV-GEOFENCE message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_HPPOSECEF_I2C_U1    = 0x02e, /**< output rate of the UBX-NAV-HPPOSECEF message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_HPPOSECEF_SPI_U1    = 0x032, /**< output rate of the UBX-NAV-HPPOSECEF message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_HPPOSECEF_UART1_U1  = 0x02f, /**< output rate of the UBX-NAV-HPPOSECEF message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_HPPOSECEF_UART2_U1  = 0x030, /**< output rate of the UBX-NAV-HPPOSECEF message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_HPPOSECEF_USB_U1    = 0x031, /**< output rate of the UBX-NAV-HPPOSECEF message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_HPPOSLLH_I2C_U1     = 0x033, /**< output rate of the UBX-NAV-HPPOSLLH message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_HPPOSLLH_SPI_U1     = 0x037, /**< output rate of the UBX-NAV-HPPOSLLH message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_HPPOSLLH_UART1_U1   = 0x034, /**< output rate of the UBX-NAV-HPPOSLLH message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_HPPOSLLH_UART2_U1   = 0x035, /**< output rate of the UBX-NAV-HPPOSLLH message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_HPPOSLLH_USB_U1     = 0x036, /**< output rate of the UBX-NAV-HPPOSLLH message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_ODO_I2C_U1          = 0x07e, /**< output rate of the UBX-NAV-ODO message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_ODO_SPI_U1          = 0x082, /**< output rate of the UBX-NAV-ODO message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_ODO_UART1_U1        = 0x07f, /**< output rate of the UBX-NAV-ODO message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_ODO_UART2_U1        = 0x080, /**< output rate of the UBX-NAV-ODO message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_ODO_USB_U1          = 0x081, /**< output rate of the UBX-NAV-ODO message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_ORB_I2C_U1          = 0x010, /**< output rate of the UBX-NAV-ORB message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_ORB_SPI_U1          = 0x014, /**< output rate of the UBX-NAV-ORB message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_ORB_UART1_U1        = 0x011, /**< output rate of the UBX-NAV-ORB message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_ORB_UART2_U1        = 0x012, /**< output rate of the UBX-NAV-ORB message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_ORB_USB_U1          = 0x013, /**< output rate of the UBX-NAV-ORB message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_PL_I2C_U1           = 0x415, /**< output rate of the UBX-NAV-PL message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_PL_SPI_U1           = 0x419, /**< output rate of the UBX-NAV-PL message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_PL_UART1_U1         = 0x416, /**< output rate of the UBX-NAV-PL message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_PL_UART2_U1         = 0x417, /**< output rate of the UBX-NAV-PL message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_PL_USB_U1           = 0x418, /**< output rate of the UBX-NAV-PL message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_POSECEF_I2C_U1      = 0x024, /**< output rate of the UBX-NAV-POSECEF message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_POSECEF_SPI_U1      = 0x028, /**< output rate of the UBX-NAV-POSECEF message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_POSECEF_UART1_U1    = 0x025, /**< output rate of the UBX-NAV-POSECEF message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_POSECEF_UART2_U1    = 0x026, /**< output rate of the UBX-NAV-POSECEF message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_POSECEF_USB_U1      = 0x027, /**< output rate of the UBX-NAV-POSECEF message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_POSLLH_I2C_U1       = 0x029, /**< output rate of the UBX-NAV-POSLLH message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_POSLLH_SPI_U1       = 0x02d, /**< output rate of the UBX-NAV-POSLLH message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_POSLLH_UART1_U1     = 0x02a, /**< output rate of the UBX-NAV-POSLLH message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_POSLLH_UART2_U1     = 0x02b, /**< output rate of the UBX-NAV-POSLLH message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_POSLLH_USB_U1       = 0x02c, /**< output rate of the UBX-NAV-POSLLH message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_PVT_I2C_U1          = 0x006, /**< output rate of the UBX-NAV-PVT message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_PVT_SPI_U1          = 0x00a, /**< output rate of the UBX-NAV-PVT message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_PVT_UART1_U1        = 0x007, /**< output rate of the UBX-NAV-PVT message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_PVT_UART2_U1        = 0x008, /**< output rate of the UBX-NAV-PVT message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_PVT_USB_U1          = 0x009, /**< output rate of the UBX-NAV-PVT message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_RELPOSNED_I2C_U1    = 0x08d, /**< output rate of the UBX-NAV-RELPOSNED message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_RELPOSNED_SPI_U1    = 0x091, /**< output rate of the UBX-NAV-RELPOSNED message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_RELPOSNED_UART1_U1  = 0x08e, /**< output rate of the UBX-NAV-RELPOSNED message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_RELPOSNED_UART2_U1  = 0x08f, /**< output rate of the UBX-NAV-RELPOSNED message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_RELPOSNED_USB_U1    = 0x090, /**< output rate of the UBX-NAV-RELPOSNED message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SAT_I2C_U1          = 0x015, /**< output rate of the UBX-NAV-SAT message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SAT_SPI_U1          = 0x019, /**< output rate of the UBX-NAV-SAT message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SAT_UART1_U1        = 0x016, /**< output rate of the UBX-NAV-SAT message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SAT_UART2_U1        = 0x017, /**< output rate of the UBX-NAV-SAT message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SAT_USB_U1          = 0x018, /**< output rate of the UBX-NAV-SAT message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SBAS_I2C_U1         = 0x06a, /**< output rate of the UBX-NAV-SBAS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SBAS_SPI_U1         = 0x06e, /**< output rate of the UBX-NAV-SBAS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SBAS_UART1_U1       = 0x06b, /**< output rate of the UBX-NAV-SBAS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SBAS_UART2_U1       = 0x06c, /**< output rate of the UBX-NAV-SBAS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SBAS_USB_U1         = 0x06d, /**< output rate of the UBX-NAV-SBAS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SIG_I2C_U1          = 0x345, /**< output rate of the UBX-NAV-SIG message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SIG_SPI_U1          = 0x349, /**< output rate of the UBX-NAV-SIG message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SIG_UART1_U1        = 0x346, /**< output rate of the UBX-NAV-SIG message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SIG_UART2_U1        = 0x347, /**< output rate of the UBX-NAV-SIG message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SIG_USB_U1          = 0x348, /**< output rate of the UBX-NAV-SIG message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SLAS_I2C_U1         = 0x336, /**< output rate of the UBX-NAV-SLAS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SLAS_SPI_U1         = 0x33a, /**< output rate of the UBX-NAV-SLAS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SLAS_UART1_U1       = 0x337, /**< output rate of the UBX-NAV-SLAS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SLAS_UART2_U1       = 0x338, /**< output rate of the UBX-NAV-SLAS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SLAS_USB_U1         = 0x339, /**< output rate of the UBX-NAV-SLAS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_STATUS_I2C_U1       = 0x01a, /**< output rate of the UBX-NAV-STATUS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_STATUS_SPI_U1       = 0x01e, /**< output rate of the UBX-NAV-STATUS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_STATUS_UART1_U1     = 0x01b, /**< output rate of the UBX-NAV-STATUS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_STATUS_UART2_U1     = 0x01c, /**< output rate of the UBX-NAV-STATUS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_STATUS_USB_U1       = 0x01d, /**< output rate of the UBX-NAV-STATUS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SVIN_I2C_U1         = 0x088, /**< output rate of the UBX-NAV-SVIN message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SVIN_SPI_U1         = 0x08c, /**< output rate of the UBX-NAV-SVIN message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SVIN_UART1_U1       = 0x089, /**< output rate of the UBX-NAV-SVIN message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SVIN_UART2_U1       = 0x08a, /**< output rate of the UBX-NAV-SVIN message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_SVIN_USB_U1         = 0x08b, /**< output rate of the UBX-NAV-SVIN message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEBDS_I2C_U1      = 0x051, /**< output rate of the UBX-NAV-TIMEBDS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEBDS_SPI_U1      = 0x055, /**< output rate of the UBX-NAV-TIMEBDS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEBDS_UART1_U1    = 0x052, /**< output rate of the UBX-NAV-TIMEBDS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEBDS_UART2_U1    = 0x053, /**< output rate of the UBX-NAV-TIMEBDS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEBDS_USB_U1      = 0x054, /**< output rate of the UBX-NAV-TIMEBDS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEGAL_I2C_U1      = 0x056, /**< output rate of the UBX-NAV-TIMEGAL message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEGAL_SPI_U1      = 0x05a, /**< output rate of the UBX-NAV-TIMEGAL message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEGAL_UART1_U1    = 0x057, /**< output rate of the UBX-NAV-TIMEGAL message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEGAL_UART2_U1    = 0x058, /**< output rate of the UBX-NAV-TIMEGAL message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEGAL_USB_U1      = 0x059, /**< output rate of the UBX-NAV-TIMEGAL message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEGLO_I2C_U1      = 0x04c, /**< output rate of the UBX-NAV-TIMEGLO message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEGLO_SPI_U1      = 0x050, /**< output rate of the UBX-NAV-TIMEGLO message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEGLO_UART1_U1    = 0x04d, /**< output rate of the UBX-NAV-TIMEGLO message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEGLO_UART2_U1    = 0x04e, /**< output rate of the UBX-NAV-TIMEGLO message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEGLO_USB_U1      = 0x04f, /**< output rate of the UBX-NAV-TIMEGLO message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEGPS_I2C_U1      = 0x047, /**< output rate of the UBX-NAV-TIMEGPS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEGPS_SPI_U1      = 0x04b, /**< output rate of the UBX-NAV-TIMEGPS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEGPS_UART1_U1    = 0x048, /**< output rate of the UBX-NAV-TIMEGPS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEGPS_UART2_U1    = 0x049, /**< output rate of the UBX-NAV-TIMEGPS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEGPS_USB_U1      = 0x04a, /**< output rate of the UBX-NAV-TIMEGPS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMELS_I2C_U1       = 0x060, /**< output rate of the UBX-NAV-TIMELS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMELS_SPI_U1       = 0x064, /**< output rate of the UBX-NAV-TIMELS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMELS_UART1_U1     = 0x061, /**< output rate of the UBX-NAV-TIMELS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMELS_UART2_U1     = 0x062, /**< output rate of the UBX-NAV-TIMELS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMELS_USB_U1       = 0x063, /**< output rate of the UBX-NAV-TIMELS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEQZSS_I2C_U1     = 0x386, /**< output rate of the UBX-NAV-TIMEQZSS message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEQZSS_SPI_U1     = 0x38a, /**< output rate of the UBX-NAV-TIMEQZSS message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEQZSS_UART1_U1   = 0x387, /**< output rate of the UBX-NAV-TIMEQZSS message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEQZSS_UART2_U1   = 0x388, /**< output rate of the UBX-NAV-TIMEQZSS message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEQZSS_USB_U1     = 0x389, /**< output rate of the UBX-NAV-TIMEQZSS message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEUTC_I2C_U1      = 0x05b, /**< output rate of the UBX-NAV-TIMEUTC message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEUTC_SPI_U1      = 0x05f, /**< output rate of the UBX-NAV-TIMEUTC message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEUTC_UART1_U1    = 0x05c, /**< output rate of the UBX-NAV-TIMEUTC message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEUTC_UART2_U1    = 0x05d, /**< output rate of the UBX-NAV-TIMEUTC message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_TIMEUTC_USB_U1      = 0x05e, /**< output rate of the UBX-NAV-TIMEUTC message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_VELECEF_I2C_U1      = 0x03d, /**< output rate of the UBX-NAV-VELECEF message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_VELECEF_SPI_U1      = 0x041, /**< output rate of the UBX-NAV-VELECEF message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_VELECEF_UART1_U1    = 0x03e, /**< output rate of the UBX-NAV-VELECEF message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_VELECEF_UART2_U1    = 0x03f, /**< output rate of the UBX-NAV-VELECEF message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_VELECEF_USB_U1      = 0x040, /**< output rate of the UBX-NAV-VELECEF message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_VELNED_I2C_U1       = 0x042, /**< output rate of the UBX-NAV-VELNED message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_VELNED_SPI_U1       = 0x046, /**< output rate of the UBX-NAV-VELNED message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_VELNED_UART1_U1     = 0x043, /**< output rate of the UBX-NAV-VELNED message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_VELNED_UART2_U1     = 0x044, /**< output rate of the UBX-NAV-VELNED message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_NAV_VELNED_USB_U1       = 0x045, /**< output rate of the UBX-NAV-VELNED message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_COR_I2C_U1          = 0x6b6, /**< output rate of the UBX-RXM-COR message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_COR_SPI_U1          = 0x6ba, /**< output rate of the UBX-RXM-COR message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_COR_UART1_U1        = 0x6b7, /**< output rate of the UBX-RXM-COR message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_COR_UART2_U1        = 0x6b8, /**< output rate of the UBX-RXM-COR message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_COR_USB_U1          = 0x6b9, /**< output rate of the UBX-RXM-COR message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_MEASX_I2C_U1        = 0x204, /**< output rate of the UBX-RXM-MEASX message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_MEASX_SPI_U1        = 0x208, /**< output rate of the UBX-RXM-MEASX message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_MEASX_UART1_U1      = 0x205, /**< output rate of the UBX-RXM-MEASX message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_MEASX_UART2_U1      = 0x206, /**< output rate of the UBX-RXM-MEASX message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_MEASX_USB_U1        = 0x207, /**< output rate of the UBX-RXM-MEASX message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_PMP_I2C_U1          = 0x31d, /**< output rate of the UBX-RXM-PMP message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_PMP_SPI_U1          = 0x321, /**< output rate of the UBX-RXM-PMP message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_PMP_UART1_U1        = 0x31e, /**< output rate of the UBX-RXM-PMP message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_PMP_UART2_U1        = 0x31f, /**< output rate of the UBX-RXM-PMP message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_PMP_USB_U1          = 0x320, /**< output rate of the UBX-RXM-PMP message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_QZSSL6_UART1_U1     = 0x33b, /**< output rate of the UBX-RXM-QZSSL6 message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_QZSSL6_UART2_U1     = 0x33c, /**< output rate of the UBX-RXM-QZSSL6 message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_QZSSL6_USB_U1       = 0x33d, /**< output rate of the UBX-RXM-QZSSL6 message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_QZSSL6_I2C_U1       = 0x33f, /**< output rate of the UBX-RXM-QZSSL6 message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_QZSSL6_SPI_U1       = 0x33e, /**< output rate of the UBX-RXM-QZSSL6 message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_RAWX_I2C_U1         = 0x2a4, /**< output rate of the UBX-RXM-RAWX message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_RAWX_SPI_U1         = 0x2a8, /**< output rate of the UBX-RXM-RAWX message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_RAWX_UART1_U1       = 0x2a5, /**< output rate of the UBX-RXM-RAWX message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_RAWX_UART2_U1       = 0x2a6, /**< output rate of the UBX-RXM-RAWX message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_RAWX_USB_U1         = 0x2a7, /**< output rate of the UBX-RXM-RAWX message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_RLM_I2C_U1          = 0x25e, /**< output rate of the UBX-RXM-RLM message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_RLM_SPI_U1          = 0x262, /**< output rate of the UBX-RXM-RLM message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_RLM_UART1_U1        = 0x25f, /**< output rate of the UBX-RXM-RLM message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_RLM_UART2_U1        = 0x260, /**< output rate of the UBX-RXM-RLM message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_RLM_USB_U1          = 0x261, /**< output rate of the UBX-RXM-RLM message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_RTCM_I2C_U1         = 0x268, /**< output rate of the UBX-RXM-RTCM message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_RTCM_SPI_U1         = 0x26c, /**< output rate of the UBX-RXM-RTCM message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_RTCM_UART1_U1       = 0x269, /**< output rate of the UBX-RXM-RTCM message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_RTCM_UART2_U1       = 0x26a, /**< output rate of the UBX-RXM-RTCM message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_RTCM_USB_U1         = 0x26b, /**< output rate of the UBX-RXM-RTCM message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_SFRBX_I2C_U1        = 0x231, /**< output rate of the UBX-RXM-SFRBX message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_SFRBX_SPI_U1        = 0x235, /**< output rate of the UBX-RXM-SFRBX message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_SFRBX_UART1_U1      = 0x232, /**< output rate of the UBX-RXM-SFRBX message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_SFRBX_UART2_U1      = 0x233, /**< output rate of the UBX-RXM-SFRBX message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_SFRBX_USB_U1        = 0x234, /**< output rate of the UBX-RXM-SFRBX message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_SPARTN_I2C_U1       = 0x605, /**< output rate of the UBX-RXM-SPARTN message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_SPARTN_SPI_U1       = 0x609, /**< output rate of the UBX-RXM-SPARTN message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_SPARTN_UART1_U1     = 0x606, /**< output rate of the UBX-RXM-SPARTN message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_SPARTN_UART2_U1     = 0x607, /**< output rate of the UBX-RXM-SPARTN message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_RXM_SPARTN_USB_U1       = 0x608, /**< output rate of the UBX-RXM-SPARTN message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_TIM_TM2_I2C_U1          = 0x178, /**< output rate of the UBX-TIM-TM2 message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_TIM_TM2_SPI_U1          = 0x17c, /**< output rate of the UBX-TIM-TM2 message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_TIM_TM2_UART1_U1        = 0x179, /**< output rate of the UBX-TIM-TM2 message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_TIM_TM2_UART2_U1        = 0x17a, /**< output rate of the UBX-TIM-TM2 message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_TIM_TM2_USB_U1          = 0x17b, /**< output rate of the UBX-TIM-TM2 message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_TIM_TP_I2C_U1           = 0x17d, /**< output rate of the UBX-TIM-TP message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_TIM_TP_SPI_U1           = 0x181, /**< output rate of the UBX-TIM-TP message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_TIM_TP_UART1_U1         = 0x17e, /**< output rate of the UBX-TIM-TP message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_TIM_TP_UART2_U1         = 0x17f, /**< output rate of the UBX-TIM-TP message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_TIM_TP_USB_U1           = 0x180, /**< output rate of the UBX-TIM-TP message on port USB per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_TIM_VRFY_I2C_U1         = 0x092, /**< output rate of the UBX-TIM-VRFY message on port I2C per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_TIM_VRFY_SPI_U1         = 0x096, /**< output rate of the UBX-TIM-VRFY message on port SPI per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_TIM_VRFY_UART1_U1       = 0x093, /**< output rate of the UBX-TIM-VRFY message on port UART1 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_TIM_VRFY_UART2_U1       = 0x094, /**< output rate of the UBX-TIM-VRFY message on port UART2 per epoch. */
    U_GNSS_CFG_VAL_KEY_ITEM_MSGOUT_UBX_TIM_VRFY_USB_U1         = 0x095  /**< output rate of the UBX-TIM-VRFY message on port USB per epoch. */
} uGnssCfgValKeyItemMsgout_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_NAV2.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_NAV2_OUT_ENABLED_L         = 0x01, /**< enable the secondary output (GNSS standalone output).
                                                                    It can be used simultaneously with the available primary
                                                                    output (high precision, sensor fusion or time mode output). */
    U_GNSS_CFG_VAL_KEY_ITEM_NAV2_SBAS_USE_INTEGRITY_L  = 0x02  /**< if enabled, the receiver will only use GPS satellites for
                                                                    which integrity information is available. This configuration
                                                                    item allows configuring the SBAS integrity feature differently
                                                                    for the primary output and the secondary output. For configuring
                                                                    the primary output, see #U_GNSS_CFG_VAL_KEY_ITEM_SBAS_USE_INTEGRITY_L. */
} uGnssCfgValKeyItemNav2_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_NAVHPG.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_NAVHPG_DGNSSMODE_E1          = 0x11  /**< differential corrections mode, see
                                                                      #uGnssCfgValKeyItemValueNavhpgDgnssmode_t. */
} uGnssCfgValKeyItemNavhpg_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_NAVHPG_DGNSSMODE_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NAVHPG_DGNSSMODE_RTX_FLOAT = 2, /**< no attempts made to fix ambiguities. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NAVHPG_DGNSSMODE_RTX_MIXED = 3  /**< ambiguities are fixed whenever possible. */
} uGnssCfgValKeyItemValueNavhpgDgnssmode_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_NAVSPG.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_FIXMODE_E1        = 0x11, /**< position fix mode; see #uGnssFixMode_t; uGnssCfgGetFixMode()/
                                                                  uGnssCfgSetFixMode() may also be used for a non-persistent
                                                                  setting. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_INIFIX3D_L        = 0x13, /**< set this to require the initial fix to be a 3D fix. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_WKNROLLOVER_U2    = 0x17, /**< GPS week rollover number; GPS week numbers will be set
                                                                  correctly from this week up to 1024 weeks after this week,
                                                                  range 1 to 4096. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USE_PPP_L         = 0x19, /**< set this to use precise point positioning (PPP). */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_UTCSTANDARD_E1    = 0x1c, /**< the UTC standard to be used, see #uGnssUtcStandard_t;
                                                                  uGnssCfgGetUtcStandard()/uGnssCfgSetUtcStandard() may
                                                                  also be used for a non-persistent setting. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_DYNMODEL_E1       = 0x21, /**< set the dynamic model; see #uGnssDynamic_t; uGnssCfgGetDynamic()/
                                                                  uGnssCfgSetDynamic() may also be used for a non-persistent
                                                                  setting. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_ACKAIDING_L       = 0x25, /**< set this to acknowledge assistance input messages. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_L          = 0x61, /**< set this to use user geodetic datum parameters; this must
                                                                  be set together with all of the other
                                                                  U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_XXX parameters. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_MAJA_R8    = 0x62, /**< the geodetic datum semi-major axis in meters, range
                                                                  6,300,000.0 to 6,500,000.0 meters; only used if
                                                                  #U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_L is set and
                                                                  must be set with all of the other
                                                                  U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_XXX parameters. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_FLAT_R8    = 0x63, /**< geodetic datum 1.0/flaggening, range 0.0 to 500.0; only used
                                                                  if #U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_L is set and
                                                                  must be set with all of the other
                                                                  U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_XXX parameters. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_DX_R4      = 0x64, /**< geodetic datum X-axis shift at the origin, range
                                                                  +/-5000.0 meters; only used if
                                                                  #U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_L is set and
                                                                  must be set with all of the other
                                                                  U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_XXX parameters. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_DY_R4      = 0x65, /**< geodetic datum Y-axis shift at the origin, range
                                                                  +/-5000.0 meters; only used if
                                                                  #U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_L is set and
                                                                  must be set with all of the other
                                                                  U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_XXX parameters. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_DZ_R4      = 0x66, /**< geodetic datum Z-axis shift at the origin, range
                                                                  +/-5000.0 meters; only used if
                                                                  #U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_L is set and
                                                                  must be set with all of the other
                                                                  U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_XXX parameters. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_ROTX_R4    = 0x67, /**< geodetic datum rotation about the X-axis, range
                                                                  +/-20.0 milli arc seconds; only used if
                                                                  #U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_L is set and
                                                                  must be set with all of the other
                                                                  U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_XXX parameters. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_ROTY_R4    = 0x68, /**< geodetic datum rotation about the Y-axis, range
                                                                  +/-20.0 milli arc seconds; only used if
                                                                  #U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_L is set and
                                                                  must be set with all of the other
                                                                  U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_XXX parameters. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_ROTZ_R4    = 0x69, /**< geodetic datum rotation about the Z-axis, range
                                                                  +/-20.0 milli arc seconds; only used if
                                                                  #U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_L is set and
                                                                  must be set with all of the other
                                                                  U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_XXX parameters. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_SCALE_R4   = 0x6a, /**< geodetic datum scale factor, range 0.0 to 50.0 PPM;
                                                                  only used if
                                                                  #U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_L is set and
                                                                  must be set with all of the other
                                                                  U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_USRDAT_XXX parameters. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_INFIL_MINSVS_U1   = 0xa1, /**< minimum number of satellites for navigation. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_INFIL_MAXSVS_U1   = 0xa2, /**< maximum number of satellites for navigation. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_INFIL_MINCNO_U1   = 0xa3, /**< minimum signal level for navigation in dBHz. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_INFIL_MINELEV_I1  = 0xa4, /**< minimum elevation for a satellite to be used in
                                                                  navigation in degrees. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_INFIL_NCNOTHRS_U1 = 0xaa, /**< number of satellites required to have C/N0 above
                                                                  #U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_INFIL_CNOTHRS_U1 for
                                                                  a fix to be attempted. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_INFIL_CNOTHRS_U1  = 0xab, /**< C/N0 threshold for deciding whether to attempt a fix. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_OUTFIL_PDOP_U2    = 0xb1, /**< output filter position DOP mask (threshold) (x10). */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_OUTFIL_TDOP_U2    = 0xb2, /**< output filter time DOP mask (threshold) (x10). */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_OUTFIL_PACC_U2    = 0xb3, /**< output filter position accuracy mask (threshold) in metres. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_OUTFIL_TACC_U2    = 0xb4, /**< output filter time accuracy mask (threshold). */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_OUTFIL_FACC_U2    = 0xb5, /**< output filter frequency accuracy mask (threshold) in
                                                                  centimetres/second. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_CONSTR_ALT_I4     = 0xc1, /**< fixed altitude (mean sea level) for 2D fix mode in
                                                                  centimetres. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_CONSTR_ALTVAR_U4  = 0xc2, /**< fixed altitude variance for 2D mode in centimetres
                                                                  squared. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_CONSTR_DGNSSTO_U1 = 0xc4, /**< DGNSS timeout in seconds. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_SIGATTCOMP_E1     = 0xd6, /**< permanently attenuated signal compensation mode, range
                                                                  1 to 63 dBHz or use 0 to diable attenuated signal
                                                                  compensation mode or 255 to decide automatically. */
    U_GNSS_CFG_VAL_KEY_ITEM_NAVSPG_PL_ENA_L          = 0xd7  /**< enable Protection level. */
} uGnssCfgValKeyItemNavspg_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_NMEA.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_PROTVER_E1       = 0x01, /**< NMEA protocol version, see #uGnssCfgValKeyItemValueNmeaProtver_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_MAXSVS_E1        = 0x02, /**< maximum number of SVs to report per Talker ID, see
                                                               #uGnssCfgValKeyItemValueNmeaMaxsvs_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_COMPAT_L         = 0x03, /**< this might be needed for certain applications, e.g.
                                                               for an NMEA parser that expects a fixed number of digits
                                                               in position coordinates. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_CONSIDER_L       = 0x04, /**< this will affect NMEA output used satellite count; ff set,
                                                               also considered satellites (e.g. RAIMED) are counted as used
                                                               satellites as well. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_LIMIT82_L        = 0x05, /**< enable strict limit to 82 characters maximum NMEA message length. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_HIGHPREC_L       = 0x06, /**< enable high precision mode; this flag cannot be set in
                                                               conjunction with #U_GNSS_CFG_VAL_KEY_ITEM_NMEA_COMPAT_L
                                                               or #U_GNSS_CFG_VAL_KEY_ITEM_NMEA_LIMIT82_L. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_SVNUMBERING_E1   = 0x07, /**< display satellites configuration that do not have
                                                               a value defined in NMEA, see #uGnssCfgValKeyItemValueNmeaSvnumbering_t;
                                                               this does not apply to satellites with an unknown ID. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_FILT_GPS_L       = 0x11, /**< disable reporting of GPS satellites. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_FILT_SBAS_L      = 0x12, /**< disable reporting of SBAS satellites. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_FILT_GAL_L       = 0x13, /**< disable reporting of Galileo satellites. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_FILT_QZSS_L      = 0x15, /**< disable reporting of QZSS satellites. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_FILT_GLO_L       = 0x16, /**< disable reporting of GLONASS satellites. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_FILT_BDS_L       = 0x17, /**< disable reporting of BeiDou satellites. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_OUT_INVFIX_L     = 0x21, /**< enable position output for failed or invalid fixes. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_OUT_MSKFIX_L     = 0x22, /**< enable position output for invalid fixes. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_OUT_INVTIME_L    = 0x23, /**< enable time output for invalid times. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_OUT_INVDATE_L    = 0x24, /**< enable time output for invalid dates. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_OUT_ONLYGPS_L    = 0x25, /**< enable output to GPS satellites only. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_OUT_FROZENCOG_L  = 0x26, /**< enable course over ground output even if it is frozen. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_MAINTALKERID_E1  = 0x31, /**< by default the main Talker ID (i.e. the Talker ID used
                                                               for all messages other than GSV) is determined by the GNSS
                                                               assignment of the receiver's channels (see
                                                               #U_GNSS_CFG_VAL_KEY_GROUP_ID_SIGNAL); this field enables
                                                               the main Talker ID to be overridden.  See
                                                               #uGnssCfgValKeyItemValueNmeaMaintalkerid_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_GSVTALKERID_E1   = 0x32, /**< by default the Talker ID for GSV messages is
                                                               GNSS-specific (as defined by NMEA); this field enables
                                                               the GSV  Talker ID to be overridden.  See
                                                               #uGnssCfgValKeyItemValueNmeaGsvtalkerid_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_NMEA_BDSTALKERID_U2   = 0x33  /**< sets the two ASCII characters that should be used
                                                               for the BeiDou Talker ID; if these are set to zero
                                                               the default BeiDou Talker ID will be used. */
} uGnssCfgValKeyItemNmea_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_NMEA_PROTVER_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_PROTVER_V21   = 21, /**< NMEA protocol version 2.1. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_PROTVER_V23   = 23, /**< NMEA protocol version 2.3. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_PROTVER_V40   = 40, /**< NMEA protocol version 4.0 (not available in all products). */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_PROTVER_V41   = 41, /**< NMEA protocol version 4.10 (not available in all products). */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_PROTVER_V411  = 42  /**< NMEA protocol version 4.11 (not available in all products). */
} uGnssCfgValKeyItemValueNmeaProtver_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_NMEA_MAXSVS_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_MAXSVS_UNLIM  = 0,  /**< unlimited. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_MAXSVS_8SVS   = 8,  /**< 8 satellites. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_MAXSVS_12SVS  = 12, /**< 12 satellites. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_MAXSVS_16SVS  = 16  /**< 16 satellites. */
} uGnssCfgValKeyItemValueNmeaMaxsvs_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_NMEA_SVNUMBERING_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_SVNUMBERING_STRICT    = 0, /**< satellites are not output. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_SVNUMBERING_EXTENDED  = 1  /**< use proprietary numbering. */
} uGnssCfgValKeyItemValueNmeaSvnumbering_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_NMEA_MAINTALKERID_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_MAINTALKERID_AUTO = 0, /**< main Talker ID is not overridden. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_MAINTALKERID_GP   = 1, /**< set main Talker ID to 'GP'. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_MAINTALKERID_GL   = 2, /**< set main Talker ID to 'GL'. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_MAINTALKERID_GN   = 3, /**< set main Talker ID to 'GN'. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_MAINTALKERID_GA   = 4, /**< set main Talker ID to 'GA' (not available in all products). */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_MAINTALKERID_GB   = 5, /**< set main Talker ID to 'GB' (not available in all products). */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_MAINTALKERID_GQ   = 7  /**< set main Talker ID to 'GQ' (not available in all products). */
} uGnssCfgValKeyItemValueNmeaMaintalkerid_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_NMEA_GSVTALKERID_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_GSVTALKERID_GNSS  = 0, /**< use GNSS-specific Talker ID (as defined by NMEA). */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_NMEA_GSVTALKERID_MAIN  = 1  /**< use the main Talker ID. */
} uGnssCfgValKeyItemValueNmeaGsvtalkerid_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_ODO.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_ODO_USE_ODO_L       = 0x01, /**< use odometer. */
    U_GNSS_CFG_VAL_KEY_ITEM_ODO_USE_COG_L       = 0x02, /**< use low-speed course over ground filter. */
    U_GNSS_CFG_VAL_KEY_ITEM_ODO_OUTLPVEL_L      = 0x03, /**< output low-pass filtered velocity. */
    U_GNSS_CFG_VAL_KEY_ITEM_ODO_OUTLPCOG_L      = 0x04, /**< output low-pass filtered course over ground (heading). */
    U_GNSS_CFG_VAL_KEY_ITEM_ODO_PROFILE_E1      = 0x05, /**< odometer profile configuration; see
                                                             #uGnssCfgValKeyItemValueOdoProfile_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_ODO_COGMAXSPEED_U1  = 0x21, /**< upper speed limit for low-speed course over ground
                                                             filter in metres/second. */
    U_GNSS_CFG_VAL_KEY_ITEM_ODO_COGMAXPOSACC_U1 = 0x22, /**< maximum acceptable position accuracy for computing
                                                             low-speed filtered course over ground. */
    U_GNSS_CFG_VAL_KEY_ITEM_ODO_VELLPGAIN_U1    = 0x31, /**< velocity low-pass filter level; range 0 to 255. */
    U_GNSS_CFG_VAL_KEY_ITEM_ODO_COGLPGAIN_U1    = 0x32  /**< course over ground low-pass filter level (at
                                                             speed < 8 m/s); range 0 to 255. */
} uGnssCfgValKeyItemOdo_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_ODO_PROFILE_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_ODO_PROFILE_RUN     = 0, /**< running. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_ODO_PROFILE_CYCL    = 1, /**< cycling. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_ODO_PROFILE_SWIM    = 2, /**< swimming. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_ODO_PROFILE_CAR     = 3, /**< driving. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_ODO_PROFILE_CUSTOM  = 4  /**< custom. */
} uGnssCfgValKeyItemValueOdoProfile_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_PM.
 */
typedef enum  {
    U_GNSS_CFG_VAL_KEY_ITEM_PM_OPERATEMODE_E1       = 0x01, /**< setting this to either
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_PM_OPERATEMODE_PSMOO
                                                                 or #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_PM_OPERATEMODE_PSMCT
                                                                 will turn the corresponding mode on; setting this to#
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_PM_OPERATEMODE_FULL will
                                                                 turn any PSM off. See
                                                                 #uGnssCfgValKeyItemValuePmOperatemode_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_PM_POSUPDATEPERIOD_U4   = 0x02, /**< position update period for
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_PM_OPERATEMODE_PSMOO
                                                                 in seconds, range >= 5 and smaller than the number
                                                                 of seconds in a week; if set to 0, the receiver will
                                                                 never retry a fix and will wait for external events. */
    U_GNSS_CFG_VAL_KEY_ITEM_PM_ACQPERIOD_U4         = 0x03, /**< acquisition period in seonds, used if the receiver
                                                                 previously failed to achieve a position fix; if set
                                                                 to 0 the receiver will never retry an acquisition
                                                                 and will wait for external events. */
    U_GNSS_CFG_VAL_KEY_ITEM_PM_GRIDOFFSET_U4        = 0x04, /**< position update period grid offset relative to
                                                                 GPS start of week in seconds; if set to 0 the
                                                                 position update periods are aligned to the GPS week. */
    U_GNSS_CFG_VAL_KEY_ITEM_PM_ONTIME_U2            = 0x05, /**< time to stay in tracking state in seconds; if set to 0
                                                                 the receiver will only very briefly enter tracking state
                                                                 (after acquisition) and then go back to inactive state. */
    U_GNSS_CFG_VAL_KEY_ITEM_PM_MINACQTIME_U1        = 0x06, /**< minimum time to spend in acquisition state in seconds. */
    U_GNSS_CFG_VAL_KEY_ITEM_PM_MAXACQTIME_U1        = 0x07, /**< maximum time to spend in acquisition state in seconds;
                                                                 if set to 0 the bound is disabled. */
    U_GNSS_CFG_VAL_KEY_ITEM_PM_ONOTENTEROFF_L       = 0x08, /**< disable to make the receiver enter (inactive) awaiting
                                                                 next search state, enable to make the receiver not enter
                                                                 (inactive) awaiting next search state but keep trying to
                                                                 acquire a fix instead. */
    U_GNSS_CFG_VAL_KEY_ITEM_PM_WAITTIMEFIX_L        = 0x09, /**< disable to wait for normal fix OK before starting
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_PM_ONTIME_U2,
                                                                 enable to wait for time fix OK before starting
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_PM_ONTIME_U2. */
    U_GNSS_CFG_VAL_KEY_ITEM_PM_UPDATEEPH_L          = 0x0a, /**< disable to not wake up to update ephemeris data, enable to
                                                                 add extra wake-up cycles to update the ephemeris data. */
    U_GNSS_CFG_VAL_KEY_ITEM_PM_EXTINTSEL_E1         = 0x0b, /**< EXTINT pin select; see
                                                                 #uGnssCfgValKeyItemValuePmExtintsel_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_PM_EXTINTWAKE_L         = 0x0c, /**< enable to keep the receiver awake as long as the selected
                                                                 EXTINT pin is high. */
    U_GNSS_CFG_VAL_KEY_ITEM_PM_EXTINTBACKUP_L       = 0x0d, /**< enable to force receiver into BACKUP mode when the selected
                                                                 EXTINT pin is low. */
    U_GNSS_CFG_VAL_KEY_ITEM_PM_EXTINTINACTIVE_L     = 0x0e, /**< enable to force backup in case the EXTINT pin is inactive for
                                                                 a time longer than
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_PM_EXTINTINACTIVITY_U4. */
    U_GNSS_CFG_VAL_KEY_ITEM_PM_EXTINTINACTIVITY_U4  = 0x0f, /**< inactivity timeout in milliseconds out on the EXTINT pin if
                                                                 enabled. */
    U_GNSS_CFG_VAL_KEY_ITEM_PM_LIMITPEAKCURR_L      = 0x10  /**< limit the peak current. */
} uGnssCfgValKeyItemPm_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_PM_OPERATEMODE_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_PM_OPERATEMODE_FULL    = 0, /**< normal operation, no power save mode active. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_PM_OPERATEMODE_PSMOO   = 1, /**< PSM ON/OFF operation */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_PM_OPERATEMODE_PSMCT   = 2  /**< PSM cyclic tracking operation. */
} uGnssCfgValKeyItemValuePmOperatemode_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_PM_EXTINTSEL_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_PM_EXTINTSEL_EXTINT0 = 0, /**< EXTINT0 pin 0. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_PM_EXTINTSEL_EXTINT1 = 1  /**< EXTINT0 pin 1. */
} uGnssCfgValKeyItemValuePmExtintsel_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_PMP.
 */
typedef enum  {
    U_GNSS_CFG_VAL_KEY_ITEM_PMP_CENTER_FREQUENCY_U4  = 0x11, /**< the center frequency for the receiver;
                                                                  can be set from 1525000000 to 1559000000 Hz. */
    U_GNSS_CFG_VAL_KEY_ITEM_PMP_SEARCH_WINDOW_U2     = 0x12, /**< the search window; can be set from 0 to 65535 Hz.
                                                                  It is +/- this value from the center frequency
                                                                  set by #U_GNSS_CFG_VAL_KEY_ITEM_PMP_CENTER_FREQUENCY_U4. */
    U_GNSS_CFG_VAL_KEY_ITEM_PMP_USE_SERVICE_ID_L     = 0x16, /**< enable/disable service ID check to confirm the
                                                                  correct service is received. */
    U_GNSS_CFG_VAL_KEY_ITEM_PMP_SERVICE_ID_U2        = 0x17, /**< the expected service ID. */
    U_GNSS_CFG_VAL_KEY_ITEM_PMP_DATA_RATE_E2         = 0x13, /**< the data rate of the received data, see
                                                                  #uGnssCfgValKeyItemValuePmpDataRate_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_PMP_USE_DESCRAMBLER_L    = 0x14, /**< enables or disable the descrambler. */
    U_GNSS_CFG_VAL_KEY_ITEM_PMP_DESCRAMBLER_INIT_U2  = 0x15, /**< the intialisation value for the descrambler. */
    U_GNSS_CFG_VAL_KEY_ITEM_PMP_USE_PRESCRAMBLING_L  = 0x19, /**< enables or disables prescrambling. */
    U_GNSS_CFG_VAL_KEY_ITEM_PMP_UNIQUE_WORD_U8       = 0x1a  /**< unique word. */
} uGnssCfgValKeyItemPmp_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_PMP_DATA_RATE_E2.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_PMP_DATA_RATE_B600  = 600,  /**< 600 bits per second. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_PMP_DATA_RATE_B1200 = 1200, /**< 1200 bits per second. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_PMP_DATA_RATE_B2400 = 2400, /**< 2400 bits per second. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_PMP_DATA_RATE_B4800 = 4800  /**< 4800 bits per second. */
} uGnssCfgValKeyItemValuePmpDataRate_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_QZSS.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_QZSS_USE_SLAS_DGNSS_L       = 0x05, /**< set to apply QZSS SLAS DGNSS corrections. */
    U_GNSS_CFG_VAL_KEY_ITEM_QZSS_USE_SLAS_TESTMODE_L    = 0x06, /**< set to use QZSS SLAS data when it is in test
                                                                     mode (SLAS message 0). */
    U_GNSS_CFG_VAL_KEY_ITEM_QZSS_USE_SLAS_RAIM_UNCORR_L = 0x07, /**< raim out measurements that are not corrected
                                                                     by QZSS SLAS if at least 5 measurements are
                                                                     corrected. */
    U_GNSS_CFG_VAL_KEY_ITEM_QZSS_SLAS_MAX_BASELINE_U2   = 0x08, /**< SLAS corrections are only applied if the
                                                                     receiver is at most this far away from the
                                                                     closest ground monitoring station (GMS).
                                                                     Note that due to the nature of the service,
                                                                     the usefulness of corrections degrades with
                                                                     distance. When far away from GMS, SBAS may
                                                                     be a better correction source. */
    U_GNSS_CFG_VAL_KEY_ITEM_QZSS_L6_SVIDA_I1            = 0x20, /**< QZSS L6 SV ID to be decoded by channel A;
                                                                     -1 = disable channel; 0 = automatic selection;
                                                                     1, 2, ... = manual satellite selection. */
    U_GNSS_CFG_VAL_KEY_ITEM_QZSS_L6_SVIDB_I1            = 0x30, /**< QZSS L6 SV ID to be decoded by channel B;
                                                                     -1 = disable channel; 0 = automatic selection;
                                                                     1, 2, ... = manual satellite selection. */
    U_GNSS_CFG_VAL_KEY_ITEM_QZSS_L6_MSGA_E1             = 0x50, /**< QZSS L6 messages to be decoded by channel A;
                                                                     see #uGnssCfgValKeyItemValueQzssL6Msg_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_QZSS_L6_MSGB_E1             = 0x60, /**< QZSS L6 messages to be decoded by channel B;
                                                                     see #uGnssCfgValKeyItemValueQzssL6Msg_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_QZSS_L6_RSDECODER_E1        = 0x80  /**< QZSS L6 message Reed-Solomon decoder mode;
                                                                     see #uGnssCfgValKeyItemValueQzssL6Rsdecoder_t. */
} uGnssCfgValKeyItemQzss_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_QZSS_L6_MSGA_E1 and #U_GNSS_CFG_VAL_KEY_ITEM_QZSS_L6_MSGB_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_QZSS_L6_MSG_L6D  = 0, /**< L6D messages. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_QZSS_L6_MSG_L6E  = 1  /**< L6E messages. */
} uGnssCfgValKeyItemValueQzssL6Msg_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_QZSS_L6_RSDECODER_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_QZSS_L6_RSDECODER_DISABLED   = 0, /**< disabled; received messages are output
                                                                         with unknown bit-error status. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_QZSS_L6_RSDECODER_ERRDETECT  = 1, /**< error detection; RS-decoder detects
                                                                         bit-errors in received messages. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_QZSS_L6_RSDECODER_ERRCORRECT = 2  /**< error correction; RS-decoder detects
                                                                         and corrects bit-errors in received
                                                                         messages. */
} uGnssCfgValKeyItemValueQzssL6Rsdecoder_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_RATE.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_RATE_MEAS_U2     = 0x01, /**< nominal time between GNSS measurements in milliseconds;
                                                          for example 100 ms results in a 10 Hz measurement rate,
                                                          1000 ms results in a 1 Hz measurement rate. The minimum value
                                                          is 25. */
    U_GNSS_CFG_VAL_KEY_ITEM_RATE_NAV_U2      = 0x02, /**< ratio of the number of measurements to the number of
                                                          navigation solutions; for example 5 means five measurements
                                                          for every navigation solution. Range 1 to 128. */
    U_GNSS_CFG_VAL_KEY_ITEM_RATE_TIMEREF_E1  = 0x03  /**< time system to which measurements are aligned; see
                                                          #uGnssCfgValKeyItemValueRateTimeref_t. */
} uGnssCfgValKeyItemRate_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_RATE_TIMEREF_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_RATE_TIMEREF_UTC   = 0, /**< align measurements to UTC time. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_RATE_TIMEREF_GPS   = 1, /**< align measurements to GPS time. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_RATE_TIMEREF_GLO   = 2, /**< align measurements to GLONASS time. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_RATE_TIMEREF_BDS   = 3, /**< align measurements to BeiDou time. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_RATE_TIMEREF_GAL   = 4, /**< align measurements to Galileo time. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_RATE_TIMEREF_NAVIC = 5  /**< align measurements to NavIC time. */
} uGnssCfgValKeyItemValueRateTimeref_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_RINV.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_RINV_DUMP_L       = 0x01, /**< when set, data will be dumped to the interface
                                                           on startup, unless #U_GNSS_CFG_VAL_KEY_ITEM_RINV_BINARY_L
                                                           is set. */
    U_GNSS_CFG_VAL_KEY_ITEM_RINV_BINARY_L     = 0x02, /**< when set, the data is treated as binary data. */
    U_GNSS_CFG_VAL_KEY_ITEM_RINV_DATA_SIZE_U1 = 0x03, /**< size of data to store/be stored in the remote inventory
                                                           (maximum 30 bytes). */
    U_GNSS_CFG_VAL_KEY_ITEM_RINV_CHUNK0_X8    = 0x04, /**< data bytes 1 to 8 (LSB) to store/be stored in remote
                                                           inventory, left-most is LSB, e.g. "ABCD" will
                                                           appear as 0x44434241. */
    U_GNSS_CFG_VAL_KEY_ITEM_RINV_CHUNK1_X8    = 0x05, /**< data bytes 9 to 16 to store/be stored in remote
                                                           inventory, left-most is LSB, e.g. "ABCD" will
                                                           appear as 0x44434241. */
    U_GNSS_CFG_VAL_KEY_ITEM_RINV_CHUNK2_X8    = 0x06, /**< data bytes 17 to 24 to store/be stored in remote
                                                           inventory, left-most is LSB, e.g. "ABCD" will
                                                           appear as 0x44434241. */
    U_GNSS_CFG_VAL_KEY_ITEM_RINV_CHUNK3_X8    = 0x07  /**< data bytes 25 to 30 (MSB) to store/be stored in remote
                                                           inventory, max 6 bytes, left-most is LSB, e.g. "ABCD"
                                                           will appear as 0x44434241. */
} uGnssCfgValKeyItemRinv_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_RTCM.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_RTCM_DF003_OUT_U2       = 0x01, /**< value to set in RTCM data field DF003 (reference
                                                                 station ID) in RTCM output messages containing
                                                                 DF003. The value can be 0..4095. */
    U_GNSS_CFG_VAL_KEY_ITEM_RTCM_DF003_IN_U2        = 0x08, /**< value to use for filtering out RTCM input messages
                                                                 based on their DF003 data field (reference station
                                                                 ID) value. To be used in conjunction with
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_RTCM_DF003_IN_FILTER_E1.
                                                                 The value can be 0..4095. */
    U_GNSS_CFG_VAL_KEY_ITEM_RTCM_DF003_IN_FILTER_E1 = 0x09 /**< configures if and how the filtering out of RTCM
                                                                input messages based on their DF003 data field
                                                                (reference station ID) operates, see
                                                                #uGnssCfgValKeyItemValueRtcmDf003InFilter_t. */
} uGnssCfgValKeyItemRtcm_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_RTCM_DF003_IN_FILTER_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_RTCM_DF003_IN_FILTER_DISABLED = 0, /**< disabled RTCM input filter; all input
                                                                          messages allowed. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_RTCM_DF003_IN_FILTER_RELAXED  = 1, /**< relaxed RTCM input filter; input messages
                                                                          allowed must contain a DF003 data field
                                                                          matching the
                                                                          #U_GNSS_CFG_VAL_KEY_ITEM_RTCM_DF003_IN_U2
                                                                          value or not contain by specification the
                                                                          DF003 data field. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_RTCM_DF003_IN_FILTER_STRICT   = 2  /**< strict RTCM input filter; input messages
                                                                          allowed must contain a DF003 data field
                                                                          matching the
                                                                          #U_GNSS_CFG_VAL_KEY_ITEM_RTCM_DF003_IN_U2
                                                                          value. */
} uGnssCfgValKeyItemValueRtcmDf003InFilter_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_SBAS.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_SBAS_USE_TESTMODE_L   = 0x02, /**< set to use SBAS data when it is in test mode
                                                               (SBAS messsage 0). */
    U_GNSS_CFG_VAL_KEY_ITEM_SBAS_USE_RANGING_L    = 0x03, /**< set to use SBAS as a ranging source (for navigation). */
    U_GNSS_CFG_VAL_KEY_ITEM_SBAS_USE_DIFFCORR_L   = 0x04, /**< set to use SBAS differential corrections. */
    U_GNSS_CFG_VAL_KEY_ITEM_SBAS_USE_INTEGRITY_L  = 0x05, /**< if this is set the receiver will only use GPS
                                                               satellites for which integrity information is
                                                               available. */
    U_GNSS_CFG_VAL_KEY_ITEM_SBAS_PRNSCANMASK_X8   = 0x06  /**< this configuration item determines which SBAS
                                                               PRNs should be searched. Setting it to 0 indicates
                                                               auto-scanning all SBAS PRNs. For non-zero values the
                                                               bits correspond to the allocated SBAS PRNs ranging
                                                               from PRN120 (bit 0) to PRN158 (bit 38), where a bit
                                                               set enables searching for the corresponding PRN.
                                                               See #uGnssCfgValKeyItemValueSbasPrnscanmask_t. */
} uGnssCfgValKeyItemSbas_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_SBAS_PRNSCANMASK_X8.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN120 = 0x0000000000000001, /**< enable search for SBAS PRN120. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN121 = 0x0000000000000002, /**< enable search for SBAS PRN121. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN122 = 0x0000000000000004, /**< enable search for SBAS PRN122. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN123 = 0x0000000000000008, /**< enable search for SBAS PRN123. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN124 = 0x0000000000000010, /**< enable search for SBAS PRN124. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN125 = 0x0000000000000020, /**< enable search for SBAS PRN125. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN126 = 0x0000000000000040, /**< enable search for SBAS PRN126. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN127 = 0x0000000000000080, /**< enable search for SBAS PRN127. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN128 = 0x0000000000000100, /**< enable search for SBAS PRN128. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN129 = 0x0000000000000200, /**< enable search for SBAS PRN129. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN130 = 0x0000000000000400, /**< enable search for SBAS PRN130. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN131 = 0x0000000000000800, /**< enable search for SBAS PRN131. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN132 = 0x0000000000001000, /**< enable search for SBAS PRN132. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN133 = 0x0000000000002000, /**< enable search for SBAS PRN133. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN134 = 0x0000000000004000, /**< enable search for SBAS PRN134. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN135 = 0x0000000000008000, /**< enable search for SBAS PRN135. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN136 = 0x0000000000010000, /**< enable search for SBAS PRN136. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN137 = 0x0000000000020000, /**< enable search for SBAS PRN137. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN138 = 0x0000000000040000, /**< enable search for SBAS PRN138. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN139 = 0x0000000000080000, /**< enable search for SBAS PRN139. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN140 = 0x0000000000100000, /**< enable search for SBAS PRN140. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN141 = 0x0000000000200000, /**< enable search for SBAS PRN141. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN142 = 0x0000000000400000, /**< enable search for SBAS PRN142. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN143 = 0x0000000000800000, /**< enable search for SBAS PRN143. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN144 = 0x0000000001000000, /**< enable search for SBAS PRN144. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN145 = 0x0000000002000000, /**< enable search for SBAS PRN145. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN146 = 0x0000000004000000, /**< enable search for SBAS PRN146. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN147 = 0x0000000008000000, /**< enable search for SBAS PRN147. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN148 = 0x0000000010000000, /**< enable search for SBAS PRN148. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN149 = 0x0000000020000000, /**< enable search for SBAS PRN149. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN150 = 0x0000000040000000, /**< enable search for SBAS PRN150. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN151 = 0x0000000080000000, /**< enable search for SBAS PRN151. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN152 = 0x0000000100000000, /**< enable search for SBAS PRN152. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN153 = 0x0000000200000000, /**< enable search for SBAS PRN153. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN154 = 0x0000000400000000, /**< enable search for SBAS PRN154. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN155 = 0x0000000800000000, /**< enable search for SBAS PRN155. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN156 = 0x0000001000000000, /**< enable search for SBAS PRN156. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN157 = 0x0000002000000000, /**< enable search for SBAS PRN157. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SBAS_PRNSCANMASK_PRN158 = 0x0000004000000000  /**< enable search for SBAS PRN158. */
} uGnssCfgValKeyItemValueSbasPrnscanmask_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_SEC.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_SEC_CFG_LOCK_L             = 0x09, /**< set this to lock the receiver configuration. */
    U_GNSS_CFG_VAL_KEY_ITEM_SEC_CFG_LOCK_UNLOCKGRP1_U2 = 0x0a, /**< configuration lock-down exempted group 1; this
                                                                    item can be set before enabling
                                                                    #U_GNSS_CFG_VAL_KEY_ITEM_SEC_CFG_LOCK_L; it allows
                                                                    writes to the specified group possible after
                                                                    the configuration lock-down has been enabled. */
    U_GNSS_CFG_VAL_KEY_ITEM_SEC_CFG_LOCK_UNLOCKGRP2_U2 = 0x0b  /**< configuration lock-down exempted group 2; this
                                                                    item can be set before enabling
                                                                    #U_GNSS_CFG_VAL_KEY_ITEM_SEC_CFG_LOCK_L; it makes
                                                                    writes to the specified group possible after
                                                                    the configuration lock-down has been enabled. */
} uGnssCfgValKeyItemSec_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_SFCORE.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_SFCORE_USE_SF_L             = 0x01 /**< enable or disable ADR/UDR sensor fusion. */
} uGnssCfgValKeyItemSfcore_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_SFIMU.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_SFIMU_GYRO_TC_UPDATE_PERIOD_U2 = 0x07, /**< time period between each update in seconds for
                                                                        the saved temperature-dependent gyroscope
                                                                        bias table. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFIMU_GYRO_RMSTHDL_U1          = 0x08, /**< gyroscope sensor RMS threshold threshold below
                                                                        which automatically estimated gyroscope noise-level
                                                                        (accuracy) is updated; units are 2^-8 deg/s. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFIMU_GYRO_FREQUENCY_U1        = 0x09, /**< nominal gyroscope sensor data sampling frequency
                                                                        in Hz. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFIMU_GYRO_LATENCY_U2          = 0x0a, /**< gyroscope sensor data latency due to e.g. CAN bus
                                                                        in milliseconds. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFIMU_GYRO_ACCURACY_U2         = 0x0b, /**< accuracy of gyroscope sensor data in units of
                                                                        1e-3 deg/s; if not set the accuracy is estimated
                                                                        automatically. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFIMU_ACCEL_RMSTHDL_U1         = 0x15, /**< accelerometer RMS threshold below which automatically
                                                                        estimated accelerometer noise-level (accuracy) is
                                                                        updated; units are 2^-6 m/s^2. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFIMU_ACCEL_FREQUENCY_U1       = 0x16, /**< nominal accelerometer sensor data sampling frequency
                                                                        in Hz. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFIMU_ACCEL_LATENCY_U2         = 0x17, /**< accelerometer sensor data latency due to e.g. CAN bus
                                                                        in ms. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFIMU_ACCEL_ACCURACY_U2        = 0x18, /**< accuracy of accelerometer sensor data in units of
                                                                        1e-4 m/s^2; if not set, the accuracy is estimated
                                                                        automatically. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFIMU_IMU_EN_L                 = 0x1d, /**< flag indicating that the IMU is connected to the
                                                                        sensor I2C. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFIMU_IMU_I2C_SCL_PIO_U1       = 0x1e, /**< IMU I2C SCL PIO number that should be used by the
                                                                        FW for communication of the sensor. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFIMU_IMU_I2C_SDA_PIO_U1       = 0x1f, /**< IMU I2C SDA PIO number that should be used by the
                                                                        FW for communication of the sensor. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFIMU_AUTO_MNTALG_ENA_L        = 0x27, /**< enable automatic IMU-mount alignment. This flag can
                                                                        only be used with modules containing an internal IMU. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFIMU_IMU_MNTALG_YAW_U4        = 0x2d, /**< user-defined IMU-mount yaw angle [0, 36000] in units
                                                                        of 1e-2 deg.  For example, for a 60.00 degree yaw angle
                                                                        the configured value would be 6000. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFIMU_IMU_MNTALG_PITCH_I2      = 0x2e, /**< user-defined IMU-mount pitch angle [-9000, 9000] in units
                                                                        of 1e-2 deg.  For example, for a 60.00 degree yaw angle
                                                                        the configured value would be 6000. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFIMU_IMU_MNTALG_ROLL_I2       = 0x2f  /**< user-defined IMU-mount roll angle [-18000, 18000] in units
                                                                        of 1e-2 deg.  For example, for a 60.00 degree yaw angle
                                                                        the configured value would be 6000. */
} uGnssCfgValKeyItemSfimu_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_SFODO.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_SFODO_COMBINE_TICKS_L     = 0x01, /**< use combined rear wheel ticks instead of the single tick. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFODO_USE_SPEED_L         = 0x03, /**< use speed measurements (data type 11 in ESF-MEAS) instead
                                                               of single ticks (data type 10). */
    U_GNSS_CFG_VAL_KEY_ITEM_SFODO_DIS_AUTOCOUNTMAX_L  = 0x04, /**< disable automatic estimation of maximum absolute wheel
                                                               tick counter, see #U_GNSS_CFG_VAL_KEY_ITEM_SFODO_COUNT_MAX_U4
                                                               for more details. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFODO_DIS_AUTODIRPINPOL_L = 0x05, /**< disable automatic wheel tick direction pin polarity
                                                               detection, see #U_GNSS_CFG_VAL_KEY_ITEM_SFODO_DIR_PINPOL_L
                                                               for more details. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFODO_DIS_AUTOSPEED_L     = 0x06, /**< disable automatic receiver reconfiguration for processing
                                                               speed data instead of wheel tick data if no wheel tick data
                                                               are available but speed data was detected; see
                                                               #U_GNSS_CFG_VAL_KEY_ITEM_SFODO_USE_SPEED_L for more details. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFODO_FACTOR_U4           = 0x07, /**< wheel tick scale factor to obtain distance [m] from
                                                               wheel ticks, in units of 1e-6. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFODO_QUANT_ERROR_U4      = 0x08, /**< wheel tick quantization im units of 1e-6 m (or m/s).
                                                               If #U_GNSS_CFG_VAL_KEY_ITEM_SFODO_USE_SPEED_L is set then
                                                               this is interpreted as the speed measurement error RMS. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFODO_COUNT_MAX_U4        = 0x09, /**< wheel tick counter maximum value (rollover - 1).
                                                               If zero, relative wheel tick counts are assumed
                                                               (and therefore no rollover). If not zero, absolute wheel
                                                               tick counts are assumed and the value corresponds to the
                                                               highest tick count value before rollover happens.
                                                               If #U_GNSS_CFG_VAL_KEY_ITEM_SFODO_USE_SPEED_L is set
                                                               then this value is ignored. If value is set to 1,
                                                               absolute wheel tick counts are assumed and the value will
                                                               be automatically calculated if possible. It is only
                                                               possible for automatic calibration to calculate wheel tick
                                                               counter maximum value if it can be represented as a number
                                                               of set bits (i.e. 2^N). If it cannot be represented in this
                                                               way it must be set to the correct absolute tick value manually. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFODO_LATENCY_U2          = 0x0a, /**< wheel tick data latency due to e.g. CAN bus in ms. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFODO_FREQUENCY_U1        = 0x0b, /**< nominal wheel tick data frequency in Hz (0 = not set). */
    U_GNSS_CFG_VAL_KEY_ITEM_SFODO_CNT_BOTH_EDGES_L    = 0x0d, /**< count both rising and falling edges on wheel tick signal
                                                               (only relevant if wheel tick is measured by the u-blox
                                                               receiver).  Only turn on this feature if the wheel tick
                                                               signal has 50 % duty cycle. Turning on this feature with
                                                               fixed-width pulses can lead to severe degradation of
                                                               performance. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFODO_SPEED_BAND_U2       = 0x0e, /**< speed sensor dead band in cm/s (0 = not set). */
    U_GNSS_CFG_VAL_KEY_ITEM_SFODO_USE_WT_PIN_L        = 0x0f, /**< wheel tick signal enabled. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFODO_DIR_PINPOL_L        = 0x10, /**< wheel tick direction pin polarity: 0 = pin high means
                                                               forwards direction, 1 = pin high means backwards direction. */
    U_GNSS_CFG_VAL_KEY_ITEM_SFODO_DIS_AUTOSW_L        = 0x11 /**<  disable automatic use of wheel tick or speed data received
                                                               over the software interface if available. In this case,
                                                               data coming from the hardware interface (wheel tick pins)
                                                               will automatically be ignored if the wheel tick/speed data
                                                               are available from the software interface. See
                                                               #U_GNSS_CFG_VAL_KEY_ITEM_SFODO_USE_WT_PIN_L for more details. */
} uGnssCfgValKeyItemSfodo_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_SIGNAL.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_SIGNAL_GPS_ENA_L       = 0x1f, /**< GPS enable. */
    U_GNSS_CFG_VAL_KEY_ITEM_SIGNAL_GPS_L1CA_ENA_L  = 0x01, /**< GPS L1C/A. */
    U_GNSS_CFG_VAL_KEY_ITEM_SIGNAL_GPS_L2C_ENA_L   = 0x03, /**< GPS L2C. */
    U_GNSS_CFG_VAL_KEY_ITEM_SIGNAL_SBAS_ENA_L      = 0x20, /**< SBAS enable. */
    U_GNSS_CFG_VAL_KEY_ITEM_SIGNAL_SBAS_L1CA_ENA_L = 0x05, /**< SBAS L1C/A. */
    U_GNSS_CFG_VAL_KEY_ITEM_SIGNAL_GAL_ENA_L       = 0x21, /**< Galileo enable. */
    U_GNSS_CFG_VAL_KEY_ITEM_SIGNAL_GAL_E1_ENA_L    = 0x07, /**< Galileo E1. */
    U_GNSS_CFG_VAL_KEY_ITEM_SIGNAL_GAL_E5B_ENA_L   = 0x0a, /**< Galileo E5b. */
    U_GNSS_CFG_VAL_KEY_ITEM_SIGNAL_BDS_ENA_L       = 0x22, /**< BeiDou enable. */
    U_GNSS_CFG_VAL_KEY_ITEM_SIGNAL_BDS_B1_ENA_L    = 0x0d, /**< BeiDou B1. */
    U_GNSS_CFG_VAL_KEY_ITEM_SIGNAL_BDS_B2_ENA_L    = 0x0e, /**< BeiDou B2. */
    U_GNSS_CFG_VAL_KEY_ITEM_SIGNAL_QZSS_ENA_L      = 0x24, /**< QZSS enable. */
    U_GNSS_CFG_VAL_KEY_ITEM_SIGNAL_QZSS_L1CA_ENA_L = 0x12, /**< QZSS L1C/A. */
    U_GNSS_CFG_VAL_KEY_ITEM_SIGNAL_QZSS_L1S_ENA_L  = 0x14, /**< QZSS L1S. */
    U_GNSS_CFG_VAL_KEY_ITEM_SIGNAL_QZSS_L2C_ENA_L  = 0x15, /**< QZSS L2C. */
    U_GNSS_CFG_VAL_KEY_ITEM_SIGNAL_GLO_ENA_L       = 0x25, /**< GLONASS enable. */
    U_GNSS_CFG_VAL_KEY_ITEM_SIGNAL_GLO_L1_ENA_L    = 0x18, /**< GLONASS L1. */
    U_GNSS_CFG_VAL_KEY_ITEM_SIGNAL_GLO_L2_ENA_L    = 0x1a  /**< GLONASS L2. */
} uGnssCfgValKeyItemSignal_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_SPARTN.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_SPARTN_USE_SOURCE_E1   = 0x01  /**< Selector for source SPARTN stream, see
                                                                #uGnssCfgValKeyItemSpartnUseSource_t. */
} uGnssCfgValKeyItemSpartn_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_SPARTN_USE_SOURCE_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SPARTN_USE_SOURCE_IP    = 0x00, /**< selects IP (raw) source. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_SPARTN_USE_SOURCE_LBAND = 0x01  /**< selects L-Band source. */
} uGnssCfgValKeyItemSpartnUseSource_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_SPI.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_SPI_MAXFF_U1           = 0x01, /**< number of bytes containing 0xFF to receive
                                                                before switching off reception; range: 0
                                                                (mechanism off) to 63. */
    U_GNSS_CFG_VAL_KEY_ITEM_SPI_CPOLARITY_L        = 0x02, /**< clock polarity select: 0 for active high,
                                                                SCLK idles low, 1 for active low, SCLK idles
                                                                high. */
    U_GNSS_CFG_VAL_KEY_ITEM_SPI_CPHASE_L           = 0x03, /**< clock phase select: 0 for data capture on first
                                                                edge of SCLK, 1 for data capture on second edge
                                                                of SCLK. */
    U_GNSS_CFG_VAL_KEY_ITEM_SPI_EXTENDEDTIMEOUT_L  = 0x05, /**< set this to not disable the interface after 1.5
                                                                seconds. */
    U_GNSS_CFG_VAL_KEY_ITEM_SPI_ENABLED_L          = 0x06  /**< set this to enable SPI. */
} uGnssCfgValKeyItemSpi_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_SPIINPROT.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_SPIINPROT_UBX_L    = 0x01, /**< set this flag to allow UBX protocol as input on SPI. */
    U_GNSS_CFG_VAL_KEY_ITEM_SPIINPROT_NMEA_L   = 0x02, /**< set this flag to allow NMEA protocol as input on SPI. */
    U_GNSS_CFG_VAL_KEY_ITEM_SPIINPROT_RTCM3X_L = 0x04, /**< set this flag to allow RTCM3X protocol as input on SPI. */
    U_GNSS_CFG_VAL_KEY_ITEM_SPIINPROT_SPARTN_L = 0x05  /**< set this flag to allow SPARTN protocol as input on SPI. */
} uGnssCfgValKeyItemSpiinprot_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_SPIOUTPROT.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_SPIOUTPROT_UBX_L    = 0x01, /**< set this flag to allow UBX protocol as output of SPI. */
    U_GNSS_CFG_VAL_KEY_ITEM_SPIOUTPROT_NMEA_L   = 0x02, /**< set this flag to allow NMEA protocol as output of SPI. */
    U_GNSS_CFG_VAL_KEY_ITEM_SPIOUTPROT_RTCM3X_L = 0x04  /**< set this flag to allow RTCM3X protocol as output of SPI. */
} uGnssCfgValKeyItemSpioutprot_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_TMODE.
 */
typedef enum {

    U_GNSS_CFG_VAL_KEY_ITEM_TMODE_MODE_E1           = 0x01, /**< receiver mode See #uGnssCfgValKeyItemValueTModeMode_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_TMODE_POS_TYPE_E1       = 0x02, /**< determines the ARP position units, see
                                                                 #uGnssCfgValKeyItemValueTModePosType_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_TMODE_ECEF_X_I4         = 0x03, /**< ECEF X coordinate of the ARP position in cm. This
                                                                 will only be used if
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_MODE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_MODE_FIXED
                                                                 and #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_POS_TYPE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_POS_TYPE_ECEF. */
    U_GNSS_CFG_VAL_KEY_ITEM_TMODE_ECEF_Y_I4         = 0x04, /**< ECEF Y coordinate of the ARP position in cm.  This
                                                                 will only be used if
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_MODE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_MODE_FIXED
                                                                 and #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_POS_TYPE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_POS_TYPE_ECEF. */
    U_GNSS_CFG_VAL_KEY_ITEM_TMODE_ECEF_Z_I4         = 0x05, /**< ECEF Z coordinate of the ARP position in cm.  This
                                                                 will only be used if
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_MODE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_MODE_FIXED
                                                                 and #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_POS_TYPE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_POS_TYPE_ECEF. */
    U_GNSS_CFG_VAL_KEY_ITEM_TMODE_ECEF_X_HP_I1      = 0x06, /**< high-precision ECEF X coordinate of the ARP position
                                                                 in units of 0.1 mm. Accepted range is -99 to +99.
                                                                 This will only be used if
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_MODE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_MODE_FIXED
                                                                 and #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_POS_TYPE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_POS_TYPE_ECEF. */
    U_GNSS_CFG_VAL_KEY_ITEM_TMODE_ECEF_Y_HP_I1      = 0x07, /**< high-precision ECEF Y coordinate of the ARP position
                                                                 in units of 0.1 mm. Accepted range is -99 to +99.
                                                                 This will only be used if
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_MODE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_MODE_FIXED
                                                                 and #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_POS_TYPE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_POS_TYPE_ECEF. */
    U_GNSS_CFG_VAL_KEY_ITEM_TMODE_ECEF_Z_HP_I1      = 0x08, /**< high-precision ECEF Z coordinate of the ARP
                                                                 position in units of 0.1 mm. Accepted range
                                                                 is -99 to +99. This will only be used if
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_MODE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_MODE_FIXED
                                                                 and #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_POS_TYPE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_POS_TYPE_ECEF. */
    U_GNSS_CFG_VAL_KEY_ITEM_TMODE_LAT_I4            = 0x09, /**< latitude of the ARP position in units of 1e-7 deg.
                                                                 This will only be used if
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_MODE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_MODE_FIXED
                                                                 and #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_POS_TYPE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_POS_TYPE_LLH. */
    U_GNSS_CFG_VAL_KEY_ITEM_TMODE_LON_I4            = 0x0a, /**< longitude of the ARP position in units of 1e-7 deg.
                                                                 This will only be used if
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_MODE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_MODE_FIXED
                                                                 and #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_POS_TYPE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_POS_TYPE_LLH. */
    U_GNSS_CFG_VAL_KEY_ITEM_TMODE_HEIGHT_I4         = 0x0b, /**< height of the ARP position in cm. This will only be
                                                                 used if
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_MODE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_MODE_FIXED
                                                                 and #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_POS_TYPE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_POS_TYPE_LLH. */
    U_GNSS_CFG_VAL_KEY_ITEM_TMODE_LAT_HP_I1         = 0x0c, /**< high-precision latitude of the ARP position in units
                                                                 of 1e-9 deg . Accepted range is -99 to +99. This will
                                                                 only be used if
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_MODE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_MODE_FIXED
                                                                 and #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_POS_TYPE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_POS_TYPE_LLH. */
    U_GNSS_CFG_VAL_KEY_ITEM_TMODE_LON_HP_I1         = 0x0d, /**< high-precision longitude of the ARP position in
                                                                 units of 1e-9 deg. Accepted range is -99 to +99.
                                                                 This will only be used if
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_MODE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_MODE_FIXED
                                                                 and #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_POS_TYPE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_POS_TYPE_LLH. */
    U_GNSS_CFG_VAL_KEY_ITEM_TMODE_HEIGHT_HP_I1      = 0x0e, /**< high-precision height of the ARP position in units
                                                                 of 0.1 mm. Accepted range is -99 to +99. This will
                                                                 only be used if
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_MODE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_MODE_FIXED
                                                                 and #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_POS_TYPE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_POS_TYPE_LLH. */
    U_GNSS_CFG_VAL_KEY_ITEM_TMODE_FIXED_POS_ACC_U4  = 0x0f, /**< fixed position 3D accuracy in units of 0.1 mm. */
    U_GNSS_CFG_VAL_KEY_ITEM_TMODE_SVIN_MIN_DUR_U4   = 0x10, /**< survey-in minimum duration in seconds. This will only
                                                                 be used if #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_MODE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_MODE_SURVEY_IN. */
    U_GNSS_CFG_VAL_KEY_ITEM_TMODE_SVIN_ACC_LIMIT_U4 = 0x11  /**< survey-in position accuracy limit in units of 0.1 mm.
                                                                 This will only be used if
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_MODE_E1 is
                                                                 #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_MODE_SURVEY_IN. */
} uGnssCfgValKeyItemTmode_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_MODE_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_MODE_DISABLED  = 0, /**< disabled. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_MODE_SURVEY_IN = 1, /**< survey in. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_MODE_FIXED     = 2  /**< fixed mode (true ARP position information required). */
} uGnssCfgValKeyItemValueTModeMode_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_TMODE_POS_TYPE_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_POS_TYPE_ECEF = 0, /**< position is ECEF. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TMODE_POS_TYPE_LLH  = 1  /**< position is latitude/longitude/height. */
} uGnssCfgValKeyItemValueTModePosType_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_TP.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_TP_PULSE_DEF_E1        = 0x23, /**< determines whether the time pulse is interpreted as
                                                                frequency or period, see
                                                                #uGnssCfgValKeyItemValueTpPulseDef_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_PULSE_LENGTH_DEF_E1 = 0x30, /**< determines whether the time pulse length is
                                                                interpreted as length in microseconds or pulse
                                                                ratio as a percentage, see
                                                                #uGnssCfgValKeyItemValueTpPulseLengthDef_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_ANT_CABLEDELAY_I2   = 0x01, /**< the antenna cable delay in picoseconds. */

    /* The following section is all TP1. */

    U_GNSS_CFG_VAL_KEY_ITEM_TP_PERIOD_TP1_U4       = 0x02, /**< time pulse period (TP1) in microseconds. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_PERIOD_LOCK_TP1_U4  = 0x03, /**< time pulse period (TP1) when locked to GNSS time
                                                                in microseconds; only used if
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_TP_USE_LOCKED_TP1_L is set. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_FREQ_TP1_U4         = 0x24, /**< time pulse frequency in Hertz; only used if
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_TP_PULSE_DEF_E1 is
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_PULSE_DEF_FREQ. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_FREQ_LOCK_TP1_U4    = 0x25, /**< time pulse frequency when locked to GNSS time (TP1)
                                                                in Hertz; only used if
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_TP_USE_LOCKED_TP1_L is set. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_LEN_TP1_U4          = 0x04, /**< time pulse length (TP1) in microseconds. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_LEN_LOCK_TP1_U4     = 0x05, /**< time pulse length when locked to GNSS time (TP1)
                                                                in microseconds; only used if
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_TP_USE_LOCKED_TP1_L is set. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_DUTY_TP1_R8         = 0x2a, /**< time pulse duty cycle (TP1) as a percentage; only
                                                                used if #U_GNSS_CFG_VAL_KEY_ITEM_TP_PULSE_LENGTH_DEF_E1
                                                                is
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_PULSE_LENGTH_DEF_RATIO. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_DUTY_LOCK_TP1_R8    = 0x2b, /**< time pulse duty cycle when locked to GNSS time (TP1)
                                                                as a percentage; only
                                                                used if #U_GNSS_CFG_VAL_KEY_ITEM_TP_PULSE_LENGTH_DEF_E1
                                                                is
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_PULSE_LENGTH_DEF_RATIO
                                                                and #U_GNSS_CFG_VAL_KEY_ITEM_TP_USE_LOCKED_TP1_L
                                                                is set. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_USER_DELAY_TP1_I4   = 0x06, /**< user-configurable time pulse delay (TP1) in picoseconds. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_TP1_ENA_L           = 0x07, /**< enable the first time pulse; if the pin associated with
                                                                the time pulse is assigned for another function, the
                                                                other function takes precedence.  Must be set for
                                                                frequency-time products. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_SYNC_GNSS_TP1_L     = 0x08, /**< sync the time pulse to GNSS time or local clock (TP1).
                                                                If set, sync to GNSS if GNSS time is valid otherwise,
                                                                if not set or not available, use local clock. Ignored
                                                                by time-frequency product variants, which will attempt
                                                                to use the best available time/frequency reference (not
                                                                necessarily GNSS).  This flag can be unset only in
                                                                Timing product variants. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_USE_LOCKED_TP1_L    = 0x09, /**< if set, use #U_GNSS_CFG_VAL_KEY_ITEM_TP_PERIOD_LOCK_TP1_U4
                                                                and #U_GNSS_CFG_VAL_KEY_ITEM_TP_LEN_LOCK_TP1_U4 as soon
                                                                as GNSS time is valid. Otherwise, if not valid or not set,
                                                                use #U_GNSS_CFG_VAL_KEY_ITEM_TP_PERIOD_TP1_U4 and
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_TP_LEN_TP1_U4. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_ALIGN_TO_TOW_TP1_L  = 0x0a, /**< align time pulse to top of second (TP1). To use this
                                                                feature, #U_GNSS_CFG_VAL_KEY_ITEM_TP_USE_LOCKED_TP1_L
                                                                must be set.  The time pulse period must be an integer
                                                                fraction of 1 second.  Ignored in time-frequency product
                                                                variants, where it is assumed always enabled. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_POL_TP1_L           = 0x0b, /**< if 0 the time pulse falling edge (TP1) will be aligned to
                                                                the top of the second, else the time pulse rising edge
                                                                will be aligned to the top of the second. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_TIMEGRID_TP1_E1     = 0x0c, /**< time grid to use (TP1), see
                                                                #uGnssCfgValKeyItemValueTpTimegrid_t; only relevant if
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_TP_USE_LOCKED_TP1_L and
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_TP_ALIGN_TO_TOW_TP1_L are set.
                                                                Note that the configured GNSS time is estimated by the
                                                                receiver if locked to any GNSS system; if the receiver has
                                                                a valid GNSS fix it will attempt to steer the TP to the
                                                                specified time grid even if the specified time is not based
                                                                on information from the constellation's satellites.
                                                                To ensure timing based purely on a given GNSS, restrict the
                                                                supported constellations using
                                                                #U_GNSS_CFG_VAL_KEY_GROUP_ID_SIGNAL. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_DRSTR_TP1_E1        = 0x35, /**< set drive strength of TP1, see
                                                                #uGnssCfgValKeyItemValueTpDrstr_t. */

    /* The following section is a repeat of the above but for TP2. */

    U_GNSS_CFG_VAL_KEY_ITEM_TP_PERIOD_TP2_U4       = 0x0d, /**< time pulse period (TP2) in microseconds. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_PERIOD_LOCK_TP2_U4  = 0x0e, /**< time pulse period (TP2) when locked to GNSS time
                                                                in microseconds; only used if
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_TP_USE_LOCKED_TP2_L is set. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_FREQ_TP2_U4         = 0x26, /**< time pulse frequency in Hertz; only used if
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_TP_PULSE_DEF_E1 is
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_PULSE_DEF_FREQ. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_FREQ_LOCK_TP2_U4    = 0x27, /**< time pulse frequency when locked to GNSS time (TP2)
                                                                in Hertz; only used if
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_TP_USE_LOCKED_TP2_L is set. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_LEN_TP2_U4          = 0x0f, /**< time pulse length (TP2) in microseconds. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_LEN_LOCK_TP2_U4     = 0x10, /**< time pulse length when locked to GNSS time (TP2)
                                                                in microseconds; only used if
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_TP_USE_LOCKED_TP2_L is set. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_DUTY_TP2_R8         = 0x2c, /**< time pulse duty cycle (TP2) as a percentage; only
                                                                used if #U_GNSS_CFG_VAL_KEY_ITEM_TP_PULSE_LENGTH_DEF_E1
                                                                is
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_PULSE_LENGTH_DEF_RATIO. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_DUTY_LOCK_TP2_R8    = 0x2d, /**< time pulse duty cycle when locked to GNSS time (TP2)
                                                                as a percentage; only
                                                                used if #U_GNSS_CFG_VAL_KEY_ITEM_TP_PULSE_LENGTH_DEF_E1
                                                                is
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_PULSE_LENGTH_DEF_RATIO
                                                                and #U_GNSS_CFG_VAL_KEY_ITEM_TP_USE_LOCKED_TP2_L
                                                                is set. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_USER_DELAY_TP2_I4   = 0x11, /**< user-configurable time pulse delay (TP2) in picoseconds. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_TP2_ENA_L           = 0x12, /**< enable the secondtime pulse; if the pin associated with
                                                                the time pulse is assigned for another function, the
                                                                other function takes precedence.  Must be set for
                                                                frequency-time products. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_SYNC_GNSS_TP2_L     = 0x13, /**< sync the time pulse to GNSS time or local clock (TP2).
                                                                If set, sync to GNSS if GNSS time is valid otherwise,
                                                                if not set or not available, use local clock. Ignored
                                                                by time-frequency product variants, which will attempt
                                                                to use the best available time/frequency reference (not
                                                                necessarily GNSS).  This flag can be unset only in
                                                                Timing product variants. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_USE_LOCKED_TP2_L    = 0x14, /**< if set, use #U_GNSS_CFG_VAL_KEY_ITEM_TP_PERIOD_LOCK_TP2_U4
                                                                and #U_GNSS_CFG_VAL_KEY_ITEM_TP_LEN_LOCK_TP2_U4 as soon
                                                                as GNSS time is valid. Otherwise, if not valid or not set,
                                                                use #U_GNSS_CFG_VAL_KEY_ITEM_TP_PERIOD_TP2_U4 and
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_TP_LEN_TP2_U4. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_ALIGN_TO_TOW_TP2_L  = 0x15, /**< align time pulse to top of second (TP2). To use this
                                                                feature, #U_GNSS_CFG_VAL_KEY_ITEM_TP_USE_LOCKED_TP2_L
                                                                must be set.  The time pulse period must be an integer
                                                                fraction of 1 second.  Ignored in time-frequency product
                                                                variants, where it is assumed always enabled. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_POL_TP2_L           = 0x16, /**< if 0 the time pulse falling edge (TP2) will be aligned to
                                                                the top of the second, else the time pulse rising edge
                                                                will be aligned to the top of the second. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_TIMEGRID_TP2_E1     = 0x17, /**< time grid to use (TP2), see
                                                                #uGnssCfgValKeyItemValueTpTimegrid_t; only relevant if
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_TP_USE_LOCKED_TP2_L and
                                                                #U_GNSS_CFG_VAL_KEY_ITEM_TP_ALIGN_TO_TOW_TP2_L are set.
                                                                Note that the configured GNSS time is estimated by the
                                                                receiver if locked to any GNSS system; if the receiver has
                                                                a valid GNSS fix it will attempt to steer the TP to the
                                                                specified time grid even if the specified time is not based
                                                                on information from the constellation's satellites.
                                                                To ensure timing based purely on a given GNSS, restrict the
                                                                supported constellations using
                                                                #U_GNSS_CFG_VAL_KEY_GROUP_ID_SIGNAL. */
    U_GNSS_CFG_VAL_KEY_ITEM_TP_DRSTR_TP2_E1        = 0x36, /**< set drive strength of TP2, see
                                                                #uGnssCfgValKeyItemValueTpDrstr_t. */
} uGnssCfgValKeyItemTp_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_TP_PULSE_DEF_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_PULSE_DEF_PERIOD = 0, /**< use time pulse period. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_PULSE_DEF_FREQ   = 1  /**< use time pulse frequency. */
} uGnssCfgValKeyItemValueTpPulseDef_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_TP_PULSE_LENGTH_DEF_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_PULSE_LENGTH_DEF_RATIO  = 0, /**< use time pulse ratio. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_PULSE_LENGTH_DEF_LENGTH = 1  /**< use time pulse length. */
} uGnssCfgValKeyItemValueTpPulseLengthDef_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_TP_TIMEGRID_TP1_E1 and #U_GNSS_CFG_VAL_KEY_ITEM_TP_TIMEGRID_TP2_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_TIMEGRID_UTC  = 0, /**< use UTC time reference. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_TIMEGRID_GPS  = 1, /**< use GPS time reference. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_TIMEGRID_GLO  = 2, /**< use GLONASS time reference. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_TIMEGRID_BDS  = 3, /**< use BeiDou time reference. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_TIMEGRID_GAL  = 4  /**< use Galileo time reference. */
} uGnssCfgValKeyItemValueTpTimegrid_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_TP_DRSTR_TP1_E1 and #U_GNSS_CFG_VAL_KEY_ITEM_TP_DRSTR_TP2_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_DRSTR_2MA  = 0, /**<  2 mA drive strength. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_DRSTR_4MA  = 1, /**<  4 mA drive strength. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_DRSTR_8MA  = 2, /**<  8 mA drive strength. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TP_DRSTR_12MA = 3  /**< 12 mA drive strength. */
} uGnssCfgValKeyItemValueTpDrstr_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_TXREADY.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_TXREADY_ENABLED_L    = 0x01, /**< set this to enable the TX ready mechanism. */
    U_GNSS_CFG_VAL_KEY_ITEM_TXREADY_POLARITY_L   = 0x02, /**< set this for the TX ready pin to be active low,
                                                              else it will be active high. */
    U_GNSS_CFG_VAL_KEY_ITEM_TXREADY_PIN_U1       = 0x03, /**< the pin number to use for TX ready. */
    U_GNSS_CFG_VAL_KEY_ITEM_TXREADY_THRESHOLD_U2 = 0x04, /**< the amount of data that should be ready
                                                              on the interface before triggering TX ready. */
    U_GNSS_CFG_VAL_KEY_ITEM_TXREADY_INTERFACE_E1 = 0x05  /**< the interface that the TX ready feature should
                                                              be linked to, see
                                                              #uGnssCfgValKeyItemValueTxreadyInterface_t. */
} uGnssCfgValKeyItemTxready_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_TXREADY_INTERFACE_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TXREADY_INTERFACE_I2C = 0,
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_TXREADY_INTERFACE_SPI = 1
} uGnssCfgValKeyItemValueTxreadyInterface_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_UART1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_UART1_BAUDRATE_U4  = 0x01,  /**< the baud rate that should be configured on
                                                             UART1; note that if you are currently
                                                             communicating on UART1 and you change the
                                                             baud rate of UART1 then the acknowledgement
                                                             for the baud rate change will go missing;
                                                             it is up to you to call uPortUartClose() /
                                                             uPortUartOpen() with the new baud rate
                                                             to re-establish communication with the
                                                             GNSS chip. */
    U_GNSS_CFG_VAL_KEY_ITEM_UART1_STOPBITS_E1  = 0x02,  /**< the number of stop bits on UART1, see
                                                             #uGnssCfgValKeyItemValueUartStopbits_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_UART1_DATABITS_E1  = 0x03,  /**< the number of data bits on UART1, see
                                                             #uGnssCfgValKeyItemValueUartDatabits_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_UART1_PARITY_E1    = 0x04,  /**< parity mode on UART1, see
                                                             #uGnssCfgValKeyItemValueUartParity_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_UART1_ENABLED_L    = 0x05   /**< set this to enable UART1. */
} uGnssCfgValKeyItemUart1_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_UART1_STOPBITS_E1 and #U_GNSS_CFG_VAL_KEY_ITEM_UART2_STOPBITS_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_UART_STOP_BITS_HALF     = 0, /**< 0.5 stop bits. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_UART_STOP_BITS_ONE      = 1, /**< 1 stop bit. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_UART_STOP_BITS_ONEHALF  = 2, /**< 1.5 stop bits. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_UART_STOP_BITS_TWO      = 3  /**< 2 stop bits. */
} uGnssCfgValKeyItemValueUartStopbits_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_UART1_DATABITS_E1 and #U_GNSS_CFG_VAL_KEY_ITEM_UART2_DATABITS_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_UART_DATA_BITS_EIGHT  = 0, /**< 8 data bits. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_UART_DATA_BITS_SEVEN  = 1  /**< 7 data bits. */
} uGnssCfgValKeyItemValueUartDatabits_t;

/** Values for #U_GNSS_CFG_VAL_KEY_ITEM_UART1_PARITY_E1 and #U_GNSS_CFG_VAL_KEY_ITEM_UART2_PARITY_E1.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_UART_PARITY_NONE = 0, /**< no parity bit. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_UART_PARITY_ODD  = 1, /**< add an odd parity bit. */
    U_GNSS_CFG_VAL_KEY_ITEM_VALUE_UART_PARITY_EVEN = 2  /**< add an even parity bit. */
} uGnssCfgValKeyItemValueUartParity_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_UART1INPROT.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_UART1INPROT_UBX_L    = 0x01, /**< set this flag to allow UBX protocol as input on UART1. */
    U_GNSS_CFG_VAL_KEY_ITEM_UART1INPROT_NMEA_L   = 0x02, /**< set this flag to allow NMEA protocol as input on UART1. */
    U_GNSS_CFG_VAL_KEY_ITEM_UART1INPROT_RTCM3X_L = 0x04, /**< set this flag to allow RTCM3X protocol as input on UART1. */
    U_GNSS_CFG_VAL_KEY_ITEM_UART1INPROT_SPARTN_L = 0x05  /**< set this flag to allow SPARTN protocol as input on UART1. */
} uGnssCfgValKeyItemUart1inprot_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_UART1OUTPROT.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_UART1OUTPROT_UBX_L    = 0x01, /**< set this flag to allow UBX protocol as output of UART1. */
    U_GNSS_CFG_VAL_KEY_ITEM_UART1OUTPROT_NMEA_L   = 0x02, /**< set this flag to allow NMEA protocol as output of UART1. */
    U_GNSS_CFG_VAL_KEY_ITEM_UART1OUTPROT_RTCM3X_L = 0x04  /**< set this flag to allow RTCM3X protocol as output of UART1. */
} uGnssCfgValKeyItemUart1outprot_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_UART2.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_UART2_BAUDRATE_U4  = 0x01,  /**< the baud rate that should be configured on
                                                             UART2; note that if you are currently
                                                             communicating on UART2 and you change the
                                                             baud rate of UART2 then the acknowledgement
                                                             for the baud rate change will go missing;
                                                             it is up to you to call uPortUartClose() /
                                                             uPortUartOpen() with the new baud rate
                                                             to re-establish communication with the
                                                             GNSS chip. */
    U_GNSS_CFG_VAL_KEY_ITEM_UART2_STOPBITS_E1  = 0x02,  /**< the number of stop bits on UART2, see
                                                             #uGnssCfgValKeyItemValueUartStopbits_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_UART2_DATABITS_E1  = 0x03,  /**< the number of data bits on UART2, see
                                                             #uGnssCfgValKeyItemValueUartDatabits_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_UART2_PARITY_E1    = 0x04,  /**< parity mode on UART2, see
                                                             #uGnssCfgValKeyItemValueUartParity_t. */
    U_GNSS_CFG_VAL_KEY_ITEM_UART2_ENABLED_L    = 0x05   /**< set this to enable UART2. */
} uGnssCfgValKeyItemUart2_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_UART2INPROT.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_UART2INPROT_UBX_L    = 0x01, /**< set this flag to allow UBX protocol as input on UART2. */
    U_GNSS_CFG_VAL_KEY_ITEM_UART2INPROT_NMEA_L   = 0x02, /**< set this flag to allow NMEA protocol as input on UART2. */
    U_GNSS_CFG_VAL_KEY_ITEM_UART2INPROT_RTCM3X_L = 0x04, /**< set this flag to allow RTCM3X protocol as input on UART2. */
    U_GNSS_CFG_VAL_KEY_ITEM_UART2INPROT_SPARTN_L = 0x05  /**< set this flag to allow SPARTN protocol as input on UART2. */
} uGnssCfgValKeyItemUart2inprot_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_UART2OUTPROT.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_UART2OUTPROT_UBX_L    = 0x01, /**< set this flag to allow UBX protocol as output of UART2. */
    U_GNSS_CFG_VAL_KEY_ITEM_UART2OUTPROT_NMEA_L   = 0x02, /**< set this flag to allow NMEA protocol as output of UART2. */
    U_GNSS_CFG_VAL_KEY_ITEM_UART2OUTPROT_RTCM3X_L = 0x04  /**< set this flag to allow RTCM3X protocol as output of UART2. */
} uGnssCfgValKeyItemUart2outprot_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_USB.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_USB_ENABLED_L         = 0x01, /**< set this to enable the USB interface. */
    U_GNSS_CFG_VAL_KEY_ITEM_USB_SELFPOW_L         = 0x02, /**< self-powered device. */
    U_GNSS_CFG_VAL_KEY_ITEM_USB_VENDOR_ID_U2      = 0x0a, /**< the USB vendor ID to use. */
    U_GNSS_CFG_VAL_KEY_ITEM_USB_PRODUCT_ID_U2     = 0x0b, /**< the USB product ID to use. */
    U_GNSS_CFG_VAL_KEY_ITEM_USB_POWER_U2          = 0x0c, /**< the power consumption in mA. */
    U_GNSS_CFG_VAL_KEY_ITEM_USB_VENDOR_STR0_X8    = 0x0d, /**< vendor string characters 0-7. */
    U_GNSS_CFG_VAL_KEY_ITEM_USB_VENDOR_STR1_X8    = 0x0e, /**< vendor string characters 8-15. */
    U_GNSS_CFG_VAL_KEY_ITEM_USB_VENDOR_STR2_X8    = 0x0f, /**< vendor string characters 16-23. */
    U_GNSS_CFG_VAL_KEY_ITEM_USB_VENDOR_STR3_X8    = 0x10, /**< vendor string characters 24-31. */
    U_GNSS_CFG_VAL_KEY_ITEM_USB_PRODUCT_STR0_X8   = 0x11, /**< product string characters 0-7. */
    U_GNSS_CFG_VAL_KEY_ITEM_USB_PRODUCT_STR1_X8   = 0x12, /**< product string characters 8-15. */
    U_GNSS_CFG_VAL_KEY_ITEM_USB_PRODUCT_STR2_X8   = 0x13, /**< product string characters 16-23. */
    U_GNSS_CFG_VAL_KEY_ITEM_USB_PRODUCT_STR3_X8   = 0x14, /**< product string characters 24-31. */
    U_GNSS_CFG_VAL_KEY_ITEM_USB_SERIAL_NO_STR0_X8 = 0x15, /**< serial number string characters 0-7. */
    U_GNSS_CFG_VAL_KEY_ITEM_USB_SERIAL_NO_STR1_X8 = 0x16, /**< serial number string characters 8-15. */
    U_GNSS_CFG_VAL_KEY_ITEM_USB_SERIAL_NO_STR2_X8 = 0x17, /**< serial number string characters 16-23. */
    U_GNSS_CFG_VAL_KEY_ITEM_USB_SERIAL_NO_STR3_X8 = 0x18  /**< serial number string characters 24-31. */
} uGnssCfgValKeyItemUsb_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_USBINPROT.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_USBINPROT_UBX_L    = 0x01, /**< set this flag to allow UBX protocol as input on USB. */
    U_GNSS_CFG_VAL_KEY_ITEM_USBINPROT_NMEA_L   = 0x02, /**< set this flag to allow NMEA protocol as input on USB. */
    U_GNSS_CFG_VAL_KEY_ITEM_USBINPROT_RTCM3X_L = 0x04, /**< set this flag to allow RTCM3X protocol as input on USB. */
    U_GNSS_CFG_VAL_KEY_ITEM_USBINPROT_SPARTN_L = 0x05  /**< set this flag to allow SPARTN protocol as input on USB. */
} uGnssCfgValKeyItemUsbinprot_t;

/** Item IDs for #U_GNSS_CFG_VAL_KEY_GROUP_ID_USBOUTPROT.
 */
typedef enum {
    U_GNSS_CFG_VAL_KEY_ITEM_USBOUTPROT_UBX_L    = 0x01, /**< set this flag to allow UBX protocol as output of USB. */
    U_GNSS_CFG_VAL_KEY_ITEM_USBOUTPROT_NMEA_L   = 0x02, /**< set this flag to allow NMEA protocol as output of USB. */
    U_GNSS_CFG_VAL_KEY_ITEM_USBOUTPROT_RTCM3X_L = 0x04  /**< set this flag to allow RTCM3X protocol as output of USB. */
} uGnssCfgValKeyItemUsboutprot_t;

/* ----------------------------------------------------------------
 * KEY IDS GENERATED BY u_gnss_cfg_val_key.py FROM THE ABOVE: DO NOT EDIT
 * -------------------------------------------------------------- */

// *** DO NOT MODIFY THIS LINE OR BELOW: AUTO-GENERATED BY u_gnss_cfg_val_key.py ***

#define U_GNSS_CFG_VAL_KEY_ID_ANA_USE_ANA_L                      0x10230001
#define U_GNSS_CFG_VAL_KEY_ID_ANA_ORBMAXERR_U2                   0x30230002
#define U_GNSS_CFG_VAL_KEY_ID_BATCH_ENABLE_L                     0x10260013
#define U_GNSS_CFG_VAL_KEY_ID_BATCH_PIOENABLE_L                  0x10260014
#define U_GNSS_CFG_VAL_KEY_ID_BATCH_MAXENTRIES_U2                0x30260015
#define U_GNSS_CFG_VAL_KEY_ID_BATCH_WARNTHRS_U2                  0x30260016
#define U_GNSS_CFG_VAL_KEY_ID_BATCH_PIOACTIVELOW_L               0x10260018
#define U_GNSS_CFG_VAL_KEY_ID_BATCH_PIOID_U1                     0x20260019
#define U_GNSS_CFG_VAL_KEY_ID_BATCH_EXTRAPVT_L                   0x1026001a
#define U_GNSS_CFG_VAL_KEY_ID_BATCH_EXTRAODO_L                   0x1026001b
#define U_GNSS_CFG_VAL_KEY_ID_BDS_USE_GEO_PRN_L                  0x10340014
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_CONFLVL_E1                0x20240011
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_USE_PIO_L                 0x10240012
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_PINPOL_E1                 0x20240013
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_PIN_U1                    0x20240014
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_USE_FENCE1_L              0x10240020
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE1_LAT_I4             0x40240021
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE1_LON_I4             0x40240022
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE1_RAD_U4             0x40240023
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_USE_FENCE2_L              0x10240030
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE2_LAT_I4             0x40240031
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE2_LON_I4             0x40240032
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE2_RAD_U4             0x40240033
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_USE_FENCE3_L              0x10240040
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE3_LAT_I4             0x40240041
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE3_LON_I4             0x40240042
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE3_RAD_U4             0x40240043
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_USE_FENCE4_L              0x10240050
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE4_LAT_I4             0x40240051
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE4_LON_I4             0x40240052
#define U_GNSS_CFG_VAL_KEY_ID_GEOFENCE_FENCE4_RAD_U4             0x40240053
#define U_GNSS_CFG_VAL_KEY_ID_HW_ANT_CFG_VOLTCTRL_L              0x10a3002e
#define U_GNSS_CFG_VAL_KEY_ID_HW_ANT_CFG_SHORTDET_L              0x10a3002f
#define U_GNSS_CFG_VAL_KEY_ID_HW_ANT_CFG_SHORTDET_POL_L          0x10a30030
#define U_GNSS_CFG_VAL_KEY_ID_HW_ANT_CFG_OPENDET_L               0x10a30031
#define U_GNSS_CFG_VAL_KEY_ID_HW_ANT_CFG_OPENDET_POL_L           0x10a30032
#define U_GNSS_CFG_VAL_KEY_ID_HW_ANT_CFG_PWRDOWN_L               0x10a30033
#define U_GNSS_CFG_VAL_KEY_ID_HW_ANT_CFG_PWRDOWN_POL_L           0x10a30034
#define U_GNSS_CFG_VAL_KEY_ID_HW_ANT_CFG_RECOVER_L               0x10a30035
#define U_GNSS_CFG_VAL_KEY_ID_HW_ANT_SUP_SWITCH_PIN_U1           0x20a30036
#define U_GNSS_CFG_VAL_KEY_ID_HW_ANT_SUP_SHORT_PIN_U1            0x20a30037
#define U_GNSS_CFG_VAL_KEY_ID_HW_ANT_SUP_OPEN_PIN_U1             0x20a30038
#define U_GNSS_CFG_VAL_KEY_ID_HW_ANT_SUP_ENGINE_E1               0x20a30054
#define U_GNSS_CFG_VAL_KEY_ID_HW_ANT_SUP_SHORT_THR_U1            0x20a30055
#define U_GNSS_CFG_VAL_KEY_ID_HW_ANT_SUP_OPEN_THR_U1             0x20a30056
#define U_GNSS_CFG_VAL_KEY_ID_I2C_ADDRESS_U1                     0x20510001
#define U_GNSS_CFG_VAL_KEY_ID_I2C_EXTENDEDTIMEOUT_L              0x10510002
#define U_GNSS_CFG_VAL_KEY_ID_I2C_ENABLED_L                      0x10510003
#define U_GNSS_CFG_VAL_KEY_ID_I2CINPROT_UBX_L                    0x10710001
#define U_GNSS_CFG_VAL_KEY_ID_I2CINPROT_NMEA_L                   0x10710002
#define U_GNSS_CFG_VAL_KEY_ID_I2CINPROT_RTCM3X_L                 0x10710004
#define U_GNSS_CFG_VAL_KEY_ID_I2CINPROT_SPARTN_L                 0x10710005
#define U_GNSS_CFG_VAL_KEY_ID_I2COUTPROT_UBX_L                   0x10720001
#define U_GNSS_CFG_VAL_KEY_ID_I2COUTPROT_NMEA_L                  0x10720002
#define U_GNSS_CFG_VAL_KEY_ID_I2COUTPROT_RTCM3X_L                0x10720004
#define U_GNSS_CFG_VAL_KEY_ID_INFMSG_UBX_I2C_X1                  0x20920001
#define U_GNSS_CFG_VAL_KEY_ID_INFMSG_UBX_UART1_X1                0x20920002
#define U_GNSS_CFG_VAL_KEY_ID_INFMSG_UBX_UART2_X1                0x20920003
#define U_GNSS_CFG_VAL_KEY_ID_INFMSG_UBX_USB_X1                  0x20920004
#define U_GNSS_CFG_VAL_KEY_ID_INFMSG_UBX_SPI_X1                  0x20920005
#define U_GNSS_CFG_VAL_KEY_ID_INFMSG_NMEA_I2C_X1                 0x20920006
#define U_GNSS_CFG_VAL_KEY_ID_INFMSG_NMEA_UART1_X1               0x20920007
#define U_GNSS_CFG_VAL_KEY_ID_INFMSG_NMEA_UART2_X1               0x20920008
#define U_GNSS_CFG_VAL_KEY_ID_INFMSG_NMEA_USB_X1                 0x20920009
#define U_GNSS_CFG_VAL_KEY_ID_INFMSG_NMEA_SPI_X1                 0x2092000a
#define U_GNSS_CFG_VAL_KEY_ID_ITFM_BBTHRESHOLD_U1                0x20410001
#define U_GNSS_CFG_VAL_KEY_ID_ITFM_CWTHRESHOLD_U1                0x20410002
#define U_GNSS_CFG_VAL_KEY_ID_ITFM_ENABLE_L                      0x1041000d
#define U_GNSS_CFG_VAL_KEY_ID_ITFM_ANTSETTING_E1                 0x20410010
#define U_GNSS_CFG_VAL_KEY_ID_ITFM_ENABLE_AUX_L                  0x10410013
#define U_GNSS_CFG_VAL_KEY_ID_LOGFILTER_RECORD_ENA_L             0x10de0002
#define U_GNSS_CFG_VAL_KEY_ID_LOGFILTER_ONCE_PER_WAKE_UP_ENA_L   0x10de0003
#define U_GNSS_CFG_VAL_KEY_ID_LOGFILTER_APPLY_ALL_FILTERS_L      0x10de0004
#define U_GNSS_CFG_VAL_KEY_ID_LOGFILTER_MIN_INTERVAL_U2          0x30de0005
#define U_GNSS_CFG_VAL_KEY_ID_LOGFILTER_TIME_THRS_U2             0x30de0006
#define U_GNSS_CFG_VAL_KEY_ID_LOGFILTER_SPEED_THRS_U2            0x30de0007
#define U_GNSS_CFG_VAL_KEY_ID_LOGFILTER_POSITION_THRS_U4         0x40de0008
#define U_GNSS_CFG_VAL_KEY_ID_MOT_GNSSSPEED_THRS_U1              0x20250038
#define U_GNSS_CFG_VAL_KEY_ID_MOT_GNSSDIST_THRS_U2               0x3025003b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_DTM_I2C_U1          0x209100a6
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_DTM_SPI_U1          0x209100aa
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_DTM_UART1_U1        0x209100a7
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_DTM_UART2_U1        0x209100a8
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_DTM_USB_U1          0x209100a9
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GBS_I2C_U1          0x209100dd
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GBS_SPI_U1          0x209100e1
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GBS_UART1_U1        0x209100de
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GBS_UART2_U1        0x209100df
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GBS_USB_U1          0x209100e0
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GGA_I2C_U1          0x209100ba
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GGA_SPI_U1          0x209100be
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GGA_UART1_U1        0x209100bb
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GGA_UART2_U1        0x209100bc
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GGA_USB_U1          0x209100bd
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GLL_I2C_U1          0x209100c9
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GLL_SPI_U1          0x209100cd
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GLL_UART1_U1        0x209100ca
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GLL_UART2_U1        0x209100cb
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GLL_USB_U1          0x209100cc
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GNS_I2C_U1          0x209100b5
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GNS_SPI_U1          0x209100b9
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GNS_UART1_U1        0x209100b6
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GNS_UART2_U1        0x209100b7
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GNS_USB_U1          0x209100b8
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GRS_I2C_U1          0x209100ce
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GRS_SPI_U1          0x209100d2
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GRS_UART1_U1        0x209100cf
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GRS_UART2_U1        0x209100d0
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GRS_USB_U1          0x209100d1
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GSA_I2C_U1          0x209100bf
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GSA_SPI_U1          0x209100c3
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GSA_UART1_U1        0x209100c0
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GSA_UART2_U1        0x209100c1
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GSA_USB_U1          0x209100c2
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GST_I2C_U1          0x209100d3
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GST_SPI_U1          0x209100d7
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GST_UART1_U1        0x209100d4
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GST_UART2_U1        0x209100d5
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GST_USB_U1          0x209100d6
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GSV_I2C_U1          0x209100c4
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GSV_SPI_U1          0x209100c8
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GSV_UART1_U1        0x209100c5
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GSV_UART2_U1        0x209100c6
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_GSV_USB_U1          0x209100c7
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_RLM_I2C_U1          0x20910400
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_RLM_SPI_U1          0x20910404
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_RLM_UART1_U1        0x20910401
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_RLM_UART2_U1        0x20910402
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_RLM_USB_U1          0x20910403
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_RMC_I2C_U1          0x209100ab
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_RMC_SPI_U1          0x209100af
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_RMC_UART1_U1        0x209100ac
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_RMC_UART2_U1        0x209100ad
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_RMC_USB_U1          0x209100ae
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_VLW_I2C_U1          0x209100e7
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_VLW_SPI_U1          0x209100eb
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_VLW_UART1_U1        0x209100e8
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_VLW_UART2_U1        0x209100e9
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_VLW_USB_U1          0x209100ea
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_VTG_I2C_U1          0x209100b0
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_VTG_SPI_U1          0x209100b4
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_VTG_UART1_U1        0x209100b1
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_VTG_UART2_U1        0x209100b2
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_VTG_USB_U1          0x209100b3
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_ZDA_I2C_U1          0x209100d8
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_ZDA_SPI_U1          0x209100dc
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_ZDA_UART1_U1        0x209100d9
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_ZDA_UART2_U1        0x209100da
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_ID_ZDA_USB_U1          0x209100db
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GGA_I2C_U1     0x20910661
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GGA_SPI_U1     0x20910665
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GGA_UART1_U1   0x20910662
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GGA_UART2_U1   0x20910663
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GGA_USB_U1     0x20910664
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GLL_I2C_U1     0x20910670
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GLL_SPI_U1     0x20910674
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GLL_UART1_U1   0x20910671
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GLL_UART2_U1   0x20910672
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GLL_USB_U1     0x20910673
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GNS_I2C_U1     0x2091065c
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GNS_SPI_U1     0x20910660
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GNS_UART1_U1   0x2091065d
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GNS_UART2_U1   0x2091065e
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GNS_USB_U1     0x2091065f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GSA_I2C_U1     0x20910666
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GSA_SPI_U1     0x2091066a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GSA_UART1_U1   0x20910667
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GSA_UART2_U1   0x20910668
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_GSA_USB_U1     0x20910669
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_RMC_I2C_U1     0x20910652
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_RMC_SPI_U1     0x20910656
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_RMC_UART1_U1   0x20910653
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_RMC_UART2_U1   0x20910654
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_RMC_USB_U1     0x20910655
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_VTG_I2C_U1     0x20910657
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_VTG_SPI_U1     0x2091065b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_VTG_UART1_U1   0x20910658
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_VTG_UART2_U1   0x20910649
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_VTG_USB_U1     0x2091065a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_ZDA_I2C_U1     0x2091067f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_ZDA_SPI_U1     0x20910683
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_ZDA_UART1_U1   0x20910680
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_ZDA_UART2_U1   0x20910681
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_NMEA_NAV2_ID_ZDA_USB_U1     0x20910682
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_PUBX_ID_POLYP_I2C_U1        0x209100ec
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_PUBX_ID_POLYP_SPI_U1        0x209100f0
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_PUBX_ID_POLYP_UART1_U1      0x209100ed
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_PUBX_ID_POLYP_UART2_U1      0x209100ee
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_PUBX_ID_POLYP_USB_U1        0x209100ef
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_PUBX_ID_POLYS_I2C_U1        0x209100f1
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_PUBX_ID_POLYS_SPI_U1        0x209100f5
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_PUBX_ID_POLYS_UART1_U1      0x209100f2
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_PUBX_ID_POLYS_UART2_U1      0x209100f3
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_PUBX_ID_POLYS_USB_U1        0x209100f4
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_PUBX_ID_POLYT_I2C_U1        0x209100f6
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_PUBX_ID_POLYT_SPI_U1        0x209100fa
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_PUBX_ID_POLYT_UART1_U1      0x209100f7
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_PUBX_ID_POLYT_UART2_U1      0x209100f8
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_PUBX_ID_POLYT_USB_U1        0x209100f9
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1005_I2C_U1     0x209102bd
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1005_SPI_U1     0x209102c1
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1005_UART1_U1   0x209102be
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1005_UART2_U1   0x209102bf
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1005_USB_U1     0x209102c0
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1074_I2C_U1     0x2091035e
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1074_SPI_U1     0x20910362
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1074_UART1_U1   0x2091035f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1074_UART2_U1   0x20910360
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1074_USB_U1     0x20910361
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1077_I2C_U1     0x209102cc
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1077_SPI_U1     0x209102d0
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1077_UART1_U1   0x209102cd
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1077_UART2_U1   0x209102ce
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1077_USB_U1     0x209102cf
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1084_I2C_U1     0x20910363
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1084_SPI_U1     0x20910367
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1084_UART1_U1   0x20910364
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1084_UART2_U1   0x20910365
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1084_USB_U1     0x20910366
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1087_I2C_U1     0x209102d1
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1087_SPI_U1     0x209102d5
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1087_UART1_U1   0x209102d2
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1087_UART2_U1   0x209102d3
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1087_USB_U1     0x209102d4
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1094_I2C_U1     0x20910368
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1094_SPI_U1     0x2091036c
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1094_UART1_U1   0x20910369
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1094_UART2_U1   0x2091036a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1094_USB_U1     0x2091036b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1097_I2C_U1     0x20910318
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1097_SPI_U1     0x2091031c
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1097_UART1_U1   0x20910319
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1097_UART2_U1   0x2091031a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1097_USB_U1     0x2091031b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1124_I2C_U1     0x2091036d
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1124_SPI_U1     0x20910371
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1124_UART1_U1   0x2091036e
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1124_UART2_U1   0x2091036f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1124_USB_U1     0x20910370
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1127_I2C_U1     0x209102d6
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1127_SPI_U1     0x209102da
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1127_UART1_U1   0x209102d7
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1127_UART2_U1   0x209102d8
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1127_USB_U1     0x209102d9
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1230_I2C_U1     0x20910303
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1230_SPI_U1     0x20910307
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1230_UART1_U1   0x20910304
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1230_UART2_U1   0x20910305
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE1230_USB_U1     0x20910306
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE4072_0_I2C_U1   0x209102fe
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE4072_0_SPI_U1   0x20910302
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE4072_0_UART1_U1 0x209102ff
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_RTCM_3X_TYPE4072_0_UART2_U1 0x20910300
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_ALG_I2C_U1          0x2091010f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_ALG_SPI_U1          0x20910113
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_ALG_UART1_U1        0x20910110
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_ALG_UART2_U1        0x20910111
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_ALG_USB_U1          0x20910112
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_INS_I2C_U1          0x20910114
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_INS_SPI_U1          0x20910118
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_INS_UART1_U1        0x20910115
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_INS_UART2_U1        0x20910116
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_INS_USB_U1          0x20910117
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_MEAS_I2C_U1         0x20910277
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_MEAS_SPI_U1         0x2091027b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_MEAS_UART1_U1       0x20910278
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_MEAS_UART2_U1       0x20910279
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_MEAS_USB_U1         0x2091027a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_RAW_I2C_U1          0x2091029f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_RAW_SPI_U1          0x209102a3
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_RAW_UART1_U1        0x209102a0
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_RAW_UART2_U1        0x209102a1
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_RAW_USB_U1          0x209102a2
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_STATUS_I2C_U1       0x20910105
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_STATUS_SPI_U1       0x20910109
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_STATUS_UART1_U1     0x20910106
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_STATUS_UART2_U1     0x20910107
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_ESF_STATUS_USB_U1       0x20910108
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_LOG_INFO_I2C_U1         0x20910259
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_LOG_INFO_SPI_U1         0x2091025d
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_LOG_INFO_UART1_U1       0x2091025a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_LOG_INFO_UART2_U1       0x2091025b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_LOG_INFO_USB_U1         0x2091025c
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_COMMS_I2C_U1        0x2091034f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_COMMS_SPI_U1        0x20910353
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_COMMS_UART1_U1      0x20910350
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_COMMS_UART2_U1      0x20910351
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_COMMS_USB_U1        0x20910352
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_HW2_I2C_U1          0x209101b9
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_HW2_SPI_U1          0x209101bd
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_HW2_UART1_U1        0x209101ba
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_HW2_UART2_U1        0x209101bb
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_HW2_USB_U1          0x209101bc
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_HW3_I2C_U1          0x20910354
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_HW3_SPI_U1          0x20910358
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_HW3_UART1_U1        0x20910355
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_HW3_UART2_U1        0x20910356
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_HW3_USB_U1          0x20910357
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_HW_I2C_U1           0x209101b4
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_HW_SPI_U1           0x209101b8
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_HW_UART1_U1         0x209101b5
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_HW_UART2_U1         0x209101b6
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_HW_USB_U1           0x209101b7
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_IO_I2C_U1           0x209101a5
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_IO_SPI_U1           0x209101a9
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_IO_UART1_U1         0x209101a6
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_IO_UART2_U1         0x209101a7
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_IO_USB_U1           0x209101a8
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_MSGPP_I2C_U1        0x20910196
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_MSGPP_SPI_U1        0x2091019a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_MSGPP_UART1_U1      0x20910197
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_MSGPP_UART2_U1      0x20910198
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_MSGPP_USB_U1        0x20910199
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_RF_I2C_U1           0x20910359
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_RF_SPI_U1           0x2091035d
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_RF_UART1_U1         0x2091035a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_RF_UART2_U1         0x2091035b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_RF_USB_U1           0x2091035c
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_RXBUF_I2C_U1        0x209101a0
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_RXBUF_SPI_U1        0x209101a4
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_RXBUF_UART1_U1      0x209101a1
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_RXBUF_UART2_U1      0x209101a2
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_RXBUF_USB_U1        0x209101a3
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_RXR_I2C_U1          0x20910187
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_RXR_SPI_U1          0x2091018b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_RXR_UART1_U1        0x20910188
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_RXR_UART2_U1        0x20910189
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_RXR_USB_U1          0x2091018a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_SPAN_I2C_U1         0x2091038b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_SPAN_SPI_U1         0x2091038f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_SPAN_UART1_U1       0x2091038c
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_SPAN_UART2_U1       0x2091038d
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_SPAN_USB_U1         0x2091038e
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_SYS_I2C_U1          0x2091069d
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_SYS_SPI_U1          0x209106a1
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_SYS_UART1_U1        0x2091069e
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_SYS_UART2_U1        0x2091069f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_SYS_USB_U1          0x209106a0
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_TXBUF_I2C_U1        0x2091019b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_TXBUF_SPI_U1        0x2091019f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_TXBUF_UART1_U1      0x2091019c
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_TXBUF_UART2_U1      0x2091019d
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_MON_TXBUF_USB_U1        0x2091019e
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_CLOCK_I2C_U1       0x20910430
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_CLOCK_SPI_U1       0x20910434
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_CLOCK_UART1_U1     0x20910431
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_CLOCK_UART2_U1     0x20910432
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_CLOCK_USB_U1       0x20910433
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_COV_I2C_U1         0x20910435
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_COV_SPI_U1         0x20910439
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_COV_UART1_U1       0x20910436
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_COV_UART2_U1       0x20910437
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_COV_USB_U1         0x20910438
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_DOP_I2C_U1         0x20910465
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_DOP_SPI_U1         0x20910469
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_DOP_UART1_U1       0x20910466
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_DOP_UART2_U1       0x20910467
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_DOP_USB_U1         0x20910468
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_EOE_I2C_U1         0x20910565
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_EOE_SPI_U1         0x20910569
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_EOE_UART1_U1       0x20910566
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_EOE_UART2_U1       0x20910567
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_EOE_USB_U1         0x20910568
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_ODO_I2C_U1         0x20910475
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_ODO_SPI_U1         0x20910479
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_ODO_UART1_U1       0x20910476
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_ODO_UART2_U1       0x20910477
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_ODO_USB_U1         0x20910478
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_POSECEF_I2C_U1     0x20910480
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_POSECEF_SPI_U1     0x20910484
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_POSECEF_UART1_U1   0x20910481
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_POSECEF_UART2_U1   0x20910482
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_POSECEF_USB_U1     0x20910483
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_POSLLH_I2C_U1      0x20910485
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_POSLLH_SPI_U1      0x20910489
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_POSLLH_UART1_U1    0x20910486
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_POSLLH_UART2_U1    0x20910487
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_POSLLH_USB_U1      0x20910488
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_PVT_I2C_U1         0x20910490
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_PVT_SPI_U1         0x20910494
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_PVT_UART1_U1       0x20910491
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_PVT_UART2_U1       0x20910492
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_PVT_USB_U1         0x20910493
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SAT_I2C_U1         0x20910495
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SAT_SPI_U1         0x20910499
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SAT_UART1_U1       0x20910496
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SAT_UART2_U1       0x20910497
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SAT_USB_U1         0x20910498
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SBAS_I2C_U1        0x20910500
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SBAS_SPI_U1        0x20910504
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SBAS_UART1_U1      0x20910501
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SBAS_UART2_U1      0x20910502
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SBAS_USB_U1        0x20910503
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SIG_I2C_U1         0x20910505
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SIG_SPI_U1         0x20910509
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SIG_UART1_U1       0x20910506
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SIG_UART2_U1       0x20910507
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SIG_USB_U1         0x20910508
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SLAS_I2C_U1        0x20910510
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SLAS_SPI_U1        0x20910514
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SLAS_UART1_U1      0x20910511
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SLAS_UART2_U1      0x20910512
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SLAS_USB_U1        0x20910513
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_STATUS_I2C_U1      0x20910515
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_STATUS_SPI_U1      0x20910519
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_STATUS_UART1_U1    0x20910516
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_STATUS_UART2_U1    0x20910517
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_STATUS_USB_U1      0x20910518
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SVIN_I2C_U1        0x20910520
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SVIN_SPI_U1        0x20910524
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SVIN_UART1_U1      0x20910521
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SVIN_UART2_U1      0x20910522
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_SVIN_USB_U1        0x20910523
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEBDS_I2C_U1     0x20910525
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEBDS_SPI_U1     0x20910529
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEBDS_UART1_U1   0x20910526
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEBDS_UART2_U1   0x20910527
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEBDS_USB_U1     0x20910528
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEGAL_I2C_U1     0x20910530
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEGAL_SPI_U1     0x20910534
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEGAL_UART1_U1   0x20910531
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEGAL_UART2_U1   0x20910532
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEGAL_USB_U1     0x20910533
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEGLO_I2C_U1     0x20910535
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEGLO_SPI_U1     0x20910539
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEGLO_UART1_U1   0x20910536
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEGLO_UART2_U1   0x20910537
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEGLO_USB_U1     0x20910538
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEGPS_I2C_U1     0x20910540
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEGPS_SPI_U1     0x20910544
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEGPS_UART1_U1   0x20910541
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEGPS_UART2_U1   0x20910542
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEGPS_USB_U1     0x20910543
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMELS_I2C_U1      0x20910545
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMELS_SPI_U1      0x20910549
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMELS_UART1_U1    0x20910546
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMELS_UART2_U1    0x20910547
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMELS_USB_U1      0x20910548
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEQZSS_I2C_U1    0x20910575
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEQZSS_SPI_U1    0x20910579
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEQZSS_UART1_U1  0x20910576
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEQZSS_UART2_U1  0x20910577
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEQZSS_USB_U1    0x20910578
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEUTC_I2C_U1     0x20910550
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEUTC_SPI_U1     0x20910554
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEUTC_UART1_U1   0x20910551
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEUTC_UART2_U1   0x20910552
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_TIMEUTC_USB_U1     0x20910553
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_VELECEF_I2C_U1     0x20910555
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_VELECEF_SPI_U1     0x20910559
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_VELECEF_UART1_U1   0x20910556
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_VELECEF_UART2_U1   0x20910557
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_VELECEF_USB_U1     0x20910558
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_VELNED_I2C_U1      0x20910560
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_VELNED_SPI_U1      0x20910564
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_VELNED_UART1_U1    0x20910561
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_VELNED_UART2_U1    0x20910562
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV2_VELNED_USB_U1      0x20910563
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_AOPSTATUS_I2C_U1    0x20910079
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_AOPSTATUS_SPI_U1    0x2091007d
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_AOPSTATUS_UART1_U1  0x2091007a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_AOPSTATUS_UART2_U1  0x2091007b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_AOPSTATUS_USB_U1    0x2091007c
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_CLOCK_I2C_U1        0x20910065
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_CLOCK_SPI_U1        0x20910069
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_CLOCK_UART1_U1      0x20910066
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_CLOCK_UART2_U1      0x20910067
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_CLOCK_USB_U1        0x20910068
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_COV_I2C_U1          0x20910083
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_COV_SPI_U1          0x20910087
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_COV_UART1_U1        0x20910084
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_COV_UART2_U1        0x20910085
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_COV_USB_U1          0x20910086
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_DOP_I2C_U1          0x20910038
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_DOP_SPI_U1          0x2091003c
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_DOP_UART1_U1        0x20910039
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_DOP_UART2_U1        0x2091003a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_DOP_USB_U1          0x2091003b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_EOE_I2C_U1          0x2091015f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_EOE_SPI_U1          0x20910163
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_EOE_UART1_U1        0x20910160
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_EOE_UART2_U1        0x20910161
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_EOE_USB_U1          0x20910162
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_GEOFENCE_I2C_U1     0x209100a1
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_GEOFENCE_SPI_U1     0x209100a5
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_GEOFENCE_UART1_U1   0x209100a2
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_GEOFENCE_UART2_U1   0x209100a3
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_GEOFENCE_USB_U1     0x209100a4
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSECEF_I2C_U1    0x2091002e
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSECEF_SPI_U1    0x20910032
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSECEF_UART1_U1  0x2091002f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSECEF_UART2_U1  0x20910030
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSECEF_USB_U1    0x20910031
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSLLH_I2C_U1     0x20910033
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSLLH_SPI_U1     0x20910037
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSLLH_UART1_U1   0x20910034
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSLLH_UART2_U1   0x20910035
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_HPPOSLLH_USB_U1     0x20910036
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_ODO_I2C_U1          0x2091007e
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_ODO_SPI_U1          0x20910082
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_ODO_UART1_U1        0x2091007f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_ODO_UART2_U1        0x20910080
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_ODO_USB_U1          0x20910081
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_ORB_I2C_U1          0x20910010
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_ORB_SPI_U1          0x20910014
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_ORB_UART1_U1        0x20910011
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_ORB_UART2_U1        0x20910012
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_ORB_USB_U1          0x20910013
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PL_I2C_U1           0x20910415
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PL_SPI_U1           0x20910419
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PL_UART1_U1         0x20910416
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PL_UART2_U1         0x20910417
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PL_USB_U1           0x20910418
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_POSECEF_I2C_U1      0x20910024
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_POSECEF_SPI_U1      0x20910028
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_POSECEF_UART1_U1    0x20910025
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_POSECEF_UART2_U1    0x20910026
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_POSECEF_USB_U1      0x20910027
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_POSLLH_I2C_U1       0x20910029
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_POSLLH_SPI_U1       0x2091002d
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_POSLLH_UART1_U1     0x2091002a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_POSLLH_UART2_U1     0x2091002b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_POSLLH_USB_U1       0x2091002c
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PVT_I2C_U1          0x20910006
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PVT_SPI_U1          0x2091000a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PVT_UART1_U1        0x20910007
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PVT_UART2_U1        0x20910008
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PVT_USB_U1          0x20910009
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_RELPOSNED_I2C_U1    0x2091008d
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_RELPOSNED_SPI_U1    0x20910091
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_RELPOSNED_UART1_U1  0x2091008e
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_RELPOSNED_UART2_U1  0x2091008f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_RELPOSNED_USB_U1    0x20910090
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SAT_I2C_U1          0x20910015
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SAT_SPI_U1          0x20910019
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SAT_UART1_U1        0x20910016
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SAT_UART2_U1        0x20910017
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SAT_USB_U1          0x20910018
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SBAS_I2C_U1         0x2091006a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SBAS_SPI_U1         0x2091006e
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SBAS_UART1_U1       0x2091006b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SBAS_UART2_U1       0x2091006c
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SBAS_USB_U1         0x2091006d
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SIG_I2C_U1          0x20910345
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SIG_SPI_U1          0x20910349
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SIG_UART1_U1        0x20910346
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SIG_UART2_U1        0x20910347
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SIG_USB_U1          0x20910348
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SLAS_I2C_U1         0x20910336
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SLAS_SPI_U1         0x2091033a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SLAS_UART1_U1       0x20910337
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SLAS_UART2_U1       0x20910338
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SLAS_USB_U1         0x20910339
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_STATUS_I2C_U1       0x2091001a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_STATUS_SPI_U1       0x2091001e
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_STATUS_UART1_U1     0x2091001b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_STATUS_UART2_U1     0x2091001c
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_STATUS_USB_U1       0x2091001d
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SVIN_I2C_U1         0x20910088
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SVIN_SPI_U1         0x2091008c
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SVIN_UART1_U1       0x20910089
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SVIN_UART2_U1       0x2091008a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_SVIN_USB_U1         0x2091008b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEBDS_I2C_U1      0x20910051
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEBDS_SPI_U1      0x20910055
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEBDS_UART1_U1    0x20910052
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEBDS_UART2_U1    0x20910053
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEBDS_USB_U1      0x20910054
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEGAL_I2C_U1      0x20910056
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEGAL_SPI_U1      0x2091005a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEGAL_UART1_U1    0x20910057
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEGAL_UART2_U1    0x20910058
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEGAL_USB_U1      0x20910059
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEGLO_I2C_U1      0x2091004c
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEGLO_SPI_U1      0x20910050
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEGLO_UART1_U1    0x2091004d
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEGLO_UART2_U1    0x2091004e
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEGLO_USB_U1      0x2091004f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEGPS_I2C_U1      0x20910047
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEGPS_SPI_U1      0x2091004b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEGPS_UART1_U1    0x20910048
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEGPS_UART2_U1    0x20910049
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEGPS_USB_U1      0x2091004a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMELS_I2C_U1       0x20910060
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMELS_SPI_U1       0x20910064
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMELS_UART1_U1     0x20910061
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMELS_UART2_U1     0x20910062
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMELS_USB_U1       0x20910063
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEQZSS_I2C_U1     0x20910386
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEQZSS_SPI_U1     0x2091038a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEQZSS_UART1_U1   0x20910387
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEQZSS_UART2_U1   0x20910388
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEQZSS_USB_U1     0x20910389
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEUTC_I2C_U1      0x2091005b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEUTC_SPI_U1      0x2091005f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEUTC_UART1_U1    0x2091005c
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEUTC_UART2_U1    0x2091005d
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_TIMEUTC_USB_U1      0x2091005e
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_VELECEF_I2C_U1      0x2091003d
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_VELECEF_SPI_U1      0x20910041
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_VELECEF_UART1_U1    0x2091003e
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_VELECEF_UART2_U1    0x2091003f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_VELECEF_USB_U1      0x20910040
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_VELNED_I2C_U1       0x20910042
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_VELNED_SPI_U1       0x20910046
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_VELNED_UART1_U1     0x20910043
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_VELNED_UART2_U1     0x20910044
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_VELNED_USB_U1       0x20910045
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_COR_I2C_U1          0x209106b6
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_COR_SPI_U1          0x209106ba
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_COR_UART1_U1        0x209106b7
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_COR_UART2_U1        0x209106b8
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_COR_USB_U1          0x209106b9
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_MEASX_I2C_U1        0x20910204
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_MEASX_SPI_U1        0x20910208
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_MEASX_UART1_U1      0x20910205
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_MEASX_UART2_U1      0x20910206
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_MEASX_USB_U1        0x20910207
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_PMP_I2C_U1          0x2091031d
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_PMP_SPI_U1          0x20910321
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_PMP_UART1_U1        0x2091031e
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_PMP_UART2_U1        0x2091031f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_PMP_USB_U1          0x20910320
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_QZSSL6_UART1_U1     0x2091033b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_QZSSL6_UART2_U1     0x2091033c
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_QZSSL6_USB_U1       0x2091033d
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_QZSSL6_I2C_U1       0x2091033f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_QZSSL6_SPI_U1       0x2091033e
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_RAWX_I2C_U1         0x209102a4
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_RAWX_SPI_U1         0x209102a8
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_RAWX_UART1_U1       0x209102a5
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_RAWX_UART2_U1       0x209102a6
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_RAWX_USB_U1         0x209102a7
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_RLM_I2C_U1          0x2091025e
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_RLM_SPI_U1          0x20910262
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_RLM_UART1_U1        0x2091025f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_RLM_UART2_U1        0x20910260
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_RLM_USB_U1          0x20910261
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_RTCM_I2C_U1         0x20910268
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_RTCM_SPI_U1         0x2091026c
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_RTCM_UART1_U1       0x20910269
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_RTCM_UART2_U1       0x2091026a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_RTCM_USB_U1         0x2091026b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_SFRBX_I2C_U1        0x20910231
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_SFRBX_SPI_U1        0x20910235
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_SFRBX_UART1_U1      0x20910232
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_SFRBX_UART2_U1      0x20910233
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_SFRBX_USB_U1        0x20910234
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_SPARTN_I2C_U1       0x20910605
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_SPARTN_SPI_U1       0x20910609
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_SPARTN_UART1_U1     0x20910606
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_SPARTN_UART2_U1     0x20910607
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_RXM_SPARTN_USB_U1       0x20910608
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_TIM_TM2_I2C_U1          0x20910178
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_TIM_TM2_SPI_U1          0x2091017c
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_TIM_TM2_UART1_U1        0x20910179
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_TIM_TM2_UART2_U1        0x2091017a
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_TIM_TM2_USB_U1          0x2091017b
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_TIM_TP_I2C_U1           0x2091017d
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_TIM_TP_SPI_U1           0x20910181
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_TIM_TP_UART1_U1         0x2091017e
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_TIM_TP_UART2_U1         0x2091017f
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_TIM_TP_USB_U1           0x20910180
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_TIM_VRFY_I2C_U1         0x20910092
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_TIM_VRFY_SPI_U1         0x20910096
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_TIM_VRFY_UART1_U1       0x20910093
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_TIM_VRFY_UART2_U1       0x20910094
#define U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_TIM_VRFY_USB_U1         0x20910095
#define U_GNSS_CFG_VAL_KEY_ID_NAV2_OUT_ENABLED_L                 0x10170001
#define U_GNSS_CFG_VAL_KEY_ID_NAV2_SBAS_USE_INTEGRITY_L          0x10170002
#define U_GNSS_CFG_VAL_KEY_ID_NAVHPG_DGNSSMODE_E1                0x20140011
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_FIXMODE_E1                  0x20110011
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_INIFIX3D_L                  0x10110013
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_WKNROLLOVER_U2              0x30110017
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_USE_PPP_L                   0x10110019
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_UTCSTANDARD_E1              0x2011001c
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_DYNMODEL_E1                 0x20110021
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_ACKAIDING_L                 0x10110025
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_USRDAT_L                    0x10110061
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_USRDAT_MAJA_R8              0x50110062
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_USRDAT_FLAT_R8              0x50110063
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_USRDAT_DX_R4                0x40110064
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_USRDAT_DY_R4                0x40110065
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_USRDAT_DZ_R4                0x40110066
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_USRDAT_ROTX_R4              0x40110067
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_USRDAT_ROTY_R4              0x40110068
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_USRDAT_ROTZ_R4              0x40110069
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_USRDAT_SCALE_R4             0x4011006a
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_INFIL_MINSVS_U1             0x201100a1
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_INFIL_MAXSVS_U1             0x201100a2
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_INFIL_MINCNO_U1             0x201100a3
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_INFIL_MINELEV_I1            0x201100a4
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_INFIL_NCNOTHRS_U1           0x201100aa
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_INFIL_CNOTHRS_U1            0x201100ab
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_OUTFIL_PDOP_U2              0x301100b1
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_OUTFIL_TDOP_U2              0x301100b2
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_OUTFIL_PACC_U2              0x301100b3
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_OUTFIL_TACC_U2              0x301100b4
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_OUTFIL_FACC_U2              0x301100b5
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_CONSTR_ALT_I4               0x401100c1
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_CONSTR_ALTVAR_U4            0x401100c2
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_CONSTR_DGNSSTO_U1           0x201100c4
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_SIGATTCOMP_E1               0x201100d6
#define U_GNSS_CFG_VAL_KEY_ID_NAVSPG_PL_ENA_L                    0x101100d7
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_PROTVER_E1                    0x20930001
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_MAXSVS_E1                     0x20930002
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_COMPAT_L                      0x10930003
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_CONSIDER_L                    0x10930004
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_LIMIT82_L                     0x10930005
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_HIGHPREC_L                    0x10930006
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_SVNUMBERING_E1                0x20930007
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_FILT_GPS_L                    0x10930011
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_FILT_SBAS_L                   0x10930012
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_FILT_GAL_L                    0x10930013
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_FILT_QZSS_L                   0x10930015
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_FILT_GLO_L                    0x10930016
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_FILT_BDS_L                    0x10930017
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_OUT_INVFIX_L                  0x10930021
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_OUT_MSKFIX_L                  0x10930022
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_OUT_INVTIME_L                 0x10930023
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_OUT_INVDATE_L                 0x10930024
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_OUT_ONLYGPS_L                 0x10930025
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_OUT_FROZENCOG_L               0x10930026
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_MAINTALKERID_E1               0x20930031
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_GSVTALKERID_E1                0x20930032
#define U_GNSS_CFG_VAL_KEY_ID_NMEA_BDSTALKERID_U2                0x30930033
#define U_GNSS_CFG_VAL_KEY_ID_ODO_USE_ODO_L                      0x10220001
#define U_GNSS_CFG_VAL_KEY_ID_ODO_USE_COG_L                      0x10220002
#define U_GNSS_CFG_VAL_KEY_ID_ODO_OUTLPVEL_L                     0x10220003
#define U_GNSS_CFG_VAL_KEY_ID_ODO_OUTLPCOG_L                     0x10220004
#define U_GNSS_CFG_VAL_KEY_ID_ODO_PROFILE_E1                     0x20220005
#define U_GNSS_CFG_VAL_KEY_ID_ODO_COGMAXSPEED_U1                 0x20220021
#define U_GNSS_CFG_VAL_KEY_ID_ODO_COGMAXPOSACC_U1                0x20220022
#define U_GNSS_CFG_VAL_KEY_ID_ODO_VELLPGAIN_U1                   0x20220031
#define U_GNSS_CFG_VAL_KEY_ID_ODO_COGLPGAIN_U1                   0x20220032
#define U_GNSS_CFG_VAL_KEY_ID_PM_OPERATEMODE_E1                  0x20d00001
#define U_GNSS_CFG_VAL_KEY_ID_PM_POSUPDATEPERIOD_U4              0x40d00002
#define U_GNSS_CFG_VAL_KEY_ID_PM_ACQPERIOD_U4                    0x40d00003
#define U_GNSS_CFG_VAL_KEY_ID_PM_GRIDOFFSET_U4                   0x40d00004
#define U_GNSS_CFG_VAL_KEY_ID_PM_ONTIME_U2                       0x30d00005
#define U_GNSS_CFG_VAL_KEY_ID_PM_MINACQTIME_U1                   0x20d00006
#define U_GNSS_CFG_VAL_KEY_ID_PM_MAXACQTIME_U1                   0x20d00007
#define U_GNSS_CFG_VAL_KEY_ID_PM_ONOTENTEROFF_L                  0x10d00008
#define U_GNSS_CFG_VAL_KEY_ID_PM_WAITTIMEFIX_L                   0x10d00009
#define U_GNSS_CFG_VAL_KEY_ID_PM_UPDATEEPH_L                     0x10d0000a
#define U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTSEL_E1                    0x20d0000b
#define U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTWAKE_L                    0x10d0000c
#define U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTBACKUP_L                  0x10d0000d
#define U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTINACTIVE_L                0x10d0000e
#define U_GNSS_CFG_VAL_KEY_ID_PM_EXTINTINACTIVITY_U4             0x40d0000f
#define U_GNSS_CFG_VAL_KEY_ID_PM_LIMITPEAKCURR_L                 0x10d00010
#define U_GNSS_CFG_VAL_KEY_ID_PMP_CENTER_FREQUENCY_U4            0x40b10011
#define U_GNSS_CFG_VAL_KEY_ID_PMP_SEARCH_WINDOW_U2               0x30b10012
#define U_GNSS_CFG_VAL_KEY_ID_PMP_USE_SERVICE_ID_L               0x10b10016
#define U_GNSS_CFG_VAL_KEY_ID_PMP_SERVICE_ID_U2                  0x30b10017
#define U_GNSS_CFG_VAL_KEY_ID_PMP_DATA_RATE_E2                   0x30b10013
#define U_GNSS_CFG_VAL_KEY_ID_PMP_USE_DESCRAMBLER_L              0x10b10014
#define U_GNSS_CFG_VAL_KEY_ID_PMP_DESCRAMBLER_INIT_U2            0x30b10015
#define U_GNSS_CFG_VAL_KEY_ID_PMP_USE_PRESCRAMBLING_L            0x10b10019
#define U_GNSS_CFG_VAL_KEY_ID_PMP_UNIQUE_WORD_U8                 0x50b1001a
#define U_GNSS_CFG_VAL_KEY_ID_QZSS_USE_SLAS_DGNSS_L              0x10370005
#define U_GNSS_CFG_VAL_KEY_ID_QZSS_USE_SLAS_TESTMODE_L           0x10370006
#define U_GNSS_CFG_VAL_KEY_ID_QZSS_USE_SLAS_RAIM_UNCORR_L        0x10370007
#define U_GNSS_CFG_VAL_KEY_ID_QZSS_SLAS_MAX_BASELINE_U2          0x30370008
#define U_GNSS_CFG_VAL_KEY_ID_QZSS_L6_SVIDA_I1                   0x20370020
#define U_GNSS_CFG_VAL_KEY_ID_QZSS_L6_SVIDB_I1                   0x20370030
#define U_GNSS_CFG_VAL_KEY_ID_QZSS_L6_MSGA_E1                    0x20370050
#define U_GNSS_CFG_VAL_KEY_ID_QZSS_L6_MSGB_E1                    0x20370060
#define U_GNSS_CFG_VAL_KEY_ID_QZSS_L6_RSDECODER_E1               0x20370080
#define U_GNSS_CFG_VAL_KEY_ID_RATE_MEAS_U2                       0x30210001
#define U_GNSS_CFG_VAL_KEY_ID_RATE_NAV_U2                        0x30210002
#define U_GNSS_CFG_VAL_KEY_ID_RATE_TIMEREF_E1                    0x20210003
#define U_GNSS_CFG_VAL_KEY_ID_RINV_DUMP_L                        0x10c70001
#define U_GNSS_CFG_VAL_KEY_ID_RINV_BINARY_L                      0x10c70002
#define U_GNSS_CFG_VAL_KEY_ID_RINV_DATA_SIZE_U1                  0x20c70003
#define U_GNSS_CFG_VAL_KEY_ID_RINV_CHUNK0_X8                     0x50c70004
#define U_GNSS_CFG_VAL_KEY_ID_RINV_CHUNK1_X8                     0x50c70005
#define U_GNSS_CFG_VAL_KEY_ID_RINV_CHUNK2_X8                     0x50c70006
#define U_GNSS_CFG_VAL_KEY_ID_RINV_CHUNK3_X8                     0x50c70007
#define U_GNSS_CFG_VAL_KEY_ID_RTCM_DF003_OUT_U2                  0x30090001
#define U_GNSS_CFG_VAL_KEY_ID_RTCM_DF003_IN_U2                   0x30090008
#define U_GNSS_CFG_VAL_KEY_ID_RTCM_DF003_IN_FILTER_E1            0x20090009
#define U_GNSS_CFG_VAL_KEY_ID_SBAS_USE_TESTMODE_L                0x10360002
#define U_GNSS_CFG_VAL_KEY_ID_SBAS_USE_RANGING_L                 0x10360003
#define U_GNSS_CFG_VAL_KEY_ID_SBAS_USE_DIFFCORR_L                0x10360004
#define U_GNSS_CFG_VAL_KEY_ID_SBAS_USE_INTEGRITY_L               0x10360005
#define U_GNSS_CFG_VAL_KEY_ID_SBAS_PRNSCANMASK_X8                0x50360006
#define U_GNSS_CFG_VAL_KEY_ID_SEC_CFG_LOCK_L                     0x10f60009
#define U_GNSS_CFG_VAL_KEY_ID_SEC_CFG_LOCK_UNLOCKGRP1_U2         0x30f6000a
#define U_GNSS_CFG_VAL_KEY_ID_SEC_CFG_LOCK_UNLOCKGRP2_U2         0x30f6000b
#define U_GNSS_CFG_VAL_KEY_ID_SFCORE_USE_SF_L                    0x10080001
#define U_GNSS_CFG_VAL_KEY_ID_SFIMU_GYRO_TC_UPDATE_PERIOD_U2     0x30060007
#define U_GNSS_CFG_VAL_KEY_ID_SFIMU_GYRO_RMSTHDL_U1              0x20060008
#define U_GNSS_CFG_VAL_KEY_ID_SFIMU_GYRO_FREQUENCY_U1            0x20060009
#define U_GNSS_CFG_VAL_KEY_ID_SFIMU_GYRO_LATENCY_U2              0x3006000a
#define U_GNSS_CFG_VAL_KEY_ID_SFIMU_GYRO_ACCURACY_U2             0x3006000b
#define U_GNSS_CFG_VAL_KEY_ID_SFIMU_ACCEL_RMSTHDL_U1             0x20060015
#define U_GNSS_CFG_VAL_KEY_ID_SFIMU_ACCEL_FREQUENCY_U1           0x20060016
#define U_GNSS_CFG_VAL_KEY_ID_SFIMU_ACCEL_LATENCY_U2             0x30060017
#define U_GNSS_CFG_VAL_KEY_ID_SFIMU_ACCEL_ACCURACY_U2            0x30060018
#define U_GNSS_CFG_VAL_KEY_ID_SFIMU_IMU_EN_L                     0x1006001d
#define U_GNSS_CFG_VAL_KEY_ID_SFIMU_IMU_I2C_SCL_PIO_U1           0x2006001e
#define U_GNSS_CFG_VAL_KEY_ID_SFIMU_IMU_I2C_SDA_PIO_U1           0x2006001f
#define U_GNSS_CFG_VAL_KEY_ID_SFIMU_AUTO_MNTALG_ENA_L            0x10060027
#define U_GNSS_CFG_VAL_KEY_ID_SFIMU_IMU_MNTALG_YAW_U4            0x4006002d
#define U_GNSS_CFG_VAL_KEY_ID_SFIMU_IMU_MNTALG_PITCH_I2          0x3006002e
#define U_GNSS_CFG_VAL_KEY_ID_SFIMU_IMU_MNTALG_ROLL_I2           0x3006002f
#define U_GNSS_CFG_VAL_KEY_ID_SFODO_COMBINE_TICKS_L              0x10070001
#define U_GNSS_CFG_VAL_KEY_ID_SFODO_USE_SPEED_L                  0x10070003
#define U_GNSS_CFG_VAL_KEY_ID_SFODO_DIS_AUTOCOUNTMAX_L           0x10070004
#define U_GNSS_CFG_VAL_KEY_ID_SFODO_DIS_AUTODIRPINPOL_L          0x10070005
#define U_GNSS_CFG_VAL_KEY_ID_SFODO_DIS_AUTOSPEED_L              0x10070006
#define U_GNSS_CFG_VAL_KEY_ID_SFODO_FACTOR_U4                    0x40070007
#define U_GNSS_CFG_VAL_KEY_ID_SFODO_QUANT_ERROR_U4               0x40070008
#define U_GNSS_CFG_VAL_KEY_ID_SFODO_COUNT_MAX_U4                 0x40070009
#define U_GNSS_CFG_VAL_KEY_ID_SFODO_LATENCY_U2                   0x3007000a
#define U_GNSS_CFG_VAL_KEY_ID_SFODO_FREQUENCY_U1                 0x2007000b
#define U_GNSS_CFG_VAL_KEY_ID_SFODO_CNT_BOTH_EDGES_L             0x1007000d
#define U_GNSS_CFG_VAL_KEY_ID_SFODO_SPEED_BAND_U2                0x3007000e
#define U_GNSS_CFG_VAL_KEY_ID_SFODO_USE_WT_PIN_L                 0x1007000f
#define U_GNSS_CFG_VAL_KEY_ID_SFODO_DIR_PINPOL_L                 0x10070010
#define U_GNSS_CFG_VAL_KEY_ID_SFODO_DIS_AUTOSW_L                 0x10070011
#define U_GNSS_CFG_VAL_KEY_ID_SIGNAL_GPS_ENA_L                   0x1031001f
#define U_GNSS_CFG_VAL_KEY_ID_SIGNAL_GPS_L1CA_ENA_L              0x10310001
#define U_GNSS_CFG_VAL_KEY_ID_SIGNAL_GPS_L2C_ENA_L               0x10310003
#define U_GNSS_CFG_VAL_KEY_ID_SIGNAL_SBAS_ENA_L                  0x10310020
#define U_GNSS_CFG_VAL_KEY_ID_SIGNAL_SBAS_L1CA_ENA_L             0x10310005
#define U_GNSS_CFG_VAL_KEY_ID_SIGNAL_GAL_ENA_L                   0x10310021
#define U_GNSS_CFG_VAL_KEY_ID_SIGNAL_GAL_E1_ENA_L                0x10310007
#define U_GNSS_CFG_VAL_KEY_ID_SIGNAL_GAL_E5B_ENA_L               0x1031000a
#define U_GNSS_CFG_VAL_KEY_ID_SIGNAL_BDS_ENA_L                   0x10310022
#define U_GNSS_CFG_VAL_KEY_ID_SIGNAL_BDS_B1_ENA_L                0x1031000d
#define U_GNSS_CFG_VAL_KEY_ID_SIGNAL_BDS_B2_ENA_L                0x1031000e
#define U_GNSS_CFG_VAL_KEY_ID_SIGNAL_QZSS_ENA_L                  0x10310024
#define U_GNSS_CFG_VAL_KEY_ID_SIGNAL_QZSS_L1CA_ENA_L             0x10310012
#define U_GNSS_CFG_VAL_KEY_ID_SIGNAL_QZSS_L1S_ENA_L              0x10310014
#define U_GNSS_CFG_VAL_KEY_ID_SIGNAL_QZSS_L2C_ENA_L              0x10310015
#define U_GNSS_CFG_VAL_KEY_ID_SIGNAL_GLO_ENA_L                   0x10310025
#define U_GNSS_CFG_VAL_KEY_ID_SIGNAL_GLO_L1_ENA_L                0x10310018
#define U_GNSS_CFG_VAL_KEY_ID_SIGNAL_GLO_L2_ENA_L                0x1031001a
#define U_GNSS_CFG_VAL_KEY_ID_SPARTN_USE_SOURCE_E1               0x20a70001
#define U_GNSS_CFG_VAL_KEY_ID_SPI_MAXFF_U1                       0x20640001
#define U_GNSS_CFG_VAL_KEY_ID_SPI_CPOLARITY_L                    0x10640002
#define U_GNSS_CFG_VAL_KEY_ID_SPI_CPHASE_L                       0x10640003
#define U_GNSS_CFG_VAL_KEY_ID_SPI_EXTENDEDTIMEOUT_L              0x10640005
#define U_GNSS_CFG_VAL_KEY_ID_SPI_ENABLED_L                      0x10640006
#define U_GNSS_CFG_VAL_KEY_ID_SPIINPROT_UBX_L                    0x10790001
#define U_GNSS_CFG_VAL_KEY_ID_SPIINPROT_NMEA_L                   0x10790002
#define U_GNSS_CFG_VAL_KEY_ID_SPIINPROT_RTCM3X_L                 0x10790004
#define U_GNSS_CFG_VAL_KEY_ID_SPIINPROT_SPARTN_L                 0x10790005
#define U_GNSS_CFG_VAL_KEY_ID_SPIOUTPROT_UBX_L                   0x107a0001
#define U_GNSS_CFG_VAL_KEY_ID_SPIOUTPROT_NMEA_L                  0x107a0002
#define U_GNSS_CFG_VAL_KEY_ID_SPIOUTPROT_RTCM3X_L                0x107a0004
#define U_GNSS_CFG_VAL_KEY_ID_TMODE_MODE_E1                      0x20030001
#define U_GNSS_CFG_VAL_KEY_ID_TMODE_POS_TYPE_E1                  0x20030002
#define U_GNSS_CFG_VAL_KEY_ID_TMODE_ECEF_X_I4                    0x40030003
#define U_GNSS_CFG_VAL_KEY_ID_TMODE_ECEF_Y_I4                    0x40030004
#define U_GNSS_CFG_VAL_KEY_ID_TMODE_ECEF_Z_I4                    0x40030005
#define U_GNSS_CFG_VAL_KEY_ID_TMODE_ECEF_X_HP_I1                 0x20030006
#define U_GNSS_CFG_VAL_KEY_ID_TMODE_ECEF_Y_HP_I1                 0x20030007
#define U_GNSS_CFG_VAL_KEY_ID_TMODE_ECEF_Z_HP_I1                 0x20030008
#define U_GNSS_CFG_VAL_KEY_ID_TMODE_LAT_I4                       0x40030009
#define U_GNSS_CFG_VAL_KEY_ID_TMODE_LON_I4                       0x4003000a
#define U_GNSS_CFG_VAL_KEY_ID_TMODE_HEIGHT_I4                    0x4003000b
#define U_GNSS_CFG_VAL_KEY_ID_TMODE_LAT_HP_I1                    0x2003000c
#define U_GNSS_CFG_VAL_KEY_ID_TMODE_LON_HP_I1                    0x2003000d
#define U_GNSS_CFG_VAL_KEY_ID_TMODE_HEIGHT_HP_I1                 0x2003000e
#define U_GNSS_CFG_VAL_KEY_ID_TMODE_FIXED_POS_ACC_U4             0x4003000f
#define U_GNSS_CFG_VAL_KEY_ID_TMODE_SVIN_MIN_DUR_U4              0x40030010
#define U_GNSS_CFG_VAL_KEY_ID_TMODE_SVIN_ACC_LIMIT_U4            0x40030011
#define U_GNSS_CFG_VAL_KEY_ID_TP_PULSE_DEF_E1                    0x20050023
#define U_GNSS_CFG_VAL_KEY_ID_TP_PULSE_LENGTH_DEF_E1             0x20050030
#define U_GNSS_CFG_VAL_KEY_ID_TP_ANT_CABLEDELAY_I2               0x30050001
#define U_GNSS_CFG_VAL_KEY_ID_TP_PERIOD_TP1_U4                   0x40050002
#define U_GNSS_CFG_VAL_KEY_ID_TP_PERIOD_LOCK_TP1_U4              0x40050003
#define U_GNSS_CFG_VAL_KEY_ID_TP_FREQ_TP1_U4                     0x40050024
#define U_GNSS_CFG_VAL_KEY_ID_TP_FREQ_LOCK_TP1_U4                0x40050025
#define U_GNSS_CFG_VAL_KEY_ID_TP_LEN_TP1_U4                      0x40050004
#define U_GNSS_CFG_VAL_KEY_ID_TP_LEN_LOCK_TP1_U4                 0x40050005
#define U_GNSS_CFG_VAL_KEY_ID_TP_DUTY_TP1_R8                     0x5005002a
#define U_GNSS_CFG_VAL_KEY_ID_TP_DUTY_LOCK_TP1_R8                0x5005002b
#define U_GNSS_CFG_VAL_KEY_ID_TP_USER_DELAY_TP1_I4               0x40050006
#define U_GNSS_CFG_VAL_KEY_ID_TP_TP1_ENA_L                       0x10050007
#define U_GNSS_CFG_VAL_KEY_ID_TP_SYNC_GNSS_TP1_L                 0x10050008
#define U_GNSS_CFG_VAL_KEY_ID_TP_USE_LOCKED_TP1_L                0x10050009
#define U_GNSS_CFG_VAL_KEY_ID_TP_ALIGN_TO_TOW_TP1_L              0x1005000a
#define U_GNSS_CFG_VAL_KEY_ID_TP_POL_TP1_L                       0x1005000b
#define U_GNSS_CFG_VAL_KEY_ID_TP_TIMEGRID_TP1_E1                 0x2005000c
#define U_GNSS_CFG_VAL_KEY_ID_TP_DRSTR_TP1_E1                    0x20050035
#define U_GNSS_CFG_VAL_KEY_ID_TP_PERIOD_TP2_U4                   0x4005000d
#define U_GNSS_CFG_VAL_KEY_ID_TP_PERIOD_LOCK_TP2_U4              0x4005000e
#define U_GNSS_CFG_VAL_KEY_ID_TP_FREQ_TP2_U4                     0x40050026
#define U_GNSS_CFG_VAL_KEY_ID_TP_FREQ_LOCK_TP2_U4                0x40050027
#define U_GNSS_CFG_VAL_KEY_ID_TP_LEN_TP2_U4                      0x4005000f
#define U_GNSS_CFG_VAL_KEY_ID_TP_LEN_LOCK_TP2_U4                 0x40050010
#define U_GNSS_CFG_VAL_KEY_ID_TP_DUTY_TP2_R8                     0x5005002c
#define U_GNSS_CFG_VAL_KEY_ID_TP_DUTY_LOCK_TP2_R8                0x5005002d
#define U_GNSS_CFG_VAL_KEY_ID_TP_USER_DELAY_TP2_I4               0x40050011
#define U_GNSS_CFG_VAL_KEY_ID_TP_TP2_ENA_L                       0x10050012
#define U_GNSS_CFG_VAL_KEY_ID_TP_SYNC_GNSS_TP2_L                 0x10050013
#define U_GNSS_CFG_VAL_KEY_ID_TP_USE_LOCKED_TP2_L                0x10050014
#define U_GNSS_CFG_VAL_KEY_ID_TP_ALIGN_TO_TOW_TP2_L              0x10050015
#define U_GNSS_CFG_VAL_KEY_ID_TP_POL_TP2_L                       0x10050016
#define U_GNSS_CFG_VAL_KEY_ID_TP_TIMEGRID_TP2_E1                 0x20050017
#define U_GNSS_CFG_VAL_KEY_ID_TP_DRSTR_TP2_E1                    0x20050036
#define U_GNSS_CFG_VAL_KEY_ID_TXREADY_ENABLED_L                  0x10a20001
#define U_GNSS_CFG_VAL_KEY_ID_TXREADY_POLARITY_L                 0x10a20002
#define U_GNSS_CFG_VAL_KEY_ID_TXREADY_PIN_U1                     0x20a20003
#define U_GNSS_CFG_VAL_KEY_ID_TXREADY_THRESHOLD_U2               0x30a20004
#define U_GNSS_CFG_VAL_KEY_ID_TXREADY_INTERFACE_E1               0x20a20005
#define U_GNSS_CFG_VAL_KEY_ID_UART1_BAUDRATE_U4                  0x40520001
#define U_GNSS_CFG_VAL_KEY_ID_UART1_STOPBITS_E1                  0x20520002
#define U_GNSS_CFG_VAL_KEY_ID_UART1_DATABITS_E1                  0x20520003
#define U_GNSS_CFG_VAL_KEY_ID_UART1_PARITY_E1                    0x20520004
#define U_GNSS_CFG_VAL_KEY_ID_UART1_ENABLED_L                    0x10520005
#define U_GNSS_CFG_VAL_KEY_ID_UART1INPROT_UBX_L                  0x10730001
#define U_GNSS_CFG_VAL_KEY_ID_UART1INPROT_NMEA_L                 0x10730002
#define U_GNSS_CFG_VAL_KEY_ID_UART1INPROT_RTCM3X_L               0x10730004
#define U_GNSS_CFG_VAL_KEY_ID_UART1INPROT_SPARTN_L               0x10730005
#define U_GNSS_CFG_VAL_KEY_ID_UART1OUTPROT_UBX_L                 0x10740001
#define U_GNSS_CFG_VAL_KEY_ID_UART1OUTPROT_NMEA_L                0x10740002
#define U_GNSS_CFG_VAL_KEY_ID_UART1OUTPROT_RTCM3X_L              0x10740004
#define U_GNSS_CFG_VAL_KEY_ID_UART2_BAUDRATE_U4                  0x40530001
#define U_GNSS_CFG_VAL_KEY_ID_UART2_STOPBITS_E1                  0x20530002
#define U_GNSS_CFG_VAL_KEY_ID_UART2_DATABITS_E1                  0x20530003
#define U_GNSS_CFG_VAL_KEY_ID_UART2_PARITY_E1                    0x20530004
#define U_GNSS_CFG_VAL_KEY_ID_UART2_ENABLED_L                    0x10530005
#define U_GNSS_CFG_VAL_KEY_ID_UART2INPROT_UBX_L                  0x10750001
#define U_GNSS_CFG_VAL_KEY_ID_UART2INPROT_NMEA_L                 0x10750002
#define U_GNSS_CFG_VAL_KEY_ID_UART2INPROT_RTCM3X_L               0x10750004
#define U_GNSS_CFG_VAL_KEY_ID_UART2INPROT_SPARTN_L               0x10750005
#define U_GNSS_CFG_VAL_KEY_ID_UART2OUTPROT_UBX_L                 0x10760001
#define U_GNSS_CFG_VAL_KEY_ID_UART2OUTPROT_NMEA_L                0x10760002
#define U_GNSS_CFG_VAL_KEY_ID_UART2OUTPROT_RTCM3X_L              0x10760004
#define U_GNSS_CFG_VAL_KEY_ID_USB_ENABLED_L                      0x10650001
#define U_GNSS_CFG_VAL_KEY_ID_USB_SELFPOW_L                      0x10650002
#define U_GNSS_CFG_VAL_KEY_ID_USB_VENDOR_ID_U2                   0x3065000a
#define U_GNSS_CFG_VAL_KEY_ID_USB_PRODUCT_ID_U2                  0x3065000b
#define U_GNSS_CFG_VAL_KEY_ID_USB_POWER_U2                       0x3065000c
#define U_GNSS_CFG_VAL_KEY_ID_USB_VENDOR_STR0_X8                 0x5065000d
#define U_GNSS_CFG_VAL_KEY_ID_USB_VENDOR_STR1_X8                 0x5065000e
#define U_GNSS_CFG_VAL_KEY_ID_USB_VENDOR_STR2_X8                 0x5065000f
#define U_GNSS_CFG_VAL_KEY_ID_USB_VENDOR_STR3_X8                 0x50650010
#define U_GNSS_CFG_VAL_KEY_ID_USB_PRODUCT_STR0_X8                0x50650011
#define U_GNSS_CFG_VAL_KEY_ID_USB_PRODUCT_STR1_X8                0x50650012
#define U_GNSS_CFG_VAL_KEY_ID_USB_PRODUCT_STR2_X8                0x50650013
#define U_GNSS_CFG_VAL_KEY_ID_USB_PRODUCT_STR3_X8                0x50650014
#define U_GNSS_CFG_VAL_KEY_ID_USB_SERIAL_NO_STR0_X8              0x50650015
#define U_GNSS_CFG_VAL_KEY_ID_USB_SERIAL_NO_STR1_X8              0x50650016
#define U_GNSS_CFG_VAL_KEY_ID_USB_SERIAL_NO_STR2_X8              0x50650017
#define U_GNSS_CFG_VAL_KEY_ID_USB_SERIAL_NO_STR3_X8              0x50650018
#define U_GNSS_CFG_VAL_KEY_ID_USBINPROT_UBX_L                    0x10770001
#define U_GNSS_CFG_VAL_KEY_ID_USBINPROT_NMEA_L                   0x10770002
#define U_GNSS_CFG_VAL_KEY_ID_USBINPROT_RTCM3X_L                 0x10770004
#define U_GNSS_CFG_VAL_KEY_ID_USBINPROT_SPARTN_L                 0x10770005
#define U_GNSS_CFG_VAL_KEY_ID_USBOUTPROT_UBX_L                   0x10780001
#define U_GNSS_CFG_VAL_KEY_ID_USBOUTPROT_NMEA_L                  0x10780002
#define U_GNSS_CFG_VAL_KEY_ID_USBOUTPROT_RTCM3X_L                0x10780004

// *** DO NOT MODIFY THIS LINE OR ABOVE: DO NOT MODIFY AREA ENDS ***

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_GNSS_CFG_VAL_KEY_H_

// End of file
