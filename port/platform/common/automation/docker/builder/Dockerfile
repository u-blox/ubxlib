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
        sudo clang-14 clang-tidy-14 clang-format-14 cppcheck libpq-dev make build-essential \
        curl git python3-venv python3-dev python3-pip python3-setuptools libsasl2-dev \
        libldap2-dev libssl-dev && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-14 9999 && \
    update-alternatives --install /usr/bin/clang clang /usr/bin/clang-14 9999 && \
    update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-14 9999 && \
    update-alternatives --install /usr/bin/python python /usr/bin/python3 1 && \
    pip3 install codechecker && \
# Cleanup
    apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Install a later version of CMake (required by Zephyr 3 and AStyle 3.4)
RUN                                                                                          \
    if [ "$(arch)" != "aarch64" ]; then                                                      \
        wget -q -P /tmp https://cmake.org/files/v3.24/cmake-3.24.1-linux-x86_64.tar.gz &&    \
        tar --strip-components=1 -xf /tmp/cmake-3.24.1-linux-x86_64.tar.gz -C /usr/local &&  \
        rm -rf /tmp/*;                                                                       \
    else                                                                                     \
        wget -q -P /tmp https://cmake.org/files/v3.24/cmake-3.24.1-linux-aarch64.tar.gz &&   \
        tar --strip-components=1 -xf /tmp/cmake-3.24.1-linux-aarch64.tar.gz -C /usr/local && \
        rm -rf /tmp/*;                                                                       \
    fi

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
    apt-get install -y --no-install-recommends doxygen unzip && \
# Install AStyle 3.4.10 (apt-get currently installs only 3.1)
    wget https://sourceforge.net/projects/astyle/files/astyle/astyle%203.4/astyle-3.4.10.tar.bz2/download -O astyle.tar.bz2 && \
    tar -xf astyle.tar.bz2 && \
    cd astyle-3.4.10 && \
    mkdir as-gcc-exe && \
    cd as-gcc-exe && \
    cmake ../ && \
    make && \
    make install && \
    cd ../.. && \
# Install GPIO API library for Linux; gpiod not strictly necessary but useful for debugging
   apt-get install -y --no-install-recommends libgpiod-dev gpiod && \
# Cleanup
    apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* astyle-3.4.10

# Install Nordic command-line tools
RUN apt-get update && apt-get install -y wget && \
    if [ "$(arch)" != "aarch64" ]; then                                \
        wget -q -P /tmp https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/desktop-software/nrf-command-line-tools/sw/versions-10-x-x/10-21-0/nrf-command-line-tools_10.21.0_amd64.deb && \
        apt-get update && apt-get install --no-install-recommends -y /tmp/nrf-command-line-tools_10.21.0_amd64.deb;                                                                                               \
    else                                                                                                                                                                                                          \
        wget -q -P /tmp https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/desktop-software/nrf-command-line-tools/sw/versions-10-x-x/10-21-0/nrf-command-line-tools_10.21.0_arm64.deb && \
        apt-get update && apt-get install --no-install-recommends -y /tmp/nrf-command-line-tools_10.21.0_arm64.deb;                                                                                               \
    fi &&                                                                                                                                                                                                         \
# Cleanup
    apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Install Segger tools: this is a bit of a nightmare...
RUN apt-get update && apt-get install -y wget &&                                    \
# apt-get needs this to allow it to create temporary files
    chmod 1777 /tmp &&                                                              \
    if [ "$(arch)" != "aarch64" ]; then                                             \
        wget -q --post-data="accept_license_agreement=accepted" -P /tmp https://www.segger.com/downloads/jlink/JLink_Linux_V794m_x86_64.deb && \
# use dpkg to unpack the .deb file, but don't configure quite yet
        dpkg --unpack /tmp/JLink_Linux_V794m_x86_64.deb;                            \
    else                                                                            \
        wget -q --post-data="accept_license_agreement=accepted" -P /tmp https://www.segger.com/downloads/jlink/JLink_Linux_V794m_arm64.deb && \
# use dpkg to unpack the .deb file, but don't configure quite yet.
        dpkg --unpack /tmp/JLink_Linux_V794m_arm64.deb;                             \
    fi &&                                                                           \
# the JLink installer includes a post-install step that will fail 'cos this is Docker
# and udevadm stuff doesn't really work; we fix that by deleting the post install step
# before calling dpkg to configure JLink
    rm /var/lib/dpkg/info/jlink.postinst -f &&                                      \
# the JLink tools end up being dependent on a shed-load of GUI stuff that is utterly
# unnecessary and not available, hence we call dpkg with --force-all to make it ignore them
    dpkg --configure --force-all jlink &&                                           \
# Future installations are now stuck, since apt cannot be made to ignore these
# dependencies: the only way to fix this is to remove the dependencies from
# /var/lib/dpkg/status: the very long SED command below creates the edited file,
# looking for each offending lib dependency by exact name (e.g. "libxcb-render0") in
# the "Depends" line inside the "jlink" section of the file
    sed -r '/^Package:.*jlink/,/^Depends:/{/^Depends:/{s/libxcb-render0[^,\n]*[,\n]//};{s/libxcb-render-util0[^,\n]*[,\n]//};{s/libxcb-shape0[^,\n]*[,\n]//};{s/libxcb-randr0[^,\n]*[,\n]//};{s/libxcb-xfixes0[^,\n]*[,\n]//};{s/libxcb-sync1[^,\n]*[,\n]//};{s/libxcb-shm0[^,\n]*[,\n]//};{s/libxcb-icccm4[^,\n]*[,\n]//};{s/libxcb-keysyms1[^,\n]*[,\n]//};{s/libxcb-image0[^,\n]*[,\n]//};{s/libxkbcommon0[^,\n]*[,\n]//};{s/libxkbcommon-x11-0[^,\n]*[,\n]//};{s/libx11-xcb1 [^,\n]*[,\n]//};{s/libsm6 [^,\n]*[,\n]//};{s/libice6 [^,\n]*[,\n]//}}' /var/lib/dpkg/status > dpkg_status_mod && \
    cp dpkg_status_mod /var/lib/dpkg/status &&                                      \
# Cleanup
    apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# To gain access to the GPIOs we need the user we create at the end to be part of
# the gpio group in the underlying system, so we need to create exactly that
# group, with the same group ID as Raspbian uses, in a udev rule
RUN groupadd -g 997 gpio &&                                                                   \
    echo SUBSYSTEM=="gpio", GROUP="gpio", MODE="0660" >> /etc/udev/rules.d/99-com.rules

# Similar for i2c
RUN groupadd -g 998 i2c &&                                                                   \
    echo SUBSYSTEM=="i2c", GROUP="i2c", MODE="0660" >> /etc/udev/rules.d/99-com.rules

# Similar for spi
RUN groupadd -g 999 spi &&                                                                   \
    echo SUBSYSTEM=="spi", GROUP="spi", MODE="0660" >> /etc/udev/rules.d/99-com.rules

# Note: the dialout group is already present (group ID 20) in the container

# When on a Pi, add Valgrind so that we can apply it when we run on Linux natively
RUN if [ "$(arch)" == "aarch64" ]; then                                         \
        apt-get update && apt-get install --no-install-recommends -y valgrind;  \
    fi

WORKDIR /workdir

#***************************************************
# Add missing Python modules and apk packages here
#***************************************************

# Note: there is a bug in version 2.2.1 of invoke which stops it loading module directories
# correctly, hence the fixing to version 2.0.0 below, which is known to work
RUN chmod 1777 /tmp && \
    pip3 install pyserial pylint psutil pylink-square requests_toolbelt rpyc debugpy invoke==2.0.0 coloredlogs verboselogs && \
    apt-get update && apt-get install -y --no-install-recommends  \
        usbutils gawk iputils-ping openssh-client socat ppp picocom \
# Needed for OpenOCD
        libhidapi-hidraw0 && \
# Cleanup
    apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Set permission on pppd so that we can run it without having to be root
# and it doesn't require a serial modem to be a server capable of
# authenticating itself
RUN chown root /usr/sbin/pppd && chmod 4755 /usr/sbin/pppd && \
    sed 's/^auth/noauth/' /etc/ppp/options > options && \
    cp options /etc/ppp/options && \
    rm options

#***************************************************
# Setup environmental variables
#***************************************************

#***************************************************
# Some last steps
#***************************************************

# Add and switch to "ubxlib" user, making it a member of the right groups
ARG USER="ubxlib"
RUN groupadd -f -g 1000 -o $USER && \
    useradd -ms /bin/bash -u 1000 -g 1000 -G gpio,i2c,spi,dialout $USER && \
    chown ubxlib:ubxlib /workdir
USER ubxlib
