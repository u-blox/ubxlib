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

/* Only ubxlib header files which are intended to be public, i.e.
 * are in an api directory and are not test or example files, should
 * be included here.  If you find you have to include in here a
 * file which is not in an api directory in order to compile the
 * ubxlib code then likely something is wrong, please check! */

/** \addtogroup ubxlib Headers
 *  @{
 */

/** @file
 * @brief This header file includes all of the public ubxlib headers.
 * It is intended for use in customer code to bring in all of the
 * ubxlib APIs.
 */

#ifndef _U_UBXLIB_H_
#define _U_UBXLIB_H_

#ifdef U_CFG_OVERRIDE
// For a customer's configuration override
# include <u_cfg_override.h>
#endif

// Types from C standard headers
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Files containing macros that are used in APIs
#include <u_cfg_sw.h>
#include <u_compiler.h>
#include <u_error_common.h>
#include <u_assert.h>

// Porting APIs
#include <u_port.h>
#include <u_port_crypto.h>
#include <u_port_debug.h>
#include <u_port_event_queue.h>
#include <u_port_gatt.h>
#include <u_port_gpio.h>
#include <u_port_os.h>
#include <u_port_uart.h>
#include <u_port_i2c.h>

// Module types: used in common APIs hence must come first
#include <u_ble_module_type.h>
#include <u_cell_module_type.h>
#include <u_gnss_module_type.h>
#include <u_wifi_module_type.h>

// Other common APIs
#include <u_device.h>
#include <u_network.h>
#include <u_network_config_ble.h>
#include <u_network_config_cell.h>
#include <u_network_config_gnss.h>
#include <u_network_config_wifi.h>
#include <u_base64.h>
#include <u_hex_bin_convert.h>
#include <u_mempool.h>
#include <u_ringbuffer.h>
#include <u_time.h>
#include <u_debug_utils.h>
#include <u_at_client.h>
#include <u_security.h>
#include <u_security_credential.h>
#include <u_security_tls.h>
#include <u_sock.h>
#include <u_sock_errno.h>
#include <u_sock_security.h>
#include <u_mqtt_common.h>
#include <u_mqtt_client.h>
#include <u_http_client.h>
#include <u_location.h>
#include <u_ubx_protocol.h>
#include <u_spartn.h>
#include <u_spartn_crc.h>
#include <u_short_range.h>
#include <u_short_range_pbuf.h>
#include <u_short_range_edm_stream.h>
#include <u_short_range_sec_tls.h>
#include <u_short_range_module_type.h>

// BLE/cellular/GNSS/Wi-Fi APIs
#include <u_ble.h>
#include <u_ble_cfg.h>
#include <u_ble_sps.h>
#include <u_cell_net.h>
#include <u_cell.h>
#include <u_cell_cfg.h>
#include <u_cell_file.h>
#include <u_cell_gpio.h>
#include <u_cell_info.h>
#include <u_cell_loc.h>
#include <u_cell_mqtt.h>
#include <u_cell_http.h>
#include <u_cell_pwr.h>
#include <u_cell_sec.h>
#include <u_cell_sec_tls.h>
#include <u_cell_sock.h>
#include <u_cell_fota.h>
#include <u_gnss_type.h>
#include <u_gnss.h>
#include <u_gnss_cfg_val_key.h>
#include <u_gnss_cfg.h>
#include <u_gnss_info.h>
#include <u_gnss_pos.h>
#include <u_gnss_pwr.h>
#include <u_gnss_msg.h>
#include <u_gnss_util.h>
#include <u_wifi.h>
#include <u_wifi_cfg.h>
#include <u_wifi_mqtt.h>
#include <u_wifi_sock.h>

/** @}*/

#endif // _U_UBXLIB_H_

// End of file
