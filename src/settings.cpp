#include "settings.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include "config.h"

namespace {

QVariantMap defaults() {
    return {
        {QStringLiteral("setup_completed"), false},
        // Opt-in: claim Win+V via the low-level keyboard hook.
        {QStringLiteral("use_win_v"), false},
        // Separate, optional, NOT required for Win+V (the hook runs ahead of the shell).
        {QStringLiteral("disable_windows_history"), false},
        // 0 = keep everything. Cloud retention mirrors this. Pinned items never prune.
        {QStringLiteral("retention_days"), 0},
        {QStringLiteral("sync_enabled"), true},
        // Sync is on out of the box, so the popup discloses what leaves the machine.
        {QStringLiteral("sync_notice_seen"), false},
        // Hide the tray icon (reopen by searching for the app / the hotkey).
        {QStringLiteral("hide_tray_icon"), false},
    };
}

}  // namespace

Settings::Settings() {
    data_ = defaults();
    load();
}

QVariant Settings::get(const QString &key) const {
    return data_.value(key, defaults().value(key));
}

void Settings::set(const QString &key, const QVariant &value, bool save_) {
    data_[key] = value;
    if (save_)
        save();
}

void Settings::load() {
    QFile f(cfg::settingsPath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return;
    const QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it)
        data_[it.key()] = it.value().toVariant();
}

void Settings::save() const {
    cfg::ensureDirs();
    QFile f(cfg::settingsPath());
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(QJsonDocument(QJsonObject::fromVariantMap(data_)).toJson(QJsonDocument::Indented));
}
