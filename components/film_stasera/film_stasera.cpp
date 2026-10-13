#include "film_stasera.h"
#include <algorithm>
#include <string>
#include <vector>

namespace esphome::film_stasera {

using tessera::Font;
namespace ui = tessera::ui;

// One film as Home Assistant's sensor has it: four lists of the same length, one entry per film (README.md).
struct Film {
  std::string title, channel;
  uint32_t at = 0;   // when it starts, seconds since 1970
  int group = 0;     // the channel's group (1 to 4, 0 for none): the colour of the dot before its name
};

// A film that started less than this long ago is still on, so it stays on the tile. The guide has no end times.
static constexpr uint32_t ON_AIR_SECONDS = 150 * 60;
static constexpr int MAX_PER_COLUMN = 8;
static constexpr size_t MAX_FILMS = 40;
// How long the card shows a page of films before it turns to the next one, when they do not fit on one page.
static constexpr uint32_t PAGE_SECONDS = 8;
// The card's week: today and the days after it, asked of Home Assistant one at a time (README.md, "The whole week").
static constexpr int WEEK_DAYS = 7;
static constexpr uint32_t ANSWER_SECONDS = 10;   // no answer by then: the week is not there, the card stays on today
static constexpr const char *DAY_COMMAND = "call_service:script.film_stasera_day";

// What the sensor says, read the same way by the tile and the card.
struct Guide {
  std::vector<Film> films;   // in order of start
  bool has_entity = false, no_value = false, stale = false;
  std::string wait;

  void read(JsonObjectConst data) {
    JsonObjectConst a = data["attributes"];
    read_lists(a);
    // Before a person has chosen the sensor the tile gets {}; a sensor without a value says unavailable or unknown.
    has_entity = !data["state"].isNull();
    const std::string state = std::string(data["state"] | "");
    no_value = state == "unavailable" || state == "unknown";
    stale = a["stale"] | false;
    wait = std::string(data["wait"] | "");
  }

  // The films of the lists in `a`: a sensor's attributes, or the answer for one day of the week.
  void read_lists(JsonObjectConst a) {
    films.clear();
    JsonArrayConst titles = a["titles"].as<JsonArrayConst>();
    JsonArrayConst channels = a["channels"].as<JsonArrayConst>();
    JsonArrayConst starts = a["starts"].as<JsonArrayConst>();
    JsonArrayConst groups = a["groups"].as<JsonArrayConst>();
    for (size_t i = 0; i < titles.size() && films.size() < MAX_FILMS; ++i) {
      Film f;
      f.title = std::string(titles[i] | "");
      f.channel = std::string(channels[i] | "");
      f.at = starts[i] | 0u;
      f.group = groups[i] | 0;
      if (f.at && !f.title.empty()) films.push_back(std::move(f));
    }
    std::sort(films.begin(), films.end(), [](const Film &x, const Film &y) { return x.at < y.at; });
  }

  // The first film still worth showing (not over yet). The films are in order of start, so the ones from here on are
  // the ones to show: films.size() when none is left.
  size_t first(uint32_t now) const {
    size_t i = 0;
    while (i < films.size() && (!now || films[i].at + ON_AIR_SECONDS < now)) ++i;
    return i;
  }

  // The message in place of the films, or null when there are films to show.
  const char *why(uint32_t now, size_t first) const {
    return !has_entity || wait == "wrong_entity" ? "choose_sensor"
           : !wait.empty() || no_value || !now   ? "waiting"
           : first >= films.size()                ? "none"
                                                  : nullptr;
  }
};

// The colour of a group's dot: the four colours of the Tessera mark, the same in both looks. No group: no dot.
static theme::Role group_role(int group) {
  switch (group) {
    case 1: return theme::MARK_BLUE;
    case 2: return theme::MARK_AMBER;
    case 3: return theme::MARK_GREEN;
    default: return theme::MARK_PURPLE;
  }
}

// The text as it fits in `width`, cut with dots when it does not. It cuts between characters, never inside one, and
// finds the cut in a few measures (a halving search) instead of one measure per character.
static std::string fit_text(const std::string &text, int width, Font font) {
  if (text.empty() || width <= 0) return "";
  if (ui::text_width(text, font) <= width) return text;
  const std::string ellipsis = "...";
  std::vector<size_t> cuts;   // where each character after the first starts
  for (size_t i = 1; i < text.size(); ++i)
    if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) cuts.push_back(i);
  size_t lo = 0, hi = cuts.size();   // the most characters (cuts[k - 1] bytes) that fit with the dots
  while (lo < hi) {
    const size_t mid = (lo + hi + 1) / 2;
    if (ui::text_width(text.substr(0, cuts[mid - 1]) + ellipsis, font) <= width)
      lo = mid;
    else
      hi = mid - 1;
  }
  return lo ? text.substr(0, cuts[lo - 1]) + ellipsis : ellipsis;
}

