/*
 * Copyright 2019-2024 u-blox
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

/** @file
 * @brief This file makes a connection from the bottom of ESP NETIF, i.e.
 * the bottom of the IP stack inside ESP-IDF, to a PPP interface inside
 * ubxlib.  Such a PPP interface is provided by a cellular module.
 *
 * It is only compiled if CONFIG_LWIP_PPP_SUPPORT is set in your
 * sdkconfig.h and U_CFG_PPP_ENABLE is defined.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()

#include "u_cfg_sw.h"
#include "u_error_common.h"

#include "u_linked_list.h"

#include "u_sock.h" // uSockStringToAddress()

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_ppp.h"
#include "u_port_debug.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Bring in the ESP-IDF CONFIG_ #defines
#include "sdkconfig.h"

#ifdef CONFIG_LWIP_PPP_SUPPORT
#include "esp_event.h"
#include "esp_netif_ip_addr.h"
#include "esp_netif.h"
#include "esp_netif_defaults.h"
#include "esp_netif_ppp.h"
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_PPP_TX_LOOP_GUARD
/** How many times around the transmit loop to allow if stuff
 * won't send.
 */
# define U_PORT_PPP_TX_LOOP_GUARD 100
#endif

#ifndef U_PORT_PPP_TX_LOOP_DELAY_MS
/** How long to wait between transmit attempts in milliseconds
 * when the data to transmit won't go all at once.
 */
# define U_PORT_PPP_TX_LOOP_DELAY_MS 10
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

#if defined(CONFIG_LWIP_PPP_SUPPORT) && defined(U_CFG_PPP_ENABLE)

// Forward declaration.
struct uPortPppInterface_t;

/** Define a NETIF driver for ESP-IDF, used to provide a PPP
 * connection to the bottom of the ESP-IDF IP stack.
 */
typedef struct {
    esp_netif_driver_base_t base;
    struct uPortPppInterface_t *pPppInterface;
    uSockIpAddress_t *pIpAddress;
    uSockIpAddress_t *pDnsIpAddressPrimary;
    const char *pUsername;
    const char *pPassword;
    uPortPppAuthenticationMode_t authenticationMode;
} uPortPppNetifDriver_t;

/** Define a PPP interface.
 */
typedef struct uPortPppInterface_t {
    void *pDevHandle;
    uPortSemaphoreHandle_t semaphoreExit; /**< This is created set to
                                               0 when the interface is
                                               created and is given
                                               when the eventPppChanged()
                                               is informed that the PPP
                                               interface has been taken
                                               down by the attached IP
                                               stack. */
    uPortPppConnectCallback_t *pConnectCallback;
    uPortPppDisconnectCallback_t *pDisconnectCallback;
    uPortPppTransmitCallback_t *pTransmitCallback;
    bool pppRunning;
    bool ipConnected;
    uPortPppNetifDriver_t netifDriver;
} uPortPppInterface_t;

#endif // #if defined(CONFIG_LWIP_PPP_SUPPORT) && defined(U_CFG_PPP_ENABLE)

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

#if defined(CONFIG_LWIP_PPP_SUPPORT) && defined(U_CFG_PPP_ENABLE)

/** Root of the linked list of PPP entities.
 */
static uLinkedList_t *gpPppInterfaceList = NULL; /**< A linked list of uPortPppInterface_t. */

/** Mutex to protect the linked list of PPP entities.
 */
static uPortMutexHandle_t gMutex = NULL;

#endif // #if defined(CONFIG_LWIP_PPP_SUPPORT) && defined(U_CFG_PPP_ENABLE)

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

#if defined(CONFIG_LWIP_PPP_SUPPORT) && defined(U_CFG_PPP_ENABLE)

// Find the PPP interface structure for the given handle.
static uPortPppInterface_t *pFindPppInterface(void *pDevHandle)
{
    uPortPppInterface_t *pPppInterface = NULL;
    uLinkedList_t *pList;

    pList = gpPppInterfaceList;
    while ((pList != NULL) && (pPppInterface == NULL)) {
        if (((uPortPppInterface_t *) pList->p)->pDevHandle == pDevHandle) {
            pPppInterface = (uPortPppInterface_t *) pList->p;
        } else {
            pList = pList->pNext;
        }
    }

    return pPppInterface;
}

/** Convert an IP address of ours to ESP-IDF format.
 */
