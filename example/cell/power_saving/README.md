# Introduction
A u-blox cellular module has two sleep states and three ways to get to them.  A quick summary is as follows:

- this code automatically configures the cellular module for "32 kHz sleep"; it can do this because this sleep mode has no adverse effect on the application, does not need to be configured by the application, etc.,
- a typical application using a module that supports Cat-M1 or NB1 may then configure E-DRX, with timings of the application's choosing, to save more power by allowing the module to switch its radio off for longer periods; [cell_power_saving_e_drx_main.c](cell_power_saving_e_drx_main.c) is an example of how to configure E-DRX,
- a very sleepy application, one which perhaps wakes up just a few times a day, again when using a module that supports Cat-M1 or NB1, may instead configure 3GPP power saving to save the most power, provided that application is happy to lose all module state (sockets, MQTT broker connections, etc.) on entry to sleep; [cell_power_saving_3gpp_main.c](cell_power_saving_3gpp_main.c) is an example of how to configure 3GPP power saving.  For this example to work both the `PWR_ON` pin of the cellular module and the `VINT` pin of the cellular module have to be connected to this MCU.

# Detailed Description
The two sleep states of a u-blox cellular module are as follows:

- "UART sleep"/"32 kHz sleep": in this sleep state the speed of the module's clocks are reduced to save a lot of power.  Because of these reduced clock rates the module is not able to drive the UART HW, hence this is often termed "UART sleep".  However, all of the module's RAM is still on, state is fully retained, the module is still actually running, is still connected to the network, and it can be woken-up quickly by toggling lines of the UART AT interface.

- "deep sleep": in this sleep state the module is basically off, almost all state is lost, what is retained is only a basic notion of time and whether the module was attached to the cellular network when deep sleep began.  The IP stack on the module, the MQTT client on the module, etc, are all reset by deep sleep.  To exit from deep sleep the module `PWR_ON` pin must be toggled, hence it is a requirement that a pin of your MCU is connected to the `PWR_ON` pin of the module for deep sleep to be entered.  It is also a requirement that this code is able to detect that the module has entered deep sleep, so for this purpose the `VINT` pin of the module must be connected to a pin of this MCU; note that while there is an unsolicited result code or URC, `+UUPSMR`, which can indicate the module sleep state, this does not actually indicate whether the module is about to enter deep asleep or not, only whether the cellular protocol stack inside the module has entered a deactivated state, hence it is not possible to use this as a true indication of deep sleep.

The three ways of entering these sleep states are as follows:

- `AT+UPSV`: this command permits the module to enter "32 kHz sleep" after a given amount of inactivity.  `ubxlib` enables `AT+UPSV` power saving automatically with a timer of 6 seconds and wakes the module up again as required by the application.  You need do nothing.  If necessary, this behaviour (and hence all power saving behaviours) can be disabled by applying the conditional compilation flag `U_CFG_CELL_DISABLE_UART_POWER_SAVING`.