// One time column as wide as the widest time of the films from `first` on (at most `count` of them), so every title
// starts at the same place.
static int when_width(const std::vector<Film> &films, size_t first, size_t count) {
  int w = 0;
  for (size_t i = first; i < films.size() && i - first < count; ++i)
    w = std::max(w, ui::text_width(tessera::clock_text(films[i].at), Font::TITLE));
  return w ? w + ui::px(ui::large() ? 12 : 8) : 0;
}

// A film's row: the start time, beside it the title, and under the title the channel's name with a dot in the colour of
// its group. The tile and the card draw the same row.
struct Row {
  lv_obj_t *when{}, *title{}, *dot{}, *chan{};
  int x = 0, y = 0;

  void make(lv_obj_t *parent, int at_x, int at_y) {
    x = at_x;
    y = at_y;
    when = ui::label(parent, Font::TITLE, theme::INK);
    title = ui::label(parent, Font::BODY_LARGE, theme::INK);
    dot = ui::block(parent, theme::MARK_BLUE);
    chan = ui::label(parent, Font::BODY, theme::INK);
    lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
    lv_label_set_long_mode(chan, LV_LABEL_LONG_CLIP);
  }

  void paint_colors(int group) {
    ui::set_color(when, theme::INK);
    ui::set_color(title, theme::INK);
    ui::set_color(chan, theme::INK);
    lv_obj_set_style_opa(dot, group > 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    if (group > 0) lv_obj_set_style_bg_color(dot, ui::color(group_role(group)), 0);
  }

  // The time in a column of `when_w`, the title beside it and under the title the channel (`film`; null
  // for a message, which has only the title), in a row `row_w` wide whose first line is `line` high. A message takes
  // the title's place from the row's left edge. Nothing to show: everything empty.
  void place(const std::string &time, const std::string &text, const Film *film, int when_w, int row_w, int line) {
    ui::set_text(when, fit_text(time, std::max(1, when_w), Font::TITLE));
    const int tw = time.empty() ? 0 : when_w;
    lv_obj_set_size(when, std::max(1, tw), ui::line_height(Font::TITLE));
    lv_obj_set_pos(when, x, y + (line - ui::line_height(Font::TITLE)) / 2);
    const int w = std::max(1, row_w - tw);
    ui::set_text(title, fit_text(text, w, Font::BODY_LARGE));
    lv_obj_set_size(title, w, ui::line_height(Font::BODY_LARGE));
    lv_obj_set_pos(title, x + tw, y + (line - ui::line_height(Font::BODY_LARGE)) / 2);
    // The second line: [dot] Channel
    const int lh = ui::line_height(Font::BODY), y2 = y + line;
    const std::string channel = film ? film->channel : std::string();
    const int group = film ? film->group : 0;
    const int d = std::min(w, std::max(ui::px(7), lh * 6 / 10));
    const int dot_w = group > 0 ? std::min(w, d + ui::px(6)) : 0;
    paint_colors(group);
    lv_obj_set_size(dot, std::max(1, d), std::max(1, d));
    lv_obj_set_pos(dot, x + tw, y2 + (lh - d) / 2);
    const int chan_w = std::max(1, w - dot_w);
    ui::set_text(chan, fit_text(channel, chan_w, Font::BODY));
    lv_obj_set_size(chan, chan_w, lh);
    lv_obj_set_pos(chan, x + tw + dot_w, y2);
  }
};

// The tile: a header (its name and the date), then one row per film as many as fit. A tile at least 13 cm wide is
// split into two columns that fill one after the other, so a whole evening fits on a page. The sensor changes twice a
// day, the clock every second: the rows are laid out again only when what they show changes.
class FilmsTile : public tessera::Tile {
 public:
  explicit FilmsTile(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    name_ = c.name;
    entity_ = c.entity;
    tile_ = c.tile;
    gap_ = ui::px(ui::large() ? 8 : 5);
    line_ = std::max(ui::line_height(Font::TITLE), ui::line_height(Font::BODY_LARGE));
    row_h_ = line_ + ui::line_height(Font::BODY);
    // The header only where at least two rows still fit under it.
    const int head = ui::line_height(Font::TITLE) + gap_;
    const int step = row_h_ + gap_;
    has_head_ = (height_ - head + gap_) / step >= 2;
    top_ = has_head_ ? head : 0;
    // Wide: two columns, by the glass's own size, so a 4-column tile of a 7-inch screen splits and a narrow one does not.
    columns_ = width_ >= ui::mm(130) ? 2 : 1;
    col_gap_ = ui::px(ui::large() ? 28 : 18);
    col_w_ = (width_ - (columns_ - 1) * col_gap_) / columns_;
    const int fit = (height_ - top_ + gap_) / step;
    per_column_ = std::max(1, std::min(MAX_PER_COLUMN, fit));
    rows_.resize(static_cast<size_t>(per_column_ * columns_));
    for (size_t i = 0; i < rows_.size(); ++i) {
      const int col = static_cast<int>(i) / per_column_, r = static_cast<int>(i) % per_column_;
      rows_[i].make(c.parent, col * (col_w_ + col_gap_), top_ + r * step);
    }
    if (has_head_) {
      head_ = ui::label(c.parent, Font::TITLE, theme::INK);
      head_note_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    }
  }