static int32_t convertIpAddress(uSockIpAddress_t *pIn, esp_ip_addr_t *pOut)
{
    int32_t espError = ESP_ERR_INVALID_ARG;

    memset(pOut, 0, sizeof(*pOut));
    switch (pIn->type) {
        case U_SOCK_ADDRESS_TYPE_V4:
            pOut->type = ESP_IPADDR_TYPE_V4;
            pOut->u_addr.ip4.addr = ESP_IP4TOADDR(((pIn->address.ipv4 >> 24) && 0xFF),
                                                  ((pIn->address.ipv4 >> 16) && 0xFF),
                                                  ((pIn->address.ipv4 >> 8) && 0xFF),
                                                  (pIn->address.ipv4 && 0xFF));
            espError = ESP_OK;
            break;
        case U_SOCK_ADDRESS_TYPE_V6:
            pOut->type = ESP_IPADDR_TYPE_V6;
            for (size_t x = 0; x < sizeof(pOut->u_addr.ip6.addr) / sizeof(pOut->u_addr.ip6.addr[0]); x++) {
                pOut->u_addr.ip6.addr[x] = ESP_IP4TOADDR(((pIn->address.ipv6[x] >> 24) && 0xFF),
                                                         ((pIn->address.ipv6[x] >> 16) && 0xFF),
                                                         ((pIn->address.ipv6[x] >> 8) && 0xFF),
                                                         (pIn->address.ipv6[x] && 0xFF));
            }
            espError = ESP_OK;
            break;
        default:
            break;
    }

    return espError;
}

// Switch off DHCP and tell the IP stack what our IP address is.
static esp_err_t setIpAddress(esp_netif_t *pEspNetif, uSockIpAddress_t *pIpAddress)
{
    esp_err_t espError = ESP_ERR_INVALID_ARG;
    esp_ip_addr_t espIpAddress = {0};
    esp_netif_ip_info_t ipInfo = {0};

    switch (pIpAddress->type) {
        case U_SOCK_ADDRESS_TYPE_V4:
            ipInfo.netmask.addr = ESP_IP4TOADDR(0xFF, 0xFF, 0xFF, 0);
            // TODO ipInfo.gw
            espError = convertIpAddress(pIpAddress, &espIpAddress);
            break;
        case U_SOCK_ADDRESS_TYPE_V6:
            espError = ESP_ERR_NOT_SUPPORTED;
            break;
        default:
            break;
    }
    if (espError == ESP_OK) {
        memcpy(&ipInfo.ip, &espIpAddress.u_addr.ip4, sizeof(ipInfo.ip));
        espError = esp_netif_dhcpc_stop(pEspNetif);
        if (espError == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
            espError = ESP_OK;
        }
        if (espError == ESP_OK) {
            espError = esp_netif_set_ip_info(pEspNetif, &ipInfo);
        }
    }

    return espError;
}

// Set a DNS address.
static esp_err_t setDnsAddress(esp_netif_t *pEspNetif, esp_netif_dns_type_t type,
                               uSockIpAddress_t *pIpAddress)
{
    esp_err_t espError = ESP_ERR_INVALID_ARG;
    esp_netif_dns_info_t dnsInfo = {0};

    switch (pIpAddress->type) {
        case U_SOCK_ADDRESS_TYPE_V4:
            espError = convertIpAddress(pIpAddress, &dnsInfo.ip);
            break;
        case U_SOCK_ADDRESS_TYPE_V6:
            espError = ESP_ERR_NOT_SUPPORTED;
            break;
        default:
            break;
    }
    if (espError == ESP_OK) {
        espError = esp_netif_set_dns_info(pEspNetif, type, &dnsInfo);
    }

    return espError;
}

// This function is provided as a callback to the NETIF layer of
// ESP-IDF in the structure esp_netif_driver_ifconfig_t, see
// postAttachStart().
static esp_err_t espNetifTransmit(void *pHandle,
                                  void *pData, size_t length)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_IGNORED;;
    uPortPppNetifDriver_t *pDriver = (uPortPppNetifDriver_t *) pHandle;
    struct uPortPppInterface_t *pPppInterface = pDriver->pPppInterface;
    size_t guard = 0;
    size_t sent = 0;

    if ((pPppInterface->pTransmitCallback != NULL) && (pPppInterface->pppRunning)) {
        errorCode = 0;
        while ((length > 0) && (errorCode >= 0) && (guard < U_PORT_PPP_TX_LOOP_GUARD)) {
            errorCode = pPppInterface->pTransmitCallback(pPppInterface->pDevHandle, pData + sent,
                                                         length - sent);
            if (errorCode > 0) {
                length -= errorCode;
                sent += errorCode;
            } else {
                vTaskDelay(U_PORT_PPP_TX_LOOP_DELAY_MS / portTICK_PERIOD_MS);
            }
            guard++;
        }
    }

    return (length == 0) ? ESP_OK : (esp_err_t) errorCode;
}

