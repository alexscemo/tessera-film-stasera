#include "film_stasera.h"
#include <algorithm>
#include <string>
#include <vector>

namespace esphome::film_stasera {

using tessera::Font;
namespace ui = tessera::ui;

// One film as Home Assistant's sensor has it: four lists of the same length, one entry per film (README.md).
struct Film {
  std::string title, channel, genre;
  uint32_t at = 0;   // when it starts, seconds since 1970
  int group = 0;     // the channel's group (1 to 4, 0 for none): the colour of the dot before its name
};

// A film that started less than this long ago is still on, so it stays on the tile. The guide has no end times.
static constexpr uint32_t ON_AIR_SECONDS = 150 * 60;
static constexpr int MAX_PER_COLUMN = 8;
static constexpr size_t MAX_FILMS = 20;

// The tile: a header (its name and the date), then one row per film as many as fit. A row has the start time on the left;
// beside it the title, and under the title the channel's name with a dot in the colour of its group (Rai, Mediaset...)
// and the genre. A film that has already started shows its time in the accent. A tile at least WIDE_MM wide is split into two columns that fill one after the other, so a whole evening
// fits on a page. The sensor changes twice a day, the clock every second: only a text that changes is set again.
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
      make_row(rows_[i], c.parent, col * (col_w_ + col_gap_), top_ + r * step);
    }
    if (has_head_) {
      head_ = ui::label(c.parent, Font::TITLE, theme::INK);
      head_note_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    }
  }

  void on_state(JsonObjectConst data) override {
    films_.clear();
    JsonObjectConst a = data["attributes"];
    JsonArrayConst titles = a["titles"].as<JsonArrayConst>();
    JsonArrayConst channels = a["channels"].as<JsonArrayConst>();
    JsonArrayConst starts = a["starts"].as<JsonArrayConst>();
    JsonArrayConst genres = a["genres"].as<JsonArrayConst>();
    JsonArrayConst groups = a["groups"].as<JsonArrayConst>();
    for (size_t i = 0; i < titles.size() && films_.size() < MAX_FILMS; ++i) {
      Film f;
      f.title = std::string(titles[i] | "");
      f.channel = std::string(channels[i] | "");
      f.genre = std::string(genres[i] | "");
      f.at = starts[i] | 0u;
      f.group = groups[i] | 0;
      if (f.at && !f.title.empty()) films_.push_back(std::move(f));
    }
    std::sort(films_.begin(), films_.end(), [](const Film &x, const Film &y) { return x.at < y.at; });
    // Before a person has chosen the sensor the tile gets {}; a sensor without a value says unavailable or unknown.
    has_entity_ = !data["state"].isNull();
    const std::string state = std::string(data["state"] | "");
    no_value_ = state == "unavailable" || state == "unknown";
    stale_ = a["stale"] | false;
    wait_ = std::string(data["wait"] | "");
  }

  void on_tick(uint32_t now) override {
    // What is still worth showing: not over yet. The list comes in order of start, so the first rows are the next ones.
    std::vector<const Film *> shown;
    for (const auto &f : films_)
      if (now && f.at + ON_AIR_SECONDS >= now) shown.push_back(&f);
    const char *why = !has_entity_ || wait_ == "wrong_entity" ? "choose_sensor"
                      : !wait_.empty() || no_value_ || !now   ? "waiting"
                      : shown.empty()                          ? "none"
                                                               : nullptr;
    // One time column as wide as the widest time on the tile, so every title starts at the same place.
    int when_w = 0;
    if (!why)
      for (size_t i = 0; i < rows_.size() && i < shown.size(); ++i)
        when_w = std::max(when_w, ui::text_width(tessera::clock_text(shown[i]->at), Font::TITLE));
    when_w += when_w ? ui::px(ui::large() ? 12 : 8) : 0;
    paint_head(now);
    for (size_t i = 0; i < rows_.size(); ++i) {
      if (i == 0 && why) {
        place(rows_[0], "", plugin_->text(why), nullptr, false, 0, width_);
      } else if (why || i >= shown.size()) {
        place(rows_[i], "", "", nullptr, false, 0, col_w_);
      } else {
        const Film &f = *shown[i];
        place(rows_[i], tessera::clock_text(f.at), f.title, &f, f.at <= now, when_w, col_w_);
      }
    }
  }

  void on_theme() override {
    if (head_) ui::set_color(head_, theme::INK);
    if (head_note_) ui::set_color(head_note_, theme::MUTED);
    for (auto &r : rows_) {
      ui::set_color(r.when, theme::INK);
      ui::set_color(r.title, theme::INK);
      ui::set_color(r.chan, theme::INK);
      ui::set_color(r.sub, theme::MUTED);
      paint_dot(r);
    }
  }

  void on_tap() override { tessera::open_card(plugin_->plugin_id(), "movies", entity_, tile_); }

 private:
  struct Row {
    lv_obj_t *when{}, *title{}, *dot{}, *chan{}, *sub{};
    int x = 0, y = 0, group = 0;
    bool on_air = false;
  };

  void make_row(Row &r, lv_obj_t *parent, int x, int y) {
    r.x = x;
    r.y = y;
    r.when = ui::label(parent, Font::TITLE, theme::INK);
    r.title = ui::label(parent, Font::BODY_LARGE, theme::INK);
    r.dot = ui::block(parent, theme::MARK_BLUE);
    r.chan = ui::label(parent, Font::BODY, theme::INK);
    r.sub = ui::label(parent, Font::BODY, theme::MUTED);
    lv_label_set_long_mode(r.title, LV_LABEL_LONG_CLIP);
    lv_label_set_long_mode(r.chan, LV_LABEL_LONG_CLIP);
    lv_label_set_long_mode(r.sub, LV_LABEL_LONG_CLIP);
  }

  // The colour of a group's dot: the four colours of the Tessera mark, the same in both looks. No group: no dot.
  static theme::Role group_role(int group) {
    switch (group) {
      case 1: return theme::MARK_BLUE;
      case 2: return theme::MARK_AMBER;
      case 3: return theme::MARK_GREEN;
      default: return theme::MARK_PURPLE;
    }
  }
  void paint_dot(Row &r) {
    lv_obj_set_style_opa(r.dot, r.group > 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    if (r.group > 0) lv_obj_set_style_bg_color(r.dot, ui::color(group_role(r.group)), 0);
  }

  static std::string fit_text(const std::string &text, int width, Font font) {
    if (text.empty() || width <= 0) return "";
    if (ui::text_width(text, font) <= width) return text;
    std::string fit = text;
    const std::string ellipsis = "...";
    while (!fit.empty() && ui::text_width(fit + ellipsis, font) > width) fit.pop_back();
    return fit.empty() ? ellipsis : fit + ellipsis;
  }

  // The time in a column of `when_w`, the title beside it and under the title the channel and the genre (`film`; null for
  // a message, which has only the title), in a row `row_w` wide. A message takes the title's place from the row's left
  // edge. Nothing to show: everything empty.
  void place(Row &r, const std::string &when, const std::string &title, const Film *film, bool on_air, int when_w,
             int row_w) {
    r.on_air = on_air;
    ui::set_text(r.when, fit_text(when, std::max(1, when_w), Font::TITLE));
    ui::set_color(r.when, theme::INK);
    const int tw = when.empty() ? 0 : when_w;
    lv_obj_set_size(r.when, std::max(1, tw), ui::line_height(Font::TITLE));
    lv_obj_set_pos(r.when, r.x, r.y + (line_ - ui::line_height(Font::TITLE)) / 2);
    const int w = std::max(1, row_w - tw);
    const std::string title_fit = fit_text(title, w, Font::BODY_LARGE);
    ui::set_text(r.title, title_fit);
    lv_obj_set_size(r.title, w, ui::line_height(Font::BODY_LARGE));
    lv_obj_set_pos(r.title, r.x + tw, r.y + (line_ - ui::line_height(Font::BODY_LARGE)) / 2);
    lv_label_set_long_mode(r.title, LV_LABEL_LONG_CLIP);
    // The second line: [dot] Channel . Genre
    const int lh = ui::line_height(Font::BODY), y2 = r.y + line_;
    const std::string channel = film ? film->channel : std::string();
    const std::string genre = film && !film->genre.empty() ? "\u00B7 " + film->genre : std::string();
    r.group = film ? film->group : 0;
    const int d = std::max(ui::px(7), lh * 6 / 10);
    const int dot_w = r.group > 0 ? d + ui::px(6) : 0;
    paint_dot(r);
    lv_obj_set_size(r.dot, std::max(1, d), std::max(1, d));
    lv_obj_set_pos(r.dot, r.x + tw, y2 + (lh - d) / 2);
    const int chan_w = std::min(w - dot_w, channel.empty() ? 0 : ui::text_width(channel, Font::BODY) + ui::px(2));
    const std::string chan = fit_text(channel, std::max(1, chan_w), Font::BODY);
    ui::set_text(r.chan, chan);
    lv_obj_set_size(r.chan, std::max(1, chan_w), lh);
    lv_obj_set_pos(r.chan, r.x + tw + dot_w, y2);
    lv_label_set_long_mode(r.chan, LV_LABEL_LONG_CLIP);
    const int gap_x = chan_w > 0 ? ui::px(6) : 0;
    const int gx = std::min(w, dot_w + chan_w + gap_x);
    const std::string sub = fit_text(genre, std::max(1, w - gx), Font::BODY);
    ui::set_text(r.sub, sub);
    lv_obj_set_size(r.sub, std::max(1, w - gx), lh);
    lv_obj_set_pos(r.sub, r.x + tw + gx, y2);
    lv_label_set_long_mode(r.sub, LV_LABEL_LONG_CLIP);
  }

  // The tile's name on the left; on the right the date, or that the guide is old.
  void paint_head(uint32_t now) {
    if (!head_) return;
    const std::string note = stale_ ? plugin_->text("old") : now ? tessera::date_text(now) : std::string();
    const int note_w = note.empty() ? 0 : ui::text_width(note, Font::BODY) + ui::px(2);
    const std::string title = name_.empty() ? plugin_->text("header") : name_;
    ui::set_text(head_, title);
    lv_obj_set_size(head_, std::max(1, width_ - note_w - (note_w ? ui::px(8) : 0)), ui::line_height(Font::TITLE));
    lv_obj_set_pos(head_, 0, 0);
    ui::set_text(head_note_, note);
    lv_obj_set_size(head_note_, std::max(1, note_w), ui::line_height(Font::BODY));
    lv_obj_set_pos(head_note_, width_ - note_w, (ui::line_height(Font::TITLE) - ui::line_height(Font::BODY)) / 2);
    lv_obj_set_style_text_align(head_note_, LV_TEXT_ALIGN_RIGHT, 0);
  }

  const tessera::Plugin *plugin_;
  int width_ = 0, height_ = 0, gap_ = 0, line_ = 0, row_h_ = 0, top_ = 0;
  int columns_ = 1, col_gap_ = 0, col_w_ = 0, per_column_ = 1;
  int tile_ = -1;
  bool has_head_ = false, has_entity_ = false, no_value_ = false, stale_ = false;
  std::string name_, entity_, wait_;
  std::vector<Row> rows_;
  lv_obj_t *head_{}, *head_note_{};
  std::vector<Film> films_;
};

