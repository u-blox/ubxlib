/*
 * Copyright 2023 u-blox
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
 * @brief Application intended for the Ubxlib test farm.
 * Acts as a BLE NUS server available for DUT client to connect to.
 * Whenever a DUT is connected and sends a command a response will be
 * sent back.
 *
 * This application must be built using easy_nrf52, https://github.com/plerup/easy_nrf52
 */

#include <enrf.h>
#include <app_timer.h>

#define SERVER_NAME "UbxExtNusServer"
#define ADV_INTERVAL_MS 100
#define USED_LED BSP_BOARD_LED_0

// Red led is blinking when advertising
APP_TIMER_DEF(m_blink_timer);

static void blink_cb(void *p_context)
{
    bsp_board_led_invert(BSP_BOARD_LED_0);
}

//--------------------------------------------------------------------------

static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context)
{
    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            app_timer_stop(m_blink_timer);
            SET_LED(BSP_BOARD_LED_0, true);
            NRF_LOG_INFO("Client connected: %s",
                         enrf_addr_to_str((ble_gap_addr_t *) & (p_ble_evt->evt.gap_evt.params.connected.peer_addr)));
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Client disconnected, reason: 0x%x.",
                         p_ble_evt->evt.gap_evt.params.disconnected.reason);
            app_timer_start(m_blink_timer, APP_TIMER_TICKS(500), NULL);
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------

static bool nus_data_received(uint8_t *data, uint32_t length)
{
    char str[32];
    bool taken = true;
    strlcpy(str, (const char *)data, MIN(sizeof(str), length + 1));
    NRF_LOG_INFO("Client sent: %s", str);
    if (strcasestr(str, "Hello")) {
        enrf_nus_string_send("Hello from server");
    } else if (strcasestr(str, "led")) {
        bsp_board_led_invert(USED_LED);
        snprintf(str, sizeof(str), "LED is %s", bsp_board_led_state_get(USED_LED) ? "on" : "off");
        enrf_nus_string_send(str);
    } else if (strcasestr(str, "echo")) {
        enrf_nus_data_send(data + 4, length - 4);
    } else {
        taken = false;
    }
    return taken;
}

//--------------------------------------------------------------------------

int main()
{
    enrf_init(SERVER_NAME, ble_evt_handler);
    bsp_init(BSP_INIT_LEDS, NULL);
    app_timer_create(&m_blink_timer, APP_TIMER_MODE_REPEATED, blink_cb);
    app_timer_start(m_blink_timer, APP_TIMER_TICKS(500), NULL);
    // Start advertising
    enrf_start_advertise(true,
                         0, BLE_ADVDATA_FULL_NAME,
                         NULL, 0,
                         ADV_INTERVAL_MS, 0,
                         nus_data_received
                        );
    NRF_LOG_INFO("Started: %s", SERVER_NAME);
    while (true) {
        enrf_wait_for_event();
    }
}