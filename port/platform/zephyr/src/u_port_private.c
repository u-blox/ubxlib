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
 * @brief Stuff private to the Zephyr porting layer.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // strncpy()

#include "u_cfg_os_platform_specific.h"
#include "u_error_common.h"
#include "u_linked_list.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_event_queue.h"

#include <version.h>

#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(3,1,0)
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#else
#include <kernel.h>
#include <device.h>
#include <drivers/gpio.h>
#endif

#include "u_port_private.h"  // Down here because it needs to know about the Zephyr device tree

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Define a timer, intended to be used as part of a linked-list.
 */
typedef struct uPortPrivateTimer_t {
    struct k_timer *pKTimer; /**< this is used as the handle. */
    uint32_t intervalMs;
    bool periodic;
    pTimerCallback_t *pCallback;
    void *pCallbackParam;
    struct uPortPrivateTimer_t *pNext;
} uPortPrivateTimer_t;

/** Type to hold a Zephyr GPIO callback with the user's callback
 * in a linked list.
 */
typedef struct {
    int32_t pin;
    struct gpio_callback callback;
    void (*pUserCallback)(void);
} uPortPrivateGpioCallback_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Root of the linked list of timers.
 */
static uPortPrivateTimer_t *gpTimerList = NULL;

/** Mutex to protect the linked list of timers.
 */
static uPortMutexHandle_t gMutexTimers = NULL;

/** Array of timer structures; we do this as a fixed
 * array since, in the Zephyr API, the callback gets a pointer
 * to the timer structure itself.  If that structure were inside
 * the linked list then, should any timers expire after the list
 * had been modified, it could either go bang or end up with the
 * wrong timer.
 */
static struct k_timer gKTimer[U_CFG_OS_TIMER_MAX_NUM];

/** If a user creates and destroys timers dynamically from different
 * threads during the life of an application without making completely
 * sure that the timer expiry calls have not yet landed in any
 * cross-over case then it is technically possible for a kTimer
 * structure to have been re-allocated, resulting in the wrong callback
 * being called.  To combat this, keep a record of the next entry in
 * the gKTimer array that is potentially free and always start the search
 * for a new free entry from there, minimizing the chance that a recently
 * used gKTimer entry will be picked up again.
 */
static size_t gLastKTimerNext = 0;

/** Zephry timer callbacks are called inside ISRs so, in order to put them
 * into task space, we use an event queue.
 */
static int32_t gTimerEventQueueHandle = -1;

/** The number of pins in each GPIO port.
 */
static int32_t gGpioNumPinsPerPort = -1;

/** Root of the linked list of interrupt callbacks.
 */
static uLinkedList_t *gpGpioCallbackList = NULL; // List of uPortPrivateGpioCallback_t

/** Mutex to protect GPIO data.
 */
static uPortMutexHandle_t gMutexGpio = NULL;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: GPIO RELATED
 * -------------------------------------------------------------- */

// GPIO interrupt handler callback.
static void gpioCallbackHandler(const struct device *pPort,
                                struct gpio_callback *pCb,
                                gpio_port_pins_t pins)
{
    uPortPrivateGpioCallback_t *pGpioCallback = CONTAINER_OF(pCb, uPortPrivateGpioCallback_t, callback);

    (void) pPort;
    (void) pins;

    if (pGpioCallback->pUserCallback != NULL) {
        pGpioCallback->pUserCallback();
    }
}

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: TIMER RELATED
 * -------------------------------------------------------------- */

// Find a free kernal timer structure
// gMutexTimers should be locked before this is called.
static struct k_timer *pKTimerFindFree()
{
    struct k_timer *pKTimer = NULL;
    uPortPrivateTimer_t *pTimer;
    size_t x = 0;
    size_t i = gLastKTimerNext;

