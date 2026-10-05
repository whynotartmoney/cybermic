<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

### Cyber Mick City

Generated LVGL 9 bitmap fonts for the moving-art UI. Source OpenType files are not committed.

| File | Size and format | Coverage | Use |
| --- | --- | --- | --- |
| [`fonts/mick_latin_16.c`](fonts/mick_latin_16.c) | 16 px, 4 bpp, uncompressed | Printable ASCII except `U+005E` | Catalog labels, battery, and the form name |
| [`fonts/mick_latin_28.c`](fonts/mick_latin_28.c) | 28 px, 4 bpp, uncompressed | `CYBER` | Catalog wordmark |
| [`fonts/OFL-Orbitron.txt`](fonts/OFL-Orbitron.txt) | SIL Open Font License 1.1 | Orbitron license | Accompanies the Latin bitmaps |

Source: Orbitron Medium, The League of Moveable Type, SIL OFL 1.1. Converter: `lv_font_conv` 1.5.3. `main/CMakeLists.txt` compiles the `.c` files, and the labels select them explicitly. The on-screen copy is English. `tests/test_mick_font_coverage.py` checks the cmap and treats U+9F98 as absent.

```bash
lv_font_conv --font "Orbitron-Medium.ttf" --size 16 --bpp 4 \
  --format lvgl --no-compress --no-kerning \
  --lv-font-name mick_latin_16 --lv-include lvgl.h \
  --range 0x20-0x7E --output assets/fonts/mick_latin_16.c
lv_font_conv --font "Orbitron-Medium.ttf" --size 28 --bpp 4 \
  --format lvgl --no-compress --no-kerning \
  --lv-font-name mick_latin_28 --lv-include lvgl.h \
  --symbols "CYBER" --output assets/fonts/mick_latin_28.c
```

## Images

Store reusable source images and generated display assets in `images/`.

| File | Dimensions and format | Use and source |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160, JPEG | Product hero image embedded in both project README files to foreground AI Passport and its open, maker-oriented identity. |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724, PNG RGBA | Optional technical infographic retained as a reference asset; it is no longer used as the homepage hero. Generated for this repository with the built-in image generation tool on 2026-09-17; the six labels and values were checked against the documented hardware contract. |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336, PNG RGBA | Transparent black wordmark extracted from the repository's original `images/logo.png`; embedded in both project README files for light backgrounds. |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336, PNG RGBA | White version of the extracted wordmark, used by the README `<picture>` element when GitHub is in dark mode. |

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.
