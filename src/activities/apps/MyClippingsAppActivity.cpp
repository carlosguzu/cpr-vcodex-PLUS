#include "MyClippingsAppActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>

#include "components/UITheme.h"
#include "fontIds.h"
#include "util/HeaderDateUtils.h"

namespace {

// Parse the title and author from "Book Title (Author Name)" format
void parseTitleLine(const std::string& line, std::string& title, std::string& author) {
  // Find the last '(' to split title and author
  size_t parenPos = line.rfind('(');
  if (parenPos != std::string::npos && parenPos > 0) {
    title = line.substr(0, parenPos - 1);  // skip the space before '('
    size_t closePos = line.rfind(')');
    if (closePos != std::string::npos && closePos > parenPos) {
      author = line.substr(parenPos + 1, closePos - parenPos - 1);
    }
  } else {
    title = line;
  }
}

// Trim leading/trailing whitespace
std::string trim(const std::string& str) {
  size_t start = str.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = str.find_last_not_of(" \t\r\n");
  return str.substr(start, end - start + 1);
}

}  // namespace

void MyClippingsAppActivity::loadClippings() {
  books.clear();

  FsFile file = Storage.open("/MyClippings.txt", O_READ);
  if (!file) return;

  // Read entire file (limited to a reasonable size)
  const size_t fileSize = file.size();
  if (fileSize == 0 || fileSize > 512 * 1024) {
    file.close();
    return;
  }

  std::string content;
  content.resize(fileSize);
  file.read(&content[0], fileSize);
  file.close();

  // Parse entries separated by "=========="
  size_t pos = 0;
  while (pos < content.size()) {
    // Find "=========="
    size_t sepPos = content.find("==========", pos);
    if (sepPos == std::string::npos) break;

    // Move past the separator line
    size_t lineEnd = content.find('\n', sepPos);
    if (lineEnd == std::string::npos) break;
    size_t entryStart = lineEnd + 1;

    // Read title line
    size_t titleEnd = content.find('\n', entryStart);
    if (titleEnd == std::string::npos) break;
    std::string titleLine = trim(content.substr(entryStart, titleEnd - entryStart));

    // Read location/metadata line
    size_t metaEnd = content.find('\n', titleEnd + 1);
    if (metaEnd == std::string::npos) break;
    std::string metaLine = trim(content.substr(titleEnd + 1, metaEnd - titleEnd - 1));

    // Read highlight text (skip empty line, then read until next "==========" or end)
    size_t textStart = metaEnd + 1;
    // Skip empty lines
    while (textStart < content.size() && (content[textStart] == '\n' || content[textStart] == '\r')) {
      textStart++;
    }

    // Find end of highlight text (next "==========" or end of file)
    size_t nextSep = content.find("==========", textStart);
    std::string highlightText;
    if (nextSep != std::string::npos) {
      highlightText = trim(content.substr(textStart, nextSep - textStart));
      pos = nextSep;  // Will find this same separator in next iteration
    } else {
      highlightText = trim(content.substr(textStart));
      pos = content.size();
    }

    if (titleLine.empty() || highlightText.empty()) {
      if (nextSep != std::string::npos) {
        pos = nextSep + 10;
      }
      continue;
    }

    // Parse title and author
    std::string bookTitle, bookAuthor;
    parseTitleLine(titleLine, bookTitle, bookAuthor);

    // Extract location from metadata
    std::string location;
    size_t locPos = metaLine.find("Location:");
    if (locPos != std::string::npos) {
      location = trim(metaLine.substr(locPos));
    }

    // Find or create book entry
    auto it = std::find_if(books.begin(), books.end(),
                           [&bookTitle](const BookClippings& b) { return b.title == bookTitle; });

    if (it == books.end()) {
      books.push_back(BookClippings{bookTitle, bookAuthor, {}});
      it = books.end() - 1;
    }

    it->clippings.push_back(ClippingEntry{highlightText, location});
  }
}

void MyClippingsAppActivity::onEnter() {
  Activity::onEnter();
  loadClippings();
  selectedIndex = 0;
  showingDetail = false;
  requestUpdate();
}

