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
                                         "MIIDiDCCAnACCQC8IOP+9fCfSTANBgkqhkiG9w0BAQsFADCBhTELMAkGA1UEBhMC"
                                         "VVMxCzAJBgNVBAgMAldBMRAwDgYDVQQHDAdUaGFsd2lsMQ8wDQYDVQQKDAZ1LWJs"
                                         "b3gxCzAJBgNVBAsMAklUMRcwFQYDVQQDDA53d3cudS1ibG94LmNvbTEgMB4GCSqG"
                                         "SIb3DQEJARYRdWJ4bGliQHUtYmxveC5jb20wHhcNMjAxMDI2MjMzNDI3WhcNMjEx"
                                         "MDI2MjMzNDI3WjCBhTELMAkGA1UEBhMCVVMxCzAJBgNVBAgMAldBMRAwDgYDVQQH"
                                         "DAdUaGFsd2lsMQ8wDQYDVQQKDAZ1LWJsb3gxCzAJBgNVBAsMAklUMRcwFQYDVQQD"
                                         "DA53d3cudS1ibG94LmNvbTEgMB4GCSqGSIb3DQEJARYRdWJ4bGliQHUtYmxveC5j"
                                         "b20wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQD5W8MnZEA9Dzl9/dGE"
                                         "ObXC13ZnKZVdjiR4/pyDEGzzMzS8JdJKMNE7GhczNoLY8pZYzWdfLidwgJm59hZ2"
                                         "oj4ddqqeXsa5wMY2zZXICygNpRdI8HJ/59tyoIYy4FR8yxUHDOfM2B+fqbw00+ER"
                                         "UBGEvbkF4F3xxheQ0jE77QPsEq0xj+rXeicGkZNISTocOWzwOgj9hD3NvIpNGG34"
                                         "quX5rT3V/ELsXHh15cTkH0isAa4uM89qKhIQQfKKQBh12hzK0rI5PbJaM3KrxjN3"
                                         "UYq+kRslNMNJHYtYDIowTAGgB8QfqydYtWxjk5Cfi9igfEirgxqQUM3IzC3XsDFM"
                                         "11MJAgMBAAEwDQYJKoZIhvcNAQELBQADggEBABxmy0IVEbppUaCxSbW0YrREbqWk"
                                         "k2VT8erUyPhAlbmyLbD+VrwYNSedWQlkVbIeU+N7OJ/RtJT3Nno84clczTe1pB7p"
                                         "t7vXGM1EG1t/EBrEreyMXJKmLItnuO1btxhQXcU619x1SY65NeqX4Gv7X2r14Ij5"
                                         "1IwueTuzXT+iWD89eIxrNWPFI+6Xwxcm05smdukuX1Hiq2VVoqDbJKRN4FfPowFy"
                                         "2MLsDlw0bYZGNyaIBweb2NJH2zU/qPHLVKrMf3LAx35sv+nq4vDnZS/Nn/vI2MD7"
                                         "mVbRICHB7Zg8UoNnPToqy1o8xqqUB4h3KKIgHtw4tLtPZlaM3AiDSdkZQ6g="
                                         "-----END CERTIFICATE-----";

/** The hash of gpUEchoServerClientCertPem once stored on the module.
 */
const char gUEchoServerClientCertHash[] = {0x76, 0x1a, 0x0c, 0xa6, 0x95, 0xd0, 0x88, 0x58,
                                           0x30, 0xe9, 0x21, 0x02, 0xce, 0x2b, 0x4b, 0x08
                                          };

/** The private key to go with uEchoServerClientCertPem.
 */
