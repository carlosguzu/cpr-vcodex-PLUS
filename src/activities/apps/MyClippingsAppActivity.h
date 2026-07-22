#pragma once

#include <string>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class MyClippingsAppActivity final : public Activity {
  struct ClippingEntry {
    std::string text;
    std::string location;
  };

  struct BookClippings {
    std::string title;
    std::string author;
    std::vector<ClippingEntry> clippings;
  };

  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  std::vector<BookClippings> books;
  bool showingDetail = false;
  int detailIndex = 0;
  int detailScrollOffset = 0;

  std::string currentBookTitle;

  void loadClippings();

 public:
  explicit MyClippingsAppActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string currentBookTitle = "")
      : Activity("MyClippingsApp", renderer, mappedInput), currentBookTitle(std::move(currentBookTitle)) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
