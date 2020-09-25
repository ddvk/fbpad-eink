git submodule update --init --recursive
patch -N -d FBInk < patch_fbink
. /usr/local/oecore-x86_64/environment-setup-cortexa9hf-neon-oe-linux-gnueabi
make remarkable -C FBInk
make
