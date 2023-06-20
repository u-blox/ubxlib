# A Note About Testing WiFi Captive Portal

The test `wifiCaptivePortal()` requires:

1) a (test peer)[test_peer]: a Wifi client which is constantly trying to connect to `UBXLIB_CAPTIVE_PORTAL` (see [u_wifi_captive_portal_test.c](../u_wifi_captive_portal_test.c)); this is acting as the mobile phone which is trying to configure the captive portal `UBXLIB_CAPTIVE_PORTAL` to point to, say, a domestic WiFi AP,
2) a test access point, the SSID and password of which is programmed into (1): this is pretending to be the domestic WiFi AP, the ultimate destination that the device under test is being directed towards.

The code for the test peer can be found in (test peer)[test_peer] and will work on any standard ESP32 board.  A standard u-blox Wifi board running uConnectExpress can be configured to act as (2) with the following command sequence:

```
AT+UWSC=0,0,0
OK
AT+UWAPCA=0,0
OK
AT+UWAPC=0,0,0
OK
AT+UWAPC=0,2,"test_peer_no_internet"
OK
AT+UWAPC=0,5,2
OK
AT+UWAPC=0,8,"test_peer_no_internet_password"
OK
AT+UWAPC=0,0,1
OK
AT+UWAPC=0,106,1
OK
AT+UWAPCA=0,1
OK
```

...then reboot the board and it should appear as an access point with SSID `test_peer_no_internet`, password `test_peer_no_internet_password`.

Program this SSID/password pair into the (test peer)[test_peer] and you can leave both boards powered-up, requiring no other interaction to do their work.