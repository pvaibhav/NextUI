#!/bin/bash
# Opens PortMaster, then updates the Ports list's box art. Run by launch.sh.

source "$XDG_DATA_HOME/PortMaster/control.txt"
get_controls
cd "$controlfolder" || exit 1

# Offline installs: port zips and runtimes copied to Ports/autoinstall.
for file in "$PORTS_DIR"/autoinstall/*.squashfs; do
  [ -f "$file" ] && mv -f "$file" "$controlfolder/libs/"
done
for file in "$PORTS_DIR"/autoinstall/runtimes*.zip; do
  [ -f "$file" ] || continue
  # Runtime bundles name their architecture in the zip comment (old ones don't).
  arch="$(unzip -z "$file" | tail -n1 | cut -d' ' -f1)"
  if [ "$arch" = "$DEVICE_ARCH" ] || [ "$arch" = "Archive:" ]; then
    unzip -q -o "$file" -d "$controlfolder/libs" && rm -f "$file"
  else
    echo "Skipping $file: it is for $arch"
  fi
done
for file in "$PORTS_DIR"/autoinstall/*.zip; do
  case "${file##*/}" in runtimes*.zip) continue ;; esac
  [ -f "$file" ] && python3 ./harbourmaster --quiet --no-check install "$file" && rm -f "$file"
done

# In PortMaster itself, A and B follow the button labels, as PortMaster does on
# TrimUI and other handhelds with A on the right. Games keep SDL's usual layout.
export SDL_GAMECONTROLLERCONFIG="19000000010000000100000000016e01,ANBERNIC-keys,a:b1,b:b0,x:b3,y:b2,leftshoulder:b4,rightshoulder:b5,back:b6,start:b7,guide:b8,leftstick:b9,rightstick:b10,lefttrigger:a2,righttrigger:a5,leftx:a0,lefty:a1,rightx:a3,righty:a4,dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,platform:Linux,"

rm -f .pugwash-reboot
while true; do
  python3 ./pugwash
  # PortMaster asks to be restarted after it updates itself.
  [ -f .pugwash-reboot ] || break
  rm -f .pugwash-reboot
  cp "$PAK_DIR/control.txt" control.txt
done
cp "$PAK_DIR/control.txt" control.txt

python3 "$PAK_DIR/media.py"
[ -f "$HM_SCRIPTS_DIR/.media/0) PortMaster.png" ] || cp trimui/icon.png "$HM_SCRIPTS_DIR/.media/0) PortMaster.png"
