#!/bin/sh

SDCARD_PATH="${SDCARD_PATH:-/mnt/SDCARD}"
PLATFORM="${PLATFORM:-tg5040}"
USERDATA_PATH="${USERDATA_PATH:-$SDCARD_PATH/.userdata/$PLATFORM}"
LOGS_PATH="${LOGS_PATH:-$USERDATA_PATH/logs}"
PAK_PATH="$(dirname "$0")"
CONFIG_PATH="$USERDATA_PATH/displaycal.cfg"
HOOK_DIR="$USERDATA_PATH/.hooks/boot.d"
HOOK_PATH="$HOOK_DIR/50-color-correction.sh"

if [ -z "$DEVICE" ]; then
	TRIMUI_MODEL=$(strings /usr/trimui/bin/MainUI 2>/dev/null | grep '^Trimui')
	if [ "$TRIMUI_MODEL" = "Trimui Brick" ]; then
		DEVICE="brick"
	fi
fi

mkdir -p "$USERDATA_PATH" "$LOGS_PATH"

if [ "$DEVICE" != "brick" ]; then
	echo "Color Correction is only supported on Trimui Brick." > "$LOGS_PATH/displaycal.txt"
	exit 0
fi

mkdir -p "$HOOK_DIR"
cat > "$HOOK_PATH" <<'EOF'
#!/bin/sh

[ "$DEVICE" = "brick" ] || exit 0

SDCARD_PATH="${SDCARD_PATH:-/mnt/SDCARD}"
PLATFORM="${PLATFORM:-tg5040}"
USERDATA_PATH="${USERDATA_PATH:-$SDCARD_PATH/.userdata/$PLATFORM}"
DISPLAYCAL_PAK="$SDCARD_PATH/Tools/$PLATFORM/Color Correction.pak"
DISPLAYCAL_BIN="$DISPLAYCAL_PAK/displaycal.elf"
DISPLAYCAL_CONFIG="$USERDATA_PATH/displaycal.cfg"

[ -x "$DISPLAYCAL_BIN" ] || exit 0
"$DISPLAYCAL_BIN" apply "$DISPLAYCAL_CONFIG"
EOF
chmod +x "$HOOK_PATH"

if [ ! -f "$CONFIG_PATH" ]; then
	cat > "$CONFIG_PATH" <<EOF
enabled=1
red=1.0000000000000000
green=0.9233642796405507
blue=0.5833412353395729
EOF
fi

cd "$PAK_PATH"
./displaycal.elf ui "$CONFIG_PATH" > "$LOGS_PATH/displaycal.txt" 2>&1
