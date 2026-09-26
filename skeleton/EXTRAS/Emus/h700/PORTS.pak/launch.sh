#!/bin/sh
# Ports: games installed with PortMaster (https://portmaster.games).
#
# "PortMaster" at the top of the Ports list opens PortMaster to download
# games; every other entry is a game it installed. Game files that you supply
# yourself go in that game's folder inside Ports/ at the root of the SD card.
#
# Everything PortMaster needs from the OS (bash, GNU tools, Python, newer SDL
# libraries) comes from this pak, so it works the same on stock OS, StockMod
# and BaseOS.

PAK_DIR="$(cd "$(dirname "$0")" && pwd)"
ROM="$1"

export PAK_DIR
export PORTS_DIR="$SDCARD_PATH/Ports"
export XDG_DATA_HOME="$HOME/.local/share"
export XDG_CONFIG_HOME="$HOME/.config"
export HM_TOOLS_DIR="$XDG_DATA_HOME"
export HM_PORTS_DIR="$PORTS_DIR"
export HM_SCRIPTS_DIR="$(dirname "$ROM")"
export PATH="$PAK_DIR/bin:$PATH"
export LD_LIBRARY_PATH="$PAK_DIR/lib:$LD_LIBRARY_PATH"
PM_DIR="$XDG_DATA_HOME/PortMaster"
PM_URL="https://github.com/PortsMaster/PortMaster-GUI/releases/latest/download/PortMaster.zip"

mkdir -p "$PORTS_DIR" "$XDG_CONFIG_HOME" "$LOGS_PATH"
exec >"$LOGS_PATH/PORTS.txt" 2>&1

# Status screens while PortMaster installs, with an optional pause.
show() {
	killall show2.elf 2>/dev/null
	show2.elf --mode=simple --image="$SDCARD_PATH/.system/res/logo.png" --logoheight=128 --text="$1" &
	[ -z "$2" ] || { sleep "$2"; killall show2.elf 2>/dev/null; }
}

# First launch: download PortMaster, or use Ports/PortMaster.zip if present.
if [ ! -f "$PM_DIR/pugwash" ]; then
	ZIP="$PORTS_DIR/PortMaster.zip"
	if [ ! -f "$ZIP" ]; then
		ZIP=/tmp/PortMaster.zip
		show "Downloading PortMaster..."
		# Give up on a stalled download instead of waiting minutes.
		if ! curl -fsSL --connect-timeout 10 --speed-limit 1024 --speed-time 30 -o "$ZIP" "$PM_URL"; then
			rm -f "$ZIP"
			show "Couldn't download PortMaster. Connect to Wi-Fi in Settings and try again." 5
			exit 1
		fi
	fi
	show "Installing PortMaster..."
	mkdir -p "$XDG_DATA_HOME"
	if ! unzip -q -o "$ZIP" -d "$XDG_DATA_HOME" || [ ! -f "$PM_DIR/pugwash" ]; then
		rm -f /tmp/PortMaster.zip
		show "PortMaster didn't install correctly. Please try again." 5
		exit 1
	fi
	rm -f /tmp/PortMaster.zip "$PORTS_DIR/PortMaster.zip"
	killall show2.elf 2>/dev/null
fi

# PortMaster updates replace control.txt, so put NextUI's back every time.
cp "$PAK_DIR/control.txt" "$PM_DIR/control.txt"

case "$DEVICE" in # PortMaster's name for this model
	rg34xx|rgsp) echo rg34xx-h ;; # 720x480, no sticks
	rg34xxsp) echo rg34xx-sp ;;
	rg35xxplus) echo rg35xx-plus ;;
	rg35xxpro) echo rg35xx-h ;;
	rg35xx*|rg40xx*) echo "$DEVICE" | sed 's/xx/xx-/' ;;
	rgcubexx) echo rgcubexx-h ;;
	*) echo "$DEVICE" ;;
esac > "$XDG_CONFIG_HOME/.DEVICE"

# Port scripts log through bash's >(...), which needs /dev/fd. BaseOS has no
# such link; it is the standard one and only lasts until the next restart.
[ -e /dev/fd ] || ln -s /proc/self/fd /dev/fd

unshare -m sh "$PAK_DIR/run.sh" "$ROM"
