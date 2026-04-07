#include "display_codec.h"

#include <cstring>
#include <stddef.h>

#include "display_segments.h"

namespace
{
  using namespace display_segments;

  void overwriteText(char (&text)[9], const char *replacement)
  {
    std::strncpy(text, replacement, sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';
  }

  void correctRenderedText(char (&text)[9])
  {
    if (std::strncmp(text, "1n1n", 4) == 0)
    {
      overwriteText(text, "init");
      return;
    }

    if (std::strncmp(text, "F1L", 3) == 0)
    {
      overwriteText(text, "FIL");
      return;
    }

    if (std::strncmp(text, "1n.", 3) == 0)
    {
      text[0] = 'I';
      return;
    }

    if (std::strncmp(text, "t5.", 3) == 0)
    {
      text[1] = 's';
      return;
    }

    if (std::strncmp(text, "5t.", 3) == 0)
    {
      text[0] = 's';
      return;
    }

    if (text[0] == '1' && text[1] == ' ')
    {
      text[0] = 'I';
    }
  }

  char decodeDigit(uint8_t mask)
  {
    const uint8_t sevenSegmentMask = static_cast<uint8_t>(mask & 0x7F);
    //  A
    // F B
    //  G
    // E C
    //  D   DP
    // nr1: 1.200
    // nr2:   C 0
    // nr3: bP. 0
    // nr4: tp.11
    // nr5: ts.21
    // nr6: In. 1
    // nr7: u.200
    // nr8: u.200
    // nr9: t.  0
    // nr10:A. 24
    // nr11:u0. 0
    // nr12:ST.80
    // nr13:PT.80
    switch (sevenSegmentMask)
    {
    case 0x00:
      return ' ';
    case kSegmentA | kSegmentB | kSegmentC | kSegmentD | kSegmentE | kSegmentF:
      return '0';
    case kSegmentB | kSegmentC:
      return '1';
    case kSegmentA | kSegmentB | kSegmentD | kSegmentE | kSegmentG:
      return '2';
    case kSegmentA | kSegmentB | kSegmentC | kSegmentD | kSegmentG:
      return '3';
    case kSegmentB | kSegmentC | kSegmentF | kSegmentG:
      return '4';
    case kSegmentA | kSegmentC | kSegmentD | kSegmentF | kSegmentG:
      return '5';
    case kSegmentA | kSegmentC | kSegmentD | kSegmentE | kSegmentF | kSegmentG:
      return '6';
    case kSegmentA | kSegmentB | kSegmentC:
      return '7';
    case kSegmentA | kSegmentB | kSegmentC | kSegmentD | kSegmentE | kSegmentF | kSegmentG:
      return '8';
    case kSegmentA | kSegmentB | kSegmentC | kSegmentD | kSegmentF | kSegmentG:
      return '9';
    case kSegmentA | kSegmentD | kSegmentE | kSegmentF:
      return 'C';
    case kSegmentC | kSegmentD | kSegmentE | kSegmentF | kSegmentG:
      return 'b';
    case kSegmentD | kSegmentE | kSegmentF | kSegmentG:
      return 't';
    case kSegmentA | kSegmentB | kSegmentE | kSegmentF | kSegmentG:
      return 'P';
    case kSegmentA | kSegmentB | kSegmentC | kSegmentE | kSegmentF | kSegmentG:
      return 'A'; // Druk afvoer
    case kSegmentA | kSegmentE | kSegmentF | kSegmentG:
      return 'F'; // Foutcode
    case kSegmentC | kSegmentD | kSegmentE:
      return 'u'; // uOnder afvoer volume of u0 status vorstbeveiliging
    case kSegmentB | kSegmentF | kSegmentG:
      return 'n'; // uBoven toevoer volume
    case kSegmentC | kSegmentG | kSegmentE:
      return 'n'; // n in In.0 (n.v.t.)
    case kSegmentB | kSegmentC | kSegmentD | kSegmentE | kSegmentF:
      return 'U'; // U (User instelling)
    case kSegmentD | kSegmentE | kSegmentF:
      return 'L'; // L FIL
    case kSegmentG:
      return '-';
    default:
      return '?';
    }
  }
} // namespace

void renderDisplayText(const uint8_t digitMasks[4], char (&text)[9])
{
  size_t writeIndex = 0;

  for (uint8_t digitIndex = 0; digitIndex < 4; ++digitIndex)
  {
    text[writeIndex++] = decodeDigit(digitMasks[digitIndex]);

    if ((digitMasks[digitIndex] & kDecimalPoint) != 0U)
    {
      text[writeIndex++] = '.';
    }
  }

  text[writeIndex] = '\0';
  correctRenderedText(text);
}