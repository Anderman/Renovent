#include "mqtt_config.h"

#include "storage.h"

namespace
{
    constexpr char kMqttNodeIdKey[] = "mqttNode";
    constexpr char kMqttHostKey[] = "mqttHost";
    constexpr char kMqttPortKey[] = "mqttPort";
    constexpr char kMqttUserKey[] = "mqttUser";
    constexpr char kMqttPasswordKey[] = "mqttPass";

    constexpr char kDefaultMqttNodeId[] = "1";

    KeyValueStore g_store;
    MqttConfig g_config;

    bool isAllowedNodeIdCharacter(char current)
    {
        return current >= '0' && current <= '9';
    }

    String sanitizeNodeId(const String &value)
    {
        String sanitized;
        sanitized.reserve(value.length());
        for (size_t index = 0; index < value.length(); ++index)
        {
            const char current = value[index];
            if (isAllowedNodeIdCharacter(current))
            {
                sanitized += current;
            }
        }

        if (sanitized.isEmpty())
        {
            sanitized = kDefaultMqttNodeId;
        }

        return sanitized;
    }

    MqttConfig loadConfigFromFlash()
    {
        MqttConfig config;
        config.mqttNodeId = sanitizeNodeId(g_store.getString(kMqttNodeIdKey, kDefaultMqttNodeId));
        config.mqttHost = g_store.getString(kMqttHostKey, "");
        config.mqttPort = g_store.getUShort(kMqttPortKey, 1883);
        config.mqttUser = g_store.getString(kMqttUserKey, "");
        config.mqttPassword = g_store.getString(kMqttPasswordKey, "");
        return config;
    }

} // namespace

void mqttConfigSetup()
{
    g_config = loadConfigFromFlash();
}

const MqttConfig &getMqttConfig()
{
    return g_config;
}

bool updateMqttConfig(const MqttConfig &config)
{
    MqttConfig nextConfig = config;
    nextConfig.mqttNodeId = sanitizeNodeId(nextConfig.mqttNodeId);
    nextConfig.mqttHost.trim();
    nextConfig.mqttUser.trim();

    bool ok = true;
    ok = g_store.putString(kMqttNodeIdKey, nextConfig.mqttNodeId) && ok;
    ok = g_store.putString(kMqttHostKey, nextConfig.mqttHost) && ok;
    ok = g_store.putUShort(kMqttPortKey, nextConfig.mqttPort) && ok;
    ok = g_store.putString(kMqttUserKey, nextConfig.mqttUser) && ok;

    if (!config.mqttPassword.isEmpty())
    {
        ok = g_store.putString(kMqttPasswordKey, config.mqttPassword) && ok;
        nextConfig.mqttPassword = config.mqttPassword;
    }
    else
    {
        nextConfig.mqttPassword = g_config.mqttPassword;
    }

    if (ok)
    {
        g_config = nextConfig;
    }
    return ok;
}