  void on_state(JsonObjectConst data) override {
    guide_.read(data);
    dirty_ = true;
  }

  void on_tick(uint32_t now) override {
    const size_t first = guide_.first(now);
    const char *why = guide_.why(now, first);
    const std::string note = guide_.stale ? plugin_->text("old") : now ? tessera::date_text(now) : std::string();
    // Most ticks change nothing on the tile: then nothing is measured or moved.
    if (!dirty_ && first == first_ && why == why_ && note == note_) return;
    dirty_ = false;
    first_ = first;
    why_ = why;
    note_ = note;
    paint_head();
    const int when_w = why ? 0 : when_width(guide_.films, first, rows_.size());
    for (size_t i = 0; i < rows_.size(); ++i) {
      if (i == 0 && why)
        rows_[0].place("", plugin_->text(why), nullptr, 0, width_, line_);
      else if (why || first + i >= guide_.films.size())
        rows_[i].place("", "", nullptr, 0, col_w_, line_);
      else {
        const Film &f = guide_.films[first + i];
        rows_[i].place(tessera::clock_text(f.at), f.title, &f, when_w, col_w_, line_);
      }
    }
  }

  void on_theme() override {
    if (head_) ui::set_color(head_, theme::INK);
    if (head_note_) ui::set_color(head_note_, theme::MUTED);
    dirty_ = true;
    on_tick(tessera::epoch());
  }

  void on_tap() override { tessera::open_card(plugin_->plugin_id(), "movies", entity_, tile_); }

 private:
  // The tile's name on the left; on the right the date, or that the guide is old.
  void paint_head() {
    if (!head_) return;
    const int note_w = note_.empty() ? 0 : ui::text_width(note_, Font::BODY) + ui::px(2);
    const std::string title = name_.empty() ? plugin_->text("header") : name_;
    ui::set_text(head_, title);
    lv_obj_set_size(head_, std::max(1, width_ - note_w - (note_w ? ui::px(8) : 0)), ui::line_height(Font::TITLE));
    lv_obj_set_pos(head_, 0, 0);
    ui::set_text(head_note_, note_);
    lv_obj_set_size(head_note_, std::max(1, note_w), ui::line_height(Font::BODY));
    lv_obj_set_pos(head_note_, width_ - note_w, (ui::line_height(Font::TITLE) - ui::line_height(Font::BODY)) / 2);
    lv_obj_set_style_text_align(head_note_, LV_TEXT_ALIGN_RIGHT, 0);
  }

