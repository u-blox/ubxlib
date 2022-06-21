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

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/* ----------------------------------------------------------------
   APN stands for Access Point Name, a setting on your modem or phone
   that identifies an external network your phone can access for data
   (e.g. 3G or 4G Internet service on your phone).

   The APN settings can be forced when calling the join function.
   Below is a list of known APNs that us used if no APN pConfig
   is forced. This list could be extended by other settings.

   For further reading:
   wiki APN: http://en.wikipedia.org/wiki/Access_Point_Name
   wiki MCC/MNC: http://en.wikipedia.org/wiki/Mobile_country_code
   google: https://www.google.de/search?q=APN+list
---------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Helper to generate the APN string.
 */
//lint -emacro((786), _APN) Suppress use of null
//lint -emacro((840), _APN) in the macro
#define _APN(apn, username, password) apn "\0" username "\0" password "\0"

/** Helper to extract a field from the pCfg string.
 */
#define _APN_GET(pCfg) *pCfg ? pCfg : NULL; pCfg  += strlen(pCfg) + 1

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** APN lookup structure.
 */
typedef struct {
    const char *pMccMnc; /**< mobile country code (MCC) and
                              mobile network code (MNC). */
    const char *pCfg;    /**< APN configuration string, use
                              _APN macro to generate. */
} uCellNetApn_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Default APN settings used by many networks.
 */
static const char *const pApnDefault = _APN("internet",,);

/** List of special APNs for different network operators.
 *
 * No need to add default, "internet" will be used as a default if
 * no entry matches.
 * The APN without username/password have to be listed first.
 */
