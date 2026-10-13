# Changelog

## 0.3.1 - 2026-10-13
- The genre is gone: under each title only the channel, which now has the whole line. The sensor's `genres` is no
  longer read, so you can drop it (and its line in the script `film_stasera_day`).
- The tile needs a little less room in the screen's layout memory (10000 instead of 11500 bytes).

## 0.3.0 - 2026-10-13
- The card can show the whole week: after the last page of a day it turns to the next day, up to six days ahead, and
  then back to today. The line on top says the day and the page. Back on another day returns to today.
- For the week, add one sensor per day and the script `film_stasera_day` to Home Assistant (README, "The whole week").
  Without them the card shows today, as before.
- A day can now hold up to 40 films instead of 20. The tile needs a little more room in the screen's layout memory
  (11500 instead of 9000 bytes).
- The card's first page now stays its whole 8 seconds; in 0.2.5 it could turn almost at once.

## 0.2.5 - 2026-10-13
- The card now shows every film still to come: when they do not fit on one page, it turns to the next page by itself
  every 8 seconds and starts again after the last. The line on top says which page it shows ("Page 1 of 3").
  Tessera's cards take no taps yet, so there are no buttons to turn the pages.

## 0.2.4 - 2026-10-13
- The card no longer scrolls, which made it slow: the films are laid out on the card in columns (up to three on a wide
  screen), and when more are left than fit, the line on top says how many.
- The card shows the films also when the guide is old, with "Old guide" on top instead of only the words.
- The tile and the card no longer measure and lay out every row each second: only when a film ends or the guide changes.
- A title cut short no longer breaks an accented letter.

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
