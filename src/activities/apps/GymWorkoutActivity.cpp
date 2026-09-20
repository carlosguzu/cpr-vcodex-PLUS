#include "GymWorkoutActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "GymTrackerStore.h"
#include "activities/util/ConfirmationActivity.h"
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

  // Find previous record from history (excluding today's active session)
  hasLastRecord = GYM_TRACKER.getLastLoggedSet(exerciseId, lastWeight, lastReps);
  if (hasLastRecord) {
    currentWeight = lastWeight;
    currentReps = lastReps;
  } else {
    currentWeight = defaultWeight;
    currentReps = defaultReps;
  }

  // Check all-time Personal Record
  hasPrRecord = GYM_TRACKER.getPersonalRecord(exerciseId, prWeight, prReps);

  // Calculate current set from already completed sets today
  const int doneToday = GYM_TRACKER.getCompletedSetsCount(exerciseId);
  currentSet = doneToday + 1;

  selectedField = 0;  // Start with Weight selected
  isTimerRunning = false;
  timerDurationMs = 90000;
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

void GymWorkoutActivity::adjustTimer(const int deltaSeconds) {
  if (!isTimerRunning) return;
  const uint32_t deltaMs = static_cast<uint32_t>(std::abs(deltaSeconds)) * 1000U;
  const uint32_t elapsed = millis() - timerStartMs;
  if (deltaSeconds > 0) {
    timerDurationMs += deltaMs;
  } else {
    if (elapsed + deltaMs + 5000U < timerDurationMs) {
      timerDurationMs -= deltaMs;
    } else {
      // Keep at least 5s remaining
      timerDurationMs = elapsed + 5000U;
    }
  }
  lastTimerRenderSec = 0;
  requestUpdate();
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

  // Advance to next set and start rest timer (90s default)
  currentSet++;
  isTimerRunning = true;
  timerDurationMs = 90000;
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

    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm) ||
        mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      isTimerRunning = false;
      requestUpdate();
      return;
    }

    // Left decreases timer by 15s, Right increases timer by 15s
    if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
      adjustTimer(-15);
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      adjustTimer(15);
      return;
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    // If workout is in progress, ask for confirmation to avoid accidental exit
    if (currentSet > 1 && currentSet <= totalSets) {
      startActivityForResult(
          std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_GYM_EXIT_CONFIRM), ""),
          [this](const ActivityResult& result) {
            if (!result.isCancelled) {
              finish();
            }
          });
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
    renderer.drawCenteredText(UI_12_FONT_ID, currentY + 40, tr(STR_GYM_EXERCISE_DONE), true, EpdFontFamily::BOLD);

    char summaryBuf[80];
    snprintf(summaryBuf, sizeof(summaryBuf), tr(STR_GYM_SETS_LOGGED), totalSets);
    renderer.drawCenteredText(UI_10_FONT_ID, currentY + 80, summaryBuf, true, EpdFontFamily::REGULAR);

    const int boxW = pageWidth - sidePadding * 2;
    renderer.drawRect(sidePadding, currentY + 120, boxW, 140, true);

    const auto* exSession = GYM_TRACKER.getExerciseSession(exerciseId);
    int logY = currentY + 135;
    if (exSession) {
      for (const auto& set : exSession->sets) {
        char setLine[64];
        snprintf(setLine, sizeof(setLine), tr(STR_GYM_SET_LINE_OK), set.setNumber, set.weight, set.reps);
        renderer.drawText(UI_10_FONT_ID, sidePadding + 16, logY, setLine, true, EpdFontFamily::BOLD);
        logY += 28;
      }
    }

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_GYM_FINISH), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // 1. Target & Previous Record Card
  const int cardW = pageWidth - sidePadding * 2;
  const int cardH = 65;
  renderer.drawRect(sidePadding, currentY, cardW, cardH, true);

  char setBadge[40];
  snprintf(setBadge, sizeof(setBadge), tr(STR_GYM_SET_N_OF_N), currentSet, totalSets);
  renderer.drawText(UI_10_FONT_ID, sidePadding + 10, currentY + 10, setBadge, true, EpdFontFamily::BOLD);

  char prevBadge[80];
  if (hasLastRecord) {
    snprintf(prevBadge, sizeof(prevBadge), tr(STR_GYM_LAST_RECORD), lastWeight, lastReps);
  } else {
    snprintf(prevBadge, sizeof(prevBadge), tr(STR_GYM_TARGET_DETAIL), totalSets, defaultReps, defaultWeight);
  }
  renderer.drawText(SMALL_FONT_ID, sidePadding + 10, currentY + 36, prevBadge, true, EpdFontFamily::REGULAR);

  currentY += cardH + 12;

  // 2. Interactive Editing Boxes for WEIGHT and REPS
  const int halfW = (cardW - 12) / 2;

  // Weight Box
  const int weightX = sidePadding;
  const bool weightSelected = (selectedField == 0);
  const bool isPr = (hasPrRecord && currentWeight > prWeight);

  if (weightSelected) {
    renderer.fillRect(weightX, currentY, halfW, 70, true);
    std::string wLabel = std::string(tr(STR_GYM_WEIGHT_KG)) + " [>]";
    renderer.drawText(SMALL_FONT_ID, weightX + 8, currentY + 8, wLabel.c_str(), false, EpdFontFamily::BOLD);
    if (isPr) {
      renderer.drawText(SMALL_FONT_ID, weightX + halfW - 36, currentY + 8, tr(STR_GYM_PR_TAG), false, EpdFontFamily::BOLD);
    }
    char wBuf[32];
    snprintf(wBuf, sizeof(wBuf), "%.1f kg", currentWeight);
    renderer.drawText(UI_12_FONT_ID, weightX + 10, currentY + 32, wBuf, false, EpdFontFamily::BOLD);
  } else {
    renderer.drawRect(weightX, currentY, halfW, 70, true);
    renderer.drawText(SMALL_FONT_ID, weightX + 8, currentY + 8, tr(STR_GYM_WEIGHT_KG), true, EpdFontFamily::REGULAR);
    if (isPr) {
      renderer.drawText(SMALL_FONT_ID, weightX + halfW - 36, currentY + 8, tr(STR_GYM_PR_TAG), true, EpdFontFamily::BOLD);
    }
    char wBuf[32];
    snprintf(wBuf, sizeof(wBuf), "%.1f kg", currentWeight);
    renderer.drawText(UI_12_FONT_ID, weightX + 10, currentY + 32, wBuf, true, EpdFontFamily::BOLD);
  }

  // Reps Box
  const int repsX = weightX + halfW + 12;
  const bool repsSelected = (selectedField == 1);
  if (repsSelected) {
    renderer.fillRect(repsX, currentY, halfW, 70, true);
    std::string rLabel = std::string(tr(STR_GYM_REPS)) + " [>]";
    renderer.drawText(SMALL_FONT_ID, repsX + 8, currentY + 8, rLabel.c_str(), false, EpdFontFamily::BOLD);
    char rBuf[32];
    snprintf(rBuf, sizeof(rBuf), "%d reps", currentReps);
    renderer.drawText(UI_12_FONT_ID, repsX + 14, currentY + 32, rBuf, false, EpdFontFamily::BOLD);
  } else {
    renderer.drawRect(repsX, currentY, halfW, 70, true);
    renderer.drawText(SMALL_FONT_ID, repsX + 8, currentY + 8, tr(STR_GYM_REPS), true, EpdFontFamily::REGULAR);
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
    snprintf(timerText, sizeof(timerText), tr(STR_GYM_REST_COUNTDOWN), remSec / 60, remSec % 60);
    renderer.drawText(UI_10_FONT_ID, sidePadding + 10, currentY + 12, timerText, true, EpdFontFamily::BOLD);

    const int barTotalW = cardW - 160;
    const int barX = sidePadding + 140;
    renderer.drawRect(barX, currentY + 14, barTotalW, 14, true);
    const int totalSec = static_cast<int>(timerDurationMs / 1000U);
    const int fillW = totalSec > 0 ? (remSec * (barTotalW - 4)) / totalSec : 0;
    if (fillW > 0) {
      renderer.fillRect(barX + 2, currentY + 16, fillW, 10, true);
    }
    currentY += 42 + 10;
  }

  // 4. Completed Sets Table for Today
  renderer.drawText(UI_10_FONT_ID, sidePadding, currentY, tr(STR_GYM_COMPLETED_SETS), true, EpdFontFamily::BOLD);
  currentY += 24;

  const auto* exSession = GYM_TRACKER.getExerciseSession(exerciseId);
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
      snprintf(setLine, sizeof(setLine), tr(STR_GYM_SET_LINE_OK), s, sWeight, sReps);
      renderer.drawText(SMALL_FONT_ID, sidePadding + 8, currentY, setLine, true, EpdFontFamily::REGULAR);
    } else if (s == currentSet) {
      snprintf(setLine, sizeof(setLine), tr(STR_GYM_SET_IN_PROGRESS), s);
      renderer.drawText(SMALL_FONT_ID, sidePadding + 8, currentY, setLine, true, EpdFontFamily::BOLD);
    } else {
      snprintf(setLine, sizeof(setLine), tr(STR_GYM_SET_PENDING), s);
      renderer.drawText(SMALL_FONT_ID, sidePadding + 8, currentY, setLine, true, EpdFontFamily::REGULAR);
    }
    currentY += 22;
  }

  // Button Hints
  if (isTimerRunning) {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_GYM_SKIP_TIMER), "[-15s]", "[+15s]");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else {
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_GYM_LOG_BTN), "[-]", "[+]");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}
