# v6.15.0-pvaibhav.2

Changes since v6.15.0-pvaibhav.1.

- Add **Power off timeout** in Settings. Set how long the device stays suspended before shutting down, from 2 minutes to 24 hours. The default is **Never**. A successfully quicksaved game resumes on next boot.
- Add **DOSBox Pure** to Extras for TrimUI and Anbernic H700 devices. Put games in `Roms/MS-DOS (DOS)` and press **MENU + L2** to open the DOSBox menu. The core keeps its save-state size stable across gameplay.
- Keep emulator option selections when a core re-registers its options, and apply frame-rate and audio sample-rate changes when resuming from the menu.
- Show live GPU readings in the Debug HUD, with clock speed in MHz and dashes for unavailable readings.

On an existing fork install, open **Updater (pvaibhav)**. Choose **Full Update** to install DOSBox Pure with the firmware, or **Quick Update** for the firmware alone.

[Installation and update guide](https://nextui.loveretro.games/usage/#getting-started).