static const uCellNetApn_t gApnLookUpTable[] = {
// MCC Country
//  { /* Operator */ "MCC-MNC[,MNC]" _APN(APN, USERNAME, PASSWORD) },
// MCC must be 3 digits
// MNC must be either 2 or 3 digits
// MCC must be separated by '-' from MNC, multiple MNC can be separated by ','

// 232 Austria - AUT
    { /* T-Mobile */ "232-03",  _APN("m2m.business",,) },

// 460 China - CN
    { /* CN Mobile */"460-00",  _APN("cmnet",,)
        _APN("cmwap",,)
    },
    { /* Unicom */   "460-01",  _APN("3gnet",,)
        _APN("uninet", "uninet", "uninet")
    },

// 262 Germany - DE
    { /* T-Mobile */ "262-01",  _APN("internet.t-mobile", "t-mobile", "tm") },
    { /* T-Mobile */ "262-02,06",
        _APN("m2m.business",,)
    },

// 222 Italy - IT
    { /* TIM */      "222-01",  _APN("ibox.tim.it",,) },
    { /* Vodafone */ "222-10",  _APN("web.omnitel.it",,) },
    { /* Wind */     "222-88",  _APN("internet.wind.biz",,) },

// 440 Japan - JP
//lint -e{786} Suppress concatenation within initialiser
    { /* Softbank */ "440-04,06,20,40,41,42,43,44,45,46,47,48,90,91,92,93,94,95"
        ",96,97,98",
        _APN("open.softbank.ne.jp", "opensoftbank", "ebMNuX1FIHg9d3DA")
        _APN("smile.world", "dna1trop", "so2t3k3m2a")
    },
//lint -e{786} Suppress concatenation within initialiser
    { /* NTTDoCoMo */ "440-09,10,11,12,13,14,15,16,17,18,19,21,22,23,24,25,26,27,"
        "28,29,30,31,32,33,34,35,36,37,38,39,58,59,60,61,62,63,"
        "64,65,66,67,68,69,87,99",
        _APN("bmobilewap",,) /*BMobile*/
        _APN("mpr2.bizho.net", "Mopera U",) /* DoCoMo */
        _APN("bmobile.ne.jp", "bmobile@wifi2", "bmobile") /*BMobile*/
    },

// 204 Netherlands - NL
    { /* Vodafone */ "204-04",  _APN("public4.m2minternet.com",,) },

// 293 Slovenia - SI
    { /* Si.mobil */ "293-40",  _APN("internet.simobil.si",,) },
    { /* Tusmobil */ "293-70",  _APN("internet.tusmobil.si",,) },

// 240 Sweden SE
    { /* Telia */    "240-01",  _APN("online.telia.se",,) },
    { /* Telenor */  "240-06,08",
        _APN("services.telenor.se",,)
    },
    { /* Tele2 */    "240-07",  _APN("mobileinternet.tele2.se",,) },

// 228 Switzerland - CH
    { /* Swisscom */ "228-01",  _APN("gprs.swisscom.ch",,) },
    { /* Orange */   "228-03",  _APN("internet",,) /* contract */
        _APN("click",,)    /* pre-pay */
    },

// 234 United Kingdom - GB
    { /* Telefonica */       "234-02,10,11",
        _APN("mobile.o2.co.uk", "faster", "web") /* contract */
        _APN("mobile.o2.co.uk", "bypass", "web") /* pre-pay */
        _APN("payandgo.o2.co.uk", "payandgo", "payandgo")
    },
    { /* Vodafone */ "234-15",  _APN("internet", "web", "web") /* contract */
        _APN("pp.vodafone.co.uk", "wap", "wap")  /* pre-pay */
    },
    { /* Three */    "234-20",  _APN("three.co.uk",,) },
    { /* Jersey */   "234-50",  _APN("jtm2m",,) /* as used on u-blox C030 U201 boards */ },

// 310 United States of America - US
    { /* T-Mobile */ "310-026,260,490",
        _APN("epc.tmobile.com",,)
        _APN("fast.tmobile.com",,) /* LTE */
    },
    { /* AT&T */     "310-030,150,170,260,410,560,680",
        _APN("phone",,)
        _APN("wap.cingular", "WAP@CINGULARGPRS.COM", "CINGULAR1")
        _APN("isp.cingular", "ISP@CINGULARGPRS.COM", "CINGULAR1")
    },

// 901 International - INT
    { /* Transatel */ "901-37", _APN("netgprs.com", "tsl", "tsl") },

// 214 Spain
    { /* Telefonica */       "214-07",
        _APN("m2mtrial.telefonica.com",,) /* Cat-M1 */
    },
};

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Configuring APN by extraction from IMSI and matching the table.
 *
 * @param pImsi  string containing IMSI.
 * @return       the APN string.
 */
static const char *pApnGetConfig(const char *pImsi)
{
    const char *pConfig = NULL;
    const char *pStr;
    size_t length;

    if ((pImsi != NULL) && (*pImsi != '\0')) {
        // Many carriers use internet without username and password,
        // so use this as default now try to lookup the setting
        // for our table
        for (size_t x = 0; x < sizeof(gApnLookUpTable) / sizeof(*gApnLookUpTable) &&
             !pConfig; x++) {
            pStr = gApnLookUpTable[x].pMccMnc;
            // Check the MCC
            if (memcmp(pImsi, pStr, 3) == 0) {
                pStr += 3;
                // Check all the MNC, MNC length can be 2 or 3 digits
                while (((*(pStr + 0) == '-') || (*(pStr + 0) == ',')) &&
                       (*(pStr + 1) >= '0') && (*(pStr + 1) <= '9') &&
                       (*(pStr + 2) >= '0') && (*(pStr + 2) <= '9') && !pConfig) {
                    length = ((*(pStr + 3) >= '0') && (*(pStr + 3) <= '9')) ? 3 : 2;
                    if (memcmp(pImsi + 3, pStr + 1, length) == 0) {
                        pConfig = gApnLookUpTable[x].pCfg;
                    }
                    pStr += length + 1;
                }
            }
        }
    }

    // use default if not found
    if (!pConfig) {
        pConfig = pApnDefault;
    }

    return pConfig;
}

#ifdef __cplusplus
}
#endif

// End of file