- "E-DRX": this is 3GPP-defined and forms an agreement with the network that the module will be out of contact for short periods (think 10's or 100's, at most 1000's of seconds) so that the module can save power.  The functions with "EDrx" in the name allow you to initiate and manage E-DRX.  This is something you, the application writer, must do, since the timings, the required wakefulness, is something only the application can know.  During the "sleep" periods of E-DRX the module is in 32 kHz sleep but it can also power the cellular radio down and hence save a lot more power.  And because this code only allows the module to go into 32 kHz sleep during the E-DRX sleep periods the application never has to worry about state being lost.  Note that the cellular module _does_ support entry into deep sleep during E-DRX but that is of no use in the `ubxlib` context, since all sockets/MQTT connection etc. would be lost every E-DRX seconds, where E-DRX seconds is usually a few 10's of seconds, and the cost/complexity of recovering state would outweigh the benefit.

- "3GPP power saving mode (PSM)": also a 3GPP-defined mechanism, this forms an agreement with the network that the module will be out of contact for long periods (think hours or days).  The functions with "3gppPowerSaving" in the name allow you to initiate and manage 3GPP power saving.  During the sleep periods of 3GPP power saving mode the module enters deep sleep, all state aside from the knowledge of its cellular connection with the network is lost; module sockets/MQTT, etc. are reset.  It is like the module is actually switched off except that the network _knows_ it is off and maintains that knowledge so that when the module leaves deep sleep it doesn't necessarily have to contact the network to tell it, the two are behaving according to their 3GPP power saving agreement.  Since the module is almost entirely off during 3GPP sleep, things such as waiting for an answer from a cloud service, waiting for an attached GNSS module to do something, all of these long-term things, will be curtailed if deep sleep were to be entered; it is up to the application-writer to ensure that 3GPP power saving is configured appropriately, considering what the cellular module has been asked to do.

"E-DRX" and "3GPP power saving" are **only** supported on EUTRAN-based RATs, e.g. Cat-M1 or NB1.

# Usage
To build and run these examples on a supported platform you need to travel down into the `port/platform/<platform>/mcu/<mcu>` directory of your choice and find the `runner` build.  The instructions there will tell you how to set/override \#defines.  The following \#defines are relevant:

`U_CFG_APP_FILTER`: set this to `exampleCellPowerSaving` (noting that NO quotation marks should be included) to run *just* these examples, as opposed to all the examples and unit tests.

For the remainder of the \#defines you may either override their values in the same way or, if you are only running these examples, you may edit the values directly in the `xxx_main.c` files before compiling.

`U_CFG_TEST_CELL_MODULE_TYPE`: consult [u_cell_module_type.h](/cell/api/u_cell_module_type.h) to determine the type name for the cellular module you intend to use.  For instance, to use SARA-R5 you would set `U_CFG_TEST_CELL_MODULE_TYPE` to `U_CELL_MODULE_TYPE_SARA_R5`.

`U_CFG_APP_PIN_CELL_xxx`: the default values for the MCU pins connecting your cellular module to your MCU are \#defined in the file [port/platform](/port/platform)`/<platform>/mcu/<mcu>/cfg/cfg_app_platform_specific.h`.  You should check if these are correct for your board and, if not, override the values of the \#defines (where -1 means "not connected").  For the 3GPP sleep case, be sure that a non-negative value is given for both `U_CFG_APP_PIN_CELL_PWR_ON` and `U_CFG_APP_PIN_CELL_VINT` and that, obviously, the relevant physical pins of your MCU are connected to those of the cellular module.

`U_CFG_APP_CELL_UART`: this sets the internal HW UART block that your chosen MCU will use to talk to the cellular module.  The default is usually acceptable but if you wish to change it then consult the file [port/platform](/port/platform)`/<platform>/mcu/<mcu>/cfg/cfg_hw_platform_specific.h` for other options.

`MY_RAT`: to keep the example simple, the chosen RAT (Radio Access Technology) is set by the example code; a default value of `U_CELL_NET_RAT_CATM1` is provided in  [cell_power_saving_e_drx_main.c](cell_power_saving_e_drx_main.c) and [cell_power_saving_3gpp_main.c](cell_power_saving_3gpp_main.c).  Power saving is only supported on ETURAN RATs, e.g. `U_CELL_NET_RAT_CATM1` or `U_CELL_NET_RAT_NB1`; consult the data sheet for your module to see which RATs your module supports from the types in `uCellNetRat_t`, [u_cell_net.h](/cell/api/u_cell_net.h).

`ACTIVE_TIME_SECONDS`: the requested active time for the 3GPP power saving example; a default value is provided in [cell_power_saving_3gpp_main.c](cell_power_saving_3gpp_main.c).

`PERIODIC_WAKEUP_SECONDS`: the requested periodic wake-up time for the 3GPP power saving example; a default value is provided in [cell_power_saving_3gpp_main.c](cell_power_saving_3gpp_main.c).

`EDRX_SECONDS`: the requested E-DRX period for the E-DRX example; a default value is provided in [cell_power_saving_e_drx_main.c](cell_power_saving_e_drx_main.c).

Obviously you will need a SIM in your board, an antenna connected and you may need to know the APN associated with the SIM.