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

#ifndef _U_GNSS_CFG_H_
#define _U_GNSS_CFG_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _GNSS
 *  @{
 */

/** @file
 * @brief This header file defines the GNSS APIs to configure a GNSS chip.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** A helper macro to set a single value without a transaction and
 * with less typing: if you are using one of the key IDs from
 * u_gnss_cfg_val_key.h, you may use this macro as follows:
 *
 * To set the key #U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PVT_I2C_U1
 * to 55 in RAM and BBRAM you would write:
 *
 * U_GNSS_CFG_SET_VAL(gnssHandle, MSGOUT_UBX_NAV_PVT_I2C_U1, 55, U_GNSS_CFG_VAL_LAYER_RAM | U_GNSS_CFG_VAL_LAYER_BBRAM)
 *
 * i.e. you can leave the U_GNSS_CFG_VAL_KEY_ID_ prefix off the key ID.
 */
#define U_GNSS_CFG_SET_VAL(gnssHandle, keyIdStripped, value, layers) \
        U_GNSS_CFG_SET_VAL_(gnssHandle, keyIdStripped, value, layers)

/** #U_GNSS_CFG_SET_VAL is required to macro-expand the value of
 * keyIdStripped; this macro does the work.
 */
#define U_GNSS_CFG_SET_VAL_(gnssHandle, keyIdStripped, value, layers)                \
        uGnssCfgValSet(gnssHandle, U_GNSS_CFG_VAL_KEY_ID_##keyIdStripped,  \
                       value, U_GNSS_CFG_VAL_TRANSACTION_NONE, layers)

/** As #U_GNSS_CFG_SET_VAL but sets the value only in RAM, for example
 * to set the key #U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PVT_I2C_U1
 * to 55 in RAM you would write:
 *
 * U_GNSS_CFG_SET_VAL_RAM(gnssHandle, MSGOUT_UBX_NAV_PVT_I2C_U1, 55)
 */
#define U_GNSS_CFG_SET_VAL_RAM(gnssHandle, keyIdStripped, value)  \
        U_GNSS_CFG_SET_VAL_RAM_(gnssHandle, keyIdStripped, value)

/** #U_GNSS_CFG_SET_VAL_RAM is required to macro-expand the value of
 * keyIdStripped; this macro does the work.
 */
#define U_GNSS_CFG_SET_VAL_RAM_(gnssHandle, keyIdStripped, value)            \
        uGnssCfgValSet(gnssHandle, U_GNSS_CFG_VAL_KEY_ID_##keyIdStripped,  \
                       value, U_GNSS_CFG_VAL_TRANSACTION_NONE,             \
                       U_GNSS_CFG_VAL_LAYER_RAM)

/** As #U_GNSS_CFG_SET_VAL_RAM but sets the value in RAM and BBRAM,
 * for example to set the key #U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PVT_I2C_U1
 * to 55 in RAM and BBRAM you would write:
 *
 * U_GNSS_CFG_SET_VAL_BBRAM(gnssHandle, MSGOUT_UBX_NAV_PVT_I2C_U1, 55)
 */
#define U_GNSS_CFG_SET_VAL_BBRAM(gnssHandle, keyIdStripped, value)  \
        U_GNSS_CFG_SET_VAL_BBRAM_(gnssHandle, keyIdStripped, value)

/** #U_GNSS_CFG_SET_VAL_BBRAM is required to macro-expand the value of
 * keyIdStripped; this macro does the work.
 */
#define U_GNSS_CFG_SET_VAL_BBRAM_(gnssHandle, keyIdStripped, value)        \
        uGnssCfgValSet(gnssHandle, U_GNSS_CFG_VAL_KEY_ID_##keyIdStripped,  \
                       value, U_GNSS_CFG_VAL_TRANSACTION_NONE,             \
                       U_GNSS_CFG_VAL_LAYER_RAM | U_GNSS_CFG_VAL_LAYER_BBRAM)

/** As #U_GNSS_CFG_SET_VAL_BBRAM but sets the value in RAM, BBRAM and flash,
 * for example to set the key #U_GNSS_CFG_VAL_KEY_ID_MSGOUT_UBX_NAV_PVT_I2C_U1
 * to 55 in RAM, BBRM and flash you would write:
 *
 * U_GNSS_CFG_SET_VAL_ALL(gnssHandle, MSGOUT_UBX_NAV_PVT_I2C_U1, 55)
 */
#define U_GNSS_CFG_SET_VAL_ALL(gnssHandle, keyIdStripped, value)  \
        U_GNSS_CFG_SET_VAL_ALL_(gnssHandle, keyIdStripped, value)

/** #U_GNSS_CFG_SET_VAL_ALL is required to macro-expand the value of
 * keyIdStripped; this macro does the work.
 */
#define U_GNSS_CFG_SET_VAL_ALL_(gnssHandle, keyIdStripped, value)          \
        uGnssCfgValSet(gnssHandle, U_GNSS_CFG_VAL_KEY_ID_##keyIdStripped,  \
                       value, U_GNSS_CFG_VAL_TRANSACTION_NONE,             \
                       U_GNSS_CFG_VAL_LAYER_RAM   |                        \
                       U_GNSS_CFG_VAL_LAYER_BBRAM |                        \
                       U_GNSS_CFG_VAL_LAYER_FLASH)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure to hold a value to get or set using uGnssCfgValGetListAlloc() /
 * uGnssCfgValSetList() / uGnssCfgValDelListX().
 */
typedef struct {
    uint32_t keyId;   /**< the ID of the key to get/set/del; may be found in
                           the u-blox GNSS reference manual or you may use
                           the macros defined in u_gnss_cfg_val_key.h;
                           for instance, key ID CFG-ANA-USE_ANA would be
                           #U_GNSS_CFG_VAL_KEY_ID_ANA_USE_ANA_L (i.e. prefix
                           with U_GNSS_CFG_VAL_KEY_ID_, drop the CFG, replace
                           any dashes with underscores and add the type on
                           the end (just so it sticks in your mind)). */
    uint64_t value;   /**< the value, of size defined by the keyId. */
} uGnssCfgVal_t;

/** The state of a transaction used when setting/deleting values with the
 * VALSET/VALDEL mechanism, values chosen to match those encoded in the
 * UBX-CFG-VALXXX messages.
 */
typedef enum {
    U_GNSS_CFG_VAL_TRANSACTION_NONE     = 0, /**< no transaction, just a single set/del;
                                                  if a transaction was previously in
                                                  progress, i.e. had not been executed,
                                                  this will CANCEL it. */
    U_GNSS_CFG_VAL_TRANSACTION_BEGIN    = 1, /**< marks the first in a sequence of
                                                  set/del operations which will be stored
                                                  inside the GNSS chip and only executed
                                                  when #U_GNSS_CFG_VAL_TRANSACTION_EXCUTE
                                                  is set; if a transaction was previously
                                                  in progress, i.e. had not been executed,
                                                  this will CANCEL it. */
    U_GNSS_CFG_VAL_TRANSACTION_CONTINUE = 2, /**< this set/del operation is part of an
                                                  existing transaction; if no transaction
                                                  is in progress this will generate an
                                                  error. */
    U_GNSS_CFG_VAL_TRANSACTION_EXCUTE   = 3, /**< perform the set/del operations in the
                                                  transaction; at this point error checking
                                                  will be carried out on all of the set/del
                                                  operations if any of them write to RAM. */
    U_GNSS_CFG_VAL_TRANSACTION_MAX_NUM
} uGnssCfgValTransaction_t;

/** The layers to which a VALGET/VALSET/VALDEL operation can be applied, chosen
 * so that they can be used directly in the bitmap to the VALSET/VALDEL operations.
 */
typedef enum {
    U_GNSS_CFG_VAL_LAYER_NONE    = 0x00,
    U_GNSS_CFG_VAL_LAYER_RAM     = 0x01,  /**< the currently active value, stored non-persistently
                                               in RAM. */
    U_GNSS_CFG_VAL_LAYER_BBRAM   = 0x02,  /**< the value stored in battery-backed RAM. */
    U_GNSS_CFG_VAL_LAYER_FLASH   = 0x04,  /**< the value stored in external configuration flash
                                               connected to the GNSS chip. */
    U_GNSS_CFG_VAL_LAYER_DEFAULT = 0x07,  /**< the default value; cannot be set or deleted. */
    U_GNSS_CFG_VAL_LAYER_MAX_NUM
} uGnssCfgValLayer_t;

/* ----------------------------------------------------------------
 * FUNCTIONS: SPECIFIC CONFIGURATION FUNCTIONS
 * -------------------------------------------------------------- */

/** Get the dynamic platform model from the GNSS chip.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            the number of the dynamic platform model or
 *                    negative error code.
 */
int32_t uGnssCfgGetDynamic(uDeviceHandle_t gnssHandle);

/** Set the dynamic platform model of the GNSS chip.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param dynamic     the number of the dynamic platform model; the
 *                    value is deliberately not range-checked to allow
 *                    future dynamic platform models to be passed
 *                    in without the requirement to modify this code.
 * @return            zero on succes or negative error code.
 */
int32_t uGnssCfgSetDynamic(uDeviceHandle_t gnssHandle, uGnssDynamic_t dynamic);

/** Get the fix mode from the GNSS chip.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            the fix mode or negative error code.
 */
int32_t uGnssCfgGetFixMode(uDeviceHandle_t gnssHandle);

/** Set the fix mode of the GNSS chip.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param fixMode     the fix mode; the value is deliberately not
 *                    range-checked to allow future fix modes to be
 *                    passed in without the requirement to modify
 *                    this code.
 * @return            zero on succes or negative error code.
 */
int32_t uGnssCfgSetFixMode(uDeviceHandle_t gnssHandle, uGnssFixMode_t fixMode);

/** Get the UTC standard from the GNSS chip.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @return            the UTC standard or negative error code.
 */
int32_t uGnssCfgGetUtcStandard(uDeviceHandle_t gnssHandle);

/** Set the UTC standard of the GNSS chip.
 *
 * @param gnssHandle  the handle of the GNSS instance.
 * @param utcStandard the UTC standard; the value is deliberately not
 *                    range-checked to allow future UTC standards to be
 *                    passed in without the requirement to modify
 *                    this code.  Use #U_GNSS_UTC_STANDARD_AUTOMATIC
 *                    it you don't really care, you'd just like UTC
 *                    time please (which is the default).
 * @return            zero on succes or negative error code.
 */
int32_t uGnssCfgSetUtcStandard(uDeviceHandle_t gnssHandle,
                               uGnssUtcStandard_t utcStandard);

/** Get the protocol types output by the GNSS chip; not relevant
 * where an AT transport is in use since only the UBX protocol is
 * currently supported through that transport.
 *
 * @param gnssHandle the handle of the GNSS instance.
 * @return           a bit-map of the protocol types that are
 *                   being output else negative error code.
 */
int32_t uGnssCfgGetProtocolOut(uDeviceHandle_t gnssHandle);

/** Set the protocol type output by the GNSS chip; not relevant
 * where an AT transport is in use since only the UBX protocol is
 * currently supported through that transport.
 *
 * @param gnssHandle the handle of the GNSS instance.
 * @param protocol   the protocol type; #U_GNSS_PROTOCOL_ALL may
 *                   be used to enable all of the output protocols
 *                   supported by the GNSS chip (though using this
 *                   with onNotOff set to false will return an error).
 *                   UBX protocol output cannot be switched off
 *                   since it is used by this code. The range of
 *                   the parameter is NOT checked, hence you may set
 *                   a value which is known to the GNSS chip but not
 *                   to this code.
 * @param onNotOff   whether the given protocol should be on or off.
 * @return           zero on succes or negative error code.
 */
int32_t uGnssCfgSetProtocolOut(uDeviceHandle_t gnssHandle,
                               uGnssProtocol_t protocol,
                               bool onNotOff);

/* ----------------------------------------------------------------
 * FUNCTIONS: GENERIC CONFIGURATION USING VALGET/VALSET/VALDEL, FROM M9
 * -------------------------------------------------------------- */

/** Get the value of a single configuration item; only applicable
 * to M9 modules and beyond, uses the UBX-CFG-VALGET mechanism.
 *
 * Note: keyId is not permitted to contain wild-cards, for that see
 * uGnssCfgValGetAlloc().
 *
 * @param gnssHandle   the handle of the GNSS instance.
 * @param keyId        the ID of the key to get; may be found in the
 *                     u-blox GNSS reference manual or you may use
 *                     the macros defined in u_gnss_cfg_val_key.h;
 *                     for instance, key ID CFG-ANA-USE_ANA would be
 *                     #U_GNSS_CFG_VAL_KEY_ID_ANA_USE_ANA_L (i.e. prefix
 *                     with U_GNSS_CFG_VAL_KEY_ID_, drop the CFG, replace
 *                     any dashes with underscores and add the type on
 *                     the end (just so it sticks in your mind)).
 *                     Wild-cards are NOT permitted: please use
 *                     uGnssCfgValGetAlloc() if you want to use
 *                     wild-cards.
 * @param[out] pValue  a pointer to a place to put the value.  If
 *                     there is unsufficient room at pValue to store
 *                     what is received from the GNSS chip an error
 *                     will be returned.
 * @param size         the number of bytes of storage at pValue.
 * @param layer        the layer to get the value from: use
 *                     #U_GNSS_CFG_VAL_LAYER_RAM to get the currently
 *                     applied value.
 * @return             zero on success else negative error code.
 */
int32_t uGnssCfgValGet(uDeviceHandle_t gnssHandle, uint32_t keyId,
                       void *pValue, size_t size,
                       uGnssCfgValLayer_t layer);

/** Get the value of a configuration item; only applicable
 * to M9 modules and beyond, uses the UBX-CFG-VALGET mechanism.
 *
 * IMPORTANT: this function allocates memory for the answer, it is
 * up to the caller to uPortFree(*list) when done.
 *
 * @param gnssHandle    the handle of the GNSS instance.
 * @param keyId         the ID of the key to get; may be found in the
 *                      u-blox GNSS reference manual or you may use
 *                      the macros defined in u_gnss_cfg_val_key.h;
 *                      for instance, key ID CFG-ANA-USE_ANA would be
 *                      #U_GNSS_CFG_VAL_KEY_ID_ANA_USE_ANA_L (i.e. prefix
 *                      with U_GNSS_CFG_VAL_KEY_ID_, drop the CFG, replace
 *                      any dashes with underscores and add the type on
 *                      the end (just so it sticks in your mind)).
 *                      Wild-cards are permitted: you may, for instance
 *                      construct a keyId using the #U_GNSS_CFG_VAL_KEY
 *                      macro with the group ID set to
 *                      #U_GNSS_CFG_VAL_KEY_GROUP_ID_ALL, which would
 *                      return absolutely everything (if you have
 *                      enough memory for it) or, more optimally,
 *                      #U_GNSS_CFG_VAL_KEY_ITEM_ID_ALL to fetch
 *                      all the items for a given group ID.
 * @param[out] pList    a pointer to a place to put an array
 *                      containing the values; cannot be NULL.
 *                      Note that though this is double-indirected
 *                      a single "p" is used in the variable name in
 *                      order to encourage the list to be treated as
 *                      an array.  If this function returns success
 *                      it is UP TO THE CALLER to uPortFree(*list) when done.
 * @param layer         the layer to get the values from: use
 *                      #U_GNSS_CFG_VAL_LAYER_RAM to get the currently
 *                      applied values.
 * @return              on success the number of items in pList,
 *                      else negative error code.
 */
int32_t uGnssCfgValGetAlloc(uDeviceHandle_t gnssHandle,
                            uint32_t keyId,
                            uGnssCfgVal_t **pList,
                            uGnssCfgValLayer_t layer);

/** Get the value of several configuration items at once; only
 * applicable to M9 modules and beyond, uses the UBX-CFG-VALGET
 * mechanism.
 *
 * IMPORTANT: this function allocates memory for the answer, it is
 * up to the caller to uPortFree(*list) when done.
 *
 * @param gnssHandle     the handle of the GNSS instance.
 * @param[in] pKeyIdList a pointer to an array of key IDs to get;
 *                       cannot be NULL.  Wild-cards may be included
 *                       in any of the entries in the list.
 * @param numKeyIds      the number of items in the array pointed-to
 *                       by pKeyIdList.
 * @param[out] pList     a pointer to a place to put an array
 *                       containing the values; cannot be NULL.
 *                       Note that though this is double-indirected
 *                       a single "p" is used in the variable name in
 *                       order to encourage the list to be treated as
 *                       an array.  If this function returns success
 *                       it is UP TO THE CALLER to uPortFree(*list) when done.
 * @param layer          the layer to get the values from: use
 *                       #U_GNSS_CFG_VAL_LAYER_RAM to get the currently
 *                       applied values.
 * @return               on success the number of items in pList,
 *                       else negative error code.
 */
int32_t uGnssCfgValGetListAlloc(uDeviceHandle_t gnssHandle,
                                const uint32_t *pKeyIdList, size_t numKeyIds,
                                uGnssCfgVal_t **pList,
                                uGnssCfgValLayer_t layer);

/** Set the value of a configuration item; only applicable to M9
 * modules and beyond, using the UBX-CFG-VALSET mechanism.
 *
 * Note: to set the current value of an item using one of the keys
 * from u_gnss_cfg_val_key.h you may find it easier to use the macro
 * #U_GNSS_CFG_SET_VAL_RAM.
 *
 * @param gnssHandle   the handle of the GNSS instance.
 * @param keyId        the ID of the key to set; may be found in the
 *                     u-blox GNSS reference manual or you may use
 *                     the macros defined in u_gnss_cfg_val_key.h;
 *                     for instance, key ID CFG-ANA-USE_ANA would be
 *                     #U_GNSS_CFG_VAL_KEY_ID_ANA_USE_ANA_L (i.e. prefix
 *                     with U_GNSS_CFG_VAL_KEY_ID_, drop the CFG, replace
 *                     any dashes with underscores and add the type on
 *                     the end (just so it sticks in your mind)).
 *                     IMPORTANT: keyId defines the size of the value
 *                     (up to 8 bytes), it is up to you to get this
 *                     right.
 * @param value        the value to set, of size defined by keyId.
 * @param transaction  use #U_GNSS_CFG_VAL_TRANSACTION_NONE to set a single
 *                     value; if you wish to begin setting a sequence
 *                     of values that will be applied all at once in a later
 *                     uGnssCfgValSet() / uGnssCfgValSetList() call then use
 *                     #U_GNSS_CFG_VAL_TRANSACTION_BEGIN.  If this is part
 *                     of such a sequence use
 *                     #U_GNSS_CFG_VAL_TRANSACTION_CONTINUE.  If this is
 *                     the last in such a sequence and the values should
 *                     now be applied, use
 *                     #U_GNSS_CFG_VAL_TRANSACTION_EXCUTE.  Note that once
 *                     a "set" transaction has begun all of the set operations
 *                     must follow with #U_GNSS_CFG_VAL_TRANSACTION_CONTINUE
 *                     and then be executed - interleaving any other set/del
 *                     operation, or a del operation during a set transaction,
 *                     will result in the transaction being cancelled.  If you
 *                     don't want to set a value but just execute a "set"
 *                     transaction then call uGnssCfgValSetList() with no
 *                     items and #U_GNSS_CFG_VAL_TRANSACTION_EXCUTE.
 * @param layers       the layers to set the value in, a bit-map of
 *                     #uGnssCfgValLayer_t values OR'ed together.  Use
 *                     #U_GNSS_CFG_VAL_LAYER_RAM to just set the current value
 *                     without persistent storage, otherwise you may choose to
 *                     OR-in battery-backed RAM or flash (where flash has been
 *                     connected to the GNSS chip); if you are using a transaction
 *                     then the set of layers used for ALL of the operations
 *                     in the transaction MUST be the same.
 * @return             zero on success else negative error code.
 */
int32_t uGnssCfgValSet(uDeviceHandle_t gnssHandle,
                       uint32_t keyId, uint64_t value,
                       uGnssCfgValTransaction_t transaction,
                       uint32_t layers);

/** Set the value of several configuration items at once; only applicable
 * to M9 modules and beyond, uses the UBX-CFG-VALSET mechanism.
 *
 * Note: if you wish to set the current value of a small number of items
 * spread across different groups using keys from u_gnss_cfg_val_key.h you may
 * find it easier to use the macro #U_GNSS_CFG_SET_VAL_RAM multiple times;
 * this function comes into its own when setting values that have been read
 * using uGnssCfgValGetAlloc() or uGnssCfgValGetListAlloc(), e.g. with wildcards.
 *
 * @param gnssHandle   the handle of the GNSS instance.
 * @param[in] pList    a pointer to an array defining one or more
 *                     values to set; must be NULL if numValues is 0.
 * @param numValues    the number of items in the array pointed-to by pList;
 *                     may be zero if the only purpose of this call is to
 *                     execute a transaction.
 * @param transaction  use #U_GNSS_CFG_VAL_TRANSACTION_NONE to set a single list
 *                     of values; if you wish to begin setting a sequence
 *                     of values (which can each be single values or lists) that
 *                     will be applied all at once in a later uGnssCfgValSet() /
 *                     uGnssCfgValSetList() call then use
 *                     #U_GNSS_CFG_VAL_TRANSACTION_BEGIN.  If this is part
 *                     of such a sequence use
 *                     #U_GNSS_CFG_VAL_TRANSACTION_CONTINUE.  If this is
 *                     the last in such a sequence and the values should
 *                     now be applied, use
 *                     #U_GNSS_CFG_VAL_TRANSACTION_EXCUTE.  Note that once
 *                     a "set" transaction has begun all of the set operations
 *                     must follow with #U_GNSS_CFG_VAL_TRANSACTION_CONTINUE and
 *                     then be executed - interleaving any other set/del operation,
 *                     or a del operation during a set transaction, will result in
 *                     the transaction being cancelled.
 * @param layers       the layers to set the values in, a bit-map of
 *                     #uGnssCfgValLayer_t values OR'ed together.  Use
 *                     #U_GNSS_CFG_VAL_LAYER_RAM to just set the current value
 *                     without persistent storage, otherwise you may choose to
 *                     OR-in battery-backed RAM or flash (where flash has been
 *                     connected to the GNSS chip); if you are using a transaction
 *                     then the set of layers used for ALL of the operations in
 *                     that transaction MUST be the same.
 * @return             zero on success else negative error code.
 */
int32_t uGnssCfgValSetList(uDeviceHandle_t gnssHandle,
                           const uGnssCfgVal_t *pList, size_t numValues,
                           uGnssCfgValTransaction_t transaction,
                           uint32_t layers);

/** Delete a configuration item; only applicable to M9 modules
 * and beyond, using the UBX-CFG-VALDEL mechanism.
 *
 * @param gnssHandle   the handle of the GNSS instance.
 * @param keyId        the ID of the key to be deleted; may be found in the
 *                     u-blox GNSS reference manual or you may use the macros
 *                     defined in u_gnss_cfg_val_key.h; for instance, key ID
 *                     CFG-ANA-USE_ANA would be #U_GNSS_CFG_VAL_KEY_ID_ANA_USE_ANA_L
 *                     (i.e. prefix with U_GNSS_CFG_VAL_KEY_ID_, drop the CFG,
 *                     replace any dashes with underscores and add the type on
 *                     the end (just so it sticks in your mind)).
 *                     Wild-cards are permitted: you may, for instance
 *                     construct a keyId using the #U_GNSS_CFG_VAL_KEY
 *                     macro with the group ID set to
 *                     #U_GNSS_CFG_VAL_KEY_GROUP_ID_ALL, which would
 *                     delete absolutely everything, or you could use
 *                     #U_GNSS_CFG_VAL_KEY_ITEM_ID_ALL to delete
 *                     all the items for a given group ID.
 * @param transaction  use #U_GNSS_CFG_VAL_TRANSACTION_NONE to delete a single
 *                     value; if you wish to begin deleting a sequence
 *                     of values (which can each be single values or lists) that
 *                     will be applied all at once in a later uGnssCfgValSet() /
 *                     uGnssCfgValSetList() call then use
 *                     #U_GNSS_CFG_VAL_TRANSACTION_BEGIN.  If this is part
 *                     of such a sequence use
 *                     #U_GNSS_CFG_VAL_TRANSACTION_CONTINUE.  If this is
 *                     the last in such a sequence and the values should
 *                     now be applied, use
 *                     #U_GNSS_CFG_VAL_TRANSACTION_EXCUTE.  Note that once
 *                     a "del" transaction has begun all of the del operations
 *                     must follow with #U_GNSS_CFG_VAL_TRANSACTION_CONTINUE and
 *                     then be executed - interleaving any other set/del operation,
 *                     or a set operation during a del transaction, will result in
 *                     the transaction being cancelled.  If you don't want to set
*                      a value but just execute a "del" transaction then call
 *                     uGnssCfgValDelList() / uGnssCfgValDelListX() with no items
 *                     and #U_GNSS_CFG_VAL_TRANSACTION_EXCUTE.
 * @param layers       the layers to delete the value from, a bit-map of
 *                     #uGnssCfgValLayer_t values OR'ed together.  Only
 *                     #U_GNSS_CFG_VAL_LAYER_BBRAM and #U_GNSS_CFG_VAL_LAYER_FLASH
 *                     (where flash has been connected to the GNSS chip) are
 *                     permitted.  If you are using a transaction then the set
 *                     of layers used for ALL of the operations in that
 *                     transaction MUST be the same.
 * @return             zero on success else negative error code.
 */
int32_t uGnssCfgValDel(uDeviceHandle_t gnssHandle, uint32_t keyId,
                       uGnssCfgValTransaction_t transaction,
                       uint32_t layers);

/** Delete several configuration items at once; only applicable
 * to M9 modules and beyond, uses the UBX-CFG-VALDEL mechanism.
 * Note if you want to perform a deletion using an existing array of
 * uGnssCfgVal_t items then use uGnssCfgValDelListX() instead.
 *
 * @param gnssHandle     the handle of the GNSS instance.
 * @param[in] pKeyIdList a pointer to an array of key IDs to delete; must be
 *                       NULL if numKeyIds is 0. Wild-cards are permitted.
 * @param numKeyIds      the number of items in the array pointed-to by pKeyIdList;
 *                       may be zero if the only purpose of this call is to
 *                       execute a transaction.
 * @param transaction    use #U_GNSS_CFG_VAL_TRANSACTION_NONE to delete a single
 *                       list of values; if you wish to begin deleting a sequence
 *                       of values (which can each be single values or lists) that
 *                       will be applied all at once in a later uGnssCfgValDel() /
 *                       uGnssCfgValDelList() / uGnssCfgValDelListX() call then use
 *                       #U_GNSS_CFG_VAL_TRANSACTION_BEGIN.  If this is part
 *                       of such a sequence use
 *                       #U_GNSS_CFG_VAL_TRANSACTION_CONTINUE.  If this is
 *                       the last in such a sequence and the values should
 *                       now be applied, use
 *                       #U_GNSS_CFG_VAL_TRANSACTION_EXCUTE.  Note that once
 *                       a "del" transaction has begun all of the del operations
 *                       must follow with #U_GNSS_CFG_VAL_TRANSACTION_CONTINUE and
 *                       then be executed - interleaving any other set/del operation,
 *                       or a set operation during a del transaction, will result in
 *                       the transaction being cancelled.
 * @param layers         the layers to delete the values from, a bit-map of
 *                       #uGnssCfgValLayer_t values OR'ed together.  Only
 *                       #U_GNSS_CFG_VAL_LAYER_BBRAM and #U_GNSS_CFG_VAL_LAYER_FLASH
 *                       (where flash has been connected to the GNSS chip) are
 *                       permitted.  If you are using a transaction
 *                       then the set of layers used for ALL of the operations in
 *                       that transaction MUST be the same.
 * @return               zero on success else negative error code.
 */
int32_t uGnssCfgValDelList(uDeviceHandle_t gnssHandle,
                           const uint32_t *pKeyIdList, size_t numKeyIds,
                           uGnssCfgValTransaction_t transaction,
                           uint32_t layers);

/** As uGnssCfgValDelList() but takes an array of type uGnssCfgVal_t as a
 * parameter rather than an array of uint32_t keys, allowing the same array to
 * be used for deletion as was used for uGnssCfgValSetList(); only applicable
 * to M9 modules and beyond, uses the UBX-CFG-VALDEL mechanism.
 *
 * @param gnssHandle   the handle of the GNSS instance.
 * @param[in] pList    a pointer to an array defining one or more
 *                     values to delete; only the keyId member of each
 *                     item is relevant, the others are ignored; must
 *                     be NULL if numValues is 0.  Wild-cards are permitted.
 * @param numValues    the number of items in the array pointed-to by pList;
 *                     may be zero if the only purpose of this call is to
 *                     execute a transaction.
 * @param transaction  use #U_GNSS_CFG_VAL_TRANSACTION_NONE to delete a single
 *                     list of values; if you wish to begin deleting a sequence
 *                     of values (which can each be single values or lists) that
 *                     will be applied all at once in a later uGnssCfgValDel() /
 *                     uGnssCfgValDelList() / uGnssCfgValDelListX() call then use
 *                     #U_GNSS_CFG_VAL_TRANSACTION_BEGIN.  If this is part
 *                     of such a sequence use
 *                     #U_GNSS_CFG_VAL_TRANSACTION_CONTINUE.  If this is
 *                     the last in such a sequence and the values should
 *                     now be applied, use
 *                     #U_GNSS_CFG_VAL_TRANSACTION_EXCUTE.  Note that once
 *                     a "del" transaction has begun all of the del operations
 *                     must follow with #U_GNSS_CFG_VAL_TRANSACTION_CONTINUE and
 *                     then be executed - interleaving any other set/del operation,
 *                     or a set operation during a del transaction, will result in
 *                     the transaction being cancelled.
 * @param layers       the layers to delete the values from, a bit-map of
 *                     #uGnssCfgValLayer_t values OR'ed together.  Use
 *                     #U_GNSS_CFG_VAL_LAYER_RAM to just delete a current
 *                     set of values which you may have previously overridden
 *                     persistent values with, otherwise you may choose to
 *                     OR-in battery-backed RAM or flash (where flash has been
 *                     connected to the GNSS chip); if you are using a transaction
 *                     then the set of layers used for ALL of the operations in
 *                     that transaction MUST be the same.
 * @return             zero on success else negative error code.
 */
int32_t uGnssCfgValDelListX(uDeviceHandle_t gnssHandle,
                            const uGnssCfgVal_t *pList, size_t numValues,
                            uGnssCfgValTransaction_t transaction,
                            uint32_t layers);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_GNSS_CFG_H_

// End of file
