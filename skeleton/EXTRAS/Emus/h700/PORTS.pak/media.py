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

media.mkdir(exist_ok=True)
for name, image in wanted.items():
    target = media / name
    if not target.exists() or target.stat().st_size != image.stat().st_size:
        shutil.copyfile(image, target)

# Remove box art left behind by uninstalled ports (ours only: they came from Ports/).
for target in media.glob("*.png"):
    if target.name not in wanted and not (scripts / (target.stem + ".sh")).exists():
        target.unlink()
