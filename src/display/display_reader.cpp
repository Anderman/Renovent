#include "display_reader.h"

#include <soc/gpio_struct.h>

#include "display_codec.h"
#include "display_segments.h"
#include "../input/key_writer_internal.h"
#include "../hardware/pins.h"

namespace
{
    constexpr uint8_t kUpperBankBasePin = 32;
    constexpr uint8_t kAllSelectsSeenMask = 0xFF;

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

    portMUX_TYPE g_displayMux = portMUX_INITIALIZER_UNLOCKED;
    portMUX_TYPE g_frameReadyMux = portMUX_INITIALIZER_UNLOCKED;

    FrameState g_isrWorkingFrame;
    FrameState g_lastCompletedFrame;
    FrameState g_readyFrame;
    DisplaySnapshot g_snapshot = {"----"};
    uint8_t g_snapshotDigitMasks[4] = {0, 0, 0, 0};
    volatile bool g_frameReadyPending = false;
    volatile uint8_t g_isrLastSelectIndex = 0;
    volatile uint32_t g_completeFrameCount = 0;
    volatile uint32_t g_missedSelectFrameCount = 0;
    uint32_t g_publishedFrameCount = 0;
    uint32_t g_lastStatsLogMs = 0;

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
        portENTER_CRITICAL(&g_displayMux);
        renderDisplayText(frame.digitMasks, g_snapshot.text);
        for (uint8_t digitIndex = 0; digitIndex < 4; ++digitIndex)
        {
            g_snapshotDigitMasks[digitIndex] = frame.digitMasks[digitIndex];
        }
        portEXIT_CRITICAL(&g_displayMux);

        keyWriterOnDisplayChangedHook(g_snapshot.text);
        g_publishedFrameCount = g_publishedFrameCount + 1U;
    }

    uint8_t toMissedSelectPercent(uint32_t completeFrames, uint32_t missedFrames)
    {
        const uint32_t totalFrames = completeFrames + missedFrames;
        if (totalFrames == 0U)
        {
            return 0U;
        }

        return static_cast<uint8_t>((missedFrames * 100U + (totalFrames / 2U)) / totalFrames);
    }

    void logDisplayReaderStats()
    {
        const uint32_t now = millis();
        if (now - g_lastStatsLogMs < 5000U)
        {
            return;
        }

        uint32_t completeFrames = 0;
        uint32_t missedFrames = 0;
        portENTER_CRITICAL(&g_frameReadyMux);
        completeFrames = g_completeFrameCount;
        missedFrames = g_missedSelectFrameCount;
        portEXIT_CRITICAL(&g_frameReadyMux);

        g_lastStatsLogMs = now;
        Serial.printf("[display] frames complete=%lu missedSelect=%lu missedPct=%u published=%lu\n",
                      static_cast<unsigned long>(completeFrames),
                      static_cast<unsigned long>(missedFrames),
                      static_cast<unsigned>(toMissedSelectPercent(completeFrames, missedFrames)),
                      static_cast<unsigned long>(g_publishedFrameCount));
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

    void IRAM_ATTR applySampleFromSnapshot(uint8_t selectIndex, uint32_t gpioSnapshot)
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
            g_missedSelectFrameCount = g_missedSelectFrameCount + 1U;
            clearIsrWorkingFrame();
            g_isrLastSelectIndex = 7U;
            return;
        }

        g_isrLastSelectIndex = selectIndex;

        keyWriterApplySelectIndexHook(selectIndex);
        applySampleFromSnapshot(selectIndex, gpioSnapshot);

        if (selectIndex == 7U)
        {
            g_completeFrameCount = g_completeFrameCount + 1U;
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
    logDisplayReaderStats();

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
    DisplaySnapshot snapshot;
    portENTER_CRITICAL(&g_displayMux);
    snapshot = g_snapshot;
    portEXIT_CRITICAL(&g_displayMux);
    return snapshot;
}
