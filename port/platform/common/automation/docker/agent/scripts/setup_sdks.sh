#!/bin/bash
set -e

# Uncomment to enable debug
#set -x

SDK_DIR="$HOME/sdks"

SDK_NRF_TAG="v1.4.2"
NRF5_SDK_VERSION="15.3.0_59ac345"
STM32CUBE_F4_TAG="v1.26.1"

install_ncs()
{
    local TARGET_DIR=$1
    local TAG=$2

    local NCS_DIR="${SDK_DIR}/ncs"
    local ZEPHYR_URL="https://github.com/zephyrproject-rtos/zephyr.git"
    local SDK_NRF_URL="https://github.com/nrfconnect/sdk-nrf"
    local NCS_INSTALL_DIR="${TARGET_DIR}/${TAG}"
    local VERSION_FILE="${TARGET_DIR}/version"

    export NCS_INSTALL_DIR="${TARGET_DIR}/${TAG}"
    if [ -f "$VERSION_FILE" ] && grep -q "$TAG" "$VERSION_FILE"; then
        echo "NCS found: $TAG"
        return
    fi
    if [ -d "$NCS_INSTALL_DIR" ]; then
        echo "Detected corrupt NCS installation - removing $NCS_INSTALL_DIR..."
        rm -rf $NCS_INSTALL_DIR
    fi

    mkdir -p $NCS_INSTALL_DIR
    cd $NCS_INSTALL_DIR
    west init -m $SDK_NRF_URL --mr $TAG
    west update
    # Patch zephyr so it will compile
    cd $NCS_INSTALL_DIR/zephyr
    git config user.email "dummy"
    git config user.name "dummy"
    git fetch $ZEPHYR_URL 5db486e48c906e220ab225146ccfa57ce5f2c32a
    git cherry-pick 5db486e48c906e220ab225146ccfa57ce5f2c32a

    ln -s $NCS_INSTALL_DIR $TARGET_DIR/current
    echo "$TAG" > $VERSION_FILE
}

install_nrf5_sdk()
{
    local TARGET_DIR=$1
    local VERSION=$2

    local NRF5_SDK_DIR="${SDK_DIR}/nrf5_sdk"
    local NRF5_SDK_URL="https://developer.nordicsemi.com/nRF5_SDK/nRF5_SDK_v15.x.x/nRF5_SDK_${VERSION}.zip"
    local NRF5_SDK_INSTALL_DIR="${TARGET_DIR}/${VERSION}"
    local VERSION_FILE="${TARGET_DIR}/version"

    if [ -f "$VERSION_FILE" ] && grep -q "$VERSION" "$VERSION_FILE"; then
        echo "nRF5 SDK found: $VERSION"
        return
    fi
    if [ -d "$NRF5_SDK_INSTALL_DIR" ]; then
        echo "Detected corrupt nRF5 SDK installation - removing $NRF5_SDK_INSTALL_DIR..."
        rm -rf $NRF5_SDK_INSTALL_DIR
    fi

    mkdir -p $NRF5_SDK_INSTALL_DIR
    cd $NRF5_SDK_INSTALL_DIR
    wget $NRF5_SDK_URL -O nordic_sdk.zip
    unzip -q nordic_sdk.zip
    mv nRF5_SDK_*/* .
    rm -rf nRF5_SDK_*
    rm nordic_sdk.zip
    ln -s $NRF5_SDK_INSTALL_DIR $TARGET_DIR/current
    echo "$VERSION" > $VERSION_FILE
}

install_stm32_cube_f4()
{
    local TARGET_DIR=$1
    local TAG=$2

    local STM32CUBE_F4_URL="https://github.com/STMicroelectronics/STM32CubeF4.git"
    local STM32CUBE_INSTALL_DIR="${TARGET_DIR}/${TAG}"
    local VERSION_FILE="${TARGET_DIR}/version"

    if [ -f "$VERSION_FILE" ] && grep -q "$TAG" "$VERSION_FILE"; then
        echo "STM32CubeF4 found: $TAG"
        return
    fi
    if [ -d "$STM32CUBE_INSTALL_DIR" ]; then
        echo "Detected corrupt STM32CubeF4 installation - removing $STM32CUBE_INSTALL_DIR..."
        rm -rf $STM32CUBE_INSTALL_DIR
    fi

    git clone --depth 1 --branch $TAG $STM32CUBE_F4_URL $STM32CUBE_INSTALL_DIR

    ln -s $STM32CUBE_INSTALL_DIR $TARGET_DIR/current
    echo "$TAG" > $VERSION_FILE
}

install_unity()
{
    local UNITY_DIR=$1
    local BRANCH=$2

    local UNITY_URL="https://github.com/ThrowTheSwitch/Unity"

    if [ ! -d "$UNITY_DIR" ]; then
        git clone $UNITY_URL $UNITY_DIR
    fi

    git -C $UNITY_DIR fetch
    git -C $UNITY_DIR checkout $BRANCH
    git -C $UNITY_DIR pull
}

if [ "$#" -eq 1 ]; then
  SDK_DIR=$1
fi

echo "--- Checking SDK installations (in $SDK_DIR) ---"
install_ncs "$SDK_DIR/ncs" "$SDK_NRF_TAG"
install_nrf5_sdk "$SDK_DIR/nrf5_sdk" "$NRF5_SDK_VERSION"
install_stm32_cube_f4 "$SDK_DIR/stm32cubef4" "$STM32CUBE_F4_TAG"
install_unity "$SDK_DIR/unity" "master"
