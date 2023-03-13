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

#ifndef _U_GNSS_CFG_PRIVATE_H_
#define _U_GNSS_CFG_PRIVATE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines a few configuration functions that
 * are needed in internal form inside the GNSS API.  These few
 * functions are made available this way in order to avoid dragging the
 * whole of the cfg part of the GNSS API into u_gnss_private.c.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Get the value of several configuration items at once; only
 * applicable to M9 modules and beyond, uses the UBX-CFG-VALGET
 * mechanism.
 *
 * IMPORTANT: this function allocates memory for the answer, it is
 * up to the caller to uPortFree(*list) when done.
 *
 * @param[in] pInstance  a pointer to the GNSS instance, cannot be NULL.
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
int32_t uGnssCfgPrivateValGetListAlloc(uGnssPrivateInstance_t *pInstance,
                                       const uint32_t *pKeyIdList,
                                       size_t numKeyIds,
                                       uGnssCfgVal_t **pList,
                                       uGnssCfgValLayer_t layer);

/** Set the value of several configuration items at once; only applicable
 * to M9 modules and beyond, uses the UBX-CFG-VALSET mechanism.
 *
 * Note: if you wish to set the current value of a small number of items
 * spread across different groups using keys from u_gnss_cfg_val_key.h you may
 * find it easier to use the macro #U_GNSS_CFG_SET_VAL_RAM multiple times;
 * this function comes into its own when setting values that have been read
 * using uGnssCfgValGetAlloc() or uGnssCfgValGetListAlloc(), e.g. with wildcards.
 *
 * @param[in] pInstance a pointer to the GNSS instance, cannot be NULL.
 * @param[in] pList     a pointer to an array defining one or more
 *                      values to set; must be NULL if numValues is 0.
 * @param numValues     the number of items in the array pointed-to by pList;
 *                      may be zero if the only purpose of this call is to
 *                      execute a transaction.
 * @param transaction   use #U_GNSS_CFG_VAL_TRANSACTION_NONE to set a single list
 *                      of values; if you wish to begin setting a sequence
 *                      of values (which can each be single values or lists) that
 *                      will be applied all at once in a later uGnssCfgValSet() /
 *                      uGnssCfgValSetList() call then use
 *                      #U_GNSS_CFG_VAL_TRANSACTION_BEGIN.  If this is part
 *                      of such a sequence use
 *                      #U_GNSS_CFG_VAL_TRANSACTION_CONTINUE.  If this is
 *                      the last in such a sequence and the values should
 *                      now be applied, use
 *                      #U_GNSS_CFG_VAL_TRANSACTION_EXECUTE.  Note that once
 *                      a "set" transaction has begun all of the set operations
 *                      must follow with #U_GNSS_CFG_VAL_TRANSACTION_CONTINUE and
 *                      then be executed - interleaving any other set/del operation,
 *                      or a del operation during a set transaction, will result in
 *                      the transaction being cancelled.
 * @param layers        the layers to set the values in, a bit-map of
 *                      #uGnssCfgValLayer_t values OR'ed together.  Use
 *                      #U_GNSS_CFG_VAL_LAYER_RAM to just set the current value
 *                      without persistent storage, otherwise you may choose to
 *                      OR-in battery-backed RAM or flash (where flash has been
 *                      connected to the GNSS chip); if you are using a transaction
 *                      then the set of layers used for ALL of the operations in
 *                      that transaction MUST be the same.
 * @return              zero on success else negative error code.
 */
int32_t uGnssCfgPrivateValSetList(uGnssPrivateInstance_t *pInstance,
                                  const uGnssCfgVal_t *pList,
                                  size_t numValues,
                                  uGnssCfgValTransaction_t transaction,
                                  int32_t layers);

/** Delete several configuration items at once; only applicable
 * to M9 modules and beyond, uses the UBX-CFG-VALDEL mechanism.
 * Note if you want to perform a deletion using an existing array of
 * uGnssCfgVal_t items then use uGnssCfgValDelListX() instead.
 *
 * @param[in] pInstance  a pointer to the GNSS instance, cannot be NULL.
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
 *                       #U_GNSS_CFG_VAL_TRANSACTION_EXECUTE.  Note that once
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
int32_t uGnssCfgPrivateValDelList(uGnssPrivateInstance_t *pInstance,
                                  const uint32_t *pKeyIdList,
                                  size_t numKeyIds,
                                  uGnssCfgValTransaction_t transaction,
                                  uint32_t layers);

#ifdef __cplusplus
}
#endif

#endif // _U_GNSS_CFG_PRIVATE_H_

// End of file