const char *gpUEchoServerClientKeyPem = "-----BEGIN RSA PRIVATE KEY-----"
                                        "MIIEpQIBAAKCAQEA+VvDJ2RAPQ85ff3RhDm1wtd2ZymVXY4keP6cgxBs8zM0vCXS"
                                        "SjDROxoXMzaC2PKWWM1nXy4ncICZufYWdqI+HXaqnl7GucDGNs2VyAsoDaUXSPBy"
                                        "f+fbcqCGMuBUfMsVBwznzNgfn6m8NNPhEVARhL25BeBd8cYXkNIxO+0D7BKtMY/q"
                                        "13onBpGTSEk6HDls8DoI/YQ9zbyKTRht+Krl+a091fxC7Fx4deXE5B9IrAGuLjPP"
                                        "aioSEEHyikAYddocytKyOT2yWjNyq8Yzd1GKvpEbJTTDSR2LWAyKMEwBoAfEH6sn"
                                        "WLVsY5OQn4vYoHxIq4MakFDNyMwt17AxTNdTCQIDAQABAoIBADluLvZFmp31gbJI"
                                        "4RZpDDnB0h1UcHhJopDTY0y0XcNtibnDpDk+IRJRogJDjcNVq9bsB+DeCmtY0w8H"
                                        "ZIkSOOgkSouLHI3vnjdFBjg6iZEK8t/zsQtQZTRzUDUrgYn0Y/VpvYFqTW5Cc3xf"
                                        "SDjqjf5ai+CUmk5y5z6NipVYs0yNVD9W+8fQUfs3wSrQzmmXSy87P1qaOePp5JxA"
                                        "D6dDlWSXPRgHfj+mgk/l7BoCRvwWrRGgf62PDD8j21dESyr3KOqSSshns64Hbdd4"
                                        "2rl4+i5Ylxy6aDtaT5hWb+1G6aXjogiNu/8s5hJFEBwBEY8OgdPDrSD495+yTDN1"
                                        "fxwqp+ECgYEA/uQKnX5WWbZGzWmuykBq1M+YlRSxjS6ICv2vezHSa98JKHzX3u4K"
                                        "Iv6UzX6YGIHZA6d6YlP5hHD8IcpezsUE5mykzhTx2Sqc81WMsN4wc2CkbIg3vVr5"
                                        "kJhx3NGzp/vDYwJkI3TiiRUmRAru2fdnvyxLd7vo0uldxuvkHvAoQIUCgYEA+nGO"
                                        "vz2YMAfo3v7Ctse7HE7KHSSH2jTVToea8v3gxsMUq/gEXVwIJlfeJQVsk9FTfQiL"
                                        "4d/40x9jwmHKP4+ebq5QQ4yPfCVTM2goLF20hExFzRXsjij73ntA2b4iuy8modT/"
                                        "BqfwPf2bw5/SMJ+mLYh8cjXTMYIDynLf8Ix7cbUCgYEAiWFp40cjzYi0EqTig7pC"
                                        "ml8l0zxrEjhBNQNUoKbSzjdRXVQkmdBdAE2M8FFKMvNRf2m2SecO9nZbPu8vOGzy"
                                        "XiuyjCy3yZ/xJio3AWFQZe9xz9l/iXzOREQWIrmYBnNo9SVlycKHEvGmRUhLQonZ"
                                        "ji2Wo3tRWtRTKhMcShyQ5W0CgYEAt9oLb+sYwRHda27cpG/ltXdFurUpog+tE9RK"
                                        "9N1ZWLC3iTMuiRbZyMQyiTz9I1rFDoHqpqvUL7DYfEdrwNN+/EOtGpmicAG6nX92"
                                        "FnPH5GNVzqOsoAQIOqCC0BZbysxncOA7Q7ifjfKSmb7G//kDdmO+790BqFOI0uMX"
                                        "8LBAow0CgYEA3qNhRhNxrg25kM+wqlkjJ+fo3jQ68r5VB8u/KLQ/Di6WnzNwOQlx"
                                        "QuSxkmMtDPNzhxMhm+IMMwzT1Z8ZyTcWhacMptXXcKrO0gboBIknRlVzSykUqpf/"
                                        "YH1TkviZaurGrrpZHWXN4/z91wqISl/B6SPoom/4ribwGB7+c3e398M="
                                        "-----END RSA PRIVATE KEY-----";

/** The hash of gpUEchoServerClientKeyPem once stored on the module.
 */
const char gUEchoServerClientKeyHash[] = {0x89, 0xed, 0xba, 0xc2, 0x3d, 0x6a, 0xd9, 0xa9,
                                          0x7d, 0xa4, 0x08, 0x4a, 0x1d, 0x28, 0x01, 0x05
                                         };

