#!/bin/bash

set -euo pipefail


# Execute slightly different scripts based on OS
# due to package managers differing
if [[ "$OSTYPE" == *"darwin"* ]]; then
    stty sane # Fix formatting issues

    # Using macOS, install and use brew
    if command -v brew >/dev/null 2>&1; then
        brewVersion="$(brew --version 2>/dev/null | tr -d '\r' | head -n 1)"
        printf '%s\n' "Homebrew is installed. Version: $brewVersion"
    else
        printf '%s\n' "Homebrew is not installed." "Installing Homebrew"
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi

    # Verify tio installation
    if command -v tio >/dev/null 2>&1; then
        tioVersion="$(tio --version | tr -d '\r' | head -n 1)"
        printf '%s\n' "tio is installed. Version: $tioVersion"
    else
        # If tio isn't installed, install it
        printf '%s\n' "tio is not installed." "Installing tio."
        brew install tio
    fi

    serialPort="${1:-}"
    if [[ -z "$serialPort" ]]; then
        shopt -s nullglob
        serialPorts=(/dev/cu.usbserial-*)
        shopt -u nullglob

        if [[ ${#serialPorts[@]} -eq 0 ]]; then
            echo "No USB-SDI12 serial device found. Pass its port explicitly." >&2
            exit 1
        fi
        serialPort="${serialPorts[0]}"
    fi

    # If nothing is connected to the serial port, close it
    if [[ ! -c "$serialPort" ]]; then
        printf -u2 "Serial device not found: $serialPort"
        exit 1
    fi

    # Open the serial port using tio
    tio -f none --input-mode line --local-echo --baudrate 9600 --databits 7 --parity even --stopbits 1 "$serialPort"

else
    clear # Fix formatting issues
    # Using Windows, install tio using msys2

    # Verify pacman is installed
    if command -v pacman >/dev/null 2>&1; then
        pacmanVersion="$(pacman --version 2>/dev/null | tr -d '\r' | head -n 1)"
        printf '%s\n' "Pacman is installed. Version: $pacmanVersion"
    fi

    # Verify installtion of tio
    if command -v tio >/dev/null 2>&1; then
        tioVersion="$(tio --version | tr -d '\r' | head -n 1)"
        printf '%s\n' "tio is installed. Version: $tioVersion"
    else
        # Install tio if it isn't present already
        printf '%s\n' "tio is not installed." "Installing tio."
        packman -S tio
    fi

    serialPort="${1:-}"
    if [[ -z "$serialPort" ]]; then
        # Find the active serial port connected to the SDI-12, if it isn't passed in
        serialPort=$(mode.com | grep Status.*COM | awk 'NR==1 { print $4 }' | sed 's/://')
        if [[ -z "$serialPort" ]]; then
            echo "No Windows COM ports found." >&2
            exit 1
        fi
        echo "Using active port: $serialPort"
    fi


    # Open the serial port using tio
    tio -f none --input-mode line --local-echo --baudrate 9600 --databits 7 --parity even --stopbits 1 "$serialPort"

fi