  const tessera::Plugin *plugin_;
  int width_ = 0, height_ = 0, gap_ = 0, line_ = 0, row_h_ = 0, top_ = 0;
  int columns_ = 1, col_gap_ = 0, col_w_ = 0, per_column_ = 1;
  int tile_ = -1;
  bool has_head_ = false, dirty_ = true;
  size_t first_ = 0;
  const char *why_ = nullptr;
  std::string name_, entity_, note_;
  std::vector<Row> rows_;
  lv_obj_t *head_{}, *head_note_{};
  Guide guide_;
};

// The card: the films of a day on a fixed grid as large as the card, in up to three columns that fill one after the
// other, with a line on top that says which day and page it shows. Nothing scrolls, and a card takes no taps: it turns
// to the next page every PAGE_SECONDS, after the last page of a day to the next day, and after the seventh day back to
// today. Today's films are the tile's; each other day is asked of Home Assistant when its turn comes, through the
// script film_stasera_day. Without that script the card stays on today. Back on another day returns to today, Back on
// today closes the card. The rows are laid out again only when what they show changes.
class FilmsCard : public tessera::Card {
 public:
  explicit FilmsCard(FilmStasera *plugin) : plugin_(plugin) {}
  ~FilmsCard() override {
    if (plugin_->open_card_ == this) plugin_->open_card_ = nullptr;
  }

  void open(const tessera::CardContext &c) override {
    plugin_->open_card_ = this;
    width_ = c.width;
    const int gap = ui::px(ui::large() ? 8 : 5);
    line_ = std::max(ui::line_height(Font::TITLE), ui::line_height(Font::BODY_LARGE));
    const int step = line_ + ui::line_height(Font::BODY) + gap;
    note_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    lv_obj_set_size(note_, width_, ui::line_height(Font::BODY));
    lv_obj_set_pos(note_, 0, 0);
    const int top = ui::line_height(Font::BODY) + gap;
    // A column for every 7 cm of glass, at most three.
    const int columns = std::max(1, std::min(3, width_ / ui::mm(70)));
    const int col_gap = ui::px(ui::large() ? 28 : 18);
    col_w_ = (width_ - (columns - 1) * col_gap) / columns;
    const int per_column = std::max(1, (c.height - top + gap) / step);
    rows_.resize(std::min(MAX_FILMS, static_cast<size_t>(per_column * columns)));
    for (size_t i = 0; i < rows_.size(); ++i) {
      const int col = static_cast<int>(i) / per_column, r = static_cast<int>(i) % per_column;
      rows_[i].make(c.parent, col * (col_w_ + col_gap), top + r * step);
    }
  }

  void on_state(JsonObjectConst data) override {
    guide_.read(data);
    if (day_ == 0) dirty_ = true;
    render();
  }

  void on_tick(uint32_t now) override {
    now_ = now;
    if (!page_at_) page_at_ = now;   // the first page gets its whole time from the first tick with a clock
    if (asked_ && now - asked_at_ >= ANSWER_SECONDS) {
      week_ = false;   // Home Assistant did not answer: stay on today
      show_day(0);
    } else if (!asked_ && now && now - page_at_ >= PAGE_SECONDS) {
      next();
    }
    render();
  }

  void on_theme() override {
    dirty_ = true;
    render();
  }

  bool on_back() override {
    if (day_ == 0) return false;
    show_day(0);
    render();
    return true;
  }

  // The answer for a day: {"re", "ok", "result": {"titles": [...], "channels", "starts", "groups"}}.
  void answer(JsonObjectConst message) {
    if (!asked_ || (message["re"] | 0u) != asked_) return;
    asked_ = 0;
    if (!(message["ok"] | false)) {
      week_ = false;   // no script film_stasera_day, or it failed: stay on today
      show_day(0);
    } else {
      day_guide_.read_lists(message["result"]);
      page_ = 0;
      page_at_ = now_;   // the day's first page gets its whole time from now
      dirty_ = true;
    }
    render();
  }

