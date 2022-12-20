# Introduction
Here we describe how to set up a complete `ubxlib` test system, including Jenkins master and the test agent(s), from scratch.  Linux, in this case either Centos Stream 8 or Raspbian (OS Lite), is assumed unless otherwise stated.  Throughout this guide you are assumed to have local access to the [network](NETWORK.md) that you are constructing; later you may set up mechanisms to allow external [access](ACCESS.md).

# Description
![setup](/readme_images/setup.png)
- Jenkins, [NGINX](https://www.nginx.com/) (for secured access) and the SMEE Client (which allows Github to trigger test runs behind a firewall) are installed in separate Docker containers on a single Linux PC.
- Desktop-type "beefy" Linux PCs are used for building/checking `ubxlib` and running the `Jenkinsfile` which distributes work around the system.
- Raspberry Pi 4's are used to download those built images of `ubxlib` to MCUs with u-blox modules attached and then monitor the tests as they run; there is a Raspberry Pi for each MCU/module HW configuration defined by [DATABASE.md](DATABASE.md).
- The third-party MCU vendor tools are built into an [Ubuntu] Docker container which runs on both the Linux desktop PCs and the Raspberry Pis, isolating those tools nicely.
- This Docker container accesses the `ubxlib` source code and automation scripts, all fetched from Github, natively on the Linux machines through mapped volumes.
- A single Windows PC runs the one required MSVC build/test instance.

This way we get fastest build/check plus expandability of download/test (since any number of Pis can be attached).

From the perspective of Jenkins, everything is driven through \[multiple\] node labels:
- all agents are labelled `ubxlib`,
- Linux agents are labelled `linux`, Windows agents are labelled `windows`,
- the agent(s) that distributes build/test jobs to all of the other agents is labelled `distributor` (this can be any one of the beefy Linux PCs, don't want to block a HW test instance of which there may be only one),
- a beefy agent with no testing HW attached, good for building \[so not a Raspberry Pi\], or performing checks that require no HW (i.e. one of the instances from [DATABASE.md](DATABASE.md) numbered less than 10), is labelled `build`,
- a Raspberry Pi with a single set of physical HW attached (i.e. an MCU board plus, probably, a Wi-Fi/BLE/GNSS/cellular module) is labelled `test` and `instance_x`, where `x` is the major number of the test instance from [DATABASE.md](DATABASE.md) \[10 or above\] that it supports, reflecting the HW that is attached: e.g. `instance_11` has an ESP-IDF MCU board attached plus a GNSS module and supports 11.0 and 11.1; note that beefy machines may also have the `test` and `instance_x` labels where the tests instances are `windowa` or `linux`.
- for admin purposes, PCs are labelled `x86_64`, Raspberry Pis are labelled `aarch64` and things that need a Docker image of 3rd party tools built on them are labelled `docker`.

Hence [Jenkinsfile](Jenkinsfile) is able to find at least one of everything it needs.

For a working test system you will need a minimum of three machines: two Linux (one for the Jenkins master and at least one Jenkins agent) and one Windows (as a Jenkins agent); since `ubxlib` supports building/running on both Linux and Windows, both must be tested.  Then, to run actual tests on real hardware, a Raspberry Pi (2.1, 3 or 4, preferably 4) is used as the controller of each individual test instance.

# Advice
- These instructions were written as the entire system was brought up \[again\], from scratch, in late 2022; so they have been "tested" at least once, however the world moves on...
- Jenkins, in particular, is a wild and semi-supported space; it is a rats-nest of plug-ins, all of which have their own versioning/existence and don't work in various ways, the very essence of OSS :-); don't believe the LTS, you need to decide on a strategy: leave things fixed and suffer a huge discontinuity later or regularly absorb all updates and deal with any issues as they arise.
- If you want to benchmark a Linux machine before deciding what to do with it, try [PassMark](https://www.passmark.com/products/pt_linux/download.php); you will need to install `ncurses-compat-libs` to get it to run but it is, usefully, command-line.
- Throughout this guide we install things as the `ubxlib` user, Jenkins stuff included; this is a deliberate simplification: all of these machines are `ubxlib` Jenkins machines where any `ubxlib` operation will also be a Jenkins operation and, should any user wish to log-in to fiddle/debug, they will want to look the same as Jenkins, fewer nasty permissions/configuration differences.

# Table Of Contents
- [Configure Jenkins master](#jenkins-master)
- [Configure Jenkins agents](#jenkins-agents)
- [Configure Jenkins](#jenkins)
- [Configure test instances (real HW)](#configure-test-instances)

# Jenkins Master
The Jenkins master is a single machine configured with Jenkins, [NGINX](https://www.nginx.com/) and the SMEE client (required to trigger builds through a firewall) each running in their own Docker containers.

## Docker
Install Docker; for Centos Stream 8 this goes like:

```
sudo yum install -y yum-utils
sudo yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo
sudo yum install --allowerasing docker-ce docker-ce-cli containerd.io docker-compose-plugin
sudo systemctl start docker
sudo systemctl enable docker
```

If, during installation, you are asked to accept a GPG key, verify that the fingerprint is `060A 61C5 1B55 8A7F 742B 77AA C52F EB6B 621E 9F35`, and if so, accept it. Should you feel a need to verify that Docker is working, run `docker run --rm hello-world`.

## Install Jenkins Inside Docker
Install and run Jenkins inside Docker, following the [ludicrously complex instructions](https://www.jenkins.io/doc/book/installing/docker/), reproduced below; this is necessary since the bare-bones Jenkins agent that Docker keeps is of no use to anyone.

- Start a Docker image inside Docker as follows:

```
docker network create jenkins
docker run --name jenkins-docker --rm --detach --privileged --network jenkins --network-alias docker --env DOCKER_TLS_CERTDIR=/certs --volume jenkins-docker-certs:/certs/client --volume $HOME/jenkins:/var/jenkins_home --publish 2376:2376 docker:dind --storage-driver overlay2
```

- Create a file called `Dockerfile` with the following contents:

```
FROM jenkins/jenkins:2.361.4-jdk11
USER root
RUN apt-get update && apt-get install -y lsb-release
RUN curl -fsSLo /usr/share/keyrings/docker-archive-keyring.asc \
  https://download.docker.com/linux/debian/gpg
RUN echo "deb [arch=$(dpkg --print-architecture) \
  signed-by=/usr/share/keyrings/docker-archive-keyring.asc] \
  https://download.docker.com/linux/debian \
  $(lsb_release -cs) stable" > /etc/apt/sources.list.d/docker.list
RUN apt-get update && apt-get install -y docker-ce-cli
USER 1000:1000
RUN jenkins-plugin-cli --plugins "blueocean:1.25.8 docker-workflow:521.v1a_a_dd2073b_2e pipeline-utility-steps:2.13.2 lockable-resources:2.18 ansicolor:1.0.2 build-discarder:139.v05696a_7fe240 nodelabelparameter:1.11.0 github-checks:1.0.19 test-results-analyzer:0.4.0"
```

- Note: the text above calls up a specific Jenkins version, instead of `lts-jdk11` which you'd have thought might be a more obvious choice; I think this is because the plugins don't have an LTS version but need to match the Jenkins version, hence LTS is of little value.

- Note: above we use the ID for user/group `1000:1000`, rather than a named user.  This is so that we can map the Jenkins volume over to the `ubxlib` user on the machine which has the same ID/group without any permissions problems.

- While in the same directory, build the Docker image by running:

```
docker build -t jenkins-custom .
```

- Execute this docker image with:

```
docker run --name jenkins-custom --restart=on-failure --detach --network jenkins --network-alias jenkins-custom --env DOCKER_HOST=tcp://docker:2376 --env DOCKER_CERT_PATH=/certs/client --env DOCKER_TLS_VERIFY=1 --publish 8080:8080 --publish 50000:50000 --volume $HOME/jenkins:/var/jenkins_home --volume jenkins-docker-certs:/certs/client:ro --log-driver=journald jenkins-custom 
```

- Note: you can start a command shell _inside_ this running Docker container (you will need this to read the `initialAdminPassword` in the next step) with:

```
docker exec -t -i jenkins-custom /bin/bash
```

- Open a browser and load `jenkinshosturloripaddress:8080` where you should see a Jenkins "Getting Started" screen telling you how to unlock Jenkins to get it running.

- With Jenkins unlocked, click on `Select plugins to install` and, along with the already-ticked items, also tick the `Github plugin` under SCM.

- With the plugins installed, create an admin user etc. as requested to finish the process and get Jenkins running.

- In another browser tab, log in to your Github account.

- In Github, under `Develop settings`/`Personal access tokens`, create a new personal access token; give it a meaningful name (e.g. "Jenkins access") and scope `repo`, `workflow` and `admin: repo_hook`, making a note of the personal access token string before it disappears forever when you save it, you will need it TWICE below.

- Back in Jenkins, find `Manage Jenkins` -> `Configure System`, scroll down to `Github` and add a new server `github.com`, with credentials of type `secret text` containing the personal access token (with a meaningful description, e.g. "u-blox repo access as secret text type"); press `Test connection` to make sure this works.  Don't forget to press `Save` when done.

- Reboot the machine to make sure that Docker and the Jenkins image runs at startup.

## Install NGINX Inside Docker
This is not covered here as you don't need to do it right now: it is only required when you are no-longer connected to the system locally and need to safely permit external access.  See [ACCESS.md](ACCESS.md) for how to do it, once you've done the stuff here.

## Install SMEE Inside Docker
In order to interact with Github automagically from behind a firewall you need to install a [SMEE](https://smee.io/) client inside [Docker](https://hub.docker.com/r/deltaprojects/smee-client)  as follows:

- Go to [https://smee.io](https://smee.io), start a new channel and copy the `Webhook Proxy URL` you get to the clipboard, keeping the browser tab open for now.

- To run a SMEE client, in this case on the same machine as the Jenkins Docker container but inside a Docker container of its own, connected to your SMEE channel, and able to talk to the `jenkins-custom` container on the machine's local Docker `jenkins` network, use the following Docker command:

```
docker run --name smee-client --restart=on-failure --detach --network jenkins --network-alias smee-client ---log-driver=journald deltaprojects/smee-client -u https://smee.io/thestringfromsmee -t http://jenkins-custom:8080/github-webhook/
```

### Workaround For SMEE Client Silent Failure
There is a [known issue](https://github.com/probot/smee-client/issues/179) that the SMEE client will fail silently, and stay failed, if the SMEE server changes IP address, which does happen periodically.

As a workaround, it is advisable to restart the SMEE client once a day.  To do this, copy the file [jenkins/docker_restart_container.sh](jenkins/docker_restart_container.sh) to the `/usr/bin` directory on the Jenkins server machine (then `chmod +x /usr/bin/docker_restart_container.sh`), and copy the files `jenkins/smee-client-restart.*` to the `/etc/systemd/system` on the Jenkins server machine.  Then:

```
systemctl start smee-client-restart.service
systemctl enable smee-client-restart.service
```

Note: it would be nice to have this run from within Jenkins so that it is visible and could be triggered manually, however it requires administrator privileges and persuading Jenkins to obtain those selectively when connecting over non-interactive SSH is just too darned difficult.

### Manually Restarting SMEE Client
To restart the SMEE client at any time, SSH into the Jenkins machine and enter:

```
sudo docker restart smee-client
```

To pick-up any Github pushes that landed while the SMEE client was not working, go to the `ubxlib` project in Jenkins and select `Scan Repository Now`.

## Configure Jenkins For Github Triggers
- Log-in to [Github](https://github.com) and, in your repo, go to `Settings` -> `Webhooks` and add a new Webhook containing the SMEE `Webhook Proxy URL` you obtained above, content type `application/json`, a secret of your choice and select `Pull requests` and `Pushes` as the event types.

- If you go back to the SMEE tab it should now have updated to show a webhook delivery from Github, e.g. `ping`.

- Back in Jenkins, find `Manage Jenkins` -> `Configure System`, scroll down to `Github`, add a new `Shared secret` of your choice (of type `Secret text` again), this time described as something like "ubxlib Github webhook secret" and give it the same secret value as you put in the Github Webhook above; select this `Shared secret` and **also** tick `Manage hooks` and `Save` the changes.

- At this point all the instruction say to go to `Manage Jenkins` -> `Configure System`, press the `Advanced` button under the `Github` plugin, tick `Specify another hook URL for GitHub configuration` and paste your SMEE `Webhook Proxy URL` into the box.  However...

### Workaround For The Broken Jenkins Github Plugin
- The Jenkins Github plugin GUI is BROKEN in respect of overriding the webhook URL, has been since 2020 (if you're interested you can [see all the pain](https://issues.jenkins.io/browse/JENKINS-60738); it is believed that [this is the cause](https://github.com/jenkinsci/github-plugin/pull/221)): it will not save the override URL.

- To fix this, you need to edit the XML configuration file that is inside the `jenkins-custom` Docker container directly, and Docker containers come with no editor, so to fix _that_ run the following:

```
docker exec -u root -t -i jenkins-custom /bin/bash
apt-get update
apt-get install nano
exit
```

- Then run this command to invoke `nano` on the XML file:

```
docker exec -t -i jenkins-custom /bin/nano /var/jenkins_home/github-plugin-configuration.xml
```

- Edit the `.xml` file to introduce a new key inside `<github-plugin-configuration>` at the same level as `<configs>` and `<hookSecretConfigs>` of the following form:

```
<hookUrl>https://smee.io/thestringfromsmee</hookUrl> 
```

- Note: if you mess up the file and the Docker image fails to start as a result, you can copy a file named `blah` out of a stopped Docker container with `docker cp nginx:/path/to/blah .`, edit `blah` and then copy it back into the container with `docker cp blah nginx:/path/to/blah`.

- Restart the `jenkins-custom` Docker container with:

```
docker restart jenkins-custom
```

- Back in Jenkins, refresh your `Manage Jenkins` -> `Configure System` page, press the `Advanced` button under the `Github` plugin and check that `Specify another hook URL for GitHub configuration` is ticked and your SMEE `Webhook Proxy URL` is now in the frame.  If you check your SMEE browser tab, it should have updated when you restarted `jenkins-custom`, this time with an `Event ID`.  You can also check what the SMEE client is up to by examining its log with the following command:

```
journalctl CONTAINER_NAME=smee-client
```

- You should see, for instance (from startup):

```
Forwarding https://smee.io/thestringfromsmee to http://jenkinshosturloripaddress:8080/github-webhook/
Connected https://smee.io/thestringfromsmee
POST http://jenkinshosturloripaddress:8080/github-webhook/ - 200
```

## Configure The Main `ubxlib` Project
For this we're using the most excellent [Cloudbees Github Branch Source Plugin](https://docs.cloudbees.com/docs/cloudbees-ci/latest/cloud-admin-guide/github-branch-source-plugin), the configuration of which goes as follows:

-  In your Jenkins dashboard click `New Item`, give it a meaningful name, e.g. "`ubxlib`" and select the type `Multibranch Pipeline`.

- Scroll down to `Branch Sources`, click on `Add source` and select `GitHub`.

- Unfortunately you CANNOT just use the "Jenkins access" credential, the one from the Github personal access token, you created above; you must create a new one of type `Username with password` (something to do with the way the Github HTTP API works), where the username can be anything (e.g. use `ubxlib` again), paste the `Personal Access Token` you recorded above from Github as the password, tick `Treat username as secret` (just to avoid confusing people with a randomly chosen username) and give it a meaningful description, e.g. "u-blox repo access as username and password type".

- Select this new credential as the Github credential.

- Fill in the URL of the repository, e.g. `https://github.com/u-blox/ubxlib_priv.git`.

- Press `Validate` and keep your fingers crossed.

- Scroll down to the `Build Configuration` item, where the `Mode` will be `by Jenkinsfile`: change the `Script Path` to tell it where `ubxlib` keeps the `Jenkinsfile`, i.e. `port/platform/common/automation/Jenkinsfile`.

- Press `Save` and the nice plugin should hoover-up all of the branches and pull requests of the selected repo into your Jenkins project.

**VERY IMPORTANT INDEED**: never, ever, point this at a _public_ Github repo.  The `Jenkinsfile` from the repo is pulled and _executed_ by Jenkins; `Jenkinsfile` must only come from a trusted source.

## Configure Jenkins Global Variables And Labels
There are a few global variables and a label to set in Jenkines:

- Select `Manage Jenkins` -> `Configure System` scroll down to `Global properties` and tick `Environment variables`.
- Add `UBXLIB_LOCAL_STATE_DIR` with value `local_state`.
- Add `UBXLIB_COUNTER_FILE` with value `counter`.
- Add `UBXLIB_TOKEN` with value `ubxlib_token`.
- Add `UBXLIB_EXTRA_DEFINES` and, if you have your own Wifi network that you wish to test with (so not the default value defined in [u_wifi_test_cfg.h](/wifi/test/u_wifi_test_cfg.h)), enter the value here, i.e.
  `U_WIFI_TEST_CFG_SSID=mySsid`; any other defines you need to pass into the build can be added here, separated by semicolons and leaving **no** spaces or, if there are none, simply leave the value empty.
- Press `Save`.

- Select `Manage Jenkins` -> `Configure System` and scroll down to `Lockable Resources Manager`.
- Add N `Lockable Resources` named `ubxlib_tokenX` (i.e. the value of `UBXLIB_TOKEN` above with a `0` after it, then another with a `1` after it, etc.), where N is at least as many as there are test instances with HW attached in [DATABASE.md](DATABASE.md), and give them all the label `ubxlib_token`; `Jenkinsfile` and the clean-up script [jenkins_ubxlib_clean_up.txt](scripts/jenkins_ubxlib_clean_up.txt) will lock this label to coordinate their activities.
- Press `Save`.

# Jenkins Agents
Here we configure Jenkins agents that will run all instances.

## Preparation
These preparatory steps, common to all future agent setups, can be carried out using any machine.

- Generate an SSH key pair with:

```
ssh-keygen -C "Jenkins agent key" -f ~/.ssh/jenkins_agent_key
```

...pressing \<enter\> to accept the default settings.

- Keep a copy of both keys somewhere safe; you will use them with all agents.

- Log in to Jenkins and navigate to `Manage Jenkins`-> `Manage Credentials`; from the `global` domain under `Stores scoped to Jenkins` select `Add Credentials` and add a new credential of type `SSH Username with private key` where the `Description` is "Jenkins agents SSH key", the `Username` is `ubxlib`, paste in the contents of the non-`.pub` file that you created in the first step, i.e. the private key, something beginning with `-----BEGIN OPENSSH PRIVATE KEY-----`, and press `Save`.

## Configure A Linux Agent For Distributing/Checking/Building (Centos 8 Assumed)
- Log in to your agent as the `ubxlib` user.

- Install Git, Java, Python (3), pip and gcc; for Centos Stream 8 this is:

```
sudo yum install git java-11-openjdk python3 python3-pip python3-devel gcc
```

- Install Docker; see above for advice on how to do this for Centos Stream 8.

Now follow the [common steps](#configure-a-linux-agent-common-steps) below, setting the number of executors to the number of cores on the agent machine (`lscpu` and then `Core(s) per socket` \* `Socket(s)`) and applying the labels `ubxlib`, `linux`, `x86_64`, `docker`, `build` and `distributor` (all lower case) to the agent.

Note: the labels `build` and `distributor` are added to this agent because it is the first/only one: ultimately you should apply the `build` label to your fast machines that have nothing else important to do (they won't be selected for testing, just building) and the `distributor` label to the machine which runs the initial part of [Jenkinsfile](/port/platform/common/automation/Jenkinsfile), farming the build pipeline out to others (though there's no harm in applying `distributor` to all your beefy agents).

## Configure First Linux Agent For Testing (Raspbian)
Note: you don't need one of these initially, you can complete the test system setup using Linux PCs which do building and the checking, then come back here to do the real test stuff.

For Raspberry Pi, we _could_ use Centos (7, since Centos 8 doesn't seem to be available for a Raspberry Pi yet), however it is likely better to use the native OS to \[reliably\] get the most out of the platform.  We do need 64-bit support, since the third-party tools don't all provide a 32-bit version.  That means that a Raspberry Pi 2.1 or above is required.  Note also that GCC does not provide multi-lib (i.e. 32-bit as an option) libraries for ARM 64-bit platforms, hence Raspbian 64-bit can ONLY be a 64-bit OS, you cannot build 32-bit ARM applications on it.

- Install Raspbian OS Lite 64-bit, e.g. using the [Raspberry Pi Imager tool](https://www.raspberrypi.com/software/), on a 32 or more gigabyte SD card (8 gigabytes is not enough, and the SD card should be reasonably fast), stick it in your Pi with a keyboard and monitor attached, wait for the Pi to boot and follow the instructions to create a `ubxlib` user (which will automatically get `sudo` privileges without a need to re-enter a password).

- Run `raspi-config` and, under the `Interface Options` menu item, enable SSH and, under the `System Options` menu item, set a sensible host name (e.g. `raspberrypi-100`).

- While you're there, if you happen to have a really noisy Raspberry Pi fan connected, go to `Performance Options` -> `Fan` and switch on fan control with the default settings (GPIO 14 is the pin you would have used if you followed their diagram); if you don't do this it will be like a mosquito in your ear all the time.

- Run `sudo apt update` followed by `sudo apt upgrade` to get everything up to date.

- To save typing, you may find it useful at this point to copy this [SETUP.md](SETUP.md) file and the `.pub` key you generated above onto a USB stick, put that into the Pi, mount it with something like `sudo mount /dev/sda1 /mnt`, then `sudo nano -u /mnt/SETUP.md` and write out the file again to change it to Linux line endings; from then on you can run stuff script-wise from that file.

- Install Git, Java, pip, cmake and libusb-dev (the latter for `uhubctl`, see below):

```
sudo apt install git openjdk-11-jdk python3-pip python3-dev cmake libusb-1.0-0-dev
```

- Uninstall the `modemmanager`, which may otherwise try to send AT commands to things that appear as serial ports, with:

```
sudo apt purge modemmanager
```

- Prepare for Docker installation by using `nano` to edit `/boot/cmdline.txt` and add to the end of it `cgroup_enable=1 cgroup_memory=1`, then **reboot**.

- Install Docker by following the instructions [here]( https://withblue.ink/2020/06/24/docker-and-docker-compose-on-raspberry-pi-os.html); replaying those instructions with minor edits to remove things that Raspbian already has installed:

```
sudo apt update
sudo apt install -y apt-transport-https gnupg2 software-properties-common
curl -fsSL https://download.docker.com/linux/$(. /etc/os-release; echo "$ID")/gpg | sudo apt-key add -
echo "deb [arch=$(dpkg --print-architecture)] https://download.docker.com/linux/$(. /etc/os-release; echo "$ID") $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list
sudo apt update
sudo apt install -y --no-install-recommends docker-ce cgroupfs-mount
sudo systemctl enable --now docker
```

Now follow the [common steps](#configure-a-linux-agent-common-steps) below (but **come back here** afterwards to do the remaining USB hub control part): in the common step, set the number of executors to 1 (since this is a test instance attached to physical HW under test, of which there is only one) and apply the labels `ubxlib`, `linux`, `aarch64`, `docker` and `test` (all lower case) to the agent.

Ultimately you will add another label in Jenkins for this agent, `instance_x`, where `x` is the instance number from [DATABASE.md](DATABASE.md) that this agent supports; that will come later.

### USB Hub Control With `uhubctl`
An incidental advantage of using a Raspberry Pi for testing is that the Raspberry Pi USB ports support power switching and the marvellous [uhubctl](https://github.com/mvp/uhubctl) allows us to take direct advantage of that, removing the need for an external USB cutter.

Obtain, build and install [uhubctl](https://github.com/mvp/uhubctl) with:

```
git clone https://github.com/mvp/uhubctl
cd uhubctl
make
sudo make install sbindir=/usr/local/bin
```

Note: installing into `/usr/local/bin` otherwise `/usr/sbin` is the default which requires `Jenkinsfile` to use `sudo`.

Set up a `udev` rule so that `uhubctl` can be called without `sudo` as follows:

- from your clone of `uhubctl`, copy the file `udev/rules.d/52-usb.rules` to the `/etc/udev/rules.d` directory of the Raspberry Pi,
- edit the file and change all occurrences of `DRIVER=="hub"` to `DRIVER=="usb"`,
- enter `sudo udevadm trigger --attr-match=subsystem=usb` to get this noticed,
- enter `sudo usermod -a -G dialout $USER` to add the `ubxlib` user to the `dialout` group,
- reboot for the changes to take effect; check that entering `uhubctl -a 1 -l 1-1` (i.e. without `sudo`) doesn't give you a permissions error.

If the above doesn't work, enter `sudo udevadm control --log-priority=debug` then `sudo udevadm trigger --attr-match=subsystem=usb` followed by `sudo journalctl -n 1000` and scroll down to the end of the log to check for `52-usb.rules` being read and then lines saying something like `Setting permissions /dev/bus/usb/001/001, uid=0, gid=20, mode=0664` for each USB hub, where `gid` should match the ID of the `dialout` group (`getent group | grep dialout` and it is the number in the middle).  If you don't see that, it might be that Raspbian has changed its `udev` strings \[again\]: to obtain the `udev` attributes of the USB hubs, enter `sudo uhubctl` to get the `idVendor:idProduct` of the hubs that `uhubctl` supports and then \[for a Raspberry Pi 4\] enter `sudo udevadm info -a -n /dev/bus/usb/001/001` to check the `udev` attributes of the first USB hub (a single USB2 port which just chains the next one), `sudo udevadm info -a -n /dev/bus/usb/001/002` for that next one (which has four ports) and `sudo udevadm info -a -n /dev/bus/usb/002/001` for the four-port USB3 hub (cross-match the IDs to be sure); you can check that the attributes match-up with the ones used in `52-usb.rules`.

FYI, on the Raspberry Pi the power to **all** USB ports is controlled at once; switch them off with `uhubctl -a 0 -l 1-1 -r 100` and on again with `uhubctl -a 1 -l 1-1`.  The `-r` on the "off" option is necessary with some Segger J-Link boxes because they try to switch themselves back on again; `-r` makes `uhubctl` retry to really shut them down.

Note: if you have just added a new Raspberry Pi and are having trouble matching the thing you are SSH'ed-into with the real world, switching the USB hub off and on again, assuming there is a board attached with LEDs on it, is a good way of determining what's what/who's who/which is which.

### Setting Up Pi N + 1
One of the advantages of the Raspberry Pi is that, once you have an SSD sorted/running, provided you buy SD cards of the same size (e.g. 32 gigabytes) you can just copy to a new one using something like [HDD Raw Copy Tool](https://hddguru.com/software/HDD-Raw-Copy-Tool/).  The procedure for adding a new Raspberry Pi then becomes:

- copy the SD card of Pi N (or your standard image) to the SD card for Pi N + 1.
- insert the card into a Pi, connect the Pi to the network and let it be given a default DHCP address (e.g. 10.10.2.3 for the network setup described in [NETWORK.md](NETWORK.md)).
- SSH into the Pi, run `sudo raspi-config` and, under the `System Configuration` options, set the correct `Host Name` for this new Raspberry Pi, e.g. `raspberrypi-xxx`, where `xxx` is the last digit of the IP address you are about to statically allocate for it,
- DON'T REBOOT THE PI YET,
- on the router, statically allocate the correct IP address for this Pi,
- on the SSH connection again, persuade the Raspberry Pi to release and renew its IP address with`sudo ifconfig eth0 down; sudo ifconfig eth0 up`; your SSH connection will close,
- make sure that you can start a new SSH connection on the new IP address to be sure that all is good,
- add the agent in Jenkins by copying an existing Raspberry Pi agent and changing the name, IP address and `instance_x` label as appropriate.

## Configure A Linux Agent: Common Steps
These steps apply to both the Centos and Raspbian Linux agent configuration processes.

- Install Docker Compose:

```
sudo curl -L "https://github.com/docker/compose/releases/download/2.12.2/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
sudo chmod +x /usr/local/bin/docker-compose
```

- Add the `ubxlib` user to the `docker` group, otherwise you will likely get the error "Got permission denied while trying to connect to the Docker daemon socket" when Docker is used later; you will need to log out and back in again for this change to take effect:

```
sudo usermod -aG docker ${USER}
```

- Add the `.pub` key created above to the list of authorized keys for the agent machine with something like:

```
cat jenkins_agent_key.pub >> ~/.ssh/authorized_keys
```

- Set the permissions of the `~/.ssh` directory and files to ones that an SSH daemon will be happy with:

```
chmod 700 ~/.ssh
chmod 600 ~/.ssh/authorized_keys
```

- Jenkins has to be able to run `sudo` commands without entering a password (otherwise it can't run Docker); to do this run `sudo visudo`, find the line below and remove the `#` and space before `%wheel` (you'll be in `vi`, so `i` to enter insert mode, do your edits, press `ESC` to go back to command mode and then enter `:wq` to write the file and exit or enter `:q!` to exit without saving):

```
# Same thing without a password
# %wheel        ALL=(ALL)       NOPASSWD: ALL
```

- In Jenkins, navigate to `Manage Jenkins`-> `Manage Nodes and Clouds` and select `New Node`; give the agent a meaningful name (it is useful to match the name with a physical label that is on the front of the machine so that you can find it in the real world), tick `Permanent Agent` and then press `Create`.

- Set the number of executors appropriately, populate `Remote root directory` with `/home/ubxlib`, add the appropriate labels, select the `Launch method` to be `Launch agents via SSH`, provide the URL/IP-address of the machine, select the credential you created in the preparation step above and set `Host Key Verification Strategy` to `Manually trusted key Verification Strategy`.

- **Raspbian** agents only: at the same point in the configuration menu press `Advanced`, find the `Prefix Start Agent Command` and in there type `. ~/.profile && env && `: without this Jenkins won't get the user's environment with vital things in it like the correct path; this will also print out the final environment so that you can check it.

- Now press `Save` and the agent should be connected/installed; if you have problems, you can see what is upsetting the SSH daemon on the agent machine with:

```
systemctl status sshd
```

## Configure A Windows Agent For Building/Testing (Windows 10 Or Later)
Note: you can complete a "first draft" of the setup using Linux PCs then come back to add this later.

- Install [Notepad++](https://notepad-plus-plus.org/downloads/), [Git](https://git-scm.com/downloads) (selecting Notepad++ as your editor; Git will also bring SSH with it), [Python3](https://www.python.org/downloads/) (ticking `Add Python.exe to path` and choosing the "disable Windows path length limit" option in the last dialogue box; Python will also bring Pip with it) and [Java JDK](https://www.oracle.com/java/technologies/downloads/) (11 or higher; use the MSI installer).

- In order to support UART loop-back testing, install a licensed version of [Virtual Serial Port](https://www.virtual-serial-port.org/).

- Open a command prompt as administrator.

- Add the Python **Roaming** scripts directory, something like `C:\Users\ubxlib\AppData\Roaming\Python\Python38\Scripts`, to your path with something like:

```
setx path="%path;C:\Users\ubxlib\AppData\Roaming\Python\Python38\Scripts"
```

- Install the Python curses library (needed 'cos `libcurses` doesn't exist on Windows and it gets dragged in by one of our required Python modules) with:

```
pip install windows-curses
```

- Add the `.pub` key created above to the list of authorized keys for the agent machine, from a command prompt run as administrator, with something like:

```
type jenkins_agent_key.pub >> %homedrive%\ProgramData\ssh\administrators_authorized_keys
```

- Set the permissions of the `administrators_authorized_keys` file to ones that the SSH daemon will be happy with:

```
icacls.exe %homedrive%\ProgramData\ssh\administrators_authorized_keys /inheritance:r /grant "Administrators:F" /grant "SYSTEM:F"
```

- Go to `Settings` -> `Apps` -> `Optional features` -> `Add a feature` and install `OpenSSH Server`.

- Back at the command prompt, type `wf.msc`, `Inbound Rules`, find the `OpenSSH SSH Server` entry, `Properties` -> `Advanced` and **untick** `Public`.

- Type `services.msc`, find the `OpenSSH SSH Server` service, set `Startup Type` to `Automatic`, then press `Start` and `Apply`.

- In Jenkins, navigate to `Manage Jenkins`-> `Manage Nodes and Clouds` and select `New Node`; give the agent a meaningful name, tick `Permanent Agent` and then press `Create`.

- If the account name on the agent is not `ubxlib` you will need to make a new credential, exactly as you created the one in the preparation step above, but with the appropriate username/password/description.

- Set the number of executors to the number of cores on the agent machine (from `Task Manager`, `Performance` tab), populate `Remote root directory` with `C:\Users\ubxlib` (or whatever: unfortunately you can't use the `%thing%` form here), add the labels `ubxlib`, `windows`, `x86_64` and `build` (all lower case), select the `Launch method` to be `Launch agents via SSH`, provide the URL/IP-address of the machine, select the credential you created in the preparation step above and set `Host Key Verification Strategy` to `Manually trusted key Verification Strategy`.

- Now press `Save` and Jenkins will try to connect the agent; if this fails because you don't yet trust it, select `Configure` from the menu on the left and say that you trust it.  The agent should then be connected; if you have problems, on the agent enter `eventvwr.msc` and look at the logs under `Applications and Services Logs` -> `OpenSSH` -> `Operational` to see if you can see what's upsetting the SSH server.  If there's nothing being logged there you could open `wf.msc` once more and, with `Windows Defender Firewall with Advanced Security` highlighted, select `Action` -> `Logging` -> `Customize` and set both `Log dropped packets` and `Log successful connections` to `Yes` for the `Domain` and `Private` tabs.  You should then be able to find the log file `%systemroot%\system32\LogFiles\Firewall\pfirewall.log` from the monitoring menu item and see if connections are being rejected on port 22.  If no connections are even being rejected from the Jenkins machine, select `Inbound Rules`, create a `New Rule`  of type `Custom` for the `ICMPv4` protocol on any port, `Allow the connection`, **untick** `Public`, give it the name `ICMP`, press `Finish` and check that the Jenkins machine can get a ping response from the agent machine.

- Ultimately you will add another label in Jenkins for this agent, `instance_x`, where `x` is the instance number from [DATABASE.md](DATABASE.md) that this agent supports; that will come later.

- Follow the instructions under the [Windows platform here](/port/platform/windows/mcu/win32#sdk-installation) to install MSVC with CMake, which is all you should need for building/running code on a Windows agent (no need for VSCode or a separate installation of CMake).

- Oh, and of course, make sure that the PC is set to never sleep.

# Jenkins
With the steps above completed, so that you have a Jenkins master plus at least one Linux Jenkins agent, a few more things need to be set up, mostly in Jenkins itself and one more thing on the agent(s).

## Building The Docker Image On The Linux Agent(s)
A Docker image needs to be built on each Linux agent containing all the basic stuff that the `ubxlib` builds need.  We do this by creating a Jenkins project that can be run on all agents as follows:

- In Jenkins, create a new `Freestyle` project and name it `ubxlib_agent_docker_build`.

- Give it a description, e.g. "Update the Docker image on all ubxlib Linux build agents from the current ubxlib_priv master branch.".

- Tick `This project is parameterised` and create three parameters:
  - Name `UBXLIB_PRIV_REV`, default value `master`, description: "The branch of ubxlib_priv to use.", tick `Trim the string`.
  - Name `UBXLIB_DOCKER_FOLDER`, default value `port/platform/common/automation/docker/builder`, description: "The folder where the Dockerfile and .yaml file can be found.", tick `Trim the string`.
  - Name `UBXLIB_NODES`, leave `Default nodes` alone, set `Possible nodes` to `ALL`, tick `Allow multi node selection for concurrent builds`, no need for a description.

- Tick `Execute concurrent builds if necessary`; this allows you to select and run the project on multiple agents at the same time.

- Tick `Restrict where this project can be run` and enter the label expression `ubxlib && linux && docker`.

- Under `Source Code Management` tick `Git`, add the repository URL, e.g. `https://github.com/u-blox/ubxlib_priv/`, select the credentials you entered previously, i.e. "u-blox repo access as username and password type", set `Branch Specifier` to `*/$UBXLIB_PRIV_REV`, under `Additional Behaviours` select `Check out to a sub-directory` and enter `ubxlib`.

- Under `Build Steps` select `Execute shell` and paste in the text below:

```
#!/bin/sh
echo Updating ubxlib Docker image on this agent.
echo Branch will be $UBXLIB_PRIV_REV, folder where the Docker files are found is assumed to be \"$WORKSPACE/ubxlib/$UBXLIB_DOCKER_FOLDER\".
cd $WORKSPACE/ubxlib/$UBXLIB_DOCKER_FOLDER
pwd
sudo -E /usr/local/bin/docker-compose build
if [ $? ]; then
    if [ -d /home/"$USER"/.docker ]; then
        sudo chown "$USER":"$USER" /home/"$USER"/.docker -R
        sudo chmod g+rwx "/home/$USER/.docker" -R
    fi
fi
```

- Press `Save` and run the new project (`Build with Parameters`) on the existing agents: it should build the Docker image on each of them, and may take quite some time (several minutes).  When you add a new Linux Jenkins agent you will need to run this project on that agent.

## Shared Resource Control
The `Jenkinsfile` of the test system uses \[KMTronic\] Ethernet-based relay boxes to power up and down share resources (BLE and Wi-Fi test peers and the like) and to provide a central power source for cellular EVKs.  The controls for these need to be configured.

Note: for KMTronic boxes it is a good idea to keep the `Relay Name` fields under `Configuration` up to date, e.g. with something like `Inst 23: SARA-R5 EVK RF box`, putting the `Inst x:` bit on the front to match with a [DATABASE.md](DATABASE.md) instance number and staying within the KMTronic's 30 character limit (the relay box will reboot when you press `Save Config` so best not done while the system is running).

### URL Strings
Once the Ethernet relay control box is connected to the test system set a pair of `UBXLIB_POWER_SWITCH_x_ON` / `UBXLIB_POWER_SWITCH_x_OFF` Jenkins environment variables to URL strings that will switch the entire thing on or off.  For example, if you had KMTronic box 1 on IP address 10.10.2.210 then you would select `Manage Jenkins` -> `Configure System` scroll down to `Global properties`, tick `Environment variables` and add:

- `UBXLIB_POWER_SWITCH_1_ON` with the value `http://10.10.2.210/FFE0FF`,
- `UBXLIB_POWER_SWITCH_1_OFF` with the value `http://10.10.2.210/FFE000`,

...etc.

### Script
- Ceate a new `Pipeline` project and name it `ubxlib_shared_resource_control`.
- Give it a description, e.g. "Power up/down shared resources.".
- Tick `This project is parameterisd` and add a `Boolean` parameter named `ON_NOT_OFF`.
- Into `Pipeline script` paste the contents of [jenkins_ubxlib_shared_resource_control.txt](scripts/jenkins_ubxlib_shared_resource_control.txt) and `Save` the project.

## First-Run Script Approvals In Jenkins
You should now manually trigger the `master` branch of the `ubxlib` project in Jenkins (click on it and select `Build Now` from the menu on the left).  It will fail first because of script method approvals that need to be applied in Jenkins.  Check the console output of the most recent run in Jenkins for a string of the form:

`"Scripts not permitted to use blah. Administrators can decide whether to approve or reject this signature."`

...where "blah" is some method or other.

You should click on the link provided, add the method to the "approved" list, and then re-run `master`; rinse and repeat until these errors go away.

Note: if you are messing with `Jenkinsfile` later you will find that it barfs a huge and meaningless Java stack trace at you, like the above, but with even less context; the thing to look for are the lines prefixed with `WorkflowScript`: these are the actual lines in your `Jenkinsfile`.

## Install Python Modules On All Agents
Next `master` will fail because the necessary Python modules are not yet installed on the agents; now that you have the `ubxlib` repo on the agent it is easy to fix this by running (see also Windows exception below):

```
pip3 install --user -r ~/workspace/ubxlib_master/ubxlib/port/platform/common/automation/requirements.txt
```

Since the agent is now connected to Jenkins you can do this through `Manage Jenkins` -> `Manage nodes and clouds`, select your agent, then choose `Script Console` from the menu on the left and enter:

```
println new ProcessBuilder("sh", "-c", "pip3 install --user -r ~/workspace/ubxlib_master/ubxlib/port/platform/common/automation/requirements.txt").redirectErrorStream(true).start().text
```

...or for a Windows machine:

```
println new ProcessBuilder("cmd", "/c", "pip3 install --user -r %homedrive%%homepath%\\workspace\\ubxlib_master\\ubxlib\\port\\platform\\common\\automation\\requirements.txt").redirectErrorStream(true).start().text
```

All the complication with `ProcessBuilder` is so that you get error messages back, which are otherwise lost to `stderr`; note that if you use this construction for other commands then, within the strings, special characters like `$` must be escaped and you should use single quotes.

**Windows** only: the Python module `lxml` requires a compilation step with some library files that aren't easily downloaded to Windows, so for this specific Python module on Windows instead do the following:

- get your Python version by entering `python --version`,
- go to [Christoph's Gohlke's Python page](https://www.lfd.uci.edu/~gohlke/pythonlibs/#lxml) and download the file `lxml-a.b.c-cpXX-cpXX-none-win_amd64.whl` where `XX` matches your Python version: e.g. for Python 3.8 you would download `lxml-4.9.0-cp38-cp38-win_amd64.whl`,
- install this with `pip install ` followed by the `.whl` file name.

Obviously the Python module installation steps here will need to be repeated for any new agent; it is best to **reboot** the agent (or for a Windows agent restart the `sshd` service from Task Manager) afterwards so that any new script locations get added to the path for the current session.

## Create Wi-Fi Passkey Credential In Jenkins
Next `master` will fail because it cannot find the credential with ID `ubxlib_wifi_passkey`.  The `ubxlib` Wi-Fi tests require access to a Wi-Fi network, which in the `ubxlib` test code is of SSID [U_WIFI_TEST_CFG_SSID](/wifi/test/u_wifi_test_cfg.h).  The passkey for that Wi-Fi network is, for obvious reasons, not stored in the repo itself, instead it is stored as a credential inside Jenkins which is parsed-out and passed into the builds as a conditional compilation value by [Jenkinsfile](Jenkinsfile) (look for the `withCredentials` bit).

In Jenkins, go to `Manage Jenkins` -> `Manage Credentials`, create a credential of type `Secret text`, paste the passkey for the Wi-Fi network into the `Secret` field, give it the `ID` `ubxlib_wifi_passkey` and a sensible `Description`, e.g. "WiFi passkey for network U_WIFI_TEST_CFG_SSID", then press `Save`.

## Wait For Platform-Specific Tools To Be Installed On The Agents
Trigger the `master` branch again and be patient; the various instances will install platform-specific tools onto the agents, which can take some time; restart the run if it times out.

## Misc
Some other things to set:

- Under `Manage Jenkins` -> `Configure System` find `Jenkins Location` set the `System Admin e-mail address` to `ubxlib@u-blox.com`.
- Under `Manage Jenkins` -> `Configure System` find `Global Build Discarders`, add a `Default Build Discarder` and set `Days to keep builds` and `Days to keep artifacts` to some sensible number (e.g. 30).
- Under `Manage Jenkins` -> `Configure System` find `Test Results Analyzer` and tick `Display run time for each test` 'cos that's useful to see.
- You can dismiss the warning about not running jobs on the Jenkins master - we need to do that as we use it for thread-safety when managing shared resources.
- In order to manage shared resources [Jenkinsfile](Jenkinsfile) uses a file, `shared_resources/counter`, on the Jenkins master, to count the number of things currently using those shared resources.  In case this ever gets out of step with reality, create a new `Pipeline` project, name it `ubxlib_clean_up`, give it a description, e.g. "Clean things up when all nodes are idle.", tick `Build periodicallty` and enter `H 6 * * *` (run at approcimately 06:00 daily), then into `Pipeline script` paste the contents of [jenkins_ubxlib_clean_up.txt](scripts/jenkins_ubxlib_clean_up.txt) and `Save` the project.

# Configure Test Instances
With all of the Jenkins stuff done, and at least one of each agent type (Linux "beefy", Linux Raspbian and Windows), you can start configuring the instances.  Instances up to and including 9, the "check" instances need no further attention; it is the "test" instances, i.e. a thing with real MCUs/modules attached, that need additional configuration.

Each test instance is defined in [DATABASE.md](DATABASE.md);  In all cases except the lone Linux and Windows test instances a Raspberry Pi is associated with the major number of each test instance, so 11.0 and 11.1 are the same set of HW attached to one Raspberry Pi (just running different OSes or some such), 12 is another Raspberry Pi, etc.  The instructions here don't go into the wiring of each set of external HW, that you need to figure out from the definitions in `u_cfg_app_platform_specific.h` for the platform plus the defintions for that test instance in [DATABASE.md](DATABASE.md); what is covered here is the generic setup of each test instance and then hints on any important aspects of a test instance.

Once you have configured an agent to support a test instance defined in [DATABASE.md](DATABASE.md), add the label `instance_x`, where `x` is the major instance number (e.g. `instance_11`), to that agent and Jenkins will be able to find it.

## Generic Setup
### Linux `udev` Rules
Devices connected via USB to a Linux host need a `udev` rule to make them accessible to Docker and have a fixed identity.  

- A file is provided [53-dut.rules](53-dut.rules) with a set of the known devices; copy this file into the directory `/etc/udev/rules.d`.

- Note: if you end up fiddling with this don't forget to make sure the USB is switched on with `uhubctl -a 1 -l 1-1`.

- If you should add to the file (and please don't forget to update the one here), reload the `/dev` folder with:

```
sudo udevadm control --reload
sudo udevadm trigger
```

- To debug issues with your `udev` rules, `sudo udevadm control --log-priority=debug` then `sudo udevadm trigger` followed by `sudo journalctl -n 1000` and scroll down to the end of the log to check for `53-dut.rules` being read and then look for lines where its rules are being triggered.

- Note: if you edit a `.rules` file, don't forget to reload them all with `sudo udevadm control --reload` before testing.

- If you end up creating a new device name and need to associate it with an instance, `x`, from [DATABASE.md](DATABASE.md), you do that by editing the `CONNECTION_INSTANCE_x` entry in the file `~/.ubx_automation/settings_v2_agent_specific.json` under the `ubxlib` home directory of the relevant agent.

### Test Servers/Broker
The sockets, MQTT and HTTP tests rely on peer entities to be present and visible from the public internet.  These can all be run on a single server somewhere (e.g. a [Digital Ocean](https://www.digitalocean.com/) droplet) where you can control the firewall.  Follow the intructions under the [sockets](/common/sock/test/echo_server), [MQTT](/common/mqtt_client/test/mqtt_broker) and [HTTP](/common/http_client/test/http_server) test directories for instructions on how to set each one up.  Remember as you do this that there will likely be _two_ firewalls in which you have to open the relevant ports: one in the network the test peer is on and one on the test peer machine itself; _both_ must be open for incoming connections of the relevant protocol type for access to work.

If the server has no DNS entry on the public internet (required for some tests) you might use a service such as [noip](https://www.noip.com/) to give it one.

### Cellular And Short Range Network Test Peers
A Nutaq cellular network box is required for cellular Cat-M1 coverage; set up of this is out of scope of this document: provided an RF link gets to the relevant test instances and the Nutaq has public internet access, that is all that is required.  To be clear, the Nutaq box does _not_ have to be on the same network as the `ubxlib` test system (though it can be if desired).

Some short-range test instances require BLE test peers; these just need to be configured, MAC addressses in [DATABASE.md](DATABASE.md) and then plugged into power from the shared resource Ethernet-based relay boxes so that they are powered up at the start of testing and powered down again afterwards.

Similarly, some of the short-range test instances require access to a Wi-Fi Access Point; any Wi-Fi AP that has public internet access is fine (see Wi-Fi passkey configuration above).

## Test Instance Hints
- Instance 23, Windows:
  - Requires you to configure the [Virtual Serial Port](https://www.virtual-serial-port.org/) application to create a `loopback` serial port.  [DATABASE.md](DATABASE.md) will tell you which is the loopback serial port (`U_CFG_TEST_UART_A`, usually 100).
  - When a cellular, short-range, whatever, board is plugged into the Windows machine you will need to go into the `Device Manager` -> `Port Settings` -> `Advanced` and set the COM port number to match the one given in [DATABASE.md](DATABASE.md); e.g. `U_CFG_APP_SHORT_RANGE_UART=101` means you set the COM port for the short-range board to `COM101`.
  - For a cellular EVK, which uses an FTDI chip, you will need to download and install the [FTDI Windows drivers](https://ftdichip.com/drivers/vcp-drivers/); if you use just the Windows 10 drivers you will end up with character loss, particularly noticable with the chip-to-chip tests.
  - This instance also runs the special test `networkOutage` which controls an external MiniCircuits Ethernet-based RF switch and KMTronic Ethernet-based relay box; you will see in [DATABASE.md](DATABASE.md) the macros `U_CFG_TEST_NET_STATUS_CELL` and `U_CFG_TEST_NET_STATUS_SHORT_RANGE` which equate to strings such as `RF_SWITCH_A` and `PWR_SWITCH_A`.  The entries under `SWITCH_LIST` in the file `%homedrive%%homepath%\.ubx_automation\settings_v2_agent_specific.json` on the Windows agent map `RF_SWITCH_A`/`PWR_SWITCH_A` to actual IP addresses and an action for `0` or `1`, e.g. `:SETA=1` to switch on the RF switch, `FF0101` to switch on port 1 of the KMTronic switch, all of which are put together by the test scripts to form a URL string.  You need to set up the Ethernet addresses for these correctly in the `%homedrive%%homepath%\.ubx_automation\settings_v2_agent_specific.json` file of the Windows agent.  For the short-range part of this test local test peers are required for both WiFi and BLE: this is done with an appropriately configured NINA-W1 board, powered from a relay on `PWR_SWITCH_A` (e.g. the first relay on the KMTronic relay box) so that it can be switched off by the test script and configured to advertise a given BLE MAC address (e.g. remote central `6009C390E4DAp`) and include a Wi-Fi AP of a known SSID ( e.g. `disconnect_test_peer`, though not broadcast).
  - Note: if you have problems getting a COM port to appear, which does happen randomly, unplug the USB cable, go to the FTDI website and download their "CDM uninstaller" tool, get it to uninstall drivers for the relevant HW ID (e.g. `4030 6011` for the FTDI chip on the NINA-W1 EVK), then just plug the USB in again and a COM port should appear.
- Instance 24 is pure Linux and needs a 32-bit compiler, which means it cannot be run on a Raspberry Pi (since GCC for ARM64 is 64-bit only); you will need to add the label `instance_24` to one of the non-Pi Linux nodes for this.
- Instances 13.x, 15.x, 17 and 18 use a SEGGER J-Link probe (either built-in or in a dedicated JLink Base box) and address it by serial number; the correct serial number needs to be set for that instance in the `~/.ubx_automation/settings_v2_agent_specific.json` file of the Raspberry Pis.  You can see what serial number is connected by running the `nrfjprog --ids` command inside the Docker container that [Jenkinsfile](Jenkinsfile) runs:
```
docker run --rm ubxlib_builder nrfjprog --ids
```

  ...and, for instance, if the serial number were 51014525, the entry in `~/.ubx_automation/settings_v2_agent_specific.json` would be (don't forget to take the `_FIX_ME` off the end):

```
 "CONNECTION_INSTANCE_18": {
    "serial_port": "/dev/segger_jlink_base",
    "debugger": "51014525"
  },
```
- Instances 14 and 19, the ST ones, include an STLink debug chip where, again, the serial number needs to be correctly populated for that entry in `~/.ubx_automation/settings_v2_agent_specific.json` on the Raspberry Pi.  If you don't know the serial number, on the Pi just run `uhubctl` and it will, helpfully, tell you the serial number of the ST-Link board.
- Instance 18 includes a test of Cloud Locate for which client ID, username and password parameters need to be entered as environment variables and a secret in Jenkins:
  - `Manage Jenkins` -> `Configure System` scroll down to `Global properties`, tick `Environment variables` and add the client ID and username on the end of the existing `UBXLIB_EXTRA_DEFINES` environment variable, separated with semicolons, e.g. `U_CFG_APP_CLOUD_LOCATE_MQTT_CLIENT_ID=device:521b5a33-2374-4547-8edc-50743c144509;U_CFG_APP_CLOUD_LOCATE_MQTT_USERNAME=WF592TTWUQ18512KLU6L`, being sure to leave **no** spaces,
  - `Manage Jenkins` -> `Manage Credentials`, create a credential of type `Secret text`, paste the password for the Thingstream account, something like `nsd8hsK/NSDFdgdblfmbQVXbx7jeZ/8vnsiltgty` into the `Secret` field, give it the `ID` `ubxlib_cloud_locate_mqtt_password` and a sensible `Description`, e.g. "Password for the Thingstream account with Cloud Locate", then press `Save`; [Jenkinsfile](Jenkinsfile) will parse this out and into a conditional compilation flag value for the builds.
