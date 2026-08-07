#pragma once

#include <EpdFontFamily.h>

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "blocks/BlockStyle.h"
#include "blocks/TextBlock.h"

class GfxRenderer;

class ParsedText {
  // std::deque, not std::vector. Ported from crosspoint feat-bluetooth
  // 200abc6aa. A vector of N tokens needs N * sizeof(std::string) in ONE
  // contiguous block, and a CJK chapter tokenises to thousands of them (each
  // ideograph is its own token so lines can wrap between characters) -- tens to
  // well over a hundred KB, which cannot be allocated on a fragmented heap with
  // NimBLE resident. A deque grows in small chunks, so the largest single
  // allocation stays bounded no matter how many tokens a chapter produces.
  // The parallel arrays below stay vectors: their elements are 1-2 bytes, so
  // even a huge chapter keeps them small.
  std::deque<std::string> words;
  std::vector<EpdFontFamily::Style> wordStyles;
  std::vector<bool> wordContinues;       // true = word attaches to previous with no break and no space
  std::vector<bool> wordNoSpaceBefore;   // true = may break before token, but no synthetic space when joined
  std::vector<bool> wordIsBionicSuffix;  // true = token is the regular tail of a bionic bold-prefix split
  std::vector<bool> wordIsGuideDot;      // true = token is a guide dot (U+00B7) inserted between words
  std::vector<uint8_t> wordBackgroundBlack;
  bool extraParagraphSpacing;
  bool forceParagraphIndents;
  bool hyphenationEnabled;
  bool bionicReadingEnabled;
  bool guideReadingEnabled;
  BlockStyle blockStyle;

  void applyParagraphIndent();
  std::vector<size_t> computeLineBreaks(const GfxRenderer& renderer, int fontId, int pageWidth,
                                        std::vector<uint16_t>& wordWidths, std::vector<bool>& continuesVec,
                                        std::vector<bool>& noSpaceBeforeVec);
  std::vector<size_t> computeHyphenatedLineBreaks(const GfxRenderer& renderer, int fontId, int pageWidth,
                                                  std::vector<uint16_t>& wordWidths, std::vector<bool>& continuesVec,
                                                  std::vector<bool>& noSpaceBeforeVec);
  bool hyphenateWordAtIndex(size_t wordIndex, int availableWidth, const GfxRenderer& renderer, int fontId,
                            std::vector<uint16_t>& wordWidths, bool allowFallbackBreaks);
  bool splitPathologicalTokenAtIndex(size_t wordIndex, int availableWidth, const GfxRenderer& renderer, int fontId,
                                     std::vector<uint16_t>& wordWidths);
  void extractLine(size_t breakIndex, int pageWidth, const std::vector<uint16_t>& wordWidths,
                   const std::vector<bool>& continuesVec, const std::vector<bool>& noSpaceBeforeVec,
                   const std::vector<size_t>& lineBreakIndices,
                   const std::function<void(std::shared_ptr<TextBlock>)>& processLine, const GfxRenderer& renderer,
                   int fontId);
  std::vector<uint16_t> calculateWordWidths(const GfxRenderer& renderer, int fontId);

 public:
  explicit ParsedText(const bool extraParagraphSpacing, const bool forceParagraphIndents = false,
                      const bool hyphenationEnabled = false, const bool bionicReadingEnabled = false,
                      const bool guideReadingEnabled = false, const BlockStyle& blockStyle = BlockStyle())
      : extraParagraphSpacing(extraParagraphSpacing),
        forceParagraphIndents(forceParagraphIndents),
        hyphenationEnabled(hyphenationEnabled),
        bionicReadingEnabled(bionicReadingEnabled),
        guideReadingEnabled(guideReadingEnabled),
        blockStyle(blockStyle) {}
  ~ParsedText() = default;

  void addWord(std::string word, EpdFontFamily::Style fontStyle, bool underline = false, bool attachToPrevious = false,
               bool backgroundBlack = false);
  void setBlockStyle(const BlockStyle& blockStyle) { this->blockStyle = blockStyle; }
  BlockStyle& getBlockStyle() { return blockStyle; }
  size_t size() const { return words.size(); }
  bool isEmpty() const { return words.empty(); }
  void layoutAndExtractLines(const GfxRenderer& renderer, int fontId, uint16_t viewportWidth,
                             const std::function<void(std::shared_ptr<TextBlock>)>& processLine,
                             bool includeLastLine = true);
};
