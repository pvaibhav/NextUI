# v6.15.0-pvaibhav.5

This release refines the gameplay performance and battery optimisations from `v6.15.0-pvaibhav.4` and extends them to Ambient LED mode.

- **Lower overhead with Ambient LEDs:** Ambient LED mode now uses much less CPU and updates lights more smoothly without forcing extra frame conversions during gameplay.
- **Smoother video, rewind, and loading:** Further cuts per-frame graphics overhead, speeds up zipped ROM loading and rewind, and reduces background Wi-Fi check overhead.
- **Reliability fixes:** Fixes rare LED state races with the battery monitor, prevents stale cached shaders after edits, and ensures clean letterbox borders on all screens.

On an existing fork install, open **Updater (pvaibhav)** and choose **Quick Update**. Choose **Full Update** if you also want to install Extras.

[Installation and update guide](https://nextui.loveretro.games/usage/#getting-started).
