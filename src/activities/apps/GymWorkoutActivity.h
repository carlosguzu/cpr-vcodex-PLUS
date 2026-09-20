#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"
#include <string>

class GymWorkoutActivity final : public Activity {
  ButtonNavigator buttonNavigator;

  std::string routineId;
  std::string routineName;
  std::string exerciseId;
  std::string exerciseName;
  int totalSets = 3;
  int defaultReps = 10;
  float defaultWeight = 50.0f;

  int currentSet = 1;
  float currentWeight = 50.0f;
  int currentReps = 10;

  bool hasLastRecord = false;
  float lastWeight = 0.0f;
  int lastReps = 0;

  bool hasPrRecord = false;
  float prWeight = 0.0f;
  int prReps = 0;

  int selectedField = 0; // 0 = Weight, 1 = Reps

  // Rest timer
  bool isTimerRunning = false;
  uint32_t timerStartMs = 0;
  uint32_t timerDurationMs = 90000; // 90 seconds
  uint32_t lastTimerRenderSec = 0;

  void logCurrentSet();
  void adjustSelectedField(int delta);
  void adjustTimer(int deltaSeconds);
  int getRemainingTimerSeconds() const;

 public:
  explicit GymWorkoutActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                              std::string routineId, std::string routineName,
                              std::string exerciseId, std::string exerciseName,
                              int totalSets, int defaultReps, float defaultWeight);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool skipLoopDelay() override { return isTimerRunning; }
};
