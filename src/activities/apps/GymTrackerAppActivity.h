#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class GymTrackerAppActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

  void openSelectedEntry();
  int getTotalEntries() const;

 public:
  explicit GymTrackerAppActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GymTrackerApp", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