void MyClippingsAppActivity::loop() {
  if (showingDetail) {
    // Detail view: show clippings for a specific book
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      showingDetail = false;
      requestUpdate();
      return;
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (!currentBookTitle.empty() && selectedIndex >= 0 && selectedIndex < static_cast<int>(books.size())) {
        const auto& book = books[selectedIndex];
        if (book.title == currentBookTitle && detailIndex >= 0 && detailIndex < static_cast<int>(book.clippings.size())) {
          const auto& clip = book.clippings[detailIndex];
          int pct = -1;
          size_t pos = clip.location.find("Location:");
          if (pos != std::string::npos) {
            std::string pctStr = clip.location.substr(pos + 9);
            std::string digits;
            for (char c : pctStr) {
              if (std::isdigit(static_cast<unsigned char>(c))) {
                digits += c;
              } else if (c == '%') {
                break;
              }
            }
            if (!digits.empty()) {
              pct = std::stoi(digits);
            }
          }
          if (pct >= 0 && pct <= 100) {
            ActivityResult res;
            res.isCancelled = false;
            res.data = PercentResult{pct};
            setResult(std::move(res));
            finish();
            return;
          }
        }
      }
    }

    buttonNavigator.onNextRelease([this] {
      if (selectedIndex < 0 || selectedIndex >= static_cast<int>(books.size())) return;
      const auto& book = books[selectedIndex];
      if (detailIndex < static_cast<int>(book.clippings.size()) - 1) {
        detailIndex++;
        requestUpdate();
      }
    });

    buttonNavigator.onPreviousRelease([this] {
      if (detailIndex > 0) {
        detailIndex--;
        requestUpdate();
      }
    });
    return;
  }

  // Book list view
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!books.empty() && selectedIndex >= 0 && selectedIndex < static_cast<int>(books.size())) {
      showingDetail = true;
      detailIndex = 0;
      detailScrollOffset = 0;
      requestUpdate();
    }
    return;
  }

  buttonNavigator.onNextRelease([this] {
    if (books.empty()) return;
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(books.size()));
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    if (books.empty()) return;
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(books.size()));
    requestUpdate();
  });
}

void MyClippingsAppActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int listHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (showingDetail) {
    // Detail view: show individual clipping
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(books.size())) {
      renderer.displayBuffer();
      return;
    }
    const auto& book = books[selectedIndex];
    const int clippingCount = static_cast<int>(book.clippings.size());

    // Header
    char header[128];
    snprintf(header, sizeof(header), "%s (%d/%d)",
             book.title.c_str(), detailIndex + 1, clippingCount);
    HeaderDateUtils::drawHeaderWithDate(renderer, header, book.author.c_str());

    if (detailIndex >= 0 && detailIndex < clippingCount) {
      const auto& clip = book.clippings[detailIndex];

      int textX = metrics.contentSidePadding;
      int textY = contentTop + 5;
      int maxTextW = pageWidth - (metrics.contentSidePadding * 2);
      int lineHeight = renderer.getLineHeight(UI_10_FONT_ID) + 4;
      int maxY = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;

      // Draw location if present
      if (!clip.location.empty()) {
        renderer.drawText(SMALL_FONT_ID, textX, textY, clip.location.c_str());
        textY += lineHeight + 4;
      }

      // Word-wrap and draw the highlight text
      const std::string& text = clip.text;
      size_t start = 0;
      while (start < text.size() && textY <= maxY) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string paragraph = text.substr(start, end - start);
        start = (end < text.size()) ? end + 1 : end;

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

    const bool isCurrentBook = (!currentBookTitle.empty() && book.title == currentBookTitle);
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), isCurrentBook ? tr(STR_SELECT) : "", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else {
    // Book list view
    HeaderDateUtils::drawHeaderWithDate(renderer, tr(STR_MY_CLIPPINGS), tr(STR_MY_CLIPPINGS_APP_DESC));

    if (books.empty()) {
      renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_CLIPPINGS));
    } else {
      GUI.drawList(renderer, Rect{0, contentTop, pageWidth, listHeight},
                   static_cast<int>(books.size()), selectedIndex,
                   [this](const int index) { return books[index].title; },
                   [this](const int index) {
                     if (!books[index].author.empty()) return books[index].author;
                     return std::string("");
                   },
                   [](const int) { return UIIcon::Book; },
                   [this](const int index) { return std::to_string(books[index].clippings.size()); });
    }

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), books.empty() ? "" : tr(STR_OPEN),
                                              tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}