    // For each kernel timer structure in the gKTimer array,
    // check if it is reference by an entry in the linked list;
    // if one isn't then that's the weener.
    for (x = 0; (pKTimer == NULL) && (x < sizeof(gKTimer) / sizeof(gKTimer[0])); x++) {
        pTimer = gpTimerList;
        while ((pTimer != NULL) && (pTimer->pKTimer != &(gKTimer[i]))) {
            pTimer = pTimer->pNext;
        }
        if (pTimer == NULL) {
            pKTimer = &(gKTimer[i]);
            gLastKTimerNext = i + 1;
            if (gLastKTimerNext >= sizeof(gKTimer) / sizeof(gKTimer[0])) {
                gLastKTimerNext = 0;
            }
        } else {
            i++;
            if (i >= sizeof(gKTimer) / sizeof(gKTimer[0])) {
                i = 0;
            }
        }
    }

    return pKTimer;
}

// Find a timer entry in the list.
// gMutexTimers should be locked before this is called.
static uPortPrivateTimer_t *pTimerFind(struct k_timer *pKTimer)
{
    uPortPrivateTimer_t *pTimer = gpTimerList;

    while ((pTimer != NULL) && (pTimer->pKTimer != pKTimer)) {
        pTimer = pTimer->pNext;
    }

    return pTimer;
}

// Remove an entry from the list.
// gMutexTimers should be locked before this is called.
static void timerRemove(struct k_timer *pKTimer)
{
    uPortPrivateTimer_t *pTimer = gpTimerList;
    uPortPrivateTimer_t *pPrevious = NULL;

    // Find the entry in the list
    while ((pTimer != NULL) && (pTimer->pKTimer != pKTimer)) {
        pPrevious = pTimer;
        pTimer = pTimer->pNext;
    }
    if (pTimer != NULL) {
        // Remove the entry from the list
        if (pPrevious != NULL) {
            pPrevious->pNext = pTimer->pNext;
        } else {
            // Must be at head
            gpTimerList = pTimer->pNext;
        }
        // Free the entry
        uPortFree(pTimer);
    }
}

// The timer event handler, where the parameter is a pointer to a
// kTimer pointer.
static void timerEventHandler(void *pParam, size_t paramLength)
{
    struct k_timer *pKTimer = *((struct k_timer **) pParam);
    uPortPrivateTimer_t *pTimer;
    pTimerCallback_t *pCallback = NULL;
    void *pCallbackParam;

    (void) paramLength;

    if (gMutexTimers != NULL) {

        U_PORT_MUTEX_LOCK(gMutexTimers);

        pTimer = pTimerFind(pKTimer);
        if (pTimer != NULL) {
            pCallback = pTimer->pCallback;
            pCallbackParam = pTimer->pCallbackParam;
        }

        U_PORT_MUTEX_UNLOCK(gMutexTimers);

        // Call the callback outside the locks so that the
        // callback itself may call the timer API
        if (pCallback != NULL) {
            pCallback((uPortTimerHandle_t) pKTimer, pCallbackParam);
        }
    }
}

