# Introduction
This document describes how to configure and then operate secure external access to the automated test system. See [ACCESS_REQUEST](ACCESS_REQUEST.md) for instructions on how a u-blox employee may request access to the system.

*** PLEASE KEEP THESE INSTRUCTIONS (AND THOSE IN ACCESS_REQUEST.MD) UP TO DATE IF YOU MAKE ANY CHANGES TO THE ACCESS MECHANISMS ***

# Description
Generic HTTPS access to the Jenkins server is protected as follows:
- a private key is created on the Jenkins machine,
- this is signed by a Certificate Authority that has verified the identity of the Jenkins machine, resulting in a server certificate that is trusted; this is the internet PKI in action, nothing new, it just means that a browser can make a HTTPS connection to the Jenkins machine: it is trusted,
- the Jenkins machine uses SSL to set up as its own Certificate Authority in order that it can perform client authentication; this is the unusual bit,
- each user generates a private key that identifies them and their device,
- the user generates a Certificate Signing Request for this key and sends that request to the system administator of the automated test system,
- the system administrator, being sure of the originator of that Certificate Signing Request, signs it using this local Certificate Authority and sends the signed certificate plus the CA certificate back to the user,
- the user installs the signed certificate and the CA certificate on their device,
- now, during the HTTPS handshake when the user makes a connection between that device and the Jenkins machine, the Jenkins machine will request the certificate and check that it is signed; there is a verified mathematical chain between the private part of the client/device key and the private part of the CA key on the Jenkins machine, though neither private thing has ever left either machine,
- the Jenkins machine maintains a list of the certificates that it has signed, allowing a signature to be revoked if required.

For administration and maintanence, SSH and SFTP connections are also available, locked using \[separate\] key pairs for secured access.

# To Be Clear: What Is Secret And What Is Not
Any private key, generated either on the server or on a client device, SHOULD NOT LEAVE THAT DEVICE except:
- if the client is not able to generate their own private key (the `"Ubxlib Test System Admin Generates Private Key"  Method` below), in which case a password-protected private key may be sent by the `ubxlib` test system administrator back to the user who MUST THEN IRRETRIEVABLY DELETE THAT FILE IN ANY PLACE IT MAY HAVE LANDED (e-mail database etc.) after it has been installed.

Nothing else is secret, let it all hang out.

Note also that, for moving stuff around, all the keys here are plain text and can be happily cut and pasted (though it is generally a good idea to paste everything, including any line-feeds at the end of a file).

# HTTPS
These steps need to be performed once to get the server sorted and each client authenticated to use it.

## Server
These steps are carried out on the same machine as the Jenkins Docker container is running.  The DNS entry through which the machine is externally visible is assumed to be `ubxlib.com` and the e-mail address of the administrator is assumed to be `ubxlib@u-blox.com`.

### DNS Address
By whatever means at your disposal give the externally-visible IP address of the `ubxlib` test system's router a DNS address on the public internet.  For instance, you might use a service such as [noip](https://www.noip.com/), with the router running the necessary dynamic DNS client (most routers support this).  Note that, if the WAN-side of your router will be behind a firewall your DNS provider must allow you to create TXT records for your domain,.

### HTTPS Certificate: "Router On Public Internet" Case
If the WAN-side of the router inside the `ubxlib` test system is on the public internet, you can use Certbot and Let's Encrypt to obtain a certificate for your HTTP server with Certbot in `--standalone` mode, i.e. it spins-up a temporary HTTP server on port 80 (it HAS to be port 80), and Let's Encrypt can make an incoming TCP connection on that port to perform the verification process.

- Set the router to port-forward incoming TCP connection requests on port 80 to the same port on the Jenkins machine.

