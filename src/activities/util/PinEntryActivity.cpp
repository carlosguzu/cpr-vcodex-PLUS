#include "PinEntryActivity.h"
#include "../../CrossPointSettings.h"
#include "../../fontIds.h"
#include <I18n.h>
#include "../../components/UITheme.h"
#include "HalDisplay.h"

void PinEntryActivity::onEnter() {
  for (int i = 0; i < 4; i++) {
    pinDigits[i] = 0;
  }
  cursorPosition = 0;
  Activity::onEnter();
  requestUpdate(true);
}

void PinEntryActivity::loop() {
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
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    // Confirm: store entered PIN as PageResult and finish
    uint32_t enteredPin = pinDigits[0] * 1000 + pinDigits[1] * 100 + pinDigits[2] * 10 + pinDigits[3];
    ActivityResult res;
    res.isCancelled = false;
    res.data = PageResult{enteredPin};
    setResult(std::move(res));
    finish();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult res;
    res.isCancelled = true;
    setResult(std::move(res));
    finish();
  }
}

void PinEntryActivity::render(RenderLock&&) {
  renderer.clearScreen();

  // Draw title centered
  renderer.drawCenteredText(UI_12_FONT_ID, 180, title.c_str());

  // Draw PIN entry boxes
  int startY = 250;
  int startX = (renderer.getScreenWidth() - (4 * 40)) / 2;

  for (int i = 0; i < 4; i++) {
    int x = startX + i * 40;
    renderer.drawRect(x, startY, 30, 40);
    // Draw digit inside the box (using x + 10, startY + 8 for UI_12_FONT_ID to center it)
    std::string digitStr = std::to_string(pinDigits[i]);
    renderer.drawText(UI_12_FONT_ID, x + 10, startY + 8, digitStr.c_str());

    if (i == cursorPosition) {
      renderer.drawLine(x, startY + 42, x + 30, startY + 42);
      renderer.drawLine(x, startY + 43, x + 30, startY + 43);
    }
  }

  // Draw button hints
  const auto labels = mappedInput.mapLabels(
      I18N.get(StrId::STR_CANCEL), I18N.get(StrId::STR_CONFIRM),
      "Move", "Move");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