// The timer expiry callback, called by Zephyr from interrupt context.
static void timerCallbackInt(struct k_timer *pKTimer)
{
    if (gTimerEventQueueHandle >= 0) {
        // Send an event to our event task with the pointer
        // pKTimer as the payload
        uPortEventQueueSendIrq(gTimerEventQueueHandle,
                               &pKTimer, sizeof(pKTimer)); // NOLINT(bugprone-sizeof-expression)
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT: MISC
 * -------------------------------------------------------------- */

// Initalise the private stuff.
int32_t uPortPrivateInit()
{
    int32_t errorCodeOrEventQueueHandle = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutexGpio == NULL) {
        errorCodeOrEventQueueHandle = uPortMutexCreate(&gMutexGpio);
    }

    if (errorCodeOrEventQueueHandle == 0) {
        if (gMutexTimers == NULL) {
            errorCodeOrEventQueueHandle = uPortMutexCreate(&gMutexTimers);
            if (errorCodeOrEventQueueHandle == 0) {
                // We need an event queue as Zephyr's timer callback is called
                // in interrupt context and we need to get it into task context
                errorCodeOrEventQueueHandle = uPortEventQueueOpen(timerEventHandler, "timerEvent",
                                                                  sizeof(struct k_timer *),
                                                                  U_CFG_OS_TIMER_EVENT_TASK_STACK_SIZE_BYTES,
                                                                  U_CFG_OS_TIMER_EVENT_TASK_PRIORITY,
                                                                  U_CFG_OS_TIMER_EVENT_QUEUE_SIZE);
                if (errorCodeOrEventQueueHandle >= 0) {
                    gTimerEventQueueHandle = errorCodeOrEventQueueHandle;
                    errorCodeOrEventQueueHandle = (int32_t) U_ERROR_COMMON_SUCCESS;
                } else {
                    // Clean up on error
                    uPortMutexDelete(gMutexGpio);
                    gMutexGpio = NULL;
                    uPortMutexDelete(gMutexTimers);
                    gMutexTimers = NULL;
                }
            } else {
                // Clean up on error
                uPortMutexDelete(gMutexGpio);
                gMutexGpio = NULL;
            }
        }
    }

    return errorCodeOrEventQueueHandle;
}

// Deinitialise the private stuff.
void uPortPrivateDeinit()
{
    uPortPrivateGpioCallback_t *pGpioCallback;
    const struct device *pPort;

    if (gMutexTimers != NULL) {

        U_PORT_MUTEX_LOCK(gMutexTimers);

        // Tidy away the timers
        while (gpTimerList != NULL) {
            k_timer_stop((struct k_timer *) gpTimerList->pKTimer);
            timerRemove(gpTimerList->pKTimer);
        }

        U_PORT_MUTEX_UNLOCK(gMutexTimers);

        // Close the event queue outside the mutex as it could be calling
        // back into this API
        if (gTimerEventQueueHandle >= 0) {
            uPortEventQueueClose(gTimerEventQueueHandle);
            gTimerEventQueueHandle = -1;
        }

        uPortMutexDelete(gMutexTimers);
        gMutexTimers = NULL;
    }

    if (gMutexGpio != NULL) {

        U_PORT_MUTEX_LOCK(gMutexGpio);

        // Tidy away any GPIO callbacks
        while (gpGpioCallbackList != NULL) {
            pGpioCallback = (uPortPrivateGpioCallback_t *) gpGpioCallbackList->p;
            pPort = pUPortPrivateGetGpioDevice(pGpioCallback->pin);
            if (pPort != NULL) {
                gpio_remove_callback(pPort, &(pGpioCallback->callback));
            }
            uPortFree(pGpioCallback);
            uLinkedListRemove(&gpGpioCallbackList, pGpioCallback);
        }

        U_PORT_MUTEX_UNLOCK(gMutexGpio);

        uPortMutexDelete(gMutexGpio);
        gMutexGpio = NULL;
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT: GPIO
 * -------------------------------------------------------------- */

// Get the ubxlib pin number for a GPIO device.
int32_t uPortPrivateGetGpioPort(const struct device *pGpioDevice,
                                int32_t pinWithinPort)
{
    int32_t errorCodeOrPin = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    int32_t portNumber = -1;

    if (pinWithinPort >= 0) {
        //NOLINTBEGIN(misc-redundant-expression)
#if KERNEL_VERSION_MAJOR < 3
        if ((pGpioDevice == device_get_binding("GPIO_0")) ||
            (pGpioDevice == device_get_binding("GPIOA")) ||
            (pGpioDevice == device_get_binding("PORTA"))) {
            portNumber = 0;
        } else if ((pGpioDevice == device_get_binding("GPIO_1")) ||
                   (pGpioDevice == device_get_binding("GPIOB")) ||
                   (pGpioDevice == device_get_binding("PORTB"))) {
            portNumber = 1;
        } else if ((pGpioDevice == device_get_binding("GPIO_2")) ||
                   (pGpioDevice == device_get_binding("GPIOC")) ||
                   (pGpioDevice == device_get_binding("PORTC"))) {
            portNumber = 2;
        } else if ((pGpioDevice == device_get_binding("GPIO_3")) ||
                   (pGpioDevice == device_get_binding("GPIOD")) ||
                   (pGpioDevice == device_get_binding("PORTD"))) {
            portNumber = 3;
        } else if ((pGpioDevice == device_get_binding("GPIO_4")) ||
                   (pGpioDevice == device_get_binding("GPIOE")) ||
                   (pGpioDevice == device_get_binding("PORTE"))) {
            portNumber = 4;
        } else if ((pGpioDevice == device_get_binding("GPIO_5")) ||
                   (pGpioDevice == device_get_binding("GPIOF")) ||
                   (pGpioDevice == device_get_binding("PORTF"))) {
            portNumber = 5;
        } else if ((pGpioDevice == device_get_binding("GPIO_6")) ||
                   (pGpioDevice == device_get_binding("GPIOG")) ||
                   (pGpioDevice == device_get_binding("PORTG"))) {
            portNumber = 6;
        } else if ((pGpioDevice == device_get_binding("GPIO_7")) ||
                   (pGpioDevice == device_get_binding("GPIOH")) ||
                   (pGpioDevice == device_get_binding("PORTH"))) {
            portNumber = 7;
        } else if ((pGpioDevice == device_get_binding("GPIO_8")) ||
                   (pGpioDevice == device_get_binding("GPIOI")) ||
                   (pGpioDevice == device_get_binding("PORTI"))) {
            portNumber = 8;
        } else if ((pGpioDevice == device_get_binding("GPIO_9")) ||
                   (pGpioDevice == device_get_binding("GPIOJ")) ||
                   (pGpioDevice == device_get_binding("PORTJ"))) {
            portNumber = 9;
        } else if ((pGpioDevice == device_get_binding("GPIO_10")) ||
                   (pGpioDevice == device_get_binding("GPIOK")) ||
                   (pGpioDevice == device_get_binding("PORTK"))) {
            portNumber = 10;
        }
#else
        if ((pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpio0)) ||
            (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpioa)) ||
            (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(porta))) {
            portNumber = 0;
        } else if ((pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpio1)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpiob)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(portb))) {
            portNumber = 1;
        } else if ((pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpio2)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpioc)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(portc))) {
            portNumber = 2;
        } else if ((pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpio3)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpiod)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(portd))) {
            portNumber = 3;
        } else if ((pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpio4)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpioe)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(porte))) {
            portNumber = 4;
        } else if ((pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpio5)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpiof)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(portf))) {
            portNumber = 5;
        } else if ((pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpio6)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpiog)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(portg))) {
            portNumber = 6;
        } else if ((pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpio7)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpioh)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(porth))) {
            portNumber = 7;
        } else if ((pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpio8)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpioi)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(porti))) {
            portNumber = 8;
        } else if ((pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpio9)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpioj)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(portj))) {
            portNumber = 9;
        } else if ((pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpio10)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(gpiok)) ||
                   (pGpioDevice == U_DEVICE_DT_GET_OR_NULL(portk))) {
            portNumber = 10;
        }