- Obtain a private key for it and get that signed by a CA (Let's Encrypt) by running Certbot in a Docker container as follows:

```
sudo docker run -it --rm --name certbot -v /etc/letsencrypt:/etc/letsencrypt -v /var/lib/letsencrypt:/var/lib/letsencrypt -p 80:80 certbot/certbot certonly --standalone
```

  ...giving it the e-mail address `ubxlib@u-blox.com` and the URL `ubxlib.com` when prompted.

- The private key and signed certificate will have been placed into `/etc/letsencrypt/live/ubxlib.com`.

- `ubxlib@u-blox.com` will be sent an e-mail a few weeks before the certificate (3 months validity) expires; if port 80 will ALWAYS be open for incoming TCP connections you may renew it automatically by running the following command on the Jenkins machine, say, once a day:

```
sudo docker run -it --rm --name certbot -v /etc/letsencrypt:/etc/letsencrypt -v /var/lib/letsencrypt:/var/lib/letsencrypt -p 80:80 certbot/certbot renew
```

- Note: once you have NGINX running, after updating certificates you will need to reload it to start using them: `docker exec -it nginx nginx -s reload`.

### HTTPS Certificate: "Router Behind Firewall" Case
If the WAN-side of the router inside the `ubxlib` test system is behind a firewall you can no longer use the usual Certbot HTTP route to get a certificate for your HTTP server.  This is because ports can only be made open for incoming connections through the [Tunneling](#tunneling) mechanism and that does NOT work for ports numbered less than 1024 (that's just the way Linux is written) and Let's Encrypt will ONLY use port 80 to establish trust over HTTP.  Instead you must use the DNS authentication mechanism that Certbot/Let's Encrypt offers, where trust is established by checking that you own the DNS entry for the server.

#### `noip` Case
If you are using `noip` to provide your DNS record/static IP address, it does not, unfortunately, support setting the necessary DNS record through an API, and hence the renewal process cannot be automated, it has to be performed manually as follows:

- Log-in to `noip` and be ready to add a new TXT record.

- Obtain a private key for the URL of the test system (e.g. `ubxlib.com`) and get that signed by a CA (Let's Encrypt) by running Certbot in a Docker container as follows:

```
sudo docker run -it --rm --name certbot -v /etc/letsencrypt:/etc/letsencrypt -v /var/lib/letsencrypt:/var/lib/letsencrypt certbot/certbot certonly --manual --debug-challenges --preferred-challenges dns -d ubxlib.com
```

  ...giving it the e-mail address `ubxlib@u-blox.com` and the URL of the test system (assumed to be `ubxlib.com`) if prompted.

- You will be asked to add a TXT record to the DNS record inside `noip`, with a `_acme-challenge` sub-domain (i.e. prefix) and with the value being a random text string that Let's Encrypt will check.  Do this and confirm to `CertBot` that you have done so.

- The private key and signed certificate will have been placed into `/etc/letsencrypt/live/ubxlib.com`.

- Note: `ubxlib@u-blox.com` will be sent an e-mail a few weeks before the certificate (3 months validity) expires, at which point you can renew it manually by repeating the process above.

- Note: once you have NGINX running, after updating certificates you will need to reload it to start using them: `docker exec -it nginx nginx -s reload`.

#### AWS Route 53 Case
If you are using Amazon Web Services Route 53 to provide your DNS record/static IP address then you can set up Cerbot/Let's Encrypt to obtain a private key for the URL of the test system (e.g. `ubxlib.com`) and get that signed by a CA (Let's Encrypt) automatically using [certbot-dns-route53](https://certbot-dns-route53.readthedocs.io/en/stable/).

Do the following in the AWS web console:

- Create an IAM (Identity and Access Management) policy named `certbot-dns-route53`, description `Permissions sufficient to auto-update Lets Encrypt certificate via DNS with Certbot`, using the following JSON, where the `HOSTEDZONEID` for `ubxlib.com` can be found in the `Hosted Zones` section of your AWS Route 53 management console:

```
{
    "Version": "2012-10-17",
    "Id": "certbot-dns-route53 policy",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "route53:ListHostedZones",
                "route53:GetChange"
            ],
            "Resource": [
                "*"
            ]
        },
        {
            "Effect" : "Allow",
            "Action" : [
                "route53:ChangeResourceRecordSets"
            ],
            "Resource" : [
                "arn:aws:route53:::hostedzone/HOSTEDZONEID"
            ]
        }
    ]
}
```

- Create an IAM user named `certbot`, without console access, and attach to it the `certbot-dns-route53` policy you just created.

- Select the newly created `certbot` user and create an access key (ID/secret pair) for it with the description `Certbot renewal`.

Do the following on the Jenkins machine:

- Create a path named `/etc/aws/config` with the following contents, replacing the example values with the access key ID/secret you obtained above:

```
[default]
aws_access_key_id=AKIAIOSFODNN7EXAMPLE
aws_secret_access_key=wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY
```

- Run the `certbot/dns-route53` Docker container as follows to create the certificate:

```
sudo docker run --rm --name certbot --env AWS_CONFIG_FILE=/etc/aws/config -v /etc/aws:/etc/aws -v /etc/letsencrypt:/etc/letsencrypt -v /var/lib/letsencrypt:/var/lib/letsencrypt certbot/dns-route53 certonly --dns-route53 -d ubxlib.com
```

- The private key and signed certificate will be placed into `/etc/letsencrypt/live/ubxlib.com`.

- Set this up to [auto-renew](https://eff-certbot.readthedocs.io/en/stable/using.html#setting-up-automated-renewal) (checked twice daily) with:

```
SLEEPTIME=$(awk 'BEGIN{srand(); print int(rand()*(3600+1))}'); echo "0 0,12 * * * root sleep $SLEEPTIME && docker run --rm --name certbot --env AWS_CONFIG_FILE=/etc/aws/config -v /etc/aws:/etc/aws -v /etc/letsencrypt:/etc/letsencrypt -v /var/lib/letsencrypt:/var/lib/letsencrypt certbot/dns-route53 renew -q && docker stop certbot" | sudo tee -a /etc/crontab > /dev/null
```

- To have `NGINX` restarted when auto-renewal has occurred, you'll need to create a named pipe on the host machine to which the `certbot/dns-route53` Docker container can send a reload command; it goes like this:
  - Create a named pipe with `mkfifo /etc/letsencrypt/docker_host_pipe`; `ls -l /etc/letsencrypt/docker_host_pipe` should show a `p` in the first character of the file flags.
  - Create a file named `/etc/letsencrypt/docker_host_pipe_eval.sh` to execute stuff coming through the pipe with contents:
    ```
    #!/bin/bash
    while true; do eval "$(cat /etc/letsencrypt/docker_host_pipe)"; done
    ```
  - Give the file the correct permissions with `sudo chmod +x /etc/letsencrypt/docker_host_pipe_eval.sh`.
  - Start the script manually with:
    ```
    sudo sh /etc/letsencrypt/docker_host_pipe_eval.sh&
    ```
  - Check that it works with something like:
    ```
    echo "echo Hello host" | sudo tee /etc/letsencrypt/docker_host_pipe
    ```
    ...which should result in something like:
    ```
    echo Hello host
    Hello host
    ```
  - Create a deploy-hook file named `/etc/letsencrypt/renewal-hooks/deploy/nginx_restart.sh` that will be run by the `certbot/dns-route53` Docker container, redirecting its commands to the named pipe:
    ```
    #!/bin/sh
    echo "echo Restarting NGINX docker on host" > /etc/letsencrypt/docker_host_pipe
    echo "docker exec nginx nginx -s reload" > /etc/letsencrypt/docker_host_pipe
    ```
  - Give the file the correct permissions with `sudo chmod +x /etc/letsencrypt/renewal-hooks/deploy/nginx_restart.sh`.
  - Check that this works with a dry-run:
    ```
    docker run --rm --name certbot --env AWS_CONFIG_FILE=/etc/aws/config -v /etc/aws:/etc/aws -v /etc/letsencrypt:/etc/letsencrypt -v /var/lib/letsencrypt:/var/lib/letsencrypt certbot/dns-route53 --dry-run --run-deploy-hooks renew
    ```
    ...you should see `Restarting NGINX docker` and something like `[notice]: signal process started` mixed-in towards the end of the usual `certbot/dns-route53` output.
  - Make this start at boot by editing `/etc/crontab` to add on the end `@reboot /etc/letsencrypt/docker_host_pipe_eval.sh`.
  - Check that this works at boot by modifying the `cron` entry to, say, run every 5 minutes (`*/5 * * * *` at the start), and have the `--dry-run --run-deploy-hooks` switches in it; try rebooting and watch `journalctl -f` to see if it runs.

### Setting Up As A Certificate Authority
Note: the keys/certificates etc. used here are entirely separate from those generated by Certbot above, don't mix the two.  Also, the naming pattern used by Cerbot (more correct in my view), in which the file extension `.pem` designates the format of the file, is replaced here by what appears to be the more usual format for SSL stuff, which is that certificates end with `.crt`, keys with `.key` and certificate signing requests with `.csr`; all are PEM format anyway.

- Set SSL up for ease of certificate management by doing:

```
mkdir /etc/ssl/CA
mkdir /etc/ssl/csr
mkdir /etc/ssl/newcerts
sh -c "echo '01' > /etc/ssl/CA/serial"
sh -c "echo '01' > /etc/ssl/CA/crlnumber"
touch /etc/ssl/CA/index.txt
```

  ...then edit `/etc/pki/tls/openssl.cnf` and change these entries in the `[ CA_default ]` section as follows:

```
dir         = /etc/ssl                                                # Where everything SLLish kept
database    = $dir/CA/index.txt                                       # database index file.
serial      = $dir/CA/serial                                          # The current serial number
crlnumber   = $dir/CA/crlnumber                                       # the current crl number
crl         = $dir/CA/ca.crl                                          # The current CRL
certificate = $dir/certs/ubxlib_test_system_ca.crt                    # The server certificate
private_key = $dir/private/ubxlib_test_system.key                     # The private key

default_crl_days = 36500                                              # how long before next CRL
```

  ...and these entries in the `[ policy_match ]` section as follows:

```
[ policy_match ]
countryName             = optional
stateOrProvinceName     = optional
organizationName        = supplied
organizationalUnitName  = optional
commonName              = optional
emailAddress            = supplied
```

- Generate a master key for your Certificate Authority, not password protected as SSL/NGINX will need to read it:

```
openssl genrsa -des3 -out /etc/ssl/private/ubxlib_test_system.key 4096
```

- Create a CA certificate for the server to sign things with, valid for 10 years, using the same key as was used to authenticate this machine through Certbot and populating `Country Name` with `GB`, `State or Province Name` empty, `Locality Name` empty, `Organization Name` with `u-blox`, `Organizational Unit Name` with `ubxlib`, `Common Name` with the URL that the test system will appear as (e.g. `ubxlib.com`) and `Email Address` with `ubxlib@u-blox.com`:

```
openssl req -new -x509 -days 3650 -key /etc/ssl/private/ubxlib_test_system.key -out /etc/ssl/certs/ubxlib_test_system_ca.crt
```

- Create an initial (empty) Certificate Revocation List with:

```
openssl ca -gencrl -keyfile /etc/ssl/private/ubxlib_test_system.key -cert /etc/ssl/certs/ubxlib_test_system_ca.crt -out /etc/ssl/CA/ca.crl -config /etc/pki/tls/openssl.cnf
```

### Install NGINX Inside Docker
- Install an NGINX Docker container with the following command-line, where `xxxx` is the port number NGINX should listen on (if you will be [Tunneling](#tunneling) this must be 1024 or greater, e.g. 8888):

```
docker run --name nginx  --restart=always --detach --mount type=bind,source=/etc/ssl,target=/etc/ssl,readonly --mount type=bind,source=/etc/pki,target=/etc/pki,readonly --mount type=bind,source=/etc/letsencrypt,target=/etc/letsencrypt,readonly -p xxxx:xxxx -d nginx
```

- Install `nano` in the container with :

```
docker exec -u root -t -i nginx /bin/bash
apt-get update
apt-get install nano
exit
```

- Configure NGINX by editing the `/etc/nginx/conf.d/default.conf` with:

```
docker exec -u root -t -i nginx nano /etc/nginx/conf.d/default.conf
```

  ...and, in the existing entry, replace the `listen 80` and `server_name` lines as below, where again `xxxx` is replaced with the port number NGINX should listen on:

```
    listen       xxxx ssl;
    listen  [::]:xxxx ssl;
    server_name  ubxlib.com;
```

  ...locate the files generated by Certbot and the local Certificate Authority CA file by adding:

```
    ssl_certificate        /etc/letsencrypt/live/ubxlib.com/fullchain.pem;
    ssl_certificate_key    /etc/letsencrypt/live/ubxlib.com/privkey.pem;

    ssl_client_certificate /etc/ssl/certs/ubxlib_test_system_ca.crt;
    ssl_crl                /etc/ssl/CA/ca.crl;
    ssl_verify_client on;
```

  ...change the `location` clause to be:

```
    location / {
      proxy_pass http://172.17.0.1:8080/;
    }
```

  ...(`172.17.0.1` being the IP address of the `jenkins-custom` docker container on the Docker network), save the file and `exit` the Docker container.

- On the router, set up port forwarding so that requests arriving on the WAN side for port `xxxx` are forwarded to the same port on the LAN-side address of the Jenkins machine.  If the WAN-side of your router is behind a firewall, see [Tunneling](#tunneling).

- Run `docker restart nginx` and open a browser window to `https://ubxlib.com:xxxx`: you should see a message something like `400 Bad Request: No required SSL certificate was sent`; this is because we've not yet set up your client device as one that is permitted to log in (if you comment out the line `ssl_verify_client on` above and restart NGINX you will get to the Jenkins log-in prompt, but don't do that 'cos were open to the internet now).

- Note: if you are unable to get to the warning stage, logs of NGINX's behaviour can be viewed with `docker logs nginx`.  If you are not able to even get that far, temporarily switch logging to "everything" on the router and see if you can see an incoming connection being dropped or accepted from the IP address of the machine you are browsing from (browse to https://whatsmyip.com/ on that machine to determine its external IP address if you don't know it).  If the connection _is_ being accepted at the router, try taking a [Wireshark](https://www.wireshark.org/) log on the Jenkins machine with `sudo yum install wireshark` followed by something like `sudo tshark -i eno1`, assuming `eno1` is the Ethernet port that is the LAN address of the Jenkins machine, to see if the connection attempt is getting from the router to the machine.

- With all of this done, in order to get Jenkins to refer to itself as being at the new external URL, in Jenkins go to `Manage Jenkins` -> `Configure System` find `Jenkins Location` and set `Jenkins URL` to be `https://ubxlib.com:xxxx`.  The Jenkins configuration page will then pop-up a warning along the lines of `It appears that your reverse proxy set up is broken`, since Jenkins itself cannot get to that URL; this warning can be dismissed.

## Clients
The steps required for initial setup of each client that wishes to access the system are set out below.  Generation of the key/Certificate Signing Request may be done either by the user (typically a Linux user will know how to do this) or by the `ubxlib` test system admin (which might be an easier option for Windows users since then there is no need to install OpenSSL on their machine).  Getting the user to do it is preferred as that way their private key never leaves their device.

### "User Generates Private Key" Method
Use this method if the user answers yes to the question "are you OK to run OpenSSL to generate private keys and signing requests?"; if so, make sure that, when they send you their Certificate Signing Request (see below), they populate the `Organisation Name` with the name of their device and the `E-mail Address` field with their e-mail address.

#### Generation
- Ask the user to install [OpenSSL](https://www.ibm.com/docs/en/ts4500-tape-library?topic=openssl-installing) on the device they wish to access the automated test system from, if they've not done so already,

- Ask the user to generate a private key that identifies them on that device with the command below (the key should be password protected):

```
openssl genrsa -des3 -out ubxlib_test_system.key 4096
```

- Tell them to keep the `ubxlib_test_system.key` file somewhere safe and NEVER to reveal it to anyone.

- Ask the user to generate a Certificate Signing Request for this private key with the command below and then e-mail the generated `.csr` file to `ubxlib@u-blox.com` for processing; they should replace `devicename` with a string representing their device (e.g. for me it would be `gb-cmb-lt-rmea`) and, so that it is possible to manage things, they **must** enter the same string in the `Organisation Name` field of the CSR and they **must** populate the `E-mail Address` field correctly in the CSR (everything else may be left blank by pressing `.` and then `<enter>`):

```
openssl req -new -key ubxlib_test_system.key -out ubxlib_test_system_devicename.csr
```

- When `ubxlib@u-blox.com` receives the `.csr` file, provided it is **definitely** from the expected user, it should be stored on the machine running Jenkins/NGINX in the directory `/etc/ssl/csr`, then a signed certificate should be generated from it on that machine with something like:

```
openssl ca -in /etc/ssl/csr/ubxlib_test_system_devicename.csr -config /etc/pki/tls/openssl.cnf
```

- Note: keeping the `.csr` file in the `ubxlib` test system means that a new certificate can be generated from the same Certificate Signing Request file when the previous one expires in 365 days.

- When done, a new file, e.g. `01.pem`, should appear in the `/etc/ssl/newcerts/` directory: e-mail this file, renamed to `ubxlib_test_system_devicename.crt`, **PLUS** `ubxlib_test_system_ca.crt` back to the user; it doesn't matter if these files go astray, they will only work for the user that has the private key.

- Note: to revoke an existing certificate, `cat /etc/ssl/CA/index.txt` to look up which `xx.pem` file was created for it and then issue the following commands, replacing `xx.pem` with the relevant file:

```
openssl ca -revoke /etc/ssl/newcerts/xx.pem
openssl ca -gencrl -keyfile /etc/ssl/private/ubxlib_test_system.key -cert /etc/ssl/certs/ubxlib_test_system_ca.crt -out /etc/ssl/CA/ca.crl -config /etc/pki/tls/openssl.cnf
docker restart nginx
```

- The default OpenSSL configuration file will not allow you to generate a new certificate for one which already exists in the index.  If a client certificate is about to expire and you want to generate a new one to send to the user _before_ the one they have expires, you will need to edit ` /etc/ssl/CA/index.txt.attr` (create it if it doesn't exist) to have the line `unique_subject = no` in it.

- If a client certificate has expired, run the following command:

```
 sudo openssl ca -updatedb -config /etc/pki/tls/openssl.cnf
```

This will update `/etc/ssl/CA/index.txt` so that the certificate is marked as expired (with an `E` in the first column).  If you wish, you may then you generate a new certificate from the same `.csr` file using exactly the same command-line as you used to create it in the first place.

#### Installation
These steps are carried out by the user on the device where they generated their private key.

- Create a `.pfx` file from the locally-generated `ubxlib_test_system.key`, the received signed certificate and the received Certificate Authority (you will be asked for the password for the `.key` file and you **must** then provide a password for the `.pfx` file, since otherwise the `.key` will be in plain text again inside the `.pfx` file) with something like:

```
openssl pkcs12 -export -out ubxlib_test_system_devicename.pfx -inkey ubxlib_test_system.key -in ubxlib_test_system_devicename.crt -certfile ubxlib_test_system_ca.crt
```

- If the user is running Linux, they should install this bundle in Firefox by going to `Settings`, searching for `Certificates`, pressing `View Certificates`, selecting the `Your Certificates` tab, then `Import` and selecting the `.pfx` file.  Then restart FireFox and try again.

- If the user is running Windows they should double-click the `.pfx` file, select `Current User` in the dialog box that pops up, confirm the file to import, enter the password for the `.pfx` file, allow the wizard to decide where to put the certificates and press `OK` to add the lot.

- Open a browser and make an HTTPS connection to the Jenkins URL; it should prompt for the certificate to use: chose the one it offers, which will be the one just installed, and the Jenkins log-in page should appear.

- Troubleshooting: if it does not you might take a Wireshark log on your local machine while doing the above and look in the SSL handshake for (a) the server sending a Certificate Request (the Distinguished Names it is asking for should be those of the CA certificate) and (b) the client responding with Certificate: is it of non-zero length and, if so, does the Public Key string match the one in the signed certificate that you installed?

### "`ubxlib` Test System Admin Generates Private Key"  Method
Use this method if the user is not able to generate a private key on their device.

- Ask them to e-mail their `devicename` to `ubxlib@u-blox.com`.

- Generate a password-protected private key for that user, a Certificate Signing Request to go with it, and then generate the actual certificate, with something like (filling in `devicename` in the `Organisation Name` field and their e-mail address in the `E-mail Address` field of the Certificate Signing Request, leaving the rest empty by just entering `.`):

```
openssl genrsa -des3 -out devicename.key 4096
openssl req -new -key devicename.key -out ubxlib_test_system_devicename.csr
```

- Handle the signing request as described in the section above (store it on the machine running Jenkins/NGINX in the directory `/etc/ssl/csr` and sign it, etc.).

- Create a password-protected PFX file which will include the private key you generated for them, the signed certificate for it and the public Certificate Authority for the`ubxlib` test system.

```
openssl pkcs12 -export -out ubxlib_test_system_devicename.pfx -inkey devicename.key -in ubxlib_test_system_devicename.crt -certfile ubxlib_test_system_ca.crt
```

- Now you can delete the file `devicename.key`.

- Send the user the `.pfx` file and, over a separate channel, let them know the password that goes with it; unlike the case where the user generated the private key, this file should be destroyed ASAP after installation (e,g, in all outgoing and incoming e-mails) as it is possible for someone to guess or brute-force the password and obtain the private key from it.

- Continue from [Installation](#installation) above, but using the `.pfx` file (and separate password) received, rather than the one locally generated.

# SSH/SFTP
These steps need to be performed once for the server and once for each client to permit SSH/SFTP access.  Throughout this section `<ssh_port>` should be replaced with the port number chosen for external SSH/SFTP access, e.g. 3000 (not 22 since that port is reserved for internal use only).

Note: this is deliberately a single account, not really intended for many/multiple users, more the main system administrator and occasional others.

## External Access
Assuming that you have already performed the steps to get HTTPS access working, the `ubxlib` test system's router will already have a DNS address on the public internet.  All that needs to be done for SSH/SFTP access is to set up port forwarding on the router so that requests arriving on the WAN side for `<ssh_port>` are forwarded to the same port on the LAN-side address of the Jenkins machine.  If the WAN-side of your router is behind a firewall, see [Tunneling](#tunneling).

## Server
On the Jenkins server machine (Centos 8 assumed), set up a second SSH daemon as follows (a version of the instructions [here](https://access.redhat.com/solutions/1166283)):

- Copy the existing `sshd_config` to `sshd_external_config` with:

```
cp /etc/ssh/sshd{,_external}_config
```

- Edit `/etc/ssh/sshd_external_config` to uncomment and change the following lines (the latter two ensuring that a broken connection is dropped by the server rather than lasting forever and preventing future logins):

```
Port <ssh_port>
PermitRootLogin no
PasswordAuthentication no
ChallengeResponseAuthentication no
KbdInteractiveAuthentication no
ClientAliveInterval 10
ClientAliveCountMax 2
```

- Add the port for the second instance of `sshd` to SSH ports, otherwise the second instance of `sshd` will be rejected when trying to bind to the port:

```
yum -y install policycoreutils-python-utils
semanage port -a -t ssh_port_t -p tcp <ssh_port>
```

- Open `<ssh_port>` on the firewall with:

```
firewall-cmd --zone=public --permanent --add-port=<ssh_port>/tcp
systemctl restart firewalld
```

- Edit `/etc/systemd/system/sshd_external.service` so that:
  - the `Description` becomes `OpenSSH server external access daemon`,
  - `ExecStart` has `-f /etc/ssh/sshd_external_config` added to it, so something like `ExecStart=/usr/sbin/sshd -D -f /etc/ssh/sshd_external_config $OPTIONS $CRYPTO_POLICY`.

- Reload daemons, start the new service and enable it to start at boot with:

```
systemctl daemon-reload
systemctl start sshd_external.service
systemctl enable sshd_external.service
```

- Note: you can check that the service is running with `systemctl status sshd_external.service`; if you try SSH-ing into it on port `<ssh_port>` nothing will happen as we have not yet set up a key pair for your device to log in with.

## Clients
- On the client machine, create a key pair (the private key password-protected) as below, where `client` in `ubxlib_test_system_client_key` is replaced with something that identifies the client machine, e.g. in my case it would be `gb-cmb-lt-rmea`, and `path/to/` is `~/.ssh/` for Linux or `%homedrive%%homepath%\.ssh\` for Windows:

```
ssh-keygen -f path/to/ubxlib_test_system_client_key -t ecdsa -b 521
```

- E-mail the generated `ubxlib_test_system_client_key.pub` file (NOT the non-`.pub` file, that must NEVER LEAVE THE CLIENT MACHINE) to `ubxlib@u-blox.com`; the `.pub` file is not secret in any way.

- When `ubxlib@u-blox.com` receives the `.pub` file, provided it is **definitely** from the expected user, they should append it to the list of authorised keys with something like (only the last line needed if the `/home/ubxlib/.ssh` directory already exists, though there's no harm in repeating them):

```
mkdir -p /home/ubxlib/.ssh
chmod 700 /home/ubxlib/.ssh
chmod 600 /home/ubxlib/.ssh/authorized_keys
cat ~/ubxlib_test_system_client_key.pub >> /home/ubxlib/.ssh/authorized_keys
```

  ...then you can delete `~/ubxlib_test_system_client_key.pub`.

- Restart the SSH service with:

```
systemctl restart sshd_external.service
```

- The client should now be able to SSH into the Jenkins machine with something like `ssh -i path/to/ubxlib_test_system_client_key -p <ssh_port> ubxlib@jenkinsurl`, or you may use [PuTTY](https://www.putty.org/), for which you need [to convert your private key to a `.ppk` file](https://sites.google.com/site/xiangyangsite/home/technical-tips/linux-unix/common-tips/how-to-convert-ssh-id_rsa-keys-to-putty-ppk).

If this doesn't work, try looking at `systemctl status sshd_external.service`, maybe set `LogLevel DEBUG` in the server's `sshd_external_config` or try taking Wireshark logs on the client and server machines to see what SSH might be objecting to.

- Similarly, the client should be able to SFTP into the Jenkins machine with something like `sftp -i path/to/ubxlib_test_system_client_key -P <ssh_port> ubxlib@jenkinsurl` (noting the capital `-P` this time) or, if a GUI is preferred, using something like [FileZilla](https://filezilla-project.org/).

- Note: should you need to revoke access for a client, simply delete the relevant line in `/home/ubxlib/.ssh/authorized_keys` on the Jenkins machine; to kill an active SSH session from the server-side, `ps -aux | grep sshd` to find an active session that is logged in as the `ubxlib` user and `kill <pid>` where `<pid>` is replaced with the process ID, which is the first number on the line.

# Tunneling
If the WAN-side of your router is not on the public internet and you don't have access to the router(s) that are between you and the public internet to forward ports, you may use an SSH reverse tunnel to achieve the same effect.  For this you will need a machine on the public internet to which you already have SSH access.  Effectively you SSH into that remote machine from the local machine with a command-line which, rather than opening an interactive SSH session, tells the SSH server on the remote machine to forward any packets sent to a given port number on that machine down the SSH tunnel to the local machine.

Linux does not permit a remote machine to forward ports lower in number than 1024, that's just the way it is; there are ways around this (using `socat`) but it is probably better just to use a non-standard port.  Also, for the links in the Jenkins web pages to operate correctly, the local port number that NGINX is listening on (`xxxx` above) and the remote port number that the user will put on the end of their URL must be the same.

Let's say we pick port 8888, for the sake of argument.

You want port 8888 on your local machine to be available to someone who accesses `remote_machine:8888`, where `remote_machine` is the machine you are able to SSH into.  To do this, on your local machine, you would execute the following command-line:

```
ssh -N -R 8888:localhost:8888 user@remote_machine
```

This tells the SSH server on `remote_machine` to send any packets headed for port 8888 on that machine (the first 8888) to the machine where you ran the above command, port 8888 (the second 8888). `-N` means don't do an interactive login.  Note: if the `remote_machine` is an Amazon EC2 instance the `user` here is a Linux user on that machine, it has nothing at all to do with the many AWS IAM users etc.

Obviously it is important to make sure that you only SSH-forward to ports where you know you have, for instance, NGINX or an SSH-key-protected SSH server listening on the local machine.  You may also need to make sure that, on the remote machine, `/etc/ssh/sshd_config` has `GatewayPorts` set to `yes`, `AllowTcpForwarding` set to `remote` and `ClientAliveInterval 30`/`ClientAliveCountMax 2` to drop unused connections (restart `ssh.service` after making changes).  And obviously the port (8888 in this case) would need to be open to the public internet for incoming TCP connections both on the remote machine and on that machine's network firewall.  Then, if you do the above command on the Jenkins machine, users will be able to access the Jenkins web pages, secured through NGINX, at the URL of the remote machine.  Obviously you will need to do the same for SSH/SFTP access.

Note: with this approach you don't actually need any port-forwarding on your router at all, since all TCP connections (i.e. the SSH tunnel) are outward.

To set this up to run at boot on the Jenkins machine, first create a key-pair (no passphrase, the SSH daemon can't type) with:

```
ssh-keygen -f path/to/tunnel_key -t ecdsa -b 521
```

...and, similar to above, on the server machine add the contents of the `.pub` file as a new line in the file `~/.ssh/authorized_keys` (which should have the right permissions set (see above)).

Rather than just running `ssh` as a command-line, or even as `systemd` service, the favoured approach seems to be to install [autossh](https://www.harding.motd.ca/autossh/) with:

```
wget -c https://www.harding.motd.ca/autossh/autossh-1.4g.tgz
gunzip -c autossh-1.4g.tgz | tar xvf -
cd autossh-1.4g
sudo yum -y install gcc make
./configure
make
sudo make install
```

...run `autossh` once, manually, as `sudo` in order to accept the signature of the remote server with something like:

```
sudo /usr/local/bin/autossh -M 0 -o "ServerAliveInterval 30" -o "ServerAliveCountMax 3" -N -R 8888:localhost:8888 user@remote_machine -i path/to/tunnel_key
```

...and start `autossh` with a `systemd` file named something like `/etc/systemd/system/tunnel-https.service` containing something like the following:

```
[Unit]
Description=Persistent SSH Tunnel for Jenkins access
After=network.target

[Service]
Restart=on-failure
RestartSec=5
Environment=AUTOSSH_GATETIME=0
ExecStart=/usr/local/bin/autossh -M 0 -o "ServerAliveInterval 30" -o "ServerAliveCountMax 3" -N -R 8888:localhost:8888 user@remote_machine -i path/to/tunnel_key
ExecStop= /usr/bin/killall autossh

[Install]
WantedBy=multi-user.target
```

...then:

```
sudo systemctl daemon-reload
sudo systemctl enable tunnel-https.service
sudo systemctl start tunnel-https.service
```

...and the same with a `tunnel-ssh` `systemd` file for the SSH tunnel also.  This will cause the server to send "alive" messages every 30 seconds and for the tunnel to be restarted after three such messages have gone missing.
