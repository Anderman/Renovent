#include "display_reader.h"

#include <soc/gpio_struct.h>

#include "display_codec.h"
#include "display_segments.h"
#include "../input/key_writer_internal.h"
#include "../hardware/pins.h"

namespace
{
    constexpr uint8_t kUpperBankBasePin = 32;

    constexpr uint32_t pinMask(uint8_t pin)
    {
        return 1UL << static_cast<uint32_t>(pin - kUpperBankBasePin);
    }

    constexpr uint32_t kBit0Mask = pinMask(pins::kBit0);
    constexpr uint32_t kBit1Mask = pinMask(pins::kBit1);
    constexpr uint32_t kBit2Mask = pinMask(pins::kBit2);
    constexpr uint32_t kDigit1Mask = pinMask(pins::kDigit1);
    constexpr uint32_t kDigit2Mask = pinMask(pins::kDigit2);
    constexpr uint32_t kDigit3Mask = pinMask(pins::kDigit3);
    constexpr uint32_t kDigit4Mask = pinMask(pins::kDigit4);
    // Select order from Docs/mappng.txt: A, C, B, DP, F, D, E, G.
    constexpr uint8_t kSelectToSegmentMask[8] = {
        display_segments::kSegmentA,
        display_segments::kSegmentC,
        display_segments::kSegmentB,
        display_segments::kDecimalPoint,
        display_segments::kSegmentF,
        display_segments::kSegmentD,
        display_segments::kSegmentE,
        display_segments::kSegmentG,
    };

    struct FrameState
    {
        uint8_t digitMasks[4] = {0, 0, 0, 0};
    };

    portMUX_TYPE g_frameReadyMux = portMUX_INITIALIZER_UNLOCKED;

    FrameState g_isrWorkingFrame;
    FrameState g_lastCompletedFrame;
    FrameState g_readyFrame;
    DisplaySnapshot g_snapshot = {"----"};
    volatile bool g_frameReadyPending = false;
    volatile uint8_t g_isrLastSelectIndex = 0;

    uint8_t IRAM_ATTR readSelectIndex(uint32_t gpioSnapshot)
    {
        return static_cast<uint8_t>(((gpioSnapshot & kBit2Mask) != 0U ? 0x04U : 0x00U) |
                                    ((gpioSnapshot & kBit1Mask) != 0U ? 0x02U : 0x00U) |
                                    ((gpioSnapshot & kBit0Mask) != 0U ? 0x01U : 0x00U));
    }

    void IRAM_ATTR clearIsrWorkingFrame()
    {
        for (uint8_t digitIndex = 0; digitIndex < 4; ++digitIndex)
        {
            g_isrWorkingFrame.digitMasks[digitIndex] = 0;
        }
    }

    bool isEqual(const FrameState &left, const FrameState &right)
    {
        for (uint8_t digitIndex = 0; digitIndex < 4; ++digitIndex)
        {
            if (left.digitMasks[digitIndex] != right.digitMasks[digitIndex])
            {
                return false;
            }
        }

        return true;
    }

    void publishFrame(const FrameState &frame)
    {
        renderDisplayText(frame.digitMasks, g_snapshot.text);

        keyWriterOnDisplayChangedHook(g_snapshot.text);
    }

    void publishIfStableFrame(const FrameState &frame)
    {
        if (!isEqual(frame, g_lastCompletedFrame))
        {
            g_lastCompletedFrame = frame;
            return;
        }

        g_lastCompletedFrame = frame;
        publishFrame(frame);
    }

    void IRAM_ATTR applySegment(uint8_t selectIndex, uint32_t gpioSnapshot)
    {
        const uint8_t segmentMask = kSelectToSegmentMask[selectIndex];

        if ((gpioSnapshot & kDigit1Mask) != 0U)
        {
            g_isrWorkingFrame.digitMasks[0] |= segmentMask;
        }
        if ((gpioSnapshot & kDigit2Mask) != 0U)
        {
            g_isrWorkingFrame.digitMasks[1] |= segmentMask;
        }
        if ((gpioSnapshot & kDigit3Mask) != 0U)
        {
            g_isrWorkingFrame.digitMasks[2] |= segmentMask;
        }
        if ((gpioSnapshot & kDigit4Mask) != 0U)
        {
            g_isrWorkingFrame.digitMasks[3] |= segmentMask;
        }

    }

    void IRAM_ATTR latchReadyFrame()
    {
        portENTER_CRITICAL_ISR(&g_frameReadyMux);
        g_readyFrame = g_isrWorkingFrame;
        g_frameReadyPending = true;
        portEXIT_CRITICAL_ISR(&g_frameReadyMux);
    }

    void IRAM_ATTR onBit0ChangeInterrupt()
    {
        const uint32_t gpioSnapshot = GPIO.in1.val;
        const uint8_t selectIndex = readSelectIndex(gpioSnapshot);

        const uint8_t expectedNextSelect = (g_isrLastSelectIndex + 1U) % 8U;
        if (selectIndex != expectedNextSelect)
        {
            clearIsrWorkingFrame();
            g_isrLastSelectIndex = 7U;
            return;
        }

        g_isrLastSelectIndex = selectIndex;

        onSelectIndexChange(selectIndex);
        applySegment(selectIndex, gpioSnapshot);

        if (selectIndex == 7U)
        {
            latchReadyFrame();
            clearIsrWorkingFrame();
        }
    }
} // namespace

void displayReaderSetup()
{
    pinMode(pins::kBit0, INPUT);
    pinMode(pins::kBit1, INPUT);
    pinMode(pins::kBit2, INPUT);
    pinMode(pins::kDigit1, INPUT);
    pinMode(pins::kDigit2, INPUT);
    pinMode(pins::kDigit3, INPUT);
    pinMode(pins::kDigit4, INPUT);
    clearIsrWorkingFrame();
    attachInterrupt(digitalPinToInterrupt(pins::kBit0), onBit0ChangeInterrupt, CHANGE);
}

void displayReaderLoop()
{
    if (!g_frameReadyPending)
    {
        return;
    }

    portENTER_CRITICAL(&g_frameReadyMux);
    FrameState frame = g_readyFrame;
    g_frameReadyPending = false;
    portEXIT_CRITICAL(&g_frameReadyMux);

    publishIfStableFrame(frame);
}

DisplaySnapshot getDisplaySnapshot()
{
    return g_snapshot;
}