// This function is provided as a callback to the NETIF layer of
// ESP-IDF in the structure esp_netif_driver_ifconfig_t, see
// postAttachStart().
static void espNetifFreeRxBuffer(void *pHandle, void *pBuffer)
{
    // Not used
    (void) pHandle;
    (void) pBuffer;
}

// This function is provided as a callback to the NETIF layer of
// ESP-IDF in the structure uPortPppNetifDriver_t.
static esp_err_t postAttachStart(esp_netif_t *pEspNetif, void *pArgs)
{
    esp_err_t espError;
    uPortPppNetifDriver_t *pDriver = (uPortPppNetifDriver_t *) pArgs;
    const esp_netif_driver_ifconfig_t driverIfconfig = {
        .handle = pDriver,
        .driver_free_rx_buffer = espNetifFreeRxBuffer,
        .transmit = espNetifTransmit
    };
    esp_netif_ppp_config_t pppConfig = {0};

    pDriver->base.netif = pEspNetif;

    espError = esp_netif_set_driver_config(pEspNetif, &driverIfconfig);
    if (espError == ESP_OK) {
        // Switch on events so that we can tell when the IP stack
        // has finished with the PPP connection

        // This pattern borrowed from
        // https://github.com/espressif/esp-protocols/blob/master/components/esp_modem/src/esp_modem_netif.cpp
        pppConfig.ppp_phase_event_enabled = true;
        pppConfig.ppp_error_event_enabled = false;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
        esp_netif_ppp_get_params(pEspNetif, &pppConfig);
#endif // ESP-IDF >= v4.4
        if (!pppConfig.ppp_error_event_enabled) {
            pppConfig.ppp_error_event_enabled = true;
            espError = esp_netif_ppp_set_params(pEspNetif, &pppConfig);
        }
    }

    if ((espError == ESP_OK) && (pDriver->pIpAddress != NULL)) {
        espError = setIpAddress(pEspNetif, pDriver->pIpAddress);
        pDriver->pIpAddress = NULL; // NULL'ed so that we don't think it can be used again
    }

    if (espError == ESP_OK) {
        if (pDriver->pDnsIpAddressPrimary != NULL) {
            espError = setDnsAddress(pEspNetif, ESP_NETIF_DNS_MAIN, pDriver->pDnsIpAddressPrimary);
            pDriver->pDnsIpAddressPrimary = NULL; // NULL'ed so that we don't think it can be used again
        } else {
            uSockAddress_t address;
            if (uSockStringToAddress(U_PORT_PPP_DNS_PRIMARY_DEFAULT_STR, &address) > 0) {
                espError = setDnsAddress(pEspNetif, ESP_NETIF_DNS_MAIN, &address.ipAddress);
            }
        }
    }
    // Note: secondary DNS address not supported by ESP-IDF for PPP

#if defined(CONFIG_LWIP_PPP_PAP_SUPPORT) || defined(CONFIG_LWIP_PPP_CHAP_SUPPORT)
    if (espError == ESP_OK) {
        // Choose at least PAP since otherwise LCP negotiation will fail
        // The enumeration used by ESP-IDF matches uPortPppAuthenticationMode_t
        esp_netif_auth_type_t authenticationType = (esp_netif_auth_type_t) pDriver->authenticationMode;
        if (authenticationType != NETIF_PPP_AUTHTYPE_CHAP) {
            authenticationType = NETIF_PPP_AUTHTYPE_PAP;
        }
        // Set the username/password fields to at least be empty strings
        // otherwise the authentication mode will not be accepted
        if (pDriver->pUsername == NULL) {
            pDriver->pUsername = "";
        }
        if (pDriver->pPassword == NULL) {
            pDriver->pPassword = "";
        }
        espError = esp_netif_ppp_set_auth(pEspNetif, authenticationType,
                                          pDriver->pUsername, pDriver->pPassword);
        pDriver->pUsername = NULL; // NULL'ed so that we don't think
        pDriver->pPassword = NULL; // they can be used again
    }
#endif

    return espError;
}

