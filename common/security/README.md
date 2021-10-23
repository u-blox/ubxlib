# Introduction
This directory contains the security API for certificate/key storage, TLS security configuration and u-blox specific security features that provide unique authentication/identification/encryption and transport services.  The support for these features is shown below.

|  Module        | Certificate/Key Storage |  TLS   |  Seal | End To End  | PSK Generation | Chip to Chip  | Zero Touch Provisioning  |
| :-------------:| :---------------------: | :----: | :---: | :---------: | :------------: | :-----------: | :----------------------: |
| NINA-B1        |             Y           |        |       |             |                |               |                          |
| NINA-B2        |             Y           |        |       |             |                |               |                          |
| NINA-B3        |             Y           |        |       |             |                |               |                          |
| NINA-B4        |             Y           |        |       |             |                |               |                          |
| NINA-W1        |             Y           |        |       |             |                |               |                          |
| SARA-U201      |             Y           |   Y    |       |             |                |               |                          |
| SARA-R4        |             Y           |   Y    |       |             |                |               |                          |
| SARA-R422      |             Y           |   Y    |   Y   |      Y      |        Y       |               |                          |
| SARA-R5        |             Y           |   Y    |   Y   |      Y      |        Y       |       Y       |             Y            |

# Usage
The [api](api) directory defines the security APIs.  The [test](test) directory contains tests for that API that can be run on any platform.
