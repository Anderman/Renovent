#include "mqtt_config.h"

#include "storage.h"

namespace
{
    constexpr char kMqttHostKey[] = "mqttHost";
    constexpr char kMqttPortKey[] = "mqttPort";
    constexpr char kMqttUserKey[] = "mqttUser";
    constexpr char kMqttPasswordKey[] = "mqttPass";

    KeyValueStore g_store;
    MqttConfig g_config;

    MqttConfig loadConfigFromFlash()
    {
        MqttConfig config;
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
    nextConfig.mqttHost.trim();
    nextConfig.mqttUser.trim();

    bool ok = true;
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
