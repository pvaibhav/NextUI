#!/usr/bin/env python3
"""Show each installed port's cover as its box art in NextUI's Ports list.

PortMaster keeps port.json and gameinfo.xml in each game's folder under
Ports/. NextUI looks for "<name>.png" in the list folder's .media directory.
SDL_image recognises JPEG data whatever the file is called, so JPEG covers are
copied as they are.
"""
import json
import os
import shutil
import xml.etree.ElementTree as ET
from pathlib import Path

ports = Path(os.environ["HM_PORTS_DIR"])
scripts = Path(os.environ["HM_SCRIPTS_DIR"])
media = scripts / ".media"


def cover(game_dir):
    info = game_dir / "gameinfo.xml"
    if info.is_file():
        try:
            for image in ET.parse(info).iter("image"):
                path = ports / (image.text or "").strip()
                if path.is_file():
                    return path
        except ET.ParseError:
            pass
    for name in ("cover.png", "screenshot.png", "cover.jpg", "screenshot.jpg"):
        if (game_dir / name).is_file():
            return game_dir / name
    return None


wanted = {}
for port_json in ports.glob("*/port.json"):
    try:
        items = json.loads(port_json.read_text()).get("items") or []
    except (OSError, ValueError):
        continue
    image = cover(port_json.parent)
    for item in items:
        if item.endswith(".sh") and image and (scripts / item).is_file():
            wanted[item[:-3] + ".png"] = image

# Only touch box art this script made (listed in the manifest): never the
# folder's bg.png or bglist.png, subfolder art, or covers the user replaced.
manifest = media / ".portmaster-art"
try:
    owned = set(manifest.read_text().split("\n")) - {""}
except OSError:
    owned = set()

media.mkdir(exist_ok=True)
for name, image in wanted.items():
    target = media / name
    if not target.exists() or (name in owned and target.stat().st_size != image.stat().st_size):
        shutil.copyfile(image, target)
        owned.add(name)

# Remove box art we made for ports that have since been uninstalled.
for name in sorted(owned):
    if name not in wanted and not (scripts / (name[:-4] + ".sh")).exists():
        try:
            (media / name).unlink()
        except FileNotFoundError:
            pass
        owned.discard(name)

manifest.write_text("".join(n + "\n" for n in sorted(owned)))
