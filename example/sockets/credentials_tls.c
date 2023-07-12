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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief certificates and keys for the sockets-over-TLS example.
 */

// Bring in all of the ubxlib public header files
#include "ubxlib.h"

// Must use quoted includes here to pick up the local file
// without it having to be on the include path
#include "credentials_tls.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** A client certificate, generated for use with the uxblib echo
 * server.
 */
const char *gpUEchoServerClientCertPem = "-----BEGIN CERTIFICATE-----"
                                         "MIICSjCCAdACFD+js1Fht6STx4lF3zGisrnThT4iMAoGCCqGSM49BAMDMIGFMQsw"
                                         "CQYDVQQGEwJVUzELMAkGA1UECAwCV0ExEDAOBgNVBAcMB1RoYWx3aWwxDzANBgNV"
                                         "BAoMBnUtYmxveDELMAkGA1UECwwCY2ExFzAVBgNVBAMMDnd3dy51LWJsb3guY29t"
                                         "MSAwHgYJKoZIhvcNAQkBFhF1YnhsaWJAdS1ibG94LmNvbTAgFw0yMzA3MDkwODI3"
                                         "NDBaGA8yMTIzMDYxNTA4Mjc0MFowgYkxCzAJBgNVBAYTAlVTMQswCQYDVQQIDAJX"
                                         "QTEQMA4GA1UEBwwHVGhhbHdpbDEPMA0GA1UECgwGdS1ibG94MQ8wDQYDVQQLDAZj"
                                         "bGllbnQxFzAVBgNVBAMMDnd3dy51LWJsb3guY29tMSAwHgYJKoZIhvcNAQkBFhF1"
                                         "YnhsaWJAdS1ibG94LmNvbTB2MBAGByqGSM49AgEGBSuBBAAiA2IABApmNYLlR8Cr"
                                         "S9MAocQX+bUU4+1EkmT61bchs6pf9RVvvbgbLkw2gk/So8vPifo6imJcjWteiIBy"
                                         "xYKKFSIyghz/o0hjmpDz1XoYPtGENrz/dyISP35ZFk9sRJZ4pSX1uDAKBggqhkjO"
                                         "PQQDAwNoADBlAjEA3scFsQb9Aj+lzC34h+AS6RGHLHr81Txm713MHnXjrpe0jEk8"
                                         "bTULtydY8Jyf9c+DAjBMEdAEODaOp5Vn02ZOkKtbm91R6rFS1IZTFJ2MQCALG50C"
                                         "GHviROz1O6YfRcRFTks="
                                         "-----END CERTIFICATE-----";

/** The hash of gpUEchoServerClientCertPem once stored on the module.
 */
const char gUEchoServerClientCertHash[] = {0x33, 0x5f, 0x89, 0x2f, 0x59, 0x84, 0x58, 0x80,
                                           0x93, 0xcc, 0xf1, 0x36, 0xa3, 0x65, 0xe4, 0x57
                                          };

/** The private key to go with uEchoServerClientCertPem.
 */
const char *gpUEchoServerClientKeyPem = "-----BEGIN EC PRIVATE KEY-----"
                                        "MIGkAgEBBDBxQnFRM8oo6gCjmfNNgTdfUQreohEDs1NFIOq84DO3120rKI4Ypf7h"
                                        "xog10lSfhhOgBwYFK4EEACKhZANiAAQKZjWC5UfAq0vTAKHEF/m1FOPtRJJk+tW3"
                                        "IbOqX/UVb724Gy5MNoJP0qPLz4n6OopiXI1rXoiAcsWCihUiMoIc/6NIY5qQ89V6"
                                        "GD7RhDa8/3ciEj9+WRZPbESWeKUl9bg="
                                        "-----END EC PRIVATE KEY-----";

/** The hash of gpUEchoServerClientKeyPem once stored on the module.
 */
const char gUEchoServerClientKeyHash[] = {0x8f, 0xe6, 0xdd, 0xdb, 0x64, 0xb8, 0xf8, 0x2e,
                                          0xa2, 0x52, 0xb2, 0xbb, 0x5e, 0x38, 0x08, 0xe8
                                         };

/** The CA certificate for the ubxlib echo server and the client.
 */
const char *gpUEchoServerCaCertPem = "-----BEGIN CERTIFICATE-----"
                                     "MIICoTCCAiagAwIBAgIUXW8iJeCsbA3ygmXIT3wqxqtZla4wCgYIKoZIzj0EAwIw"
                                     "gYUxCzAJBgNVBAYTAlVTMQswCQYDVQQIDAJXQTEQMA4GA1UEBwwHVGhhbHdpbDEP"
                                     "MA0GA1UECgwGdS1ibG94MQswCQYDVQQLDAJjYTEXMBUGA1UEAwwOd3d3LnUtYmxv"
                                     "eC5jb20xIDAeBgkqhkiG9w0BCQEWEXVieGxpYkB1LWJsb3guY29tMCAXDTIzMDcw"
                                     "OTA4MjY1NloYDzIxMjMwNjE1MDgyNjU2WjCBhTELMAkGA1UEBhMCVVMxCzAJBgNV"
                                     "BAgMAldBMRAwDgYDVQQHDAdUaGFsd2lsMQ8wDQYDVQQKDAZ1LWJsb3gxCzAJBgNV"
                                     "BAsMAmNhMRcwFQYDVQQDDA53d3cudS1ibG94LmNvbTEgMB4GCSqGSIb3DQEJARYR"
                                     "dWJ4bGliQHUtYmxveC5jb20wdjAQBgcqhkjOPQIBBgUrgQQAIgNiAAS5br7n7+wi"
                                     "Mwp5h3BojVn+cH4oZN7ngyfadR961TJZsu/g2arYE8SJTVI+qzQC4KiBb+rTXQIY"
                                     "k9sxEo+mTyJ4BWaVxoWOXjvALNRtyrbls6q36ttXoYsU5UAgNWJiH/ejUzBRMB0G"
                                     "A1UdDgQWBBRKetSAT3SQ45r2l64eXK1vf8sTzDAfBgNVHSMEGDAWgBRKetSAT3SQ"
                                     "45r2l64eXK1vf8sTzDAPBgNVHRMBAf8EBTADAQH/MAoGCCqGSM49BAMCA2kAMGYC"
                                     "MQD7WrRzaAxBikIHPuoDZo7tAdA5Zsbg9axBPS+wm3mdKLGwWjdep2IWLmn/uuFE"
                                     "VlwCMQDXxDnOuuc6p1nzmtrn9JHVE0/+HdeDj6KdnDWWtZJQsagHDAEmld8oEDlg"
                                     "iDO9Bnw="
                                     "-----END CERTIFICATE-----";

/** The hash of gpUEchoServerCaCertPem once stored on the module.
 */
const char gUEchoServerCaCertHash[] = {0xa8, 0x83, 0xa0, 0x2d, 0xe0, 0xad, 0x34, 0x64,
                                       0x26, 0xb3, 0xfb, 0x8a, 0x1b, 0x93, 0x3d, 0x84
                                      };

// End of file
