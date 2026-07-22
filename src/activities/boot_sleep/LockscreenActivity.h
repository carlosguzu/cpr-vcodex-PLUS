#pragma once
#include "../Activity.h"

class LockscreenActivity final : public Activity {
 private:
  uint8_t pinDigits[4] = {0, 0, 0, 0};
  uint8_t cursorPosition = 0;
  uint8_t failCount = 0;
  std::string ifFoundContent;

 public:
  explicit LockscreenActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Lockscreen", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&& lock) override;
};
