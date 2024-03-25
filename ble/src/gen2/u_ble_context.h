
// Ble specific state record to be held in device record
typedef struct {
    int32_t connHandle;
    int32_t mtu;
    uBleGapConnectCallback_t connectCallback;
    uBleGattNotificationCallback_t notifyCallback;
    uBleGattWriteCallback_t writeCallback;
    uBleGapPhyUpdateCallback_t phyUpdateCallback;
    uBleGapBondConfirmCallback_t confirmCb;
    uBleGapBondPasskeyRequestCallback_t passKeyRequestCb;
    uBleGapBondPasskeyEntryCallback_t passKeyEntryCb;
    uBleGapBondCompleteCallback_t bondCompleteCb;
    bool spsDataAvailable;
    int32_t spsConnHandle;
    char spsAddr[U_BD_STRING_MAX_LENGTH_BYTES];
} uBleDeviceState_t;

/** Convenience function for checking and if necessary creating a
 *  ble state record for a device
 *
 * @param pInstance  Corresponding device handle
 * @return zero on success or negative error code on failure.
 */
static inline int32_t checkCreateBleContext(uShortRangePrivateInstance_t *pInstance)
{
    if (pInstance == NULL) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
    if (pInstance->pBleContext == NULL) {
        pInstance->pBleContext = pUPortMalloc(sizeof(uBleDeviceState_t));
        memset(pInstance->pBleContext, 0, sizeof(uBleDeviceState_t));
    }
    return pInstance->pBleContext != NULL ?
           (int32_t)U_ERROR_COMMON_SUCCESS :
           (int32_t)U_ERROR_COMMON_NO_MEMORY;
}

static inline uBleDeviceState_t *pGetBleContext(uShortRangePrivateInstance_t *pInstance)
{
    return checkCreateBleContext(pInstance) == (int32_t)U_ERROR_COMMON_SUCCESS ?
           (uBleDeviceState_t *)pInstance->pBleContext : NULL;
}
