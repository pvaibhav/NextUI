#!/bin/sh
# Runs a port, or PortMaster, inside the private mount namespace launch.sh
# starts with "unshare -m". Mounts that ports make, such as game runtimes,
# disappear when they exit, and the OS itself is never changed.

if [ ! -e /bin/bash ]; then
	# BaseOS has no /bin/bash, which every port script starts with.
	mkdir -p /tmp/ports-bin
	ln -sf "$PAK_DIR/bin/bash" /tmp/ports-bin/bash
	mount -t overlay overlay -o lowerdir=/tmp/ports-bin:/usr/bin /usr/bin || exit 1
fi

# PortMaster runs libc to learn the glibc version, which decides the games it
# offers. BaseOS ships libc without the executable bit; show a copy that has it.
libc=/lib/aarch64-linux-gnu/libc.so.6
if [ -f "$libc" ] && [ ! -x "$libc" ]; then
	cp "$libc" /tmp/ports-libc.so.6 && chmod 755 /tmp/ports-libc.so.6 &&
		mount --bind /tmp/ports-libc.so.6 "$libc"
fi

case "${1##*/}" in
	*PortMaster.sh) exec bash "$PAK_DIR/portmaster.sh" ;;
esac

# Ports that run under PortMaster's Weston need udev's device descriptions,
# and it creates a stand-in DRM device node, kept private here.
if grep -q westonwrap "$1" 2>/dev/null; then
	mkdir -p /dev/dri && mount -t tmpfs weston-dri /dev/dri
	sh "$PAK_DIR/udev.sh" &
	udev=$!
	bash "$1"
	kill "$udev"
	exit
fi
exec bash "$1"
