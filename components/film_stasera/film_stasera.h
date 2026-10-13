#pragma once
#include "esphome/core/component.h"
#include "esphome/components/smart_display/plugin_api.h"

namespace esphome::film_stasera {

class FilmsCard;

// The plugin: an ESPHome component and a tessera::Plugin. It registers its tile and card types in setup(), and hands
// the answers of Home Assistant (a day of the week) to the card that is open.
class FilmStasera : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void on_message(JsonObjectConst message) override;

  FilmsCard *open_card_ = nullptr;  // the card that is open, to hand it its answer
};

}  // namespace esphome::film_stasera
