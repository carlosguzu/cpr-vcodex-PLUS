#include "GymWorkoutActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "GymTrackerStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/HeaderDateUtils.h"

GymWorkoutActivity::GymWorkoutActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                       std::string routineId, std::string routineName,
                                       std::string exerciseId, std::string exerciseName,
                                       const int totalSets, const int defaultReps, const float defaultWeight)
    : Activity("GymWorkout", renderer, mappedInput),
      routineId(std::move(routineId)),
      routineName(std::move(routineName)),
      exerciseId(std::move(exerciseId)),
      exerciseName(std::move(exerciseName)),
      totalSets(totalSets),
      defaultReps(defaultReps),
      defaultWeight(defaultWeight),
      currentWeight(defaultWeight),
      currentReps(defaultReps) {}

void GymWorkoutActivity::onEnter() {
  Activity::onEnter();

  // Find previous record from history
  hasLastRecord = GYM_TRACKER.getLastLoggedSet(exerciseId, lastWeight, lastReps);
  if (hasLastRecord) {
    currentWeight = lastWeight;
    currentReps = lastReps;
  } else {
    currentWeight = defaultWeight;
    currentReps = defaultReps;
  }

  // Calculate current set from already completed sets today
  const int doneToday = GYM_TRACKER.getCompletedSetsCount(exerciseId);
  currentSet = doneToday + 1;

  selectedField = 0;  // Start with Weight selected
  isTimerRunning = false;
  requestUpdate();
}

int GymWorkoutActivity::getRemainingTimerSeconds() const {
  if (!isTimerRunning) return 0;
  const uint32_t elapsed = millis() - timerStartMs;
  if (elapsed >= timerDurationMs) {
    return 0;
  }
  return static_cast<int>((timerDurationMs - elapsed) / 1000U);
}

void GymWorkoutActivity::adjustSelectedField(const int delta) {
  if (selectedField == 0) {
    // Weight (step: 2.5 kg)
    currentWeight = std::clamp(currentWeight + (delta * 2.5f), 0.0f, 500.0f);
  } else {
    // Reps (step: 1)
    currentReps = std::clamp(currentReps + delta, 1, 100);
  }
  requestUpdate();
}

void GymWorkoutActivity::logCurrentSet() {
  GYM_TRACKER.logSet(exerciseId, exerciseName, currentSet, currentWeight, currentReps);

  if (currentSet >= totalSets) {
    // Workout for this exercise complete!
    isTimerRunning = false;
    currentSet = totalSets + 1;
    requestUpdate();
    return;
  }

  // Advance to next set and start rest timer (90s)
  currentSet++;
  isTimerRunning = true;
  timerStartMs = millis();
  lastTimerRenderSec = 90;
  requestUpdate();
}

void GymWorkoutActivity::loop() {
  // Check rest timer progress
  if (isTimerRunning) {
    const int remSec = getRemainingTimerSeconds();
    if (remSec == 0) {
      isTimerRunning = false;
      requestUpdate();
    } else if (static_cast<uint32_t>(remSec) != lastTimerRenderSec) {
      lastTimerRenderSec = remSec;
      requestUpdate();
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (isTimerRunning) {
      isTimerRunning = false;
      requestUpdate();
      return;
    }
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (currentSet > totalSets) {
      finish();
      return;
    }
    logCurrentSet();
    return;
  }

  // Toggle field between Weight and Reps using Up / Down
  buttonNavigator.onRelease({MappedInputManager::Button::Up, MappedInputManager::Button::Down}, [this] {
    selectedField = (selectedField == 0) ? 1 : 0;
    requestUpdate();
  });

  // Adjust current field with Left (-) and Right (+)
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left}, [this] { adjustSelectedField(-1); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right}, [this] { adjustSelectedField(1); });
}

void GymWorkoutActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int sidePadding = metrics.contentSidePadding;

  // Header
  HeaderDateUtils::drawHeaderWithDate(renderer, exerciseName.c_str(), routineName.c_str());

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  int currentY = contentTop;

  if (currentSet > totalSets) {
    // All sets complete celebration screen
    renderer.drawCenteredText(UI_12_FONT_ID, currentY + 40, "¡Ejercicio Completado! 🎉", true, EpdFontFamily::BOLD);

    char summaryBuf[80];
    snprintf(summaryBuf, sizeof(summaryBuf), "%d series registradas con exito", totalSets);
    renderer.drawCenteredText(UI_10_FONT_ID, currentY + 80, summaryBuf, true, EpdFontFamily::REGULAR);

    const int boxW = pageWidth - sidePadding * 2;
    renderer.drawRect(sidePadding, currentY + 120, boxW, 140, true);

    const auto* exSession = GYM_TRACKER.getOrCreateExerciseSession(exerciseId, exerciseName);
    int logY = currentY + 135;
    if (exSession) {
      for (const auto& set : exSession->sets) {
        char setLine[64];
        snprintf(setLine, sizeof(setLine), "Serie %d:  %.1f kg  x  %d reps  [OK]", set.setNumber, set.weight, set.reps);
        renderer.drawText(UI_10_FONT_ID, sidePadding + 16, logY, setLine, true, EpdFontFamily::BOLD);
        logY += 28;
      }
    }

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Finalizar", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // 1. Target & Previous Record Card
  const int cardW = pageWidth - sidePadding * 2;
  const int cardH = 65;
  renderer.drawRect(sidePadding, currentY, cardW, cardH, true);

  char setBadge[40];
  snprintf(setBadge, sizeof(setBadge), "SERIE %d DE %d", currentSet, totalSets);
  renderer.drawText(UI_10_FONT_ID, sidePadding + 10, currentY + 10, setBadge, true, EpdFontFamily::BOLD);

  char prevBadge[64];
  if (hasLastRecord) {
    snprintf(prevBadge, sizeof(prevBadge), "Ultimo: %.1f kg x %d reps", lastWeight, lastReps);
  } else {
    snprintf(prevBadge, sizeof(prevBadge), "Objetivo: %.1f kg x %d reps", defaultWeight, defaultReps);
  }
  renderer.drawText(SMALL_FONT_ID, sidePadding + 10, currentY + 36, prevBadge, true, EpdFontFamily::REGULAR);

  currentY += cardH + 12;

  // 2. Interactive Editing Boxes for WEIGHT and REPS
  const int halfW = (cardW - 12) / 2;

  // Weight Box
  const int weightX = sidePadding;
  const bool weightSelected = (selectedField == 0);
  if (weightSelected) {
    renderer.fillRect(weightX, currentY, halfW, 70, true);
    renderer.drawText(SMALL_FONT_ID, weightX + 8, currentY + 8, "PESO (KG) [>]", false, EpdFontFamily::BOLD);
    char wBuf[32];
    snprintf(wBuf, sizeof(wBuf), "%.1f kg", currentWeight);
    renderer.drawText(UI_12_FONT_ID, weightX + 10, currentY + 32, wBuf, false, EpdFontFamily::BOLD);
  } else {
    renderer.drawRect(weightX, currentY, halfW, 70, true);
    renderer.drawText(SMALL_FONT_ID, weightX + 8, currentY + 8, "PESO (KG)", true, EpdFontFamily::REGULAR);
    char wBuf[32];
    snprintf(wBuf, sizeof(wBuf), "%.1f kg", currentWeight);
    renderer.drawText(UI_12_FONT_ID, weightX + 10, currentY + 32, wBuf, true, EpdFontFamily::BOLD);
  }

  // Reps Box
  const int repsX = weightX + halfW + 12;
  const bool repsSelected = (selectedField == 1);
  if (repsSelected) {
    renderer.fillRect(repsX, currentY, halfW, 70, true);
    renderer.drawText(SMALL_FONT_ID, repsX + 8, currentY + 8, "REPS [>]", false, EpdFontFamily::BOLD);
    char rBuf[32];
    snprintf(rBuf, sizeof(rBuf), "%d reps", currentReps);
    renderer.drawText(UI_12_FONT_ID, repsX + 14, currentY + 32, rBuf, false, EpdFontFamily::BOLD);
  } else {
    renderer.drawRect(repsX, currentY, halfW, 70, true);
    renderer.drawText(SMALL_FONT_ID, repsX + 8, currentY + 8, "REPS", true, EpdFontFamily::REGULAR);
    char rBuf[32];
    snprintf(rBuf, sizeof(rBuf), "%d reps", currentReps);
    renderer.drawText(UI_12_FONT_ID, repsX + 14, currentY + 32, rBuf, true, EpdFontFamily::BOLD);
  }

  currentY += 70 + 10;

  // 3. Rest Timer Status / Progress Bar
  if (isTimerRunning) {
    const int remSec = getRemainingTimerSeconds();
    renderer.drawRect(sidePadding, currentY, cardW, 42, true);

    char timerText[64];
    snprintf(timerText, sizeof(timerText), "Descanso: %02d:%02d", remSec / 60, remSec % 60);
    renderer.drawText(UI_10_FONT_ID, sidePadding + 10, currentY + 12, timerText, true, EpdFontFamily::BOLD);

    const int barTotalW = cardW - 160;
    const int barX = sidePadding + 140;
    renderer.drawRect(barX, currentY + 14, barTotalW, 14, true);
    const int fillW = (remSec * (barTotalW - 4)) / 90;
    if (fillW > 0) {
      renderer.fillRect(barX + 2, currentY + 16, fillW, 10, true);
    }
    currentY += 42 + 10;
  }

  // 4. Completed Sets Table for Today
  renderer.drawText(UI_10_FONT_ID, sidePadding, currentY, "Series completadas hoy:", true, EpdFontFamily::BOLD);
  currentY += 24;

  const auto* exSession = GYM_TRACKER.getOrCreateExerciseSession(exerciseId, exerciseName);
  for (int s = 1; s <= totalSets; ++s) {
    char setLine[64];
    bool setDone = false;
    float sWeight = currentWeight;
    int sReps = currentReps;

    if (exSession) {
      for (const auto& rec : exSession->sets) {
        if (rec.setNumber == s && rec.completed) {
          setDone = true;
          sWeight = rec.weight;
          sReps = rec.reps;
          break;
        }
      }
    }

    if (setDone) {
      snprintf(setLine, sizeof(setLine), "Serie %d:  %.1f kg  x  %d reps  [OK]", s, sWeight, sReps);
      renderer.drawText(SMALL_FONT_ID, sidePadding + 8, currentY, setLine, true, EpdFontFamily::REGULAR);
    } else if (s == currentSet) {
      snprintf(setLine, sizeof(setLine), "Serie %d:  [En curso...]", s);
      renderer.drawText(SMALL_FONT_ID, sidePadding + 8, currentY, setLine, true, EpdFontFamily::BOLD);
    } else {
      snprintf(setLine, sizeof(setLine), "Serie %d:  ---", s);
      renderer.drawText(SMALL_FONT_ID, sidePadding + 8, currentY, setLine, true, EpdFontFamily::REGULAR);
    }
    currentY += 22;
  }

  // Button Hints
  const char* confirmLabel = isTimerRunning ? "Saltar Reloj" : "Registrar ✓";
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, "[-]", "[+]");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