 private:
  // The next page of the day, else the next day (only today while the week is not there).
  void next() {
    page_at_ = now_;
    if (page_ + 1 < pages_) {
      ++page_;
      dirty_ = true;
    } else if (week_) {
      show_day((day_ + 1) % WEEK_DAYS);
    } else if (pages_ > 1) {
      page_ = 0;
      dirty_ = true;
    }
  }

  void show_day(int day) {
    day_ = day;
    page_ = 0;
    page_at_ = now_;
    asked_ = 0;
    dirty_ = true;
    day_guide_.films.clear();
    if (day == 0) return;
    JsonDocument request;
    request["ask"] = DAY_COMMAND;
    request["data"]["day"] = day;
    asked_ = tessera::send(plugin_, request.as<JsonObjectConst>());
    asked_at_ = now_;
    if (!asked_) {
      week_ = false;
      day_ = 0;
    }
  }

  void render() {
    const Guide &guide = day_ ? day_guide_ : guide_;
    const size_t first = day_ ? 0 : guide_.first(now_);
    const char *why = day_ ? (asked_ ? "waiting" : guide.films.empty() ? "none_day" : nullptr) : guide_.why(now_, first);
    if (!dirty_ && first == first_ && why == why_) return;
    dirty_ = false;
    first_ = first;
    why_ = why;
    // The pages: as many as the day's films (today: the ones still to come) need. A film that ends can take one away.
    const size_t left = why ? 0 : guide.films.size() - first;
    pages_ = std::max<size_t>(1, (left + rows_.size() - 1) / rows_.size());
    if (page_ >= pages_) page_ = 0;
    if (!page_at_) page_at_ = now_;
    const size_t start = first + page_ * rows_.size();
    // The line on top: the day, the message or that the guide is old, and which page this is.
    std::vector<std::string> parts;
    if (now_ && (week_ || day_)) parts.push_back(tessera::date_text(now_ + static_cast<uint32_t>(day_) * 86400));
    if (why)
      parts.push_back(plugin_->text(why));
    else if (day_ == 0 && guide_.stale)
      parts.push_back(plugin_->text("old"));
    if (pages_ > 1)
      parts.push_back(tessera::fill(tessera::fill(plugin_->text("page"), "page", std::to_string(page_ + 1)).c_str(),
                                    "pages", std::to_string(pages_)));
    std::string note;
    for (const auto &part : parts) note += (note.empty() ? "" : " · ") + part;
    ui::set_text(note_, note);
    ui::set_color(note_, theme::MUTED);
    const int when_w = why ? 0 : std::min(when_width(guide.films, start, rows_.size()), col_w_ / 3);
    for (size_t i = 0; i < rows_.size(); ++i) {
      if (why || start + i >= guide.films.size()) {
        rows_[i].place("", "", nullptr, 0, col_w_, line_);
      } else {
        const Film &f = guide.films[start + i];
        rows_[i].place(tessera::clock_text(f.at), f.title, &f, when_w, col_w_, line_);
      }
    }
  }

  FilmStasera *plugin_;
  lv_obj_t *note_{};
  int width_ = 0, line_ = 0, col_w_ = 0, day_ = 0;
  uint32_t now_ = 0, page_at_ = 0, asked_ = 0, asked_at_ = 0;   // page_at_: when the page on the glass came
  bool dirty_ = true, week_ = true;
  size_t first_ = 0, page_ = 0, pages_ = 1;
  const char *why_ = nullptr;
  std::vector<Row> rows_;
  Guide guide_, day_guide_;   // today (the tile's sensor), and the other day on the glass
};

void FilmStasera::setup() {
  // The id is the tile's id in the manifest; the core makes a FilmsTile for every card that shows one.
  add_tile("films", [this]() { return new FilmsTile(this); });
  add_card("movies", [this]() { return new FilmsCard(this); }, true);
}

void FilmStasera::on_message(JsonObjectConst message) {
  if (open_card_) open_card_->answer(message);
}

}  // namespace esphome::film_stasera
