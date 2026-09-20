#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"
#include <string>

class GymRoutineOverviewActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  std::string routineId;
  std::string routineName;
  int selectedIndex = 0;

  void openSelectedExercise();

 public:
  explicit GymRoutineOverviewActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                      std::string routineId, std::string routineName)
      : Activity("GymRoutineOverview", renderer, mappedInput),
        routineId(std::move(routineId)),
        routineName(std::move(routineName)) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
