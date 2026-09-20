#include "GymRoutineOverviewActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "GymTrackerStore.h"
#include "GymWorkoutActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/HeaderDateUtils.h"

void GymRoutineOverviewActivity::onEnter() {
  Activity::onEnter();
  GYM_TRACKER.startRoutineSession(routineId, routineName);
  cachedRoutine = GYM_TRACKER.findRoutine(routineId);
  const int total = cachedRoutine ? static_cast<int>(cachedRoutine->exercises.size()) : 0;

  // Auto-focus the first incomplete exercise if current is already completed
  if (cachedRoutine && total > 0) {
    if (GYM_TRACKER.isExerciseCompleted(cachedRoutine->exercises[selectedIndex].id,
                                        cachedRoutine->exercises[selectedIndex].defaultSets)) {
      for (int i = 0; i < total; ++i) {
        if (!GYM_TRACKER.isExerciseCompleted(cachedRoutine->exercises[i].id,
                                            cachedRoutine->exercises[i].defaultSets)) {
          selectedIndex = i;
          break;
        }
      }
    }
  }

  selectedIndex = std::clamp(selectedIndex, 0, std::max(0, total - 1));
  requestUpdate();
}

void GymRoutineOverviewActivity::openSelectedExercise() {
  if (!cachedRoutine) {
    cachedRoutine = GYM_TRACKER.findRoutine(routineId);
  }
  if (!cachedRoutine || selectedIndex < 0 || selectedIndex >= static_cast<int>(cachedRoutine->exercises.size())) {
    return;
  }

  const auto& ex = cachedRoutine->exercises[selectedIndex];
  auto activity = std::make_unique<GymWorkoutActivity>(renderer, mappedInput, routineId, routineName, ex.id, ex.name,
                                                      ex.defaultSets, ex.defaultReps, ex.defaultWeight);

  startActivityForResult(std::move(activity), [this](const ActivityResult&) {
    if (cachedRoutine) {
      const int total = static_cast<int>(cachedRoutine->exercises.size());
      // Auto-advance to next incomplete exercise if current exercise is completed
      if (GYM_TRACKER.isExerciseCompleted(cachedRoutine->exercises[selectedIndex].id,
                                          cachedRoutine->exercises[selectedIndex].defaultSets)) {
        for (int i = 0; i < total; ++i) {
          const int nextIdx = (selectedIndex + 1 + i) % total;
          if (!GYM_TRACKER.isExerciseCompleted(cachedRoutine->exercises[nextIdx].id,
                                              cachedRoutine->exercises[nextIdx].defaultSets)) {
            selectedIndex = nextIdx;
            break;
          }
        }
      }
    }
    requestUpdate();
  });
}

void GymRoutineOverviewActivity::loop() {
  const int total = cachedRoutine ? static_cast<int>(cachedRoutine->exercises.size()) : 0;

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    openSelectedExercise();
    return;
  }

  buttonNavigator.onNextPress([this, total] {
    if (total <= 0) return;
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, total);
    requestUpdate();
  });

  buttonNavigator.onPreviousPress([this, total] {
    if (total <= 0) return;
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, total);
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this, total] {
    if (total <= 0) return;
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, total);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, total] {
    if (total <= 0) return;
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, total);
    requestUpdate();
  });
}

void GymRoutineOverviewActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  const auto* routine = cachedRoutine ? cachedRoutine : GYM_TRACKER.findRoutine(routineId);
  if (!routine || routine->exercises.empty()) {
    HeaderDateUtils::drawHeaderWithDate(renderer, routineName.c_str(), tr(STR_GYM_NO_ROUTINES));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // Header & Completion calculation
  int completedCount = 0;
  for (const auto& ex : routine->exercises) {
    if (GYM_TRACKER.isExerciseCompleted(ex.id, ex.defaultSets)) {
      completedCount++;
    }
  }

  char subHeader[64];
  snprintf(subHeader, sizeof(subHeader), tr(STR_GYM_COMPLETED_OF), completedCount,
           static_cast<int>(routine->exercises.size()));
  HeaderDateUtils::drawHeaderWithDate(renderer, routineName.c_str(), subHeader);

  // Visual Progress Bar under header
  const int barX = metrics.contentSidePadding;
  const int barW = pageWidth - metrics.contentSidePadding * 2;
  const int barH = 5;
  const int barY = metrics.topPadding + metrics.headerHeight + 2;
  renderer.drawRect(barX, barY, barW, barH, true);
  if (!routine->exercises.empty() && completedCount > 0) {
    const int fillW = (completedCount * barW) / static_cast<int>(routine->exercises.size());
    renderer.fillRect(barX, barY, fillW, barH, true);
  }

  // Content area: split into list + bottom preview card
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing + 6;
  const int previewCardHeight = 110;
  const int listHeight = pageHeight - contentTop - metrics.buttonHintsHeight - previewCardHeight - metrics.verticalSpacing * 2;

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, listHeight}, static_cast<int>(routine->exercises.size()), selectedIndex,
      [routine](const int index) {
        if (index >= 0 && index < static_cast<int>(routine->exercises.size())) {
          const auto& ex = routine->exercises[index];
          const bool done = GYM_TRACKER.isExerciseCompleted(ex.id, ex.defaultSets);
          return (done ? "[*] " : "[ ] ") + ex.name;
        }
        return std::string{};
      },
      [routine](const int index) {
        if (index >= 0 && index < static_cast<int>(routine->exercises.size())) {
          const auto& ex = routine->exercises[index];
          const int doneSets = GYM_TRACKER.getCompletedSetsCount(ex.id);
          char buf[64];
          snprintf(buf, sizeof(buf), tr(STR_GYM_SETS_AT_WEIGHT), doneSets, ex.defaultSets, ex.defaultWeight);
          return std::string(buf);
        }
        return std::string{};
      },
      [](const int) { return UIIcon::Text; });

  // Draw Bottom Detail Card for highlighted exercise
  if (selectedIndex >= 0 && selectedIndex < static_cast<int>(routine->exercises.size())) {
    const auto& selEx = routine->exercises[selectedIndex];
    const int cardY = contentTop + listHeight + metrics.verticalSpacing;
    const int cardW = pageWidth - metrics.contentSidePadding * 2;
    const int cardX = metrics.contentSidePadding;

    renderer.drawRect(cardX, cardY, cardW, previewCardHeight, true);

    char targetBuf[80];
    snprintf(targetBuf, sizeof(targetBuf), tr(STR_GYM_TARGET_DETAIL), selEx.defaultSets,
             selEx.defaultReps, selEx.defaultWeight);
    renderer.drawText(UI_10_FONT_ID, cardX + 12, cardY + 12, targetBuf, true, EpdFontFamily::BOLD);

    float lastW = 0.0f;
    int lastR = 0;
    char prevBuf[80];
    if (GYM_TRACKER.getLastLoggedSet(selEx.id, lastW, lastR)) {
      snprintf(prevBuf, sizeof(prevBuf), tr(STR_GYM_LAST_RECORD), lastW, lastR);
    } else {
      snprintf(prevBuf, sizeof(prevBuf), "%s", tr(STR_GYM_NO_PREV_HISTORY));
    }
    renderer.drawText(SMALL_FONT_ID, cardX + 12, cardY + 42, prevBuf, true, EpdFontFamily::REGULAR);

    const int doneSets = GYM_TRACKER.getCompletedSetsCount(selEx.id);
    char todayBuf[80];
    snprintf(todayBuf, sizeof(todayBuf), tr(STR_GYM_PROGRESS_TODAY), doneSets, selEx.defaultSets);
    renderer.drawText(SMALL_FONT_ID, cardX + 12, cardY + 70, todayBuf, true, EpdFontFamily::REGULAR);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
