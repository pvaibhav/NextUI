#!/bin/sh
# Describes input devices the way udev would, for ports that run under
# PortMaster's Weston: its libinput only uses devices udev has labelled, and
# none of the OSes NextUI runs on has udev. run.sh starts this inside the port's
# private mount namespace, so the tmpfs at /run/udev is only seen by that port.
# Devices that appear later (gptokeyb's virtual keyboard and mouse) are picked
# up by checking again every half second.

mkdir -p /run/udev
mount -t tmpfs udev-lite /run/udev || exit 1
mkdir -p /run/udev/data

# Succeeds if bit $2 is set in sysfs capability bitmap $1 (hex words, most
# significant first). Done on the hex digits, as 64-bit words overflow sh.
has_bit() {
	awk -v b="$2" '{
		w = NF - int(b / 64); if (w < 1) exit 1
		h = tolower($w); n = int((b % 64) / 4); if (n >= length(h)) exit 1
		v = index("0123456789abcdef", substr(h, length(h) - n, 1)) - 1
		exit !(int(v / 2 ^ (b % 4)) % 2)
	}' "$1"
}

while :; do
	for dev in /sys/class/input/event*; do
		[ -r "$dev/dev" ] || continue
		db="/run/udev/data/c$(cat "$dev/dev")"
		[ -f "$db" ] && continue
		caps="$dev/device/capabilities"
		{
			echo "I:1"
			echo "E:ID_INPUT=1"
			# KEY_A: a keyboard. BTN_LEFT with relative X/Y: a mouse.
			# BTN_SOUTH: a gamepad, which libinput leaves alone.
			if has_bit "$caps/key" 30; then
				echo "E:ID_INPUT_KEY=1"
				echo "E:ID_INPUT_KEYBOARD=1"
			fi
			if has_bit "$caps/key" 272 && has_bit "$caps/rel" 0 && has_bit "$caps/rel" 1; then
				echo "E:ID_INPUT_MOUSE=1"
			fi
			if has_bit "$caps/key" 304; then
				echo "E:ID_INPUT_JOYSTICK=1"
			fi
		} > "$db"
	done
	sleep 0.5
done
