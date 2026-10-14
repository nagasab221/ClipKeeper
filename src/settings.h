#pragma once

#include <QVariant>
#include <QVariantMap>

// JSON-backed settings store (%LOCALAPPDATA%\ClipKeeperQt\settings.json) — the
// port of settings.py.
class Settings {
public:
    Settings();

    QVariant get(const QString &key) const;
    bool getBool(const QString &key) const { return get(key).toBool(); }
    int getInt(const QString &key) const { return get(key).toInt(); }
    void set(const QString &key, const QVariant &value, bool save = true);

private:
    void load();
    void save() const;

    QVariantMap data_;
};
