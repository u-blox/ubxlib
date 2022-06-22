# Introduction
Configuring a cellular module is more complex than configuring a Wi-Fi or BLE module.  For any cellular module the APN (Access Point Name) field may be required but for an LTE cellular module (Cat-M1 and NB1 are both types of LTE) the following additional parameters come into play:

- choice of Radio Access Technology (RAT) or technologies,
- choice of RF bands,
- choice of Mobile Network Operator (MNO) profile.

The code in this example shows you how to go about setting these parameters.

# Usage
To build and run this examples on a supported platform you need to travel down into the `port/platform/<platform>/mcu/<mcu>` directory of your choice and find the `runner` build.  The instructions there will tell you how to set/override defines.  The following \#defines are relevant:

`U_CFG_APP_FILTER`: set this to `exampleCellLteCfg` (noting that NO quotation marks should be included) to run *just* this example, as opposed to all the examples and unit tests.

For the remainder of the \#defines you may either override their values in the same way or, if you are only running this example, you may edit the values directly in `lte_cfg_main.c` before compiling.

`U_CFG_TEST_CELL_MODULE_TYPE`: consult [u_cell_module_type.h](/cell/api/u_cell_module_type.h) to determine the type name for the cellular module you intend to use.  For instance, to use SARA-R5 you would set `U_CFG_TEST_CELL_MODULE_TYPE` to `U_CELL_MODULE_TYPE_SARA_R5`.

`U_CFG_APP_PIN_CELL_xxx`: the default values for the MCU pins connecting your cellular module to your MCU are \#defined in the file [port/platform](/port/platform)`/<platform>/mcu/<mcu>/cfg/cfg_app_platform_specific.h`.  You should check if these are correct for your board and, if not, override the values of the \#defines (where -1 means "not connected").

`U_CFG_APP_CELL_UART`: this sets the internal HW UART block that your chosen MCU will use to talk to the cellular module.  The default is usually acceptable but if you wish to change it then consult the file [port/platform](/port/platform)`/<platform>/mcu/<mcu>/cfg/cfg_hw_platform_specific.h` for other options.

And then, for the specifics of this example:

`MY_MNO_PROFILE`: this single number will set a broad selection of sensible parameters for you, consult the u-blox AT command manual for your module to find out the possible values; 100, for example, is "Europe", 90 is "global".

`MY_RAT0`, `MY_RAT1`, `MY_RAT2`: the RATs you want the module to use, in priority order; consult the data sheet for your module to see which RATs, and how may RATs simultanously, your module supports from the types in `uCellNetRat_t`, [u_cell_net.h](/cell/api/u_cell_net.h).

`MY_CATM1_BANDMASK1`, `MY_CATM1_BANDMASK2`, `MY_NB1_BANDMASK1`, `MY_NB1_BANDMASK2`: consult the data sheet for your module to see which RF bands it supports in cat-M1 and NB1 modes respectively.  This is very much the advance class, leave the values alone unless you know what you are doing.

Obviously you will need a SIM in your board, an antenna connected and you may need to know the APN associated with the SIM.

# Background
You don't need to read the stuff below: this is addressed in the code. However, read on if you want some background.

## MNO Profile
Some mobile network operators are very specific about how a module should be configured for their network.  To help with this, most u-blox modules include the concept of an MNO profile, which aggregates these settings into a single number, and includes generic "area" numbers.  For instance, setting MNO profile 100 will cause a good setup for Europe to be employed.  Check the u-blox AT command manual for the MNO profile settings supported by your module.  This feature is specific to u-blox modules.

It is important to note that *setting* an MNO profile will cause the module to change many things: bands, registration modes, etc, potentially all the things below.  Hence it is always a good idea to set the MNO profile first, before setting anything else; then you are making small delta-changes from a known base.

It is also worth noting that MNO profile 0 is a special "none" profile; a freshly delivered module might turn out to be in MNO profile 0.  If so, it must be set to a non-zero MNO profile before it will operate.

## APN
The APN associates you with an IP network and the organisation that takes your money for the privilege: organisations such as Vodafone or Virgin or Thingstream.  Where the organisation is a 3GPP cellular network operator (e.g. Vodafone) and you have normal IP network requirements (i.e. a connection to the public internet) the APN field can be set to `NULL` and things will sort themselves out.

If the organisation is a virtual network operator (e.g. Virgin) then the first 5 or 6 digits of the IMSI of your SIM card may be unique to that virtual network operator; if you set your APN to `NULL` this code will look up those digits in an internal table [u_cell_apn_db.h](/cell/src/u_cell_apn_db.h) and will try the APNs listed against them there.  This table is not complete so a look-up may fail, in which case the most used default APN "internet" will be tried.

If you have very specific IP service arrangements (e.g. a direct connection to a private network only) or your service is via an intermediary such as Thingsteam who can provide cheaper connections, the SIM card may not carry enough information for the network to know how to connect you and who to bill.  In those situations you must specify the APN explicitly; in the Thingstream case the APN is "tsiot".

If the APN is incorrect and you are using an LTE network, registration will fail; "Deny" will likely appear in the text logs printed-out by this code.  If you are using a 2G or 3G network (e.g. SARA-U201, or SARA-R412M operating in GPRS mode) registration may succeed but you will likely still be unable to exchange data over the network.

If you are unable to register with the network and see "Deny" in the logs printed out by this code, or you believe your module has registered with the network but is unable to transfer any data, try specifying the APN if you were previously using `NULL`, or check that the `APN` you _are_ using is correct (case sensitive, no trailing spaces, etc.).

## RAT
2G, GPRS, 3G, LTE, Cat-M1 and NB1 are all examples of RATs; a combination of modulation schemes/multiplexing schemes and data transfer capabilities defined by 3GPP and used to access the cellular network.  Most u-blox modules support more than one RAT.  You can configure the module to use just one RAT or to use a combination of RATs in an order you specify.  The amount of time a module takes to find the network at your specific location within each RAT varies; in particular, NB1, which is designed to work with very low signal levels, can take a long time to find the network ON THE FIRST ATTEMPT since the network signals can be very hard to find.  To gain service the RAT you chose must be one that it is broadcast by your network provider at your location.

If you are having difficulty registering with a network, try setting just a single RAT at first: if GPRS is an option for your module, chose that, otherwise select Cat-M1 if your network supports it at your location and finally chose NB1 if both your module and the network support it.  If you wish to specify more than one RAT then put 2G/GPRS/3G first in the list, LTE/Cat-M1 second in the list (these can be searched relatively quickly) and NB1 last in the list (because of the potentially long initial search time).  Check the data sheet for your u-blox cellular module to determine which RATs it supports.

## RF Bands
2G/GPRS/3G was served by at most four RF bands across the globe, all of which could be searched rapidly.  For LTE/Cat-M1/NB1 there are many tens of RF bands which take some considerable time to search.  Hence it is sensible to configure the module with the right set of RF bands for the situations you anticipate. [u_cell_cfg.h](/cell/api/u_cell_cfg.h) includes some `#defines`, `U_CELL_CFG_BAND_MASK_x_NORTH_AMERICA_CATM1_DEFAULT` and `U_CELL_CFG_BAND_MASK_x_EUROPE_NB1_DEFAULT` to get you started.  Check the data-sheet for your u-blox cellular module to determine which bands it supports: not all modules support all bands and a band configuration will be rejected by the module if it does not support one bit in one bit-position of the proposed band mask.  Note that this is very much the advanced class: normally selecting the correct MNO profile will also select the appropriate RF bands.
