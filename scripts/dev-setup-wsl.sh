#!/usr/bin/env bash
set -euo pipefail

sudo apt update
sudo apt install -y build-essential gcc-multilib binutils binutils-multiarch \
  autoconf automake libtool pkg-config gawk bison flex nasm \
  xorriso grub-pc-bin mtools qemu-system-x86 \
  git python3 cppcheck clang-tools || true

if ! command -v mig >/dev/null 2>&1; then
  sudo apt install -y mig || true
fi

if ! command -v mig >/dev/null 2>&1; then
  sudo apt install -y texinfo
  rm -rf /tmp/mig-src
  git clone https://git.savannah.gnu.org/git/hurd/mig.git /tmp/mig-src
  pushd /tmp/mig-src
  ./bootstrap
  ./configure
  make -j"$(nproc)"
  sudo make install
  popd
fi

echo "WSL dev dependencies installed."

