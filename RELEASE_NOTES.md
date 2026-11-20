# v6.15.0-pvaibhav.4

Changes since v6.15.0-pvaibhav.3.

- Reuse video textures and shader output on duplicate frames. Upload RGB565 frames directly when the debug HUD, fade-in, and ambient LEDs do not need a copy on the CPU.
- Batch emulator audio before mixing. **High** now uses a cubic resampler. Select **Very High** for the sinc resampler previously used by **High**.
- Default FCEUmm sound quality to **Low** for NES and Famicom Disk System games.
- Cut idle wakeups and check HDMI and battery status less often during gameplay. The H700 and TrimUI Brick and Smart Pro use lower CPU speed limits in **Powersave** mode.

On an existing fork install, open **Updater (pvaibhav)** and choose **Quick Update**. Choose **Full Update** if you also want to install Extras.

[Installation and update guide](https://nextui.loveretro.games/usage/#getting-started).
