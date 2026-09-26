#!/bin/sh
# Build the private runtime for PORTS.pak (PortMaster support) without relying
# on the OS: GNU bash and command-line tools, Python 3.10, and current
# SDL2_image, SDL2_mixer and SDL2_ttf. SDL2 itself is NextUI's.
#
# The tools and Python are Ubuntu 22.04 arm64 packages (debs.txt, pinned by
# SHA-256). Stock Anbernic OS, StockMod and BaseOS all use Ubuntu 22.04's glibc,
# so they run unchanged. To update a package, replace its line in debs.txt.
#
# The SD card is FAT, so the output holds no symlinks: libraries are copied
# under their soname and command aliases become copies.
set -eu

: "${CROSS_COMPILE:?CROSS_COMPILE is required}"
: "${PREFIX_LOCAL:?PREFIX_LOCAL is required}"

UBUNTU_MIRROR=http://ports.ubuntu.com/ubuntu-ports
SDL2_MIXER_VERSION=2.8.0
SDL2_MIXER_SHA256=1cfb34c87b26dbdbc7afd68c4f545c0116ab5f90bbfecc5aebe2a9cb4bb31549
SDL2_IMAGE_VERSION=2.8.2
SDL2_IMAGE_SHA256=8f486bbfbcf8464dd58c9e5d93394ab0255ce68b51c5a966a918244820a76ddc
SDL2_TTF_VERSION=2.22.0
SDL2_TTF_SHA256=d48cbd1ce475b9e178206bf3b72d56b66d84d44f64ac05803328396234d67723
THEORA_VERSION=1.1.1 # 1.2 renamed the libraries that games link against
THEORA_SHA256=40952956c47811928d1e7922cda3bc1f427eb75680c3c37249c91e949054916b
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
DEB_DIR="$SCRIPT_DIR/debs"
ROOT="$SCRIPT_DIR/root"
OUTPUT_DIR="$SCRIPT_DIR/output"
STAMP="$OUTPUT_DIR/.stamp-$(cat "$SCRIPT_DIR/build.sh" "$SCRIPT_DIR/debs.txt" | sha256sum | cut -c1-16)"

if [ -f "$STAMP" ]; then
	exit 0
fi

download() { # url sha256 file
	if [ ! -f "$3" ] || ! echo "$2  $3" | sha256sum -c - >/dev/null 2>&1; then
		wget -q -O "$3.tmp" "$1"
		echo "$2  $3.tmp" | sha256sum -c - >/dev/null
		mv "$3.tmp" "$3"
	fi
}

## 1. Ubuntu packages
mkdir -p "$DEB_DIR"
rm -rf "$ROOT" "$OUTPUT_DIR"
mkdir -p "$ROOT" "$OUTPUT_DIR/bin" "$OUTPUT_DIR/lib"
while read -r path sha; do
	[ -n "$path" ] || continue
	deb="$DEB_DIR/${path##*/}"
	download "$UBUNTU_MIRROR/$path" "$sha" "$deb"
	dpkg-deb -x "$deb" "$ROOT"
done < "$SCRIPT_DIR/debs.txt"

# Commands: every executable in the packages' bin directories, as real files.
for dir in bin sbin usr/bin usr/sbin; do
	[ -d "$ROOT/$dir" ] || continue
	for f in "$ROOT/$dir"/*; do
		name=${f##*/}
		case "$name" in
			bashbug|rbash|python3.10-config) continue ;;
		esac
		[ -e "$f" ] && cp -L "$f" "$OUTPUT_DIR/bin/$name"
	done
done
cp -L "$OUTPUT_DIR/bin/gawk" "$OUTPUT_DIR/bin/awk"
# python3 finds its standard library in ../lib/python3.10 next to the binary.
printf '#!/bin/sh\nexec "${0%%/*}/python3.10" "$@"\n' > "$OUTPUT_DIR/bin/python3"
# PortMaster runs as root, so ports that call sudo just run the command.
cat > "$OUTPUT_DIR/bin/sudo" <<'EOF'
#!/bin/sh
while [ $# -gt 0 ]; do
	case "$1" in
		--) shift; break ;;
		-*) shift ;;
		*) break ;;
	esac
