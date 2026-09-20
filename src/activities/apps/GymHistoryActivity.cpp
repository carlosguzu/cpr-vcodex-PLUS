#include "GymHistoryActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "GymTrackerStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/HeaderDateUtils.h"

void GymHistoryActivity::onEnter() {
  Activity::onEnter();
  GYM_TRACKER.loadHistory();
  const int total = GYM_TRACKER.getHistoryCount();
  selectedIndex = std::clamp(selectedIndex, 0, std::max(0, total - 1));
  requestUpdate();
}

void GymHistoryActivity::openSelectedHistoryEntry() {
  // Can display a detail view or just toggle
  requestUpdate();
}

void GymHistoryActivity::loop() {
  const int total = GYM_TRACKER.getHistoryCount();

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    openSelectedHistoryEntry();
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
}

void GymHistoryActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  HeaderDateUtils::drawHeaderWithDate(renderer, tr(STR_GYM_HISTORY));

  const auto& history = GYM_TRACKER.getHistory();
  const int total = static_cast<int>(history.size());

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  if (total == 0) {
    renderer.drawCenteredText(UI_10_FONT_ID, contentTop + 40, tr(STR_GYM_NO_HISTORY));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  const int previewCardH = 130;
  const int listHeight = pageHeight - contentTop - metrics.buttonHintsHeight - previewCardH - metrics.verticalSpacing * 2;

  // Render sessions in reverse chronological order (newest first)
  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, listHeight}, total, selectedIndex,
      [&history, total](const int index) {
        const int revIdx = total - 1 - index;
        if (revIdx >= 0 && revIdx < total) {
          return history[revIdx].dateStr + " - " + history[revIdx].routineName;
        }
        return std::string{};
      },
      [&history, total](const int index) {
        const int revIdx = total - 1 - index;
        if (revIdx >= 0 && revIdx < total) {
          return std::to_string(history[revIdx].exercises.size()) + " ejercicios completados";
        }
        return std::string{};
      },
      [](const int) { return UIIcon::Recent; });

  // Detail preview card for selected session
  const int revIdx = total - 1 - selectedIndex;
  if (revIdx >= 0 && revIdx < total) {
    const auto& session = history[revIdx];
    const int cardY = contentTop + listHeight + metrics.verticalSpacing;
    const int cardW = pageWidth - metrics.contentSidePadding * 2;
    const int cardX = metrics.contentSidePadding;

    renderer.drawRect(cardX, cardY, cardW, previewCardH, true);
    renderer.drawText(UI_10_FONT_ID, cardX + 10, cardY + 8, session.routineName.c_str(), true, EpdFontFamily::BOLD);

    int textY = cardY + 32;
    int shown = 0;
    for (const auto& ex : session.exercises) {
      if (shown >= 3) break;
      char exLine[80];
      if (!ex.sets.empty()) {
        const auto& s = ex.sets.back();
        snprintf(exLine, sizeof(exLine), "• %s: %d series (%.1f kg x %d)", ex.exerciseName.c_str(),
                 static_cast<int>(ex.sets.size()), s.weight, s.reps);
      } else {
        snprintf(exLine, sizeof(exLine), "• %s", ex.exerciseName.c_str());
      }
      renderer.drawText(SMALL_FONT_ID, cardX + 10, textY, exLine, true, EpdFontFamily::REGULAR);
      textY += 24;
      shown++;
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
