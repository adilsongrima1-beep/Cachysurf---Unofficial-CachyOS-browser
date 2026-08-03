# Cachy Surf

An unofficial CachyOS-styled desktop browser with a macOS Safari-inspired interface. It uses Qt WebEngine's Chromium-based renderer.

## Version 0.4.1

- Full-width Smart Search field with Google search and Safari-style suggestions
- Dynamic animated tabs directly below the address bar
- Dark browser UI and forced dark websites
- Persistent cookies, cache, local storage, WebGL, full-screen media, downloads, bookmarks, and history
- Password manager backed by the Linux system keyring
- CSV password import with quoted-field parsing, column detection, duplicate updating, progress, and error reporting
- Manifest V3 extension manager on Qt WebEngine 6.10+: install ZIP/unpacked extensions, enable/disable, open popup, and remove
- Experimental WebHID-compatible device bridge with per-site permission prompts and a Linux hidraw access helper
- Private windows, zoom, PDF saving, find-on-page, and browsing-data controls

## Install on CachyOS

```fish
./install-cachyos.fish
```

Then run:

```fish
cachysurf
```

Private mode:

```fish
cachysurf --private
```

## Password CSV format

Exports from Chromium-family browsers and many password managers are accepted when the header contains a website column such as `url`, `website`, `origin`, or `host`, and a `password` column. Username headings such as `username`, `login`, or `email` are optional. Imported secrets are copied into the desktop keyring; delete the original plaintext CSV afterward.

## Extensions

Open **Menu → Extensions**. Qt WebEngine supports Manifest V3 extensions from ZIP files or unpacked folders. Direct Chrome Web Store installation is not provided.

## Device access

Open **Menu → Connected Devices**. Websites must request permission. Cachy Surf does not expose ordinary keyboard or pointer input interfaces; compatible configurators should use the device's separate vendor-defined HID interface. If Linux denies opening the device, use **Install Linux device access**, authenticate, then unplug and reconnect it.

## Important limitations

This remains an early community browser. Extension compatibility is below Chrome/Chromium, WebHID support is an experimental compatibility bridge, DRM streaming depends on system Widevine availability, and password autofill may not detect unusual login forms. Do not use this prototype as the only copy of important passwords.

This project is not affiliated with Apple or the CachyOS team.
