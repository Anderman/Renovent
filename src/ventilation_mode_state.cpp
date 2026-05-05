#include "ventilation_mode_state.h"

#include "display_reader.h"
#include "display_text_utils.h"
#include "storage.h"

namespace
{
    constexpr char kVentilationAutoModeKey[] = "vent_auto";
    KeyValueStore g_keyValueStore;
    bool g_initialized = false;
    bool g_ventilationAutoMode = false;

    void ensureInitialized()
    {
        if (g_initialized)
        {
            return;
        }

        g_ventilationAutoMode = g_keyValueStore.getBool(kVentilationAutoModeKey, false);
        g_initialized = true;
    }
}

bool ventilationModeStateGetAutoMode()
{
    ensureInitialized();
    return g_ventilationAutoMode;
}

void ventilationModeStateSetAutoMode(bool enabled)
{
    ensureInitialized();
    g_ventilationAutoMode = enabled;
    g_keyValueStore.putBool(kVentilationAutoModeKey, enabled);
}

const char *ventilationModeStateGetLabel()
{
    ensureInitialized();
    const DisplaySnapshot snapshot = getDisplaySnapshot();
    if (startsWithDisplay(snapshot.text, "OFF"))
    {
        return "UIT";
    }

    return g_ventilationAutoMode ? "AUTO" : "AAN";
}