// Callback for received data.
static void receiveCallback(void *pDevHandle, const char *pData,
                            size_t dataSize, void *pCallbackParam)
{
    uPortPppNetifDriver_t *pDriver = (uPortPppNetifDriver_t *) pCallbackParam;
    esp_netif_t *pEspNetif;

    (void) pDevHandle;

    pEspNetif = pDriver->base.netif;
    if (pEspNetif != NULL) {
        esp_netif_receive(pEspNetif, (void *) pData, dataSize, NULL);
    }
}

// Callback for IP state change events from the attached IP stack.
static void eventIpChanged(void *pArgs, esp_event_base_t eventBase,
                           int32_t eventId, void *pEventData)
{
    uPortPppNetifDriver_t *pDriver = (uPortPppNetifDriver_t *) pArgs;
    struct uPortPppInterface_t *pPppInterface = pDriver->pPppInterface;

    switch (eventId) {
        case IP_EVENT_PPP_GOT_IP:
            pPppInterface->ipConnected = true;
            break;
        case IP_EVENT_PPP_LOST_IP:
            pPppInterface->ipConnected = false;
            break;
        default:
            break;
    }
}

// Callback for PPP state change events from the attached IP stack.
static void eventPppChanged(void *pArgs, esp_event_base_t eventBase,
                            int32_t eventId, void *pEventData)
{
    uPortPppNetifDriver_t *pDriver = (uPortPppNetifDriver_t *) pArgs;
    struct uPortPppInterface_t *pPppInterface = pDriver->pPppInterface;

    uPortLog("U_PORT_PPP: received event %d.\n", eventId);
    if ((eventId > NETIF_PPP_ERRORNONE) && (eventId < NETIF_PP_PHASE_OFFSET)) {
        // This means that the IP stack is finished with us
        pPppInterface->ipConnected = false;
        uPortSemaphoreGive(pPppInterface->semaphoreExit);
    }
}

// Detach a PPP interface from the bottom of ESP NETIF.
static void pppDetach(uPortPppInterface_t *pPppInterface)
{
    if ((pPppInterface != NULL) && (pPppInterface->netifDriver.base.netif != NULL)) {
        if (pPppInterface->ipConnected) {
            esp_netif_action_disconnected(pPppInterface->netifDriver.base.netif, NULL, 0, NULL);
        }
        esp_netif_action_stop(pPppInterface->netifDriver.base.netif, NULL, 0, NULL);
        // Wait for the IP stack to let us go
        uPortLog("U_PORT_PPP: waiting to be released.\n");
        uPortSemaphoreTryTake(pPppInterface->semaphoreExit,
                              U_PORT_PPP_SHUTDOWN_TIMEOUT_SECONDS * 1000);
        uPortLog("U_PORT_PPP: released.\n");
        if (pPppInterface->pDisconnectCallback != NULL) {
            // Disconnect PPP and, if IP is still connected, also
            // get it to try to terminate the PPP link
            pPppInterface->pDisconnectCallback(pPppInterface->pDevHandle,
                                               pPppInterface->ipConnected);
        }
        esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, eventIpChanged);
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_PPP_GOT_IP, esp_netif_action_connected);
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_PPP_LOST_IP, esp_netif_action_disconnected);
        esp_event_handler_unregister(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, eventPppChanged);
        pPppInterface->pppRunning = false;
        pPppInterface->ipConnected = false;
        esp_netif_destroy(pPppInterface->netifDriver.base.netif);
        pPppInterface->netifDriver.base.netif = NULL;
    }
}

#endif // #if defined(CONFIG_LWIP_PPP_SUPPORT) && defined(U_CFG_PPP_ENABLE)

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS THAT ARE PRIVATE TO THIS PORT LAYER
 * -------------------------------------------------------------- */

#if defined(CONFIG_LWIP_PPP_SUPPORT) && defined(U_CFG_PPP_ENABLE)

// Initialise the PPP stuff.
int32_t uPortPppPrivateInit()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCode = uPortMutexCreate(&gMutex);
    }

    return errorCode;
}

