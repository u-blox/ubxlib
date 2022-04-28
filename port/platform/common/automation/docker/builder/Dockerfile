FROM ubuntu:20.04 AS ubxbuilder
MAINTAINER Andreas Anderberg <andreas.anderberg@u-blox.com>


# Non-interactive debconf package configuration
ARG DEBIAN_FRONTEND=noninteractive

# Install codechecker deps
RUN apt-get update && apt-get install -y \
        locales wget software-properties-common && \
    wget -qO - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - && \
    add-apt-repository "deb http://apt.llvm.org/focal/ llvm-toolchain-focal-14 main" && \
    apt-get update && apt-get install --no-install-recommends -y \
        clang-14 clang-tidy-14 clang-format-14 libpq-dev make build-essential \
        curl gcc-multilib git python3-venv python3-dev python3-pip python3-setuptools libsasl2-dev \
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
        git ninja-build gperf  \
        ccache dfu-util wget \
        python3-pip python3-setuptools python3-tk python3-wheel xz-utils file \
        make gcc gcc-multilib g++-multilib && \
# Install west + more deps
    pip3 install west click intelhex pyelftools cryptography && \
# Install ESP-IDF dependencies
    apt-get install --no-install-recommends -y \
        git wget flex bison gperf \
        python3 python3-pip python3-setuptools cmake ninja-build ccache \
        libffi-dev libssl-dev dfu-util libusb-1.0-0 && \
# Install ubxlib automation stuff
    apt-get install -y --no-install-recommends  \
        astyle doxygen unzip && \
# Cleanup
    apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

WORKDIR /workdir


#***************************************************
# Add missing Python modules and apk packages here
#***************************************************

RUN pip3 install \
        pyserial pylint psutil pylink-square requests_toolbelt rpyc debugpy invoke coloredlogs verboselogs && \
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

