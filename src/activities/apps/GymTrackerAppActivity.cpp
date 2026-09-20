#include "GymTrackerAppActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "GymHistoryActivity.h"
#include "GymRoutineOverviewActivity.h"
#include "GymTrackerStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/HeaderDateUtils.h"

int GymTrackerAppActivity::getTotalEntries() const {
  return static_cast<int>(GYM_TRACKER.getRoutines().size()) + 1;  // Routines + 1 for History
}

void GymTrackerAppActivity::onEnter() {
  Activity::onEnter();
  GYM_TRACKER.loadRoutines();
  GYM_TRACKER.loadHistory();
  selectedIndex = std::clamp(selectedIndex, 0, std::max(0, getTotalEntries() - 1));
  requestUpdate();
}

void GymTrackerAppActivity::openSelectedEntry() {
  const auto& routines = GYM_TRACKER.getRoutines();
  const int routineCount = static_cast<int>(routines.size());

  if (selectedIndex >= 0 && selectedIndex < routineCount) {
    // Open routine overview
    const auto& r = routines[selectedIndex];
    auto activity = std::make_unique<GymRoutineOverviewActivity>(renderer, mappedInput, r.id, r.name);
    startActivityForResult(std::move(activity), [this](const ActivityResult&) {
      requestUpdate();
    });
  } else if (selectedIndex == routineCount) {
    // Open History
    auto activity = std::make_unique<GymHistoryActivity>(renderer, mappedInput);
    startActivityForResult(std::move(activity), [this](const ActivityResult&) {
      requestUpdate();
    });
  }
}

void GymTrackerAppActivity::loop() {
  const int total = getTotalEntries();

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    openSelectedEntry();
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

void GymTrackerAppActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  HeaderDateUtils::drawHeaderWithDate(renderer, tr(STR_GYM_TRACKER));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  const auto& routines = GYM_TRACKER.getRoutines();
  const int routineCount = static_cast<int>(routines.size());
  const int totalItems = getTotalEntries();

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, totalItems, selectedIndex,
      [&routines, routineCount](const int index) {
        if (index >= 0 && index < routineCount) {
          return routines[index].name;
        }
        return std::string(tr(STR_GYM_HISTORY));
      },
      [&routines, routineCount](const int index) {
        if (index >= 0 && index < routineCount) {
          return std::to_string(routines[index].exercises.size()) + " " + std::string(tr(STR_GYM_EXERCISES));
        }
        const int histCount = GYM_TRACKER.getHistoryCount();
        return std::to_string(histCount) + " sesion(es)";
      },
      [routineCount](const int index) {
        return (index == routineCount) ? UIIcon::Recent : UIIcon::Trophy;
      });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
