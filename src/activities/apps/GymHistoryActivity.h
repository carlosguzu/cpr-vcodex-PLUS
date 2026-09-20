#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class GymHistoryActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  bool viewingDetail = false;
  int detailExerciseIndex = 0;

  void openSelectedHistoryEntry();

 public:
  explicit GymHistoryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GymHistory", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
