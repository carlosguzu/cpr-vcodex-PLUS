#include "GymHistoryActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "GymTrackerStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/HeaderDateUtils.h"

void GymHistoryActivity::onEnter() {
  Activity::onEnter();
  GYM_TRACKER.loadHistory();
  const int total = GYM_TRACKER.getHistoryCount();
  selectedIndex = std::clamp(selectedIndex, 0, std::max(0, total - 1));
  viewingDetail = false;
  detailExerciseIndex = 0;
  requestUpdate();
}

void GymHistoryActivity::openSelectedHistoryEntry() {
  const int total = GYM_TRACKER.getHistoryCount();
  if (total <= 0) return;
  viewingDetail = true;
  detailExerciseIndex = 0;
  requestUpdate();
}

void GymHistoryActivity::loop() {
  const int total = GYM_TRACKER.getHistoryCount();

  if (viewingDetail) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      viewingDetail = false;
      requestUpdate();
      return;
    }

    const auto& history = GYM_TRACKER.getHistory();
    const int revIdx = total - 1 - selectedIndex;
    if (revIdx < 0 || revIdx >= total) {
      viewingDetail = false;
      requestUpdate();
      return;
    }

    const int totalExercises = static_cast<int>(history[revIdx].exercises.size());

    buttonNavigator.onNextPress([this, totalExercises] {
      if (totalExercises <= 0) return;
      detailExerciseIndex = ButtonNavigator::nextIndex(detailExerciseIndex, totalExercises);
      requestUpdate();
    });

    buttonNavigator.onPreviousPress([this, totalExercises] {
      if (totalExercises <= 0) return;
      detailExerciseIndex = ButtonNavigator::previousIndex(detailExerciseIndex, totalExercises);
      requestUpdate();
    });
    return;
  }

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

  const auto& history = GYM_TRACKER.getHistory();
  const int total = static_cast<int>(history.size());

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;

  if (total == 0) {
    HeaderDateUtils::drawHeaderWithDate(renderer, tr(STR_GYM_HISTORY));
    renderer.drawCenteredText(UI_10_FONT_ID, contentTop + 40, tr(STR_GYM_NO_HISTORY));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (viewingDetail) {
    const int revIdx = total - 1 - selectedIndex;
    if (revIdx < 0 || revIdx >= total) {
      viewingDetail = false;
      requestUpdate();
      return;
    }

    const auto& session = history[revIdx];
    HeaderDateUtils::drawHeaderWithDate(renderer, session.routineName.c_str(), session.dateStr.c_str());

    float totalVolume = 0.0f;
    int totalSets = 0;
    for (const auto& ex : session.exercises) {
      for (const auto& s : ex.sets) {
        if (s.completed) {
          totalVolume += (s.weight * s.reps);
          totalSets++;
        }
      }
    }

    const int cardGap = 8;
    const int cardW = (pageWidth - metrics.contentSidePadding * 2 - cardGap) / 2;
    const int cardH = 50;
    const int card1X = metrics.contentSidePadding;
    const int card2X = card1X + cardW + cardGap;
    const int summaryY = contentTop;

    renderer.drawRect(card1X, summaryY, cardW, cardH, true);
    renderer.drawText(SMALL_FONT_ID, card1X + 8, summaryY + 6, tr(STR_GYM_TOTAL_VOLUME), true, EpdFontFamily::REGULAR);
    char volBuf[32];
    snprintf(volBuf, sizeof(volBuf), "%.0f kg", totalVolume);
    renderer.drawText(UI_10_FONT_ID, card1X + 8, summaryY + 26, volBuf, true, EpdFontFamily::BOLD);

    renderer.drawRect(card2X, summaryY, cardW, cardH, true);
    renderer.drawText(SMALL_FONT_ID, card2X + 8, summaryY + 6, tr(STR_GYM_TOTAL_SETS), true, EpdFontFamily::REGULAR);
    char setBuf[32];
    snprintf(setBuf, sizeof(setBuf), tr(STR_GYM_SETS_IN_EXERCISES), totalSets, static_cast<int>(session.exercises.size()));
    renderer.drawText(UI_10_FONT_ID, card2X + 8, summaryY + 26, setBuf, true, EpdFontFamily::BOLD);

    const int totalExercises = static_cast<int>(session.exercises.size());
    if (totalExercises == 0) {
      renderer.drawCenteredText(UI_10_FONT_ID, summaryY + 80, tr(STR_GYM_NO_EXERCISES));
      const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
      renderer.displayBuffer();
      return;
    }

    detailExerciseIndex = std::clamp(detailExerciseIndex, 0, totalExercises - 1);

    const int setPreviewH = 120;
    const int listY = summaryY + cardH + metrics.verticalSpacing;
    const int listH = pageHeight - listY - metrics.buttonHintsHeight - setPreviewH - metrics.verticalSpacing * 2;

    GUI.drawList(
        renderer, Rect{0, listY, pageWidth, listH}, totalExercises, detailExerciseIndex,
        [&session](const int index) {
          if (index >= 0 && index < static_cast<int>(session.exercises.size())) {
            return session.exercises[index].exerciseName;
          }
          return std::string{};
        },
        [&session](const int index) {
          if (index >= 0 && index < static_cast<int>(session.exercises.size())) {
            const auto& ex = session.exercises[index];
            float exVol = 0.0f;
            for (const auto& s : ex.sets) {
              if (s.completed) exVol += (s.weight * s.reps);
            }
            char buf[64];
            snprintf(buf, sizeof(buf), "%d series  |  Vol: %.0f kg", static_cast<int>(ex.sets.size()), exVol);
            return std::string(buf);
          }
          return std::string{};
        },
        [](const int) { return UIIcon::Text; });

    if (detailExerciseIndex >= 0 && detailExerciseIndex < totalExercises) {
      const auto& ex = session.exercises[detailExerciseIndex];
      const int boxY = listY + listH + metrics.verticalSpacing;
      const int boxW = pageWidth - metrics.contentSidePadding * 2;
      const int boxX = metrics.contentSidePadding;

      renderer.drawRect(boxX, boxY, boxW, setPreviewH, true);
      char titleBuf[64];
      snprintf(titleBuf, sizeof(titleBuf), tr(STR_GYM_DETAIL_PREFIX), ex.exerciseName.c_str());
      renderer.drawText(SMALL_FONT_ID, boxX + 8, boxY + 6, titleBuf, true, EpdFontFamily::BOLD);

      int setY = boxY + 26;
      for (const auto& s : ex.sets) {
        if (setY + 20 > boxY + setPreviewH) break;
        char sLine[64];
        snprintf(sLine, sizeof(sLine), tr(STR_GYM_SET_LINE_OK), s.setNumber, s.weight, s.reps);
        renderer.drawText(SMALL_FONT_ID, boxX + 12, setY, sLine, true, EpdFontFamily::REGULAR);
        setY += 20;
      }
    }

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // Main sessions list
  HeaderDateUtils::drawHeaderWithDate(renderer, tr(STR_GYM_HISTORY));

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
          char buf[64];
          snprintf(buf, sizeof(buf), tr(STR_GYM_EXERCISES_COMPLETED), static_cast<int>(history[revIdx].exercises.size()));
          return std::string(buf);
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
        snprintf(exLine, sizeof(exLine), "- %s: %d series (%.1f kg x %d)", ex.exerciseName.c_str(),
                 static_cast<int>(ex.sets.size()), s.weight, s.reps);
      } else {
        snprintf(exLine, sizeof(exLine), "- %s", ex.exerciseName.c_str());
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