#endif
        //NOLINTEND(misc-redundant-expression)
    }

    if (portNumber >= 0) {
        errorCodeOrPin = (portNumber * uPortPrivateGetGpioPortMaxPins()) + pinWithinPort;
    }

    return errorCodeOrPin;
}

// Get a GPIO device
const struct device *pUPortPrivateGetGpioDevice(int32_t pin)
{
    const struct device *pDev = NULL;
    int32_t portNo = pin / uPortPrivateGetGpioPortMaxPins();
    // The actual device tree name of the GPIO port may vary between
    // different boards. Try the known variants.
#if KERNEL_VERSION_MAJOR < 3
    if (portNo == 0) {
        pDev = device_get_binding("GPIO_0");
        if (!pDev) {
            pDev = device_get_binding("GPIOA");
        }
        if (!pDev) {
            pDev = device_get_binding("PORTA");
        }
    } else if (portNo == 1) {
        pDev = device_get_binding("GPIO_1");
        if (!pDev) {
            pDev = device_get_binding("GPIOB");
        }
        if (!pDev) {
            pDev = device_get_binding("PORTB");
        }
    } else if (portNo == 2) {
        pDev = device_get_binding("GPIO_2");
        if (!pDev) {
            pDev = device_get_binding("GPIOC");
        }
        if (!pDev) {
            pDev = device_get_binding("PORTC");
        }
    } else if (portNo == 3) {
        pDev = device_get_binding("GPIO_3");
        if (!pDev) {
            pDev = device_get_binding("GPIOD");
        }
        if (!pDev) {
            pDev = device_get_binding("PORTD");
        }
    } else if (portNo == 4) {
        pDev = device_get_binding("GPIO_4");
        if (!pDev) {
            pDev = device_get_binding("GPIOD");
        }
        if (!pDev) {
            pDev = device_get_binding("PORTD");
        }
    } else if (portNo == 5) {
        pDev = device_get_binding("GPIO_5");
        if (!pDev) {
            pDev = device_get_binding("GPIOE");
        }
        if (!pDev) {
            pDev = device_get_binding("PORTE");
        }
    } else if (portNo == 6) {
        pDev = device_get_binding("GPIO_6");
        if (!pDev) {
            pDev = device_get_binding("GPIOF");
        }
        if (!pDev) {
            pDev = device_get_binding("PORTF");
        }
    } else if (portNo == 7) {
        pDev = device_get_binding("GPIO_7");
        if (!pDev) {
            pDev = device_get_binding("GPIOG");
        }
        if (!pDev) {
            pDev = device_get_binding("PORTG");
        }
    } else if (portNo == 8) {
        pDev = device_get_binding("GPIO_8");
        if (!pDev) {
            pDev = device_get_binding("GPIOH");
        }
        if (!pDev) {
            pDev = device_get_binding("PORTH");
        }
    } else if (portNo == 9) {
        pDev = device_get_binding("GPIO_9");
        if (!pDev) {
            pDev = device_get_binding("GPIOI");
        }
        if (!pDev) {
            pDev = device_get_binding("PORTI");
        }
    } else if (portNo == 9) {
        pDev = device_get_binding("GPIO_");
        if (!pDev) {
            pDev = device_get_binding("GPIOJ");
        }
        if (!pDev) {
            pDev = device_get_binding("PORTJ");
        }
    } else if (portNo == 10) {
        pDev = device_get_binding("GPIO_10");
        if (!pDev) {
            pDev = device_get_binding("GPIOK");
        }
        if (!pDev) {
            pDev = device_get_binding("PORTK");
        }
    }
#else
    if (portNo == 0) {
        pDev = U_DEVICE_DT_GET_OR_NULL(gpio0);
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(gpioa);
        }
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(porta);
        }
    } else if (portNo == 1) {
        pDev = U_DEVICE_DT_GET_OR_NULL(gpio1);
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(gpiob);
        }
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(portb);
        }
    } else if (portNo == 2) {
        pDev = U_DEVICE_DT_GET_OR_NULL(gpio2);
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(gpioc);
        }
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(portc);
        }
    } else if (portNo == 3) {
        pDev = U_DEVICE_DT_GET_OR_NULL(gpio3);
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(gpiod);
        }
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(portd);
        }
    } else if (portNo == 4) {
        pDev = U_DEVICE_DT_GET_OR_NULL(gpio4);
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(gpioe);
        }
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(porte);
        }
    } else if (portNo == 5) {
        pDev = U_DEVICE_DT_GET_OR_NULL(gpio5);
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(gpiof);
        }
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(portf);
        }
    } else if (portNo == 6) {
        pDev = U_DEVICE_DT_GET_OR_NULL(gpio6);
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(gpiog);
        }
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(portg);
        }
    } else if (portNo == 7) {
        pDev = U_DEVICE_DT_GET_OR_NULL(gpio7);
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(gpioh);
        }
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(porth);
        }
    } else if (portNo == 8) {
        pDev = U_DEVICE_DT_GET_OR_NULL(gpio8);
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(gpioi);
        }
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(porti);
        }
    } else if (portNo == 9) {
        pDev = U_DEVICE_DT_GET_OR_NULL(gpio9);
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(gpioj);
        }
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(portj);
        }
    } else if (portNo == 10) {
        pDev = U_DEVICE_DT_GET_OR_NULL(gpio10);
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(gpiok);
        }
        if (!pDev) {
            pDev = U_DEVICE_DT_GET_OR_NULL(portk);
        }
    }
