#include "menu/setting_writer_internal.h"
#include "menu/setting_writer_steps.h"

#include <cstring>

#include "core/app_config.h"
#include "display/display_reader.h"
#include "display/display_text_utils.h"
#include <cstdio>

namespace setting_writer_internal
{
    namespace
    {

        constexpr uint32_t kNavigationKeyDownMs = 120;
        constexpr uint32_t kNavigationSettleMs = 120;
        constexpr uint32_t kAdjustHoldTimeoutBaseMs = 5000;
        constexpr uint32_t kAdjustHoldTimeoutPerStepMs = 220;
        constexpr uint32_t kAdjustHoldTimeoutMaxMs = 60000;

        uint32_t computeAdjustHoldTimeoutMs(int32_t currentValue, int32_t targetValue)
        {
            uint32_t delta = 0;
            if (targetValue >= currentValue)
            {
                delta = static_cast<uint32_t>(targetValue - currentValue);
            }
            else
            {
                delta = static_cast<uint32_t>(currentValue - targetValue);
            }

            const uint32_t timeoutMs = kAdjustHoldTimeoutBaseMs + delta * kAdjustHoldTimeoutPerStepMs;
            return timeoutMs > kAdjustHoldTimeoutMaxMs ? kAdjustHoldTimeoutMaxMs : timeoutMs;
        }

        bool readCurrentSettingValue(uint32_t now, ParsedSettingValue &settingValue)
        {
            const DisplaySnapshot snapshot = getDisplaySnapshot();
            if (!tryGetDisplaySettingValue(snapshot.text, settingValue))
            {
                updateInvalidDisplayTimer(now);
                return false;
            }

            g_state.invalidDisplayStartedMs = 0;
            return true;
        }

        bool readCurrentNumericSettingValue(uint32_t now, int32_t &currentValue)
        {
            ParsedSettingValue currentSettingValue{};
            if (!readCurrentSettingValue(now, currentSettingValue) || !currentSettingValue.hasNumericValue)
            {
                return false;
            }

            currentValue = currentSettingValue.numericValue;
            g_state.currentValue = currentValue;
            return true;
        }

        bool readTargetDisplayMatch(uint32_t now, bool &matchesTarget)
        {
            ParsedSettingValue currentSettingValue{};
            if (!readCurrentSettingValue(now, currentSettingValue))
            {
                return false;
            }

            matchesTarget = std::strncmp(currentSettingValue.displayValue, g_state.request.targetDisplayValue, sizeof(g_state.request.targetDisplayValue)) == 0;
            return true;
        }

        void restartAdjustStep()
        {
            g_state.stepStarted = false;
            g_state.keysReleased = false;
        }

        void finishAdjustReleasePhase(uint32_t now, uint32_t settleMs)
        {
            if (!isWaitCompleted(now, settleMs))
            {
                return;
            }

            if (g_state.adjustTimedOut)
            {
                advanceToNextStep();
                return;
            }

            restartAdjustStep();
        }

        constexpr SettingWriterStep kWriteScript[] = {
            {"enter-settings-menu", SettingWriterStepKind::FixedKey, kKeyFunction, app_config::kMenuEnterHoldMs, 100, "U0"},
            {"enter-installer-menu", SettingWriterStepKind::FixedKey, static_cast<KeyMask>(kKeyFunction | kKeyOk), app_config::kMenuEnterHoldMs, 500, nullptr},
            {"navigate-to-entry", SettingWriterStepKind::NavigateToEntry, kKeyPlus, 0, 0, nullptr},
            {"open-entry", SettingWriterStepKind::FixedKey, kKeyOk, 450, 100, nullptr},
            {"adjust-value", SettingWriterStepKind::AdjustValue, kKeyNone, 0, 150, nullptr},
            {"save-value", SettingWriterStepKind::FixedKey, static_cast<KeyMask>(kKeyFunction | kKeyPlus), 450, 200, nullptr},
            {"confirm-value", SettingWriterStepKind::FixedKey, kKeyOk, 160, 100, nullptr},
            {"exit-menu", SettingWriterStepKind::FixedKey, kKeyFunction, app_config::kMenuExitHoldMs, 100, nullptr},
        };

        constexpr uint8_t kWriteScriptStepCount = sizeof(kWriteScript) / sizeof(kWriteScript[0]);

        void runFixedStep(uint32_t now, const SettingWriterStep &step)
        {
            if (!g_state.stepStarted)
            {
                startStep(now, step.keyPressed);
                return;
            }

            if (!g_state.keysReleased)
            {
                if (isWaitCompleted(now, step.keyDownMs))
                {
                    releaseKeys(now);
                }
                return;
            }

            if (!isWaitCompleted(now, step.settleMs))
            {
                return;
            }

            const DisplaySnapshot snapshot = getDisplaySnapshot();
            if (!startsWithDisplay(snapshot.text, step.expectedDisplayPrefix))
            {
                updateInvalidDisplayTimer(now);
                return;
            }

            g_state.invalidDisplayStartedMs = 0;
            advanceToNextStep();
        }

