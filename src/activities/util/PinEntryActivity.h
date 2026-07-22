#pragma once
#include <string>
#include "../Activity.h"

// A reusable 4-digit PIN entry popup activity.
// The entered PIN can be retrieved via getEnteredPin() after the activity finishes.
// If the user presses Back, the result is cancelled.
class PinEntryActivity final : public Activity {
 private:
  uint8_t pinDigits[4] = {0, 0, 0, 0};
  uint8_t cursorPosition = 0;
  std::string title;

 public:
  explicit PinEntryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                            const std::string& title = "Enter PIN")
      : Activity("PinEntry", renderer, mappedInput), title(title) {}

  uint16_t getEnteredPin() const {
    return pinDigits[0] * 1000 + pinDigits[1] * 100 + pinDigits[2] * 10 + pinDigits[3];
  }

  void onEnter() override;
  void loop() override;
  void render(RenderLock&& lock) override;
};
