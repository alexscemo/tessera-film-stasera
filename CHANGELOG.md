# Changelog

## 0.2.3 - 2026-10-12
- Tapping the tile opens a wide, scrollable list of upcoming and currently airing films.

## 0.2.2 - 2026-10-12
- The tile can grow up to 2 columns by 3 rows.

## 0.2.1 - 2026-10-12
- Adds `store.png`, the picture of the plugin on the Plugins page, made with `tools/store_image.py`. No change to the tile.

## 0.2.0 - 2026-10-11
- Under each title the channel's name is now in the regular colour, with a dot before it in the colour of its group
  (Rai, Mediaset, Discovery, others), then the genre in grey. The sensor says the group in a new attribute, `groups`
  (a number 1 to 4); without it the dot is simply not drawn.
- The tile needs a little more room in the screen's layout memory (9000 instead of 6000 bytes).
- A tile can now start from 1x1 (it shows the next film) and grow with the editor's handles up to 4x4.

## 0.1.0 - 2026-10-11
- First version: a tile with the films on TV tonight, read from a sensor of Home Assistant.