        void runNavigateToEntryStep(uint32_t now)
        {
            if (!g_state.stepStarted)
            {
                const DisplaySnapshot snapshot = getDisplaySnapshot();
                copyDisplayText(g_state.displayBeforeKeyPress, snapshot.text);

                char displayedKey[4] = {0};
                if (tryGetSettingKey(snapshot.text, displayedKey) && std::strncmp(displayedKey, g_state.request.key, sizeof(displayedKey)) == 0)
                {
                    g_state.invalidDisplayStartedMs = 0;
                    advanceToNextStep();
                    return;
                }

                startStep(now, kKeyPlus);
                return;
            }

            if (!g_state.keysReleased)
            {
                if (isWaitCompleted(now, kNavigationKeyDownMs))
                {
                    releaseKeys(now);
                }
                return;
            }

            if (!isWaitCompleted(now, kNavigationSettleMs))
            {
                return;
            }

            const DisplaySnapshot snapshot = getDisplaySnapshot();
            if (std::strncmp(snapshot.text, g_state.displayBeforeKeyPress, sizeof(g_state.displayBeforeKeyPress)) == 0)
            {
                updateInvalidDisplayTimer(now);
                return;
            }

            char displayedKey[4] = {0};
            if (!tryGetSettingKey(snapshot.text, displayedKey))
            {
                updateInvalidDisplayTimer(now);
                return;
            }

            g_state.invalidDisplayStartedMs = 0;

            if (std::strncmp(displayedKey, g_state.request.key, sizeof(displayedKey)) == 0)
            {
                advanceToNextStep();
                return;
            }

            g_state.stepStarted = false;
            g_state.keysReleased = false;
        }

        void runAdjustValueStep(uint32_t now, const SettingWriterStep &step)
        {
            if (!g_state.request.targetHasNumericValue)
            {
                if (!g_state.stepStarted)
                {
                    bool matchesTarget = false;
                    if (!readTargetDisplayMatch(now, matchesTarget))
                    {
                        return;
                    }

                    if (matchesTarget)
                    {
                        advanceToNextStep();
                        return;
                    }

                    g_state.adjustTimedOut = false;
                    g_state.adjustHoldTimeoutMs = kAdjustHoldTimeoutMaxMs;
                    startStep(now, kKeyPlus);
                    return;
                }

                if (!g_state.keysReleased)
                {
                    bool matchesTarget = false;
                    if (!readTargetDisplayMatch(now, matchesTarget))
                    {
                        return;
                    }

                    if (isWaitCompleted(now, g_state.adjustHoldTimeoutMs))
                    {
                        releaseKeys(now);
                        g_state.adjustTimedOut = true;
                        return;
                    }

                    if (matchesTarget)
                    {
                        releaseKeys(now);
                    }
                    return;
                }

                finishAdjustReleasePhase(now, step.settleMs);
                return;
            }

            const int32_t targetValue = g_state.request.targetValue;
            if (!g_state.stepStarted)
            {
                int32_t currentValue = 0;
                if (!readCurrentNumericSettingValue(now, currentValue))
                {
                    return;
                }

                if (currentValue == targetValue)
                {
                    advanceToNextStep();
                    return;
                }

                g_state.increasing = currentValue < targetValue;
                g_state.adjustTimedOut = false;
                g_state.adjustHoldTimeoutMs = computeAdjustHoldTimeoutMs(currentValue, targetValue);

                startStep(now, g_state.increasing ? kKeyPlus : kKeyMinus);
                return;
            }

            if (!g_state.keysReleased)
            {
                int32_t currentValue = 0;
                if (!readCurrentNumericSettingValue(now, currentValue))
                {
                    return;
                }

                if (isWaitCompleted(now, g_state.adjustHoldTimeoutMs))
                {
                    releaseKeys(now);
                    g_state.adjustTimedOut = true;
                    const DisplaySnapshot snapshot = getDisplaySnapshot();
                    tryGetCompactSettingText(snapshot.text, g_state.request.targetDisplayValue);
                    return;
                }

                const bool reachedTarget = g_state.increasing
                                               ? currentValue >= targetValue
                                               : currentValue <= targetValue;
                if (reachedTarget)
                {
                    releaseKeys(now);
                }
                return;
            }

            finishAdjustReleasePhase(now, step.settleMs);
        }

    } // namespace

    void runCurrentStep(uint32_t now)
    {
        const SettingWriterStep &step = kWriteScript[g_state.currentStepIndex];

        switch (step.kind)
        {
        case SettingWriterStepKind::FixedKey:
            runFixedStep(now, step);
            break;
        case SettingWriterStepKind::NavigateToEntry:
            runNavigateToEntryStep(now);
            break;
        case SettingWriterStepKind::AdjustValue:
            runAdjustValueStep(now, step);
            break;
        }

        if (!g_state.running)
        {
            return;
        }

        if (g_state.currentStepIndex == kWriteScriptStepCount)
        {
            finishWrite();
        }
    }

} // namespace setting_writer_internal
