<div align="center">

  <img width=128px src="github/logo_outline.png">

</div>

# NextUI: pvaibhav's experimental fork

Anbernic H700 support has officially merged into NextUI and is included in [v6.15.0](https://github.com/LoveRetro/NextUI/releases/tag/v6.15.0).
For stable builds on supported Anbernic and TrimUI devices, use the [official NextUI repository](https://github.com/LoveRetro/NextUI) and its [latest stable release](https://github.com/LoveRetro/NextUI/releases/latest).

This repository is now my personal fork for experimental features and improvements before they are merged upstream.
Releases from `pvaibhav` will combine upstream `main` with those changes.

## Experimental features

Included in this fork:

- **LUT-based display calibration.** Per-device 1D lookup tables correct display colours towards sRGB.
  Bundled profiles cover the TrimUI Brick Pro and Anbernic RG40XXV, RG34XXSP, and RGSP.
  The **Colour correction** setting offers **Off**, **On**, and **Custom** modes, with manual red, green, and blue gains in Custom mode.

## Build and release this fork

Tag the commit you want to release and push the tag to this repository:

```sh
git tag -a v6.15.0-pvaibhav.1 -m "NextUI v6.15.0-pvaibhav.1"
git push origin v6.15.0-pvaibhav.1
```

Use `v<upstream-version>-pvaibhav.<revision>` and increment the revision for each release based on that upstream version.
Write the changelog in [RELEASE_NOTES.md](RELEASE_NOTES.md), with `# <tag>` on the first line.
The release workflow builds the tagged commit, including all emulator cores, for `tg5040`, `tg5050`, and `h700`.
It publishes a GitHub release marked **Latest**, with `NextUI-<tag>-all.zip`, `NextUI-<tag>-base.zip`, and `NextUI-<tag>-extras.zip`.
Tags without the `-pvaibhav.<revision>` suffix do not trigger fork releases.

To compile a candidate before tagging it, run **Release NextUI fork** manually in GitHub Actions and enter the planned version tag.
Manual runs upload build artifacts without creating a tag or publishing a release.

<hr>

<h4 align="center">
  <a href="https://nextui.loveretro.games/usage/#getting-started" target="_blank">Installation Guide</a>
  ·
  <a href="https://nextui.loveretro.games/docs/" target="_blank">Documentation</a>
  ·
  <a href="https://discord.gg/HKd7wqZk3h" target="_blank">Discord</a>
</h4>

<div align="center"><p>
    <a href="https://github.com/LoveRetro/NextUI/releases/latest">
      <img alt="Official stable release" src="https://img.shields.io/github/v/release/LoveRetro/NextUI?style=for-the-badge&logo=starship&include_prerelease=false&color=C9CBFF&logoColor=D9E0EE&labelColor=302D41&sort=semver" />
    </a>
    <a href="https://github.com/LoveRetro/NextUI/pulse">
      <img alt="Last commit" src="https://img.shields.io/github/last-commit/LoveRetro/NextUI?style=for-the-badge&logo=starship&color=8bd5ca&logoColor=D9E0EE&labelColor=302D41"/>
    </a>
    <a href="https://github.com/LoveRetro/NextUI/blob/main/LICENSE">
      <img alt="License" src="https://img.shields.io/github/license/LoveRetro/NextUI?style=for-the-badge&logo=starship&color=ee999f&logoColor=D9E0EE&labelColor=302D41" />
    </a>
    <a href="https://github.com/LoveRetro/NextUI/stargazers">
      <img alt="Stars" src="https://img.shields.io/github/stars/LoveRetro/NextUI?style=for-the-badge&logo=starship&color=c69ff5&logoColor=D9E0EE&labelColor=302D41" />
    </a>
    <a href="https://github.com/LoveRetro/NextUI/issues">
      <img alt="Issues" src="https://img.shields.io/github/issues/LoveRetro/NextUI?style=for-the-badge&logo=bilibili&color=F5E0DC&logoColor=D9E0EE&labelColor=302D41" />
    </a>
</div>

---

NextUI is custom firmware based on MinUI, with a rebuilt emulation engine and added features for supported TrimUI and Anbernic H700 devices.

For other devices we recommend checking out [MinUI](https://github.com/shauninman/MinUI)

## License

This project is licensed under [PolyForm Noncommercial 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0).
Personal, educational, and non-commercial use is freely permitted. Commercial use is not.
See [LICENSE](LICENSE) and [NOTICE](NOTICE) for full details.

> **Note:** This project was previously licensed under GPL 3.0. As of August 15, 2026, the license
> has changed to PolyForm Noncommercial 1.0.0. The full transition history and rationale
> is documented in [issue #765](https://github.com/LoveRetro/NextUI/issues/765).

## Features inherited from NextUI

- Fixed both screen tearing and sync stutter problems of MinUI by rebuilding the emulator engine core
- Game switcher menu (Onion OS style) by [@frysee](https://github.com/frysee)
- High audio quality, due to advanced resampling engine using [libsamplerate](https://github.com/libsndfile/libsamplerate) (with quality/performance setting per emulator)
- Much lower latency (average 20ms, 1 frame of 60fps)
- Shaders!
- Fully OpenGL/GPU based for faster performance!
- native WiFi support
- Game art/media support
- Game time tracker!
- Cheats support
- Integrated WiFi
- Bluetooth Audio
- Overlays support!
- Dynamic cpu speed scaling (keeps your device cool and extends battery life, but gives the performance right when needed)
- Fully GPU OpenGL based for faster performance
- Custom bootlogos contributed by [@SolvalouArt](https://bsky.app/profile/solvalouart.bsky.social)
- Color and font settings to customize the NextUI interface
- Menu animations
- LED control, change colors, effects, brightness
- LED indicators, low battery, stand by, charging (brightness can be set seperately)
- Haptic feedback in the menu by [@ExonakiDev](https://github.com/exonakidev)
- Ambient LED mode, the LEDS act like Ambient light TV's for extra immersion, configurable per emulator
- Display controls (color temperature, brightness, contrast, saturation, exposure)
- Support for automatic NTP time synchronization with timezones and realtime clock (RTC)
- Deep Sleep mode, gives instant ON and avoids the overheat bug on the Brick by [@zhaofengli](https://github.com/zhaofengli)
- Battery Monitoring including history graph and time left prediction
- Scrolling animated titles for longer game names
- Updated and optimized build emulation cores
- Rumble strength fixed and is now variable as used by the games
- FBNeo Arcade screen rotation
- PAL Mode
- Next font supports CJK for JP/CN etc rom names
- Lot of other smaller fixes and optimizations

---

## Currently supported devices:
- Trimui Brick
- Trimui Brick Pro
- Trimui Smart Pro
- Trimui Smart Pro S
- Anbernic H700 Devices

---

## Installing

Follow our [installation guide](https://nextui.loveretro.games/usage/#getting-started).

---

## How to use

While in menu:

- Hold `start` and press `volume up` or `volume down` to adjust brighness
- Hold `select` and press `volume up` or `volume down` to adjust color temperature
- Short press `select` to open the game switcher

While in game:

- `Menu` opens in game options menu, adjusting controls, scaling and what not
- Hold `menu` and `select` at same time to open up the game switcher

Deep sleep:

When the device is left idle it will enter light sleep. Light sleep turns the screen off and pulses the leds 5 times.

After two minutes the device will go into deep sleep. The leds will turn completely off.

---

## What about X feature?

NextUI supports a variety of optional extras called Paks that are maintained by community members.

Our wiki has a [list of all available Paks](https://nextui.loveretro.games/pak-store/) that can be downloaded from our Pak Store straight on your device.

---

## Credits

- [josegonzalez](https://github.com/josegonzalez) for [minui-keyboard](https://github.com/josegonzalez/minui-keyboard)
- [knulli-cfw](https://github.com/knulli-cfw)/[Mikhailzrick](https://github.com/Mikhailzrick) for [fan-control-daemon](https://github.com/knulli-cfw/knulli-linux/commit/5c1b4724330b59cbe8af79d0a7f1dac86f1c221a)
- [Kenney](www.kenney.nl) for [Input Prompts](https://kenney.nl/assets/input-prompts)

---

## Play Retro's review of NextUI

[![image](https://github.com/user-attachments/assets/43217a30-e052-4f67-88a9-c4361f82e72f)](https://www.youtube.com/watch?v=m_7ObMHAFK4)

##  Retro Handhelds' coverage of NextUI

[![image](https://github.com/user-attachments/assets/5fd538ea-285b-46e9-add4-1ef99b2ee9e5)](https://www.youtube.com/watch?v=KlYVmtYDqRI)

---

## For all other info visit our website: [nextui.loveretro.games](https://nextui.loveretro.games)
