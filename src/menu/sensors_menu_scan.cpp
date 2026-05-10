#include "menu/sensors_menu_internal.h"

#include <cstring>

#include "core/app_config.h"
#include "display/display_reader.h"
#include "display/display_text_utils.h"
#include "input/key_writer.h"

namespace sensors_menu_internal
{
    namespace
    {

        struct SensorsMenuStep
        {
            const char *phaseName = "idle";
            KeyMask keyPressed = kKeyNone;
            uint32_t keyDownMs = 0;
            uint32_t settleMs = 0;
        };

        constexpr uint32_t kStepDisplayTimeoutMs = 2000;
        constexpr uint8_t kReadCurrentEntryStepIndex = 0;
        constexpr uint8_t kNextEntryStepIndex = 1;
        constexpr uint8_t kLeaveMenuStepIndex = 2;
        constexpr uint8_t kFinishReadStepIndex = 3;

        constexpr SensorsMenuStep kReadScript[] = {
            {"enter-sensors-menu", static_cast<KeyMask>(kKeyFunction | kKeyOk), app_config::kMenuEnterHoldMs, 100},
            {"next-entry", kKeyPlus, 150, 100},
            {"leave-sensors-menu", kKeyFunction, app_config::kMenuBackHoldMs, 100},
            {"exit-menu", kKeyFunction, app_config::kMenuExitHoldMs, 100},
        };

        void startStep(uint32_t now, KeyMask keyMask)
        {
            g_scanState.phaseStartedMs = now;
            g_scanState.stepStarted = true;
            g_scanState.keysReleased = false;
            pressKeys(keyMask);
        }

        bool isWaitCompleted(uint32_t now, uint32_t ms)
        {
            return static_cast<uint32_t>(now - g_scanState.phaseStartedMs) >= ms;
        }

        void releaseKeys(uint32_t now)
        {
            pressKeys(kKeyNone);
            g_scanState.keysReleased = true;
            g_scanState.phaseStartedMs = now;
        }

        void advanceToNextStep()
        {
            ++g_scanState.currentScriptIndex;
            g_scanState.stepStarted = false;
        }

        void advanceToStep(uint8_t stepIndex)
        {
            g_scanState.currentScriptIndex = stepIndex;
            g_scanState.stepStarted = false;
            g_scanState.keysReleased = false;
        }

        void finishScan()
        {
            pressKeys(kKeyNone);

            portENTER_CRITICAL(&g_menuMux);
            std::memcpy(g_lastCompletedEntries, g_scanState.entries, sizeof(g_lastCompletedEntries));
            std::memcpy(g_lastCompletedUnknownEntries, g_scanState.unknownEntries, sizeof(g_lastCompletedUnknownEntries));
            g_lastCompletedMs = millis();
            portEXIT_CRITICAL(&g_menuMux);

            g_scanState = SensorsMenuState{};
        }

        void abortScan()
        {
            pressKeys(kKeyNone);
            g_scanState = SensorsMenuState{};
        }

        bool abortIfStepTimedOut(uint32_t now)
        {
            if (!isWaitCompleted(now, kStepDisplayTimeoutMs))
            {
                return false;
            }

            abortScan();
            return true;
        }

    } // namespace

    void runCurrentStep(uint32_t now)
    {
        const SensorsMenuStep &step = kReadScript[g_scanState.currentScriptIndex];
        if (!g_scanState.stepStarted)
        {
            startStep(now, step.keyPressed);
            return;
        }

        if (!g_scanState.keysReleased)
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

        // enter-sensors-menu
        if (g_scanState.currentScriptIndex == kReadCurrentEntryStepIndex)
        {
            // do not return need to read the first entry
            advanceToStep(kNextEntryStepIndex);
        }

        const DisplaySnapshot snapshot = getDisplaySnapshot();
        if (g_scanState.currentScriptIndex == kNextEntryStepIndex)
        {
            ParsedSensorEntry parsedEntry{};
            if (!parseSensorEntry(snapshot.text, parsedEntry))
            {
                abortIfStepTimedOut(now);
                return;
            }
            const boolean keyNotChanged = std::strncmp(parsedEntry.key, g_scanState.currentEntryKey, sizeof(parsedEntry.key)) != 0;
            if (keyNotChanged)
            {
                abortIfStepTimedOut(now);
                return;
            }

            if (g_scanState.firstEntryKey[0] != '\0' &&
                std::strncmp(parsedEntry.key, g_scanState.firstEntryKey, sizeof(parsedEntry.key)) == 0)
            {
                advanceToStep(kLeaveMenuStepIndex);
                return;
            }

            if (captureCurrentEntry(snapshot.text, parsedEntry))
            {
                advanceToStep(kNextEntryStepIndex);
                return;
            }

            abortScan();
            return;
        }

        if (g_scanState.currentScriptIndex == kLeaveMenuStepIndex)
        {
            advanceToNextStep();
            return;
        }

        if (g_scanState.currentScriptIndex == kFinishReadStepIndex)
        {
            finishScan();
            return;
        }

        abortScan();
    }

} // namespace sensors_menu_internal