FROM ubuntu:20.04 AS ubxbuilder
MAINTAINER Andreas Anderberg <andreas.anderberg@u-blox.com>


# Non-interactive debconf package configuration
ARG DEBIAN_FRONTEND=noninteractive

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# gcc-multilib (needed by codechecker and deps to compile for 32-bit
# on a 64-bit architecture) and g++-multilib (needed by Zephyr and deps
# for the same reason) are not available for GCC on ARM64 so install them
# separately here only if we're not on ARM64 architecture (where we just
# don't compile 32-bit applications)
RUN if [ "$(arch)" != "aarch64" ]; then                                \
        apt-get update && apt-get install --no-install-recommends -y \
            gcc-multilib g++-multilib;                               \
    fi

# Install codechecker and deps
RUN apt-get update && apt-get install -y \
    locales wget software-properties-common && \
    wget -qO - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - && \
    add-apt-repository "deb http://apt.llvm.org/focal/ llvm-toolchain-focal-14 main" && \
    apt-get update && apt-get install --no-install-recommends -y \
        sudo clang-14 clang-tidy-14 clang-format-14 libpq-dev make build-essential \
        curl git python3-venv python3-dev python3-pip python3-setuptools libsasl2-dev \
        libldap2-dev libssl-dev && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-14 9999 && \
    update-alternatives --install /usr/bin/clang clang /usr/bin/clang-14 9999 && \
    update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-14 9999 && \
    update-alternatives --install /usr/bin/python python /usr/bin/python3 1 && \
    pip3 install thrift codechecker && \
# Cleanup
    apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Install Zephyr dependencies
RUN apt-get update && apt-get install --no-install-recommends -y \
        git ninja-build gperf ccache dfu-util wget \
        python3-dev python3-pip python3-setuptools python3-tk python3-wheel xz-utils file \
        make gcc && \
# Install west + more deps
    pip3 install west click intelhex pyelftools cryptography && \
# Install ESP-IDF dependencies
    apt-get install --no-install-recommends -y git wget flex bison gperf \
        python3 python3-pip python3-setuptools cmake ninja-build ccache \
        libffi-dev libssl-dev dfu-util libusb-1.0-0 && \
# Install ubxlib automation stuff
    apt-get install -y --no-install-recommends  astyle doxygen unzip && \
# Cleanup
    apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Install Nordic command-line tools
RUN apt-get update && apt-get install -y wget && \
    if [ "$(arch)" != "aarch64" ]; then                                \
        wget -q -P /tmp https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/desktop-software/nrf-command-line-tools/sw/versions-10-x-x/10-18-1/nrf-command-line-tools_10.18.1_amd64.deb && \
        apt-get update && apt-get install --no-install-recommends -y /tmp/nrf-command-line-tools_10.18.1_amd64.deb;                                                                                               \
    else                                                                                                                                                                                                          \
        wget -q -P /tmp https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/desktop-software/nrf-command-line-tools/sw/versions-10-x-x/10-18-1/nrf-command-line-tools_10.18.1_arm64.deb && \
        apt-get update && apt-get install --no-install-recommends -y /tmp/nrf-command-line-tools_10.18.1_arm64.deb;                                                                                               \
    fi &&                                                                                                                                                                                                         \
# Cleanup
    apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Install Segger tools
# Segger put an "accept our terms and conditions" page
# in front of the download of their tools; fortunately
# we can download a ZIP file from Nordic which contains
# the tools
RUN apt-get update && apt-get install -y wget unzip tar &&                          \
# apt-get needs this to allow it to create temporary files
    chmod 1777 /tmp &&                                                              \
    if [ "$(arch)" != "aarch64" ]; then                                             \
        wget -q -P /tmp https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/desktop-software/nrf-command-line-tools/sw/versions-10-x-x/10-18-1/nrf-command-line-tools-10.18.1_linux-amd64.tar.gz &&  \
        tar -xf /tmp/nrf-command-line-tools-10.18.1_linux-amd64.tar.gz -C /tmp &&   \
# use dpkg to unpack the .deb file and then switch off post-install steps that will fail 'cos this is Docker and udevadm stuff doesn't really work 
        dpkg --unpack /tmp/JLink_Linux_V780c_x86_64.deb;                            \
    else                                                                            \
        wget -q  -P /tmp https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/desktop-software/nrf-command-line-tools/sw/versions-10-x-x/10-18-1/nrf-command-line-tools-10.18.1_linux-arm64.tar.gz && \
        tar -xf /tmp/nrf-command-line-tools-10.18.1_linux-arm64.tar.gz  -C /tmp &&  \
# use dpkg to unpack the .deb file and then switch off post-install steps that will fail 'cos this is Docker and udevadm stuff doesn't really work 
        dpkg --unpack /tmp/JLink_Linux_V780c_arm64.deb;                             \
    fi &&                                                                           \
    rm /var/lib/dpkg/info/jlink.postinst -f &&                                      \
# true on the end because this will emit an error code (something in the Segger installation file) that we fix in the next line
    dpkg --configure jlink || true &&                                               \
# Fix broken depedencies in the Segger installation file
    apt --fix-broken install -y &&                                                  \
# Cleanup
    apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

WORKDIR /workdir


#***************************************************
# Add missing Python modules and apk packages here
#***************************************************

RUN chmod 1777 /tmp && \
    pip3 install pyserial pylint psutil pylink-square requests_toolbelt rpyc debugpy invoke coloredlogs verboselogs && \
    apt-get update && apt-get install -y --no-install-recommends  \
        usbutils gawk iputils-ping openssh-client socat \
# Needed for OpenOCD
        libhidapi-hidraw0 && \
# Cleanup
    apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*


#***************************************************
# Setup environmental variables
#***************************************************

#***************************************************
# Some last steps
#***************************************************

# Add and switch to "ubxlib" user
ARG USER="ubxlib"
RUN groupadd -f -g 1000 -o $USER && \
    useradd -ms /bin/bash -u 1000 -g 1000 $USER && \
    chown ubxlib:ubxlib /workdir
USER ubxlib
