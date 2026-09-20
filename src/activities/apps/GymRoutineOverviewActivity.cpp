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
  const auto* routine = GYM_TRACKER.findRoutine(routineId);
  const int total = routine ? static_cast<int>(routine->exercises.size()) : 0;
  selectedIndex = std::clamp(selectedIndex, 0, std::max(0, total - 1));
  requestUpdate();
}

void GymRoutineOverviewActivity::openSelectedExercise() {
  const auto* routine = GYM_TRACKER.findRoutine(routineId);
  if (!routine || selectedIndex < 0 || selectedIndex >= static_cast<int>(routine->exercises.size())) {
    return;
  }

  const auto& ex = routine->exercises[selectedIndex];
  auto activity = std::make_unique<GymWorkoutActivity>(renderer, mappedInput, routineId, routineName, ex.id, ex.name,
                                                      ex.defaultSets, ex.defaultReps, ex.defaultWeight);

  startActivityForResult(std::move(activity), [this](const ActivityResult&) {
    requestUpdate();
  });
}

void GymRoutineOverviewActivity::loop() {
  const auto* routine = GYM_TRACKER.findRoutine(routineId);
  const int total = routine ? static_cast<int>(routine->exercises.size()) : 0;

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

  const auto* routine = GYM_TRACKER.findRoutine(routineId);
  if (!routine || routine->exercises.empty()) {
    HeaderDateUtils::drawHeaderWithDate(renderer, routineName.c_str(), tr(STR_GYM_NO_ROUTINES));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // Header
  int completedCount = 0;
  for (const auto& ex : routine->exercises) {
    if (GYM_TRACKER.isExerciseCompleted(ex.id, ex.defaultSets)) {
      completedCount++;
    }
  }

  char subHeader[64];
  snprintf(subHeader, sizeof(subHeader), "%d / %d completados", completedCount,
           static_cast<int>(routine->exercises.size()));
  HeaderDateUtils::drawHeaderWithDate(renderer, routineName.c_str(), subHeader);

  // Content area: split into list + bottom preview card
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
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
          snprintf(buf, sizeof(buf), "%d/%d series @ %.1f kg", doneSets, ex.defaultSets, ex.defaultWeight);
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
    snprintf(targetBuf, sizeof(targetBuf), "Objetivo: %d series x %d reps @ %.1f kg", selEx.defaultSets,
             selEx.defaultReps, selEx.defaultWeight);
    renderer.drawText(UI_10_FONT_ID, cardX + 12, cardY + 12, targetBuf, true, EpdFontFamily::BOLD);

    float lastW = 0.0f;
    int lastR = 0;
    char prevBuf[80];
    if (GYM_TRACKER.getLastLoggedSet(selEx.id, lastW, lastR)) {
      snprintf(prevBuf, sizeof(prevBuf), "Ultimo registro: %.1f kg x %d reps", lastW, lastR);
    } else {
      snprintf(prevBuf, sizeof(prevBuf), "Sin historial previo (se usaran valores por defecto)");
    }
    renderer.drawText(SMALL_FONT_ID, cardX + 12, cardY + 42, prevBuf, true, EpdFontFamily::REGULAR);

    const int doneSets = GYM_TRACKER.getCompletedSetsCount(selEx.id);
    char todayBuf[80];
    snprintf(todayBuf, sizeof(todayBuf), "Progreso hoy: %d de %d series completadas", doneSets, selEx.defaultSets);
    renderer.drawText(SMALL_FONT_ID, cardX + 12, cardY + 70, todayBuf, true, EpdFontFamily::REGULAR);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
