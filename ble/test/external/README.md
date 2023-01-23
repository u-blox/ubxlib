# External support applications for BLE testing

These directories contains source for building client and server applications to be used in Nina-B3 evks which will then support the BLE GAP and GATT test cases in the above directory.

The firmware has to be built using **easy_nrf52** which is available at the following location: https://github.com/plerup/easy_nrf52

The hex directory however contains ready made complete flash images for the applications and can be flashed directly using the *nrfjprog* tool.

Server:

    nrfjprog -f nrf52 --sectorerase --program hex_files/ble_nus_server_ninab3_evk_all.hex --reset

Client:

    nrfjprog -f nrf52 --sectorerase --program hex_files/ble_nus_client_ninab3_evk_all.hex --reset

When active the boards will blink the onboard led, blue for client and red for server.