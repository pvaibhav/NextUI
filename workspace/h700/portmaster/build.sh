#!/bin/sh
# Build the private runtime for PORTS.pak (PortMaster support) without relying
# on the OS: GNU bash and command-line tools, Python 3.10, current SDL2 add-on
# libraries, FFmpeg 4, libcurl and other libraries ports expect. SDL2 itself is
# NextUI's.
#
# The tools, Python and common game libraries are Ubuntu 22.04 arm64 packages
# (debs.txt; debs-build.txt holds build-only ones), fetched from Launchpad's
# permanent archive and pinned by SHA-256. Stock Anbernic OS, StockMod and BaseOS
# all use Ubuntu 22.04's glibc, so they run unchanged. debs-compat.txt adds the
# older Debian 11 libraries many ports were built against (as dArkOS does).
# To update a package, replace its line in the list.
#
# The SD card is FAT, so the output holds no symlinks: libraries are copied
# under their soname and command aliases become copies.
set -eu

: "${CROSS_COMPILE:?CROSS_COMPILE is required}"
: "${PREFIX_LOCAL:?PREFIX_LOCAL is required}"

UBUNTU_FILES=https://launchpad.net/ubuntu/+archive/primary/+files
DEBIAN_ARCHIVE=https://archive.debian.org/debian
SDL2_MIXER_VERSION=2.8.0
SDL2_MIXER_SHA256=1cfb34c87b26dbdbc7afd68c4f545c0116ab5f90bbfecc5aebe2a9cb4bb31549
SDL2_IMAGE_VERSION=2.8.2
SDL2_IMAGE_SHA256=8f486bbfbcf8464dd58c9e5d93394ab0255ce68b51c5a966a918244820a76ddc
SDL2_TTF_VERSION=2.22.0
SDL2_TTF_SHA256=d48cbd1ce475b9e178206bf3b72d56b66d84d44f64ac05803328396234d67723
SDL2_NET_VERSION=2.2.0
SDL2_NET_SHA256=4e4a891988316271974ff4e9585ed1ef729a123d22c08bd473129179dc857feb
FFMPEG_VERSION=4.4.5 # 4.x is what ports link against (libavcodec.so.58)
FFMPEG_SHA256=f9514e0d3515aee5a271283df71636e1d1ff7274b15853bcd84e144be416ab07
CURL_VERSION=8.21.0
CURL_SHA256=aa1b66a70eace83dc624508745646c08ae561de512ab403adffb93ac87fc72e6
PATCHELF_VERSION=0.18.0 # static x86-64 build, runs in the toolchain container
PATCHELF_SHA256=ce84f2447fb7a8679e58bc54a20dc2b01b37b5802e12c57eece772a6f14bf3f0
THEORA_VERSION=1.1.1 # 1.2 renamed the libraries that games link against
THEORA_SHA256=40952956c47811928d1e7922cda3bc1f427eb75680c3c37249c91e949054916b
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
DEB_DIR="$SCRIPT_DIR/debs"
ROOT="$SCRIPT_DIR/root"
OUTPUT_DIR="$SCRIPT_DIR/output"
BUILD_ROOT="$SCRIPT_DIR/build-root"
STAMP="$OUTPUT_DIR/.stamp-$(cat "$SCRIPT_DIR/build.sh" "$SCRIPT_DIR"/debs*.txt | sha256sum | cut -c1-16)"

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
extract_debs() { # list dir url-prefix (Ubuntu files are addressed by name)
	while read -r path sha; do
		[ -n "$path" ] || continue
		deb="$DEB_DIR/${path##*/}"
		case "$3" in
			"$UBUNTU_FILES") download "$3/${path##*/}" "$sha" "$deb" ;;
			*) download "$3/$path" "$sha" "$deb" ;;
		esac
		dpkg-deb -x "$deb" "$2"
	done < "$1"
}
mkdir -p "$DEB_DIR"
rm -rf "$ROOT" "$BUILD_ROOT" "$OUTPUT_DIR"
mkdir -p "$ROOT" "$BUILD_ROOT" "$OUTPUT_DIR/bin" "$OUTPUT_DIR/lib"
extract_debs "$SCRIPT_DIR/debs.txt" "$ROOT" "$UBUNTU_FILES"
extract_debs "$SCRIPT_DIR/debs-compat.txt" "$ROOT" "$DEBIAN_ARCHIVE"
extract_debs "$SCRIPT_DIR/debs-build.txt" "$BUILD_ROOT" "$UBUNTU_FILES"

# Commands: every executable in the packages' bin directories, as real files.
for dir in bin sbin usr/bin usr/sbin; do
	[ -d "$ROOT/$dir" ] || continue
	for f in "$ROOT/$dir"/*; do
		name=${f##*/}
		case "$name" in
			bashbug|rbash|python3.10-config|ldconfig*) continue ;;
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
# ar, for ports that unpack .deb files: list (t), extract (x), print (p).
cat > "$OUTPUT_DIR/bin/ar" <<'EOF'
#!/usr/bin/env python3
import os, sys
if len(sys.argv) < 3:
    sys.exit("usage: ar t|x|p[v] archive [member...]")
op, archive, wanted = sys.argv[1].lstrip("-"), sys.argv[2], set(sys.argv[3:])
with open(archive, "rb") as f:
    if f.read(8) != b"!<arch>\n":
        sys.exit(f"ar: {archive}: not an ar archive")
    while header := f.read(60):
        name = header[:16].decode().strip().rstrip("/")
        size = int(header[48:58])
        data = f.read(size)
        f.read(size % 2)
        if name in ("", "/", "//") or (wanted and name not in wanted):
            continue
        if "t" in op:
            print(name)
        elif "p" in op:
            sys.stdout.buffer.write(data)
        elif "x" in op:
            if "v" in op:
                print(f"x - {name}")
            with open(os.path.basename(name), "wb") as out:
                out.write(data)
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