class FilmsCard : public tessera::Card {
 public:
  explicit FilmsCard(const tessera::Plugin *plugin) : plugin_(plugin) {}

  void open(const tessera::CardContext &c) override {
    width_ = c.width;
    const int gap = ui::px(ui::large() ? 8 : 5);
    line_ = std::max(ui::line_height(Font::TITLE), ui::line_height(Font::BODY_LARGE));
    step_ = line_ + ui::line_height(Font::BODY) + gap;
    note_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    lv_obj_set_size(note_, width_, ui::line_height(Font::BODY));
    lv_obj_set_pos(note_, 0, 0);
    ui::set_text(note_, plugin_->text("waiting"));

    list_ = lv_obj_create(c.parent);
    const int list_y = ui::line_height(Font::BODY) + gap;
    lv_obj_set_size(list_, width_, std::max(1, c.height - list_y));
    lv_obj_set_pos(list_, 0, list_y);
    lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(list_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(list_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list_, 0, LV_PART_MAIN);

    rows_.resize(MAX_FILMS);
    for (auto &row : rows_) {
      row.when = ui::label(list_, Font::TITLE, theme::INK);
      row.title = ui::label(list_, Font::BODY_LARGE, theme::INK);
      row.dot = ui::block(list_, theme::MARK_BLUE);
      row.channel = ui::label(list_, Font::BODY, theme::INK);
      row.genre = ui::label(list_, Font::BODY, theme::MUTED);
      lv_label_set_long_mode(row.title, LV_LABEL_LONG_CLIP);
      lv_label_set_long_mode(row.channel, LV_LABEL_LONG_CLIP);
      lv_label_set_long_mode(row.genre, LV_LABEL_LONG_CLIP);
      set_row_visible(row, false);
    }
  }

  void on_state(JsonObjectConst data) override {
    films_.clear();
    JsonObjectConst attributes = data["attributes"];
    JsonArrayConst titles = attributes["titles"].as<JsonArrayConst>();
    JsonArrayConst channels = attributes["channels"].as<JsonArrayConst>();
    JsonArrayConst starts = attributes["starts"].as<JsonArrayConst>();
    JsonArrayConst genres = attributes["genres"].as<JsonArrayConst>();
    JsonArrayConst groups = attributes["groups"].as<JsonArrayConst>();
    for (size_t i = 0; i < titles.size() && films_.size() < MAX_FILMS; ++i) {
      Film film;
      film.title = std::string(titles[i] | "");
      film.channel = std::string(channels[i] | "");
      film.genre = std::string(genres[i] | "");
      film.at = starts[i] | 0u;
      film.group = groups[i] | 0;
      if (film.at && !film.title.empty()) films_.push_back(std::move(film));
    }
    std::sort(films_.begin(), films_.end(), [](const Film &a, const Film &b) { return a.at < b.at; });
    has_entity_ = !data["state"].isNull();
    const std::string state = std::string(data["state"] | "");
    no_value_ = state == "unavailable" || state == "unknown";
    stale_ = attributes["stale"] | false;
    wait_ = std::string(data["wait"] | "");
    render(now_);
  }

  void on_tick(uint32_t now) override {
    now_ = now;
    render(now_);
  }

  void on_theme() override {
    for (auto &row : rows_) ui::set_color(row.title, theme::INK);
    render(now_);
  }

 private:
  struct Row {
    lv_obj_t *when{}, *title{}, *dot{}, *channel{}, *genre{};
    int group = 0;
  };

  static theme::Role group_role(int group) {
    switch (group) {
      case 1: return theme::MARK_BLUE;
      case 2: return theme::MARK_AMBER;
      case 3: return theme::MARK_GREEN;
      default: return theme::MARK_PURPLE;
    }
  }

  void set_row_visible(Row &row, bool visible) {
    for (lv_obj_t *object : {row.when, row.title, row.dot, row.channel, row.genre}) {
      if (visible)
        lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
      else {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(object, 0, 0);
      }
    }
  }

  static std::string fit_text(const std::string &text, int width, Font font) {
    if (text.empty() || width <= 0) return "";
    if (ui::text_width(text, font) <= width) return text;
    std::string fit = text;
    const std::string ellipsis = "...";
    while (!fit.empty() && ui::text_width(fit + ellipsis, font) > width) fit.pop_back();
    return fit.empty() ? ellipsis : fit + ellipsis;
  }

  void place(Row &row, const Film &film, size_t index, int when_width) {
    const int y = static_cast<int>(index) * step_;
    const std::string when = tessera::clock_text(film.at);
    ui::set_text(row.when, fit_text(when, when_width, Font::TITLE));
    ui::set_color(row.when, theme::INK);
    lv_obj_set_size(row.when, when_width, ui::line_height(Font::TITLE));
    lv_obj_set_pos(row.when, 0, y + (line_ - ui::line_height(Font::TITLE)) / 2);
    const int title_width = std::max(1, width_ - when_width);
    ui::set_text(row.title, fit_text(film.title, title_width, Font::BODY_LARGE));
    ui::set_color(row.title, theme::INK);
    lv_obj_set_size(row.title, title_width, ui::line_height(Font::BODY_LARGE));
    lv_obj_set_pos(row.title, when_width, y + (line_ - ui::line_height(Font::BODY_LARGE)) / 2);

    const int body_line = ui::line_height(Font::BODY), second_y = y + line_;
    row.group = film.group;
    const int dot_size = std::min(title_width, std::max(ui::px(7), body_line * 6 / 10));
    const int dot_width = row.group > 0 ? std::min(title_width, dot_size + ui::px(6)) : 0;
    lv_obj_set_style_opa(row.dot, row.group > 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    if (row.group > 0) lv_obj_set_style_bg_color(row.dot, ui::color(group_role(row.group)), 0);
    lv_obj_set_size(row.dot, std::max(1, dot_size), std::max(1, dot_size));
    lv_obj_set_pos(row.dot, when_width, second_y + (body_line - dot_size) / 2);
    const int channel_width = std::min(std::max(0, title_width - dot_width),
                                       film.channel.empty() ? 0 : ui::text_width(film.channel, Font::BODY) + ui::px(2));
    ui::set_text(row.channel, fit_text(film.channel, channel_width, Font::BODY));
    ui::set_color(row.channel, theme::INK);
    lv_obj_set_size(row.channel, std::max(1, channel_width), body_line);
    lv_obj_set_pos(row.channel, std::min(width_ - 1, when_width + dot_width), second_y);
    const int genre_x = std::min(title_width, dot_width + (channel_width > 0 ? channel_width + ui::px(6) : 0));
    const std::string genre = film.genre.empty() ? std::string() : "\u00B7 " + film.genre;
    const int genre_width = title_width - genre_x;
    ui::set_text(row.genre, fit_text(genre, genre_width, Font::BODY));
    ui::set_color(row.genre, theme::MUTED);
    lv_obj_set_size(row.genre, std::max(1, genre_width), body_line);
    lv_obj_set_pos(row.genre, std::min(width_ - 1, when_width + genre_x), second_y);
  }

  void render(uint32_t now) {
    std::vector<const Film *> shown;
    for (const auto &film : films_)
      if (now && film.at + ON_AIR_SECONDS >= now) shown.push_back(&film);
    const char *message = !has_entity_ || wait_ == "wrong_entity" ? "choose_sensor"
                          : !wait_.empty() || no_value_ || !now   ? "waiting"
                          : shown.empty()                          ? "none"
                          : stale_                                 ? "old"
                                                                   : nullptr;
    ui::set_text(note_, message ? plugin_->text(message) : "");
    ui::set_color(note_, theme::MUTED);
    int when_width = 0;
    if (!message)
      for (const Film *film : shown)
        when_width = std::max(when_width, ui::text_width(tessera::clock_text(film->at), Font::TITLE));
    when_width += when_width ? ui::px(ui::large() ? 12 : 8) : 0;
    when_width = std::min(when_width, width_ / 3);
    for (size_t i = 0; i < rows_.size(); ++i) {
      if (message || i >= shown.size())
        set_row_visible(rows_[i], false);
      else {
        set_row_visible(rows_[i], true);
        place(rows_[i], *shown[i], i, when_width);
      }
    }
  }

  const tessera::Plugin *plugin_;
  lv_obj_t *note_{}, *list_{};
  int width_ = 0, line_ = 0, step_ = 0;
  uint32_t now_ = 0;
  bool has_entity_ = false, no_value_ = false, stale_ = false;
  std::string wait_;
  std::vector<Film> films_;
  std::vector<Row> rows_;
};

void FilmStasera::setup() {
  // The id is the tile's id in the manifest; the core makes a FilmsTile for every card that shows one.
  add_tile("films", [this]() { return new FilmsTile(this); });
  add_card("movies", [this]() { return new FilmsCard(this); }, true);
}

}  // namespace esphome::film_stasera
