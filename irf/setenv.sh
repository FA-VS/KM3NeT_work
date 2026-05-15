#! /bin/bash

if [ -z $ROOTSYS ]; then
    echo "ERROR! IRF/setenv.sh environment variable ROOTSYS is not set"
    return 1
fi

# Determine the script directory
if [[ -n $BASH_SOURCE ]]; then
    # Bash: Use BASH_SOURCE[0]
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
else
    # Zsh: Use $0
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
fi

# Set IRF_DIR
export IRF_DIR="$SCRIPT_DIR"
export LD_LIBRARY_PATH=$IRF_DIR/lib:${LD_LIBRARY_PATH}
export LD_LIBRARY_PATH=$IRF_DIR/km3net-dataformat/lib:${LD_LIBRARY_PATH}

echo "environment variable IRF_DIR was set to $IRF_DIR"
echo "${IRF_DIR}/lib and ${IRF_DIR}/km3net-dataformat/lib were added to LD_LIBRARY_PATH"

