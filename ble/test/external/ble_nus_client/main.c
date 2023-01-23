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
 * Acts as a BLE NUS client searching for a DUT advertising as a NUS server.
 * Whenever a DUT is discovered this unit will connect and send a command to it.
 *
 * This application must be built using easy_nrf52, https://github.com/plerup/easy_nrf52
 */

#include <app_timer.h>
#include <enrf.h>

#define SERVER_NAME "UbxDutNusServer"
#define COMMAND "Hello"

enum { IDLE,
       START_SCAN,
       SCANNING,
       CONNECT,
       DISCONNECT,
       TIMEOUT
     } m_state = START_SCAN;
ble_gap_addr_t m_client_addr;

// After connect and disconnect the client is idle for a while
APP_TIMER_DEF(m_idle_timer);
// If the sever doesn't respond a force disconnect is made after timeout
APP_TIMER_DEF(m_response_timer);
// Blue led is blinking during scanning
APP_TIMER_DEF(m_blink_timer);

static void timer_cb(void *p_context)
{
    if (p_context) {
        // Idle timer timeout
        m_state = START_SCAN;
    } else {
        // Response timeout
        m_state = TIMEOUT;
        SET_LED(BSP_BOARD_LED_0, true);
    }
}

//--------------------------------------------------------------------------

static void blink_cb(void *p_context)
{
    bsp_board_led_invert(BSP_BOARD_LED_1);
}

//--------------------------------------------------------------------------

void nus_c_rx_cb(uint8_t *data, uint32_t length)
{
    if (data) {
        static char str[100];
        strlcpy(str, (const char *)data, MIN(sizeof(str), length + 1));
        NRF_LOG_INFO("Response: %s", str);
        app_timer_stop(m_response_timer);
        m_state = DISCONNECT;
    } else if (length == 1) {
        NRF_LOG_INFO("Nus detected, sending command")
        enrf_nus_c_string_send(COMMAND);
        // Set reponse timeout
        app_timer_start(m_response_timer, APP_TIMER_TICKS(5000), (void *)false);
    }
}

//--------------------------------------------------------------------------

bool report_cb(ble_gap_evt_adv_report_t *p_adv_report)
{
    uint8_t name[32];
    uint8_t name_len = enrf_adv_parse(p_adv_report,
                                      BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME,
                                      name, sizeof(name));
    if (!name_len) {
        return false;
    }
    name[name_len] = 0;
    if (strcmp((char *)name, SERVER_NAME) == 0) {
        // DUT server was found
        m_client_addr = p_adv_report->peer_addr;
        m_state = CONNECT;
        return true;
    }
    return false;
}

//--------------------------------------------------------------------------

int main()
{
    enrf_init("connect", NULL);
    bsp_init(BSP_INIT_LEDS, NULL);
    SET_LED(BSP_BOARD_LED_0, false);
    SET_LED(BSP_BOARD_LED_1, false);

    enrf_connect_to(NULL, NULL, nus_c_rx_cb);
    app_timer_create(&m_idle_timer, APP_TIMER_MODE_SINGLE_SHOT, timer_cb);
    app_timer_create(&m_response_timer, APP_TIMER_MODE_SINGLE_SHOT, timer_cb);
    app_timer_create(&m_blink_timer, APP_TIMER_MODE_REPEATED, blink_cb);
    enrf_start_scan(report_cb, 0, false);
    NRF_LOG_INFO("Started: UBX NUS Client");
    while (true) {
        if (m_state == START_SCAN) {
            m_state = SCANNING;
            NRF_LOG_INFO("Scanning...");
            SET_LED(BSP_BOARD_LED_0, false);
            app_timer_start(m_blink_timer, APP_TIMER_TICKS(500), NULL);
            enrf_start_scan(report_cb, 0, false);
        } else if (m_state == CONNECT) {
            enrf_stop_scan();
            app_timer_stop(m_blink_timer);
            SET_LED(BSP_BOARD_LED_1, true);
            m_state = IDLE;
            NRF_LOG_INFO("Connecting to: %s", enrf_addr_to_str(&m_client_addr));
            enrf_connect_to(&m_client_addr, NULL, nus_c_rx_cb);
        } else if (m_state == DISCONNECT || m_state == TIMEOUT) {
            m_state = IDLE;
            enrf_disconnect();
            NRF_LOG_INFO("Disconnected");
            SET_LED(BSP_BOARD_LED_1, false);
            if (m_state == TIMEOUT) {
                // Show error LED
                SET_LED(BSP_BOARD_LED_0, true);
            }
            NRF_LOG_INFO("Idle");
            app_timer_start(m_idle_timer, APP_TIMER_TICKS(10000), (void *)true);
        }
        enrf_wait_for_event();
    }
}
