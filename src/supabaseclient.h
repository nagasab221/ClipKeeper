#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

// Minimal async Supabase REST client — the Qt port of supabase_client.py. Uses
// QNetworkAccessManager (non-blocking, on the GUI thread) instead of `requests`
// on a worker thread, so results come back through callbacks. Only the handful
// of endpoints ClipKeeper needs: sign in (anon / email), refresh, insert, upsert,
// delete, count, select. The publishable key ships in the app; Row-Level Security
// (supabase_schema.sql) is what actually protects the data.
class SupabaseClient : public QObject {
    Q_OBJECT
public:
    using Cb = std::function<void(bool ok, const QString &error)>;
    using MsgCb = std::function<void(bool ok, const QString &message)>;
    using Params = QList<QPair<QString, QString>>;

    explicit SupabaseClient(QObject *parent = nullptr);

    // Env vars → local supabase.json → shipped defaults; then loads any session.
    static SupabaseClient *fromConfig(QObject *parent = nullptr);

    bool isConfigured() const { return !url_.isEmpty() && !anonKey_.isEmpty(); }
    bool isSignedIn() const { return !refreshToken_.isEmpty() && !userId_.isEmpty(); }
    bool isAnonymous() const { return isAnonymous_; }
    QString userId() const { return userId_; }
    QString email() const { return email_; }
    QString url() const { return url_; }
    bool hasKey() const { return !anonKey_.isEmpty(); }

    // -- auth --
    void signInAnonymously(Cb cb);
    void signIn(const QString &email, const QString &password, Cb cb);
    void signUp(const QString &email, const QString &password, MsgCb cb);
    void upgradeToAccount(const QString &email, const QString &password, MsgCb cb);
    void signOut();

    // -- data (each ensures a fresh token first) --
    void insertRows(const QString &table, const QJsonArray &rows, Cb cb);
    void upsertRow(const QString &table, const QJsonObject &row, const QString &onConflict, Cb cb);
    void deleteOlderThan(const QString &table, const QString &isoCutoff, Cb cb);
    void count(const QString &table, std::function<void(bool, int)> cb);
    void select(const QString &table, const Params &params, std::function<void(bool, QJsonArray)> cb);

    void loadSession();
    void saveSession() const;
    void clearSession();

private:
    void ensureToken(std::function<void(bool)> then);
    void doRefresh(std::function<void(bool)> then);
    void storeAuth(const QJsonObject &payload);
    void applyHeaders(QNetworkRequest &req, bool bearer) const;  // apikey (+ bearer)

    QNetworkAccessManager *nam_;
    QString url_;
    QString anonKey_;
    QString accessToken_;
    QString refreshToken_;
    qint64 expiresAt_ = 0;
    QString userId_;
    QString email_;
    bool isAnonymous_ = false;
};
