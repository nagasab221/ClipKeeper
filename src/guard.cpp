#include "guard.h"

#include <QDateTime>
#include <QMutex>

namespace guard {
namespace {

constexpr qint64 kWindowMs = 1500;  // _SUPPRESS_WINDOW_S = 1.5

QMutex g_mutex;
QString g_text;
qint64 g_textUntil = 0;
qint64 g_imageUntil = 0;
QString g_files;
qint64 g_filesUntil = 0;

qint64 now() { return QDateTime::currentMSecsSinceEpoch(); }

}  // namespace

void markSelfText(const QString &content) {
    QMutexLocker lock(&g_mutex);
    g_text = content;
    g_textUntil = now() + kWindowMs;
}

void markSelfImage() {
    QMutexLocker lock(&g_mutex);
    g_imageUntil = now() + kWindowMs;
}

void markSelfFiles(const QString &content) {
    QMutexLocker lock(&g_mutex);
    g_files = content;
    g_filesUntil = now() + kWindowMs;
}

bool shouldSuppressText(const QString &content) {
    QMutexLocker lock(&g_mutex);
    if (!g_text.isNull() && content == g_text && now() <= g_textUntil) {
        g_text = QString();  // back to null: one-shot
        return true;
    }
    return false;
}

bool shouldSuppressImage() {
    QMutexLocker lock(&g_mutex);
    if (now() <= g_imageUntil) {
        g_imageUntil = 0;
        return true;
    }
    return false;
}

bool shouldSuppressFiles(const QString &content) {
    QMutexLocker lock(&g_mutex);
    if (!g_files.isNull() && content == g_files && now() <= g_filesUntil) {
        g_files = QString();
        return true;
    }
    return false;
}

}  // namespace guard
