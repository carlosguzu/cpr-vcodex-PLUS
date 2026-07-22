#include "LockscreenActivity.h"
#include "../../CrossPointSettings.h"
#include "../../util/IfFoundFile.h"
#include "../../fontIds.h"
#include <I18n.h>
#include <HalDisplay.h>

void LockscreenActivity::onEnter() {
  for (int i = 0; i < 4; i++) {
    pinDigits[i] = 0;
  }
  cursorPosition = 0;
  failCount = 0;
  
  if (SETTINGS.showIfFoundOnLock) {
    ifFoundContent = IfFoundFile::readNormalized(IfFoundFile::findPath());
  }
  
  Activity::onEnter();
  requestUpdate();
}

void LockscreenActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    pinDigits[cursorPosition] = (pinDigits[cursorPosition] + 1) % 10;
    requestUpdate();
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    pinDigits[cursorPosition] = (pinDigits[cursorPosition] + 9) % 10;
    requestUpdate();
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    if (cursorPosition > 0) {
      cursorPosition--;
      requestUpdate();
    }
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    if (cursorPosition < 3) {
      cursorPosition++;
      requestUpdate();
    }
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    uint16_t enteredPin = pinDigits[0] * 1000 + pinDigits[1] * 100 + pinDigits[2] * 10 + pinDigits[3];
    if (enteredPin == SETTINGS.passcodePin) {
      finish();
    } else {
      failCount++;
      for (int i = 0; i < 4; i++) {
        pinDigits[i] = 0;
      }
      cursorPosition = 0;
      requestUpdate();
    }
  }
}

void LockscreenActivity::render(RenderLock&&) {
  renderer.clearScreen();
  
  renderer.drawCenteredText(UI_12_FONT_ID, 50, tr(STR_LOCKSCREEN_TITLE));

  if (SETTINGS.showIfFoundOnLock && !ifFoundContent.empty()) {
    int boxY = 90;
    int boxH = 220;
    int boxX = 20;
    int boxW = renderer.getScreenWidth() - 40;
    renderer.drawRect(boxX, boxY, boxW, boxH);

    int textX = boxX + 15;
    int textY = boxY + 15;
    int maxTextW = boxW - 30;
    int lineHeight = renderer.getLineHeight(UI_10_FONT_ID) + 6;
    int maxY = boxY + boxH - lineHeight;

    size_t start = 0;
    while (start < ifFoundContent.size() && textY <= maxY) {
      size_t end = ifFoundContent.find('\n', start);
      if (end == std::string::npos) end = ifFoundContent.size();
      std::string paragraph = ifFoundContent.substr(start, end - start);
      start = (end < ifFoundContent.size()) ? end + 1 : end;

      if (!paragraph.empty() && paragraph.back() == '\r') {
        paragraph.pop_back();
      }

      size_t pStart = 0;
      std::string currentLine;
      while (pStart < paragraph.size() && textY <= maxY) {
        size_t spacePos = paragraph.find(' ', pStart);
        if (spacePos == std::string::npos) spacePos = paragraph.size();
        std::string word = paragraph.substr(pStart, spacePos - pStart);
        pStart = (spacePos < paragraph.size()) ? spacePos + 1 : spacePos;

        if (word.empty()) continue;
        std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
        if (renderer.getTextWidth(UI_10_FONT_ID, testLine.c_str()) > maxTextW) {
          if (!currentLine.empty()) {
            renderer.drawText(UI_10_FONT_ID, textX, textY, currentLine.c_str());
            textY += lineHeight;
          }
          currentLine = word;
        } else {
          currentLine = testLine;
        }
      }
      if (!currentLine.empty() && textY <= maxY) {
        renderer.drawText(UI_10_FONT_ID, textX, textY, currentLine.c_str());
        textY += lineHeight;
      }
    }
  }

  int startY = 350;
  int startX = (renderer.getScreenWidth() - (4 * 40)) / 2;
  
  for (int i = 0; i < 4; i++) {
    int x = startX + i * 40;
    renderer.drawRect(x, startY, 30, 40);
    std::string digitStr = std::to_string(pinDigits[i]);
    renderer.drawText(UI_12_FONT_ID, x + 10, startY + 8, digitStr.c_str());
    if (i == cursorPosition) {
      renderer.drawLine(x, startY + 42, x + 30, startY + 42);
      renderer.drawLine(x, startY + 43, x + 30, startY + 43);
    }
  }

  renderer.drawCenteredText(SMALL_FONT_ID, 450, tr(STR_LOCKSCREEN_HINT));

  if (failCount > 0) {
    renderer.drawCenteredText(UI_10_FONT_ID, 500, tr(STR_LOCKSCREEN_INCORRECT));
  }

  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