/** The certificate of the ubxlib echo server.
 */
const char *gpUEchoServerServerCertPem = "-----BEGIN CERTIFICATE-----"
                                         "MIID3zCCAsegAwIBAgIJAINm5Mhtj3MbMA0GCSqGSIb3DQEBCwUAMIGFMQswCQYD"
                                         "VQQGEwJVUzELMAkGA1UECAwCV0ExEDAOBgNVBAcMB1RoYWx3aWwxDzANBgNVBAoM"
                                         "BnUtYmxveDELMAkGA1UECwwCSVQxFzAVBgNVBAMMDnd3dy51LWJsb3guY29tMSAw"
                                         "HgYJKoZIhvcNAQkBFhF1YnhsaWJAdS1ibG94LmNvbTAeFw0yMDEwMjYyMzMyNTNa"
                                         "Fw0yMTEwMjYyMzMyNTNaMIGFMQswCQYDVQQGEwJVUzELMAkGA1UECAwCV0ExEDAO"
                                         "BgNVBAcMB1RoYWx3aWwxDzANBgNVBAoMBnUtYmxveDELMAkGA1UECwwCSVQxFzAV"
                                         "BgNVBAMMDnd3dy51LWJsb3guY29tMSAwHgYJKoZIhvcNAQkBFhF1YnhsaWJAdS1i"
                                         "bG94LmNvbTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALVsmrHdFsS3"
                                         "Zzc25sjLLnEsHANvDRQXn0J1zbDP/o5yJD77AawkAlfmapZX5wlSkr6owOfQ5LKE"
                                         "xWP/xoJiw5EgpZGMuUgqU6diZ2TSh+TACA08KGmb1pfXq6XzmMo23B+pxK0HNEpk"
                                         "cYJlmylpkiRMzI0VTzX0u7aGNLvdEqizryzzDZDtn5yK6z/UFf3RngYsvGoPz5n0"
                                         "eoSV5m8VPc9lHaihgxU0SSyXi/kV27236dnPSE6RknY7QeySVnzx9H2wiaL/A0pE"
                                         "Sg36Gh5BXvyLf+SCgTrvziyCl5EQetd4LYWcBb1MgyFdgSWs+2eiKyv6V4bCdUpG"
                                         "e1YJQ/bUD2UCAwEAAaNQME4wHQYDVR0OBBYEFPwmrNbS0hPKnfqyDUcT1wJBhosm"
                                         "MB8GA1UdIwQYMBaAFPwmrNbS0hPKnfqyDUcT1wJBhosmMAwGA1UdEwQFMAMBAf8w"
                                         "DQYJKoZIhvcNAQELBQADggEBAEMwacs1g/yH1vNQAlBFKQ8aAy+b0eNONcOMGI/0"
                                         "tPyPFDM+gX3H3Htjo8HJ/6pUWN0etLCwEd55NPkI1kfHjZjScMlPRjsToS+cSHvq"
                                         "nVIhRK/ZJIrc1z6ni0qoFFtsbY82qYCVHKqxuqhV7eyZ+drPfESqoSoFWH9Wex5H"
                                         "u1VLJeTXrhc1MqH+bUTOoRR+2qEDBSBULfw3HqXIWOAu3CLoIWr/5PGjPN6ycooD"
                                         "0UR0BU1vwQCPdntMFY6C3mgL60ZO5DjmtXI/msh+4bGgXBY8Pl1sBlhk+ya6eKJ5"
                                         "jYvoxIWq4c8DlH1+jW5/gf8QOMdZA/3CNcvs6w2ccDhh9l4="
                                         "-----END CERTIFICATE-----";

/** The hash of gpUEchoServerServerCertPem once stored on the module.
 */
const char gUEchoServerServerCertHash[] = {0xf9, 0x5c, 0xbf, 0xe7, 0x5c, 0x1a, 0xa7, 0xd5,
                                           0x82, 0x22, 0x31, 0xc8, 0x17, 0xff, 0xf3, 0x95
                                          };

// End of file