# PulseAudio keeps a private library in a subfolder; ports only need
# libpulse to load (their sound goes through ALSA).
cp -L "$ROOT"/usr/lib/aarch64-linux-gnu/pulseaudio/libpulsecommon-*.so "$OUTPUT_DIR/lib/"

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
build_sdl_lib SDL_net SDL2_net "$SDL2_NET_VERSION" "$SDL2_NET_SHA256"

## 3. FFmpeg 4 (libraries plus the ffmpeg and ffprobe commands) with only its
## own codecs, and libcurl using the OS's OpenSSL 3.
## Ubuntu's builds pull in dozens of codec, Kerberos and LDAP libraries.
unpack() { # url sha256 archive
	download "$1" "$2" "$DEB_DIR/$3"
	rm -rf "$SCRIPT_DIR/${3%.tar.*}"
	tar -xf "$DEB_DIR/$3" -C "$SCRIPT_DIR"
}
unpack "https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.xz" "$FFMPEG_SHA256" "ffmpeg-$FFMPEG_VERSION.tar.xz"
(
	cd "$SCRIPT_DIR/ffmpeg-$FFMPEG_VERSION"
	./configure --prefix="$SCRIPT_DIR/sdl-prefix" --enable-cross-compile \
		--cross-prefix="$CROSS_COMPILE" --arch=aarch64 --target-os=linux \
		--enable-shared --disable-static --disable-ffplay --disable-doc \
		--disable-avdevice --disable-postproc --disable-network \
		--disable-autodetect --enable-zlib >/dev/null
	make -s -j"$(nproc)"
	make -s install
)
unpack "https://curl.se/download/curl-$CURL_VERSION.tar.xz" "$CURL_SHA256" "curl-$CURL_VERSION.tar.xz"
(
	cd "$SCRIPT_DIR/curl-$CURL_VERSION"
	# Link against Ubuntu 22.04's OpenSSL 3, which is newer than the toolchain's
	# glibc; the device has both. Versioned symbols match Ubuntu's libcurl4.
	CPPFLAGS="-I$BUILD_ROOT/usr/include -I$BUILD_ROOT/usr/include/aarch64-linux-gnu" \
	LDFLAGS="-L$BUILD_ROOT/usr/lib/aarch64-linux-gnu -Wl,--allow-shlib-undefined" \
	./configure -q --host="${CROSS_COMPILE%-}" --prefix="$SCRIPT_DIR/sdl-prefix" \
		--disable-static --with-openssl --with-zlib --enable-versioned-symbols \
		--with-ca-bundle=/etc/ssl/certs/ca-certificates.crt --without-libpsl \
		--without-libidn2 --without-brotli --without-zstd --without-nghttp2 \
		--without-libssh2 --disable-ldap --disable-ldaps --disable-manual
	make -s -j"$(nproc)"
	make -s install
)
for prog in ffmpeg ffprobe; do
	cp "$SCRIPT_DIR/sdl-prefix/bin/$prog" "$OUTPUT_DIR/bin/"
	"${CROSS_COMPILE}strip" "$OUTPUT_DIR/bin/$prog"
done
for lib in libavcodec.so.58 libavformat.so.58 libavutil.so.56 libswscale.so.5 \
	libswresample.so.3 libavfilter.so.7 libcurl.so.4; do
	cp -L "$SCRIPT_DIR/sdl-prefix/lib/$lib" "$OUTPUT_DIR/lib/"
	"${CROSS_COMPILE}strip" --strip-unneeded "$OUTPUT_DIR/lib/$lib"
done

## 4. Theora video (Love2D games need it). Ubuntu's build pulls in cairo and X11.
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

## 5. Let the pak's programs and libraries find each other even when a port
## replaces LD_LIBRARY_PATH, as PortMaster's Weston wrapper does. The search
## path goes after LD_LIBRARY_PATH, so ports' own libraries still come first.
PATCHELF_ARCHIVE="$DEB_DIR/patchelf-$PATCHELF_VERSION-x86_64.tar.gz"
download "https://github.com/NixOS/patchelf/releases/download/$PATCHELF_VERSION/patchelf-$PATCHELF_VERSION-x86_64.tar.gz" \
	"$PATCHELF_SHA256" "$PATCHELF_ARCHIVE"
mkdir -p "$SCRIPT_DIR/patchelf"
tar -xzf "$PATCHELF_ARCHIVE" -C "$SCRIPT_DIR/patchelf" ./bin/patchelf
set_runpath() { # runpath files...
	runpath=$1
	shift
	for f in "$@"; do
		[ -f "$f" ] && [ ! -L "$f" ] && head -c4 "$f" | grep -q ELF || continue
		"$SCRIPT_DIR/patchelf/bin/patchelf" --page-size 4096 --set-rpath "$runpath" "$f"
	done
}
set_runpath '$ORIGIN/../lib' "$OUTPUT_DIR"/bin/*
set_runpath '$ORIGIN' "$OUTPUT_DIR"/lib/*.so*
set_runpath '$ORIGIN/../..' "$OUTPUT_DIR"/lib/python3.10/lib-dynload/*.so

# FAT has no symlinks: replace any left with the file itself, or drop dangling
# ones (Debian's sitecustomize.py points into /etc).
find "$OUTPUT_DIR" -type l | while read -r link; do
	if [ -e "$link" ]; then
		cp -L "$link" "$link.tmp" && mv -f "$link.tmp" "$link"
	else
		rm -f "$link"
	fi
done

rm -rf "$ROOT" "$BUILD_ROOT" "$SCRIPT_DIR/patchelf"
touch "$STAMP"
du -sh "$OUTPUT_DIR"
