#!/usr/bin/env fish
set -l root (dirname (status --current-filename))

sudo pacman -S --needed --noconfirm base-devel cmake ninja qt6-base qt6-webengine qt6-webchannel qtkeychain-qt6 hidapi polkit; or exit 1
rm -rf "$root/build"
cmake -S "$root" -B "$root/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr; or exit 1
cmake --build "$root/build" -j(nproc); or exit 1
sudo cmake --install "$root/build"; or exit 1

echo
printf 'Installed Cachy Surf. Open it from your app launcher or run: cachysurf\n'