#endif
    return pDev;
}

// Get the maximum number of pins supported by each GPIO port.
int32_t uPortPrivateGetGpioPortMaxPins()
{
    const struct device *pGpioDevice = NULL;
    const struct gpio_driver_config *pCfg;

    if (gGpioNumPinsPerPort < 0) {
        // Get the number of pins on a port that must exist
#if KERNEL_VERSION_MAJOR < 3
        pGpioDevice = device_get_binding("GPIO_0");
        if (!pGpioDevice) {
            pGpioDevice = device_get_binding("GPIOA");
        }
        if (!pGpioDevice) {
            pGpioDevice = device_get_binding("PORTA");
        }
#else
        pGpioDevice = U_DEVICE_DT_GET_OR_NULL(gpio0);
        if (!pGpioDevice) {
            pGpioDevice = U_DEVICE_DT_GET_OR_NULL(gpioa);
        }
        if (!pGpioDevice) {
            pGpioDevice = U_DEVICE_DT_GET_OR_NULL(porta);
        }
#endif

        pCfg = (const struct gpio_driver_config *) pGpioDevice->config;
        // The first item in a GPIO device configuration is always
        // port_pin_mask, in which each bit set to 1, starting with
        // bit 0 and working up, represents a valid pin
        for (int32_t x = 0; (x < 64) && (gGpioNumPinsPerPort < 0); x++) {
            if ((pCfg->port_pin_mask & (1UL << x)) == 0) {
                gGpioNumPinsPerPort = x;
            }
        }
        if (gGpioNumPinsPerPort == 0) {
            // Clang gets concerned about divisions by zero if
            // we return zero from here (which we never will,
            // since if the first bit of port_pin_mask is 0
            // then the Zephyr platform is broken), but keep
            // it happy anyway.
            gGpioNumPinsPerPort = -1;
        }
    }

    return gGpioNumPinsPerPort;
}

