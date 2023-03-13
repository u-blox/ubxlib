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

/*
 *
 * A simple example showing how to start a subprocess
 * and toggle a gpio for led blink using ubxlib.
 *
 */

#include "ubxlib.h"

#define LED_PIN        39 // Change this to your board specific gpio
#define BLINK_TIME_MS 500

static void blinkTask(void *pParameters)
{
    uPortGpioConfig_t gpioConfig;
    U_PORT_GPIO_SET_DEFAULT(&gpioConfig);
    gpioConfig.pin = LED_PIN;
    gpioConfig.direction = U_PORT_GPIO_DIRECTION_OUTPUT;
    uPortGpioConfig(&gpioConfig);
    bool on = false;
    while (1) {
        uPortGpioSet(LED_PIN, on);
        on = !on;
        uPortTaskBlock(BLINK_TIME_MS);
    }
}

void main()
{
    uPortInit();
    uPortLog("Blink program started\n");
    uPortTaskHandle_t taskHandle;
    uPortTaskCreate(blinkTask,
                    "twinkle",
                    1024,
                    NULL,
                    5,
                    &taskHandle);
}