// Deinitialise the PPP stuff.
void uPortPppPrivateDeinit()
{
    uLinkedList_t *pListNext;
    uPortPppInterface_t *pPppInterface;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        while (gpPppInterfaceList != NULL) {
            pPppInterface = (uPortPppInterface_t *) gpPppInterfaceList->p;
            pListNext = gpPppInterfaceList->pNext;
            uLinkedListRemove(&gpPppInterfaceList, pPppInterface);
            // Make sure we don't accidentally try to call the
            // down callback since the device handle will have
            // been destroyed by now
            pPppInterface->pDisconnectCallback = NULL;
            pppDetach(pPppInterface);
            uPortSemaphoreDelete(pPppInterface->semaphoreExit);
            uPortFree(pPppInterface);
            gpPppInterfaceList = pListNext;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

#else

// Initialise the PPP stuff.
int32_t uPortPppPrivateInit()
{
    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

// Deinitialise the PPP stuff.
void uPortPppPrivateDeinit()
{
}

#endif // #if defined(CONFIG_LWIP_PPP_SUPPORT) && defined(U_CFG_PPP_ENABLE)

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

#if defined(CONFIG_LWIP_PPP_SUPPORT) && defined(U_CFG_PPP_ENABLE)

// Attach a PPP interface to the bottom of ESP NETIF.
int32_t uPortPppAttach(void *pDevHandle,
                       uPortPppConnectCallback_t *pConnectCallback,
                       uPortPppDisconnectCallback_t *pDisconnectCallback,
                       uPortPppTransmitCallback_t *pTransmitCallback)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPppInterface_t *pPppInterface;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        pPppInterface = pFindPppInterface(pDevHandle);
        if (pPppInterface == NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            pPppInterface = (uPortPppInterface_t *) pUPortMalloc(sizeof(*pPppInterface));
            if (pPppInterface != NULL) {
                memset(pPppInterface, 0, sizeof(*pPppInterface));
                errorCode = uPortSemaphoreCreate(&(pPppInterface->semaphoreExit), 0, 1);
                if (errorCode == 0) {
                    pPppInterface->pDevHandle = pDevHandle;
                    pPppInterface->pConnectCallback = pConnectCallback;
                    pPppInterface->pDisconnectCallback = pDisconnectCallback;
                    pPppInterface->pTransmitCallback = pTransmitCallback;
                    if (uLinkedListAdd(&gpPppInterfaceList, pPppInterface)) {
                        // On this platform we don't do anything more
                        // at this point, everything else is done
                        // in uPortPppConnect()
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    } else {
                        uPortSemaphoreDelete(pPppInterface->semaphoreExit);
                        uPortFree(pPppInterface);
                    }
                } else {
                    uPortFree(pPppInterface);
                }
            }
        }

        if (errorCode < 0) {
            uPortLog("U_PORT_PPP: *** WARNING *** unable to attach PPP (%d).\n", errorCode);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Connect a PPP interface.
int32_t uPortPppConnect(void *pDevHandle,
                        uSockIpAddress_t *pIpAddress,
                        uSockIpAddress_t *pDnsIpAddressPrimary,
                        uSockIpAddress_t *pDnsIpAddressSecondary,
                        const char *pUsername,
                        const char *pPassword,
                        uPortPppAuthenticationMode_t authenticationMode)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPppInterface_t *pPppInterface;
    esp_netif_config_t espNetifConfigPpp = ESP_NETIF_DEFAULT_PPP();
    esp_netif_t *pEspNetif = NULL;
    size_t guardCount = 0;

    // ESP-IDF can't use a secondary DNS address on a PPP connection
    (void) pDnsIpAddressSecondary;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pUsername == NULL) && (pPassword == NULL)) {
            authenticationMode = U_PORT_PPP_AUTHENTICATION_MODE_NONE;
        }
        if (authenticationMode < U_PORT_PPP_AUTHENTICATION_MODE_MAX_NUM) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
            pPppInterface = pFindPppInterface(pDevHandle);
            if (pPppInterface != NULL) {
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                pEspNetif = esp_netif_new(&espNetifConfigPpp);
                if (pEspNetif != NULL) {
                    // Connect PPP to ESP-IDF NETIF: this
                    // will call postAttachStart() which
                    // will populate
                    // pPppInterface->netifDriver.base.netif
                    pPppInterface->netifDriver.base.post_attach = postAttachStart;
                    pPppInterface->netifDriver.pPppInterface = pPppInterface;
                    // Note that only the pointers are stored for these parameters,
                    // the contents are not copied: this is fine since they are
                    // used by postAttachStart(), which is called by
                    // esp_netif_action_start(), and that's it
                    pPppInterface->netifDriver.pIpAddress = pIpAddress;
                    pPppInterface->netifDriver.pDnsIpAddressPrimary = pDnsIpAddressPrimary;
                    pPppInterface->netifDriver.pUsername = pUsername;
                    pPppInterface->netifDriver.pPassword = pPassword;
                    pPppInterface->netifDriver.authenticationMode = authenticationMode;
                    errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                    if ((esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, eventPppChanged,
                                                    &(pPppInterface->netifDriver)) == ESP_OK) &&
                        (esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_GOT_IP, esp_netif_action_connected,
                                                    pEspNetif) == ESP_OK) &&
                        (esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_LOST_IP, esp_netif_action_disconnected,
                                                    pEspNetif) == ESP_OK) &&
                        (esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, eventIpChanged,
                                                    &(pPppInterface->netifDriver)) == ESP_OK) &&
                        (esp_netif_attach(pEspNetif, &(pPppInterface->netifDriver)) == ESP_OK)) {
                        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        if (pPppInterface->pConnectCallback != NULL) {
                            errorCode = pPppInterface->pConnectCallback(pDevHandle, receiveCallback,
                                                                        &(pPppInterface->netifDriver),
                                                                        NULL,
                                                                        U_PORT_PPP_RECEIVE_BUFFER_BYTES,
                                                                        NULL);
                        }
                        if (errorCode == 0) {
                            // Use a nice specific error message here, most likely to point
                            // people at a PPP kinda problem
                            errorCode = (int32_t) U_ERROR_COMMON_PROTOCOL_ERROR;
                            pPppInterface->pppRunning = true;
                            esp_netif_action_start(pEspNetif, NULL, 0, NULL);
                            while (!pPppInterface->ipConnected && (guardCount < 50)) {
                                // Wait a few seconds for PPP to connect so that
                                // the user gets a connection the moment we exit
                                uPortTaskBlock(100);
                                guardCount++;
                            }
                            if (pPppInterface->ipConnected) {
                                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                            }
                        }
                    }
                    if ((errorCode != 0) && (pEspNetif != NULL)) {
                        // Clean up on error
                        esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, eventIpChanged);
                        esp_event_handler_unregister(IP_EVENT, IP_EVENT_PPP_GOT_IP, esp_netif_action_connected);
                        esp_event_handler_unregister(IP_EVENT, IP_EVENT_PPP_LOST_IP, esp_netif_action_disconnected);
                        esp_event_handler_unregister(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, eventPppChanged);
                        esp_netif_destroy(pEspNetif);
                        pPppInterface->netifDriver.base.netif = NULL;
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Reconnect a PPP interface.
int32_t uPortPppReconnect(void *pDevHandle,
                          uSockIpAddress_t *pIpAddress)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPppInterface_t *pPppInterface;
    esp_netif_t *pEspNetif = NULL;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
        pPppInterface = pFindPppInterface(pDevHandle);
        if (pPppInterface != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
            pEspNetif = pPppInterface->netifDriver.base.netif;
            if ((pEspNetif != NULL) && (setIpAddress(pEspNetif, pIpAddress) == ESP_OK)) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                if (pPppInterface->pConnectCallback != NULL) {
                    errorCode = pPppInterface->pConnectCallback(pDevHandle, receiveCallback,
                                                                &(pPppInterface->netifDriver),
                                                                NULL,
                                                                U_PORT_PPP_RECEIVE_BUFFER_BYTES,
                                                                NULL);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Disconnect a PPP interface.
int32_t uPortPppDisconnect(void *pDevHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPppInterface_t *pPppInterface;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_NOT_FOUND;
        pPppInterface = pFindPppInterface(pDevHandle);
        if (pPppInterface != NULL) {
            // No different from detach, it's going dowwwwwwn...
            pppDetach(pPppInterface);
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Detach a PPP interface from the bottom of ESP NETIF.
int32_t uPortPppDetach(void *pDevHandle)
{
    uPortPppInterface_t *pPppInterface;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        pPppInterface = pFindPppInterface(pDevHandle);
        if (pPppInterface != NULL) {
            uLinkedListRemove(&gpPppInterfaceList, pPppInterface);
            pppDetach(pPppInterface);
            uPortSemaphoreDelete(pPppInterface->semaphoreExit);
            uPortFree(pPppInterface);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return (int32_t) U_ERROR_COMMON_SUCCESS;
}

#endif // #if defined(CONFIG_LWIP_PPP_SUPPORT) && defined(U_CFG_PPP_ENABLE)

// End of file