done
exec "$@"
EOF
chmod 755 "$OUTPUT_DIR/bin"/*

# Shared libraries, copied under the name the loader asks for.
for dir in "$ROOT/lib/aarch64-linux-gnu" "$ROOT/usr/lib/aarch64-linux-gnu"; do
	[ -d "$dir" ] || continue
	for f in "$dir"/*.so.*; do
		soname=$("${CROSS_COMPILE}readelf" -d "$f" 2>/dev/null | sed -n 's/.*(SONAME).*\[\(.*\)\]/\1/p')
		[ -n "$soname" ] && cp -L "$f" "$OUTPUT_DIR/lib/$soname"
	done
done

# Python standard library, without tests, GUI and packaging extras.
cp -r "$ROOT/usr/lib/python3.10" "$OUTPUT_DIR/lib/python3.10"
( cd "$OUTPUT_DIR/lib/python3.10" && rm -rf test idlelib tkinter turtledemo lib2to3 \
	ensurepip pydoc_data config-3.10-* && find . -name __pycache__ -prune -exec rm -rf {} + )

## 2. SDL2_image, SDL2_mixer and SDL2_ttf, linked against NextUI's SDL2. The toolchain SDK
## only has 2016 releases, too old for PortMaster and many ports. Both use
## built-in decoders (PNG, JPEG, OGG, FLAC, MP3) and FreeType, so they need
## nothing from the OS.
build_sdl_lib() { # project name version sha256 configure-args...
	project=$1 name=$2 version=$3 sha=$4
	shift 4
	archive="$DEB_DIR/$name-$version.tar.gz"
	download "https://github.com/libsdl-org/$project/releases/download/release-$version/$name-$version.tar.gz" "$sha" "$archive"
	rm -rf "$SCRIPT_DIR/$name-$version"
	tar -xzf "$archive" -C "$SCRIPT_DIR"
	(
		cd "$SCRIPT_DIR/$name-$version"
		./configure -q --host="${CROSS_COMPILE%-}" --prefix="$SCRIPT_DIR/sdl-prefix" \
			--with-sdl-prefix="$PREFIX_LOCAL" --disable-static --disable-sdltest "$@"
		make -s -j"$(nproc)"
		make -s install
	)
	cp -L "$SCRIPT_DIR/sdl-prefix/lib/lib$name-2.0.so.0" "$OUTPUT_DIR/lib/"
	"${CROSS_COMPILE}strip" --strip-unneeded "$OUTPUT_DIR/lib/lib$name-2.0.so.0"
	# PortMaster's Python SDL bindings prefer an exact lib<name>.so anywhere on
	# their search path, which would pick NextUI's older copy over this one.
	cp "$OUTPUT_DIR/lib/lib$name-2.0.so.0" "$OUTPUT_DIR/lib/lib$name.so"
}
build_sdl_lib SDL_image SDL2_image "$SDL2_IMAGE_VERSION" "$SDL2_IMAGE_SHA256" \
	--enable-stb-image --disable-jpg-shared --disable-png-shared \
	--disable-webp --disable-avif --disable-jxl --disable-tif
build_sdl_lib SDL_mixer SDL2_mixer "$SDL2_MIXER_VERSION" "$SDL2_MIXER_SHA256" \
	--enable-music-ogg-stb --enable-music-flac-drflac --enable-music-mp3-minimp3 \
	--disable-music-mod --disable-music-opus --disable-music-midi-fluidsynth \
	--disable-music-gme --disable-music-wavpack
build_sdl_lib SDL_ttf SDL2_ttf "$SDL2_TTF_VERSION" "$SDL2_TTF_SHA256" \
	--enable-freetype-builtin --disable-harfbuzz

## 3. Theora video (Love2D games need it). Ubuntu's build pulls in cairo and X11.
THEORA_ARCHIVE="$DEB_DIR/libtheora-$THEORA_VERSION.tar.gz"
download "https://downloads.xiph.org/releases/theora/libtheora-$THEORA_VERSION.tar.gz" "$THEORA_SHA256" "$THEORA_ARCHIVE"
rm -rf "$SCRIPT_DIR/libtheora-$THEORA_VERSION"
tar -xzf "$THEORA_ARCHIVE" -C "$SCRIPT_DIR"
(
	cd "$SCRIPT_DIR/libtheora-$THEORA_VERSION"
	# Its 2009 config.sub doesn't know aarch64.
	cp /usr/share/misc/config.sub /usr/share/misc/config.guess .
	./configure -q --host="${CROSS_COMPILE%-}" --prefix="$SCRIPT_DIR/sdl-prefix" \
		--disable-static --disable-examples --disable-encode --disable-oggtest \
		--disable-vorbistest --disable-sdltest --disable-spec --disable-telemetry
	make -s -j"$(nproc)"
	make -s install
)
for lib in libtheoradec.so.1 libtheora.so.0; do
	cp -L "$SCRIPT_DIR/sdl-prefix/lib/$lib" "$OUTPUT_DIR/lib/"
	"${CROSS_COMPILE}strip" --strip-unneeded "$OUTPUT_DIR/lib/$lib"
done

# FAT has no symlinks: replace any left with the file itself, or drop dangling
# ones (Debian's sitecustomize.py points into /etc).
find "$OUTPUT_DIR" -type l | while read -r link; do
	if [ -e "$link" ]; then
		cp -L "$link" "$link.tmp" && mv -f "$link.tmp" "$link"
	else
		rm -f "$link"
	fi
done

rm -rf "$ROOT"
touch "$STAMP"
du -sh "$OUTPUT_DIR"