// Add a GPIO callback for a pin.
int32_t uPortPrivateGpioCallbackAdd(int32_t pin, void (*pCallback)(void))
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateGpioCallback_t *pGpioCallback = NULL;
    const struct device *pPort = pUPortPrivateGetGpioDevice(pin);

    if (gMutexGpio != NULL) {

        U_PORT_MUTEX_LOCK(gMutexGpio);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((pPort != NULL) && (pin >= 0) && (pCallback != NULL)) {
            // Allocate memory for the interrupt callback
            pGpioCallback = (uPortPrivateGpioCallback_t *) pUPortMalloc(sizeof(*pGpioCallback));
            if (pGpioCallback != NULL) {
                // Populate the GPIO callback structure
                errorCode = U_ERROR_COMMON_PLATFORM;
                memset(pGpioCallback, 0, sizeof(*pGpioCallback));
                pGpioCallback->pin = pin;
                pGpioCallback->pUserCallback = pCallback;
                gpio_init_callback(&(pGpioCallback->callback), gpioCallbackHandler,
                                   1 << (pin % uPortPrivateGetGpioPortMaxPins()));
                if (gpio_add_callback(pPort, &(pGpioCallback->callback)) == 0) {
                    errorCode = U_ERROR_COMMON_NO_MEMORY;
                    if (uLinkedListAdd(&gpGpioCallbackList, pGpioCallback)) {
                        // Add the interrupt handler to the list
                        errorCode = U_ERROR_COMMON_SUCCESS;
                    } else {
                        // Clean up on error
                        gpio_remove_callback(pPort, &(pGpioCallback->callback));
                        uPortFree(pGpioCallback);
                    }
                } else {
                    // Clean up on error
                    uPortFree(pGpioCallback);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexGpio);
    }

    return errorCode;
}

// Remove any GPIO callback for a pin.
void uPortPrivateGpioCallbackRemove(int32_t pin)
{
    uLinkedList_t *p = gpGpioCallbackList;
    uPortPrivateGpioCallback_t *pGpioCallback = NULL;
    const struct device *pPort = pUPortPrivateGetGpioDevice(pin);

    if (gMutexGpio != NULL) {

        U_PORT_MUTEX_LOCK(gMutexGpio);

        // Find the entry in the list
        while ((p != NULL) && (pGpioCallback == NULL)) {
            if (((uPortPrivateGpioCallback_t *) (p->p))->pin == pin) {
                pGpioCallback = (uPortPrivateGpioCallback_t *) (p->p);
            }
            p = p->pNext;
        }

        if ((pPort != NULL) && (pGpioCallback != NULL)) {
            gpio_remove_callback(pPort, &(pGpioCallback->callback));
            uPortFree(pGpioCallback);
            uLinkedListRemove(&gpGpioCallbackList, pGpioCallback);
        }

        U_PORT_MUTEX_UNLOCK(gMutexGpio);
    }
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT: TIMERS
 * -------------------------------------------------------------- */

// Add a timer entry to the list.
int32_t uPortPrivateTimerCreate(uPortTimerHandle_t *pHandle,
                                pTimerCallback_t *pCallback,
                                void *pCallbackParam,
                                uint32_t intervalMs,
                                bool periodic)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateTimer_t *pTimer;

    if (gMutexTimers != NULL) {

        U_PORT_MUTEX_LOCK(gMutexTimers);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pHandle != NULL) {
            // Create an entry in the list
            pTimer = (uPortPrivateTimer_t *) pUPortMalloc(sizeof(uPortPrivateTimer_t));
            errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
            if (pTimer != NULL) {
                // Find a free timer structure
                pTimer->pKTimer = pKTimerFindFree();
                if (pTimer->pKTimer != NULL) {
                    // Populate the entry
                    k_timer_init(pTimer->pKTimer, timerCallbackInt, NULL);
                    pTimer->intervalMs = intervalMs;
                    pTimer->periodic = periodic;
                    pTimer->pCallback = pCallback;
                    pTimer->pCallbackParam = pCallbackParam;
                    // Add the timer to the front of the list
                    pTimer->pNext = gpTimerList;
                    gpTimerList = pTimer;
                    *pHandle = (uPortTimerHandle_t) (pTimer->pKTimer);
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                } else {
                    // Tidy up if a free timer could not be found
                    uPortFree(pTimer);
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexTimers);
    }

    return errorCode;
}

// Remove a timer entry from the list.
int32_t uPortPrivateTimerDelete(uPortTimerHandle_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutexTimers != NULL) {

        // Stop the timer in the kernel, outside the mutex in case
        // the call blocks
        k_timer_stop((struct k_timer *) handle);

        U_PORT_MUTEX_LOCK(gMutexTimers);

        timerRemove((struct k_timer *) handle);
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

        U_PORT_MUTEX_UNLOCK(gMutexTimers);
    }

    return errorCode;
}

// Start a timer.
int32_t uPortPrivateTimerStart(const uPortTimerHandle_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateTimer_t *pTimer = NULL;
    k_timeout_t duration;
    k_timeout_t period = {0};

    if (gMutexTimers != NULL) {

        U_PORT_MUTEX_LOCK(gMutexTimers);

        pTimer = pTimerFind((struct k_timer *) handle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pTimer != NULL) {
            duration = K_MSEC(pTimer->intervalMs);
            if (pTimer->periodic) {
                period = duration;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutexTimers);

        // Release the mutex before starting the timer
        // in case the OS call blocks
        if (pTimer != NULL) {
            k_timer_start((struct k_timer *) handle, duration, period);
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }
    }

    return errorCode;
}

// Change a timer interval.
int32_t uPortPrivateTimerChange(const uPortTimerHandle_t handle,
                                uint32_t intervalMs)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uPortPrivateTimer_t *pTimer;

    if (gMutexTimers != NULL) {

        U_PORT_MUTEX_LOCK(gMutexTimers);

        pTimer = pTimerFind(handle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pTimer != NULL) {
            pTimer->intervalMs = intervalMs;
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        }

        U_PORT_MUTEX_UNLOCK(gMutexTimers);
    }

    return errorCode;
}

// End of file
