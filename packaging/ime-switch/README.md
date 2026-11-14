# ime-switch

Switch 新酷音 between the Plus build and the original Arch packages.

```bash
ime-switch            # status (never switches)
ime-switch chewing    # 新酷音 Plus (libchewing 0.14 + fcitx5-chewing-plus) + fcitx5-voxkit dictation
ime-switch original   # original packages (libchewing 0.13.1 + fcitx5-chewing 5.1.13)
```

- No toggle: a mode is always explicit.
- Installs only what is missing, in one pacman transaction (answer `y` to remove conflicting packages).
- Makes 新酷音 the active input method and restarts fcitx5 via `omarchy-fcitx5.service`.
- `chewing` enables Plus features the first time, and (re)starts `voxkit.service` so an upgraded daemon is always the one running. `original` stops the service and removes `fcitx5-voxkit`.
- Snapshots the fcitx5 profile, `chewing.conf` and the Chewing user dictionary to `~/.local/share/ime-switch/snapshots/` before each switch.

Packages: Plus builds in `packaging/{libchewing,fcitx5-chewing}/`, dictation in `~/Work/fcitx5-voxkit/packaging/`, originals (signed Arch packages) in `~/.local/share/chewing-rollback/`.
