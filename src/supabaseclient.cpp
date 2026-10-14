#include "supabaseclient.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

#include "config.h"

namespace {

// Shipped defaults so a downloaded copy syncs with no setup. The publishable key
// is the client-side key; RLS (supabase_schema.sql) is what protects the data.
const QString kDefaultUrl = QStringLiteral("https://mdvicigcyzpirmofqryz.supabase.co");
const QString kDefaultAnonKey =
    QStringLiteral("sb_publishable_72H02mF75sxABbA6UIP8eA_2RydKH2a");

int statusOf(QNetworkReply *r) {
    return r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
}

QString messageFrom(const QByteArray &body, const QString &fallback, int status) {
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isObject()) {
        const QJsonObject o = doc.object();
        for (const char *k : {"msg", "error_description", "message", "error"}) {
            const QJsonValue v = o.value(QLatin1String(k));
            if (v.isString())
                return v.toString();
        }
    }
    return QStringLiteral("%1 (%2)").arg(fallback).arg(status);
}

bool ok2xx(int st) { return st == 200 || st == 201 || st == 204; }

}  // namespace

SupabaseClient::SupabaseClient(QObject *parent)
    : QObject(parent), nam_(new QNetworkAccessManager(this)) {}

SupabaseClient *SupabaseClient::fromConfig(QObject *parent) {
    auto *c = new SupabaseClient(parent);
    QString url = qEnvironmentVariable("SUPABASE_URL");
    QString key = qEnvironmentVariable("SUPABASE_ANON_KEY");
    if (url.isEmpty() || key.isEmpty()) {
        QFile f(cfg::supabaseConfigPath());
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
            if (url.isEmpty())
                url = o.value(QStringLiteral("url")).toString();
            if (key.isEmpty())
                key = o.value(QStringLiteral("anon_key")).toString();
        }
    }
    c->url_ = url.isEmpty() ? kDefaultUrl : url;
    while (c->url_.endsWith(QLatin1Char('/')))
        c->url_.chop(1);
    c->anonKey_ = key.isEmpty() ? kDefaultAnonKey : key;
    c->loadSession();
    return c;
}

void SupabaseClient::applyHeaders(QNetworkRequest &req, bool bearer) const {
    req.setRawHeader("apikey", anonKey_.toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (bearer)
        req.setRawHeader("Authorization", ("Bearer " + accessToken_).toUtf8());
}

void SupabaseClient::storeAuth(const QJsonObject &payload) {
    accessToken_ = payload.value(QStringLiteral("access_token")).toString();
    refreshToken_ = payload.value(QStringLiteral("refresh_token")).toString();
    const int expiresIn = payload.value(QStringLiteral("expires_in")).toInt(3600);
    expiresAt_ = QDateTime::currentSecsSinceEpoch() + expiresIn;
    const QJsonObject user = payload.value(QStringLiteral("user")).toObject();
    const QString id = user.value(QStringLiteral("id")).toString();
    const QString mail = user.value(QStringLiteral("email")).toString();
    if (!id.isEmpty())
        userId_ = id;
    if (!mail.isEmpty())
        email_ = mail;
    if (user.contains(QStringLiteral("is_anonymous")))
        isAnonymous_ = user.value(QStringLiteral("is_anonymous")).toBool();
    saveSession();
}

// -- auth ---------------------------------------------------------------------

void SupabaseClient::signInAnonymously(Cb cb) {
    if (!isConfigured()) {
        cb(false, QStringLiteral("Supabase URL / anon key not configured"));
        return;
    }
    QNetworkRequest req{QUrl(url_ + QStringLiteral("/auth/v1/signup"))};
    applyHeaders(req, false);
    QNetworkReply *r = nam_->post(req, QByteArray("{}"));
    connect(r, &QNetworkReply::finished, this, [this, r, cb] {
        const int st = statusOf(r);
        const QByteArray body = r->readAll();
        r->deleteLater();
        if (st == 200) {
            storeAuth(QJsonDocument::fromJson(body).object());
            isAnonymous_ = true;
            saveSession();
            cb(true, QString());
        } else {
            QString msg = messageFrom(body, QStringLiteral("Anonymous sign-in failed"), st);
            if (msg.contains(QStringLiteral("anonymous"), Qt::CaseInsensitive) || st == 422)
                msg += QStringLiteral(" — enable Anonymous Sign-Ins in the Supabase dashboard");
            cb(false, msg);
        }
    });
}

void SupabaseClient::signIn(const QString &email, const QString &password, Cb cb) {
    if (!isConfigured()) {
        cb(false, QStringLiteral("not configured"));
        return;
    }
    QNetworkRequest req{QUrl(url_ + QStringLiteral("/auth/v1/token?grant_type=password"))};
    applyHeaders(req, false);
    const QJsonObject body{{QStringLiteral("email"), email}, {QStringLiteral("password"), password}};
    QNetworkReply *r = nam_->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(r, &QNetworkReply::finished, this, [this, r, cb] {
        const int st = statusOf(r);
        const QByteArray body = r->readAll();
        r->deleteLater();
        if (st == 200) {
            storeAuth(QJsonDocument::fromJson(body).object());
            cb(true, QString());
        } else {
            cb(false, messageFrom(body, QStringLiteral("Sign in failed"), st));
        }
    });
}

void SupabaseClient::signUp(const QString &email, const QString &password, MsgCb cb) {
    if (!isConfigured()) {
        cb(false, QStringLiteral("not configured"));
        return;
    }
    QNetworkRequest req{QUrl(url_ + QStringLiteral("/auth/v1/signup"))};
    applyHeaders(req, false);
    const QJsonObject body{{QStringLiteral("email"), email}, {QStringLiteral("password"), password}};
    QNetworkReply *r = nam_->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(r, &QNetworkReply::finished, this, [this, r, cb] {
        const int st = statusOf(r);
        const QByteArray raw = r->readAll();
        r->deleteLater();
        if (st != 200 && st != 201) {
            cb(false, messageFrom(raw, QStringLiteral("Sign up failed"), st));
            return;
        }
        const QJsonObject data = QJsonDocument::fromJson(raw).object();
        if (!data.value(QStringLiteral("access_token")).toString().isEmpty()) {
            storeAuth(data);
            cb(true, QStringLiteral("Signed in."));
        } else {
            cb(true, QStringLiteral("Account created — check your email to confirm, then sign in."));
        }
    });
}

void SupabaseClient::upgradeToAccount(const QString &email, const QString &password, MsgCb cb) {
    if (!isSignedIn()) {
        cb(false, QStringLiteral("not signed in"));
        return;
    }
    ensureToken([this, email, password, cb](bool ok) {
        if (!ok) {
            cb(false, QStringLiteral("Session expired — sign in again"));
            return;
        }
        QNetworkRequest req{QUrl(url_ + QStringLiteral("/auth/v1/user"))};
        applyHeaders(req, true);
        const QJsonObject body{{QStringLiteral("email"), email}, {QStringLiteral("password"), password}};
        QNetworkReply *r = nam_->put(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
        connect(r, &QNetworkReply::finished, this, [this, r, email, cb] {
            const int st = statusOf(r);
            const QByteArray raw = r->readAll();
            r->deleteLater();
            if (st == 200) {
                email_ = email;
                saveSession();
                cb(true, QStringLiteral("Check your inbox to confirm the address. Your history is "
                                        "kept and will sync to your other devices once confirmed."));
            } else {
                cb(false, messageFrom(raw, QStringLiteral("Could not add the email"), st));
            }
        });
    });
}

void SupabaseClient::signOut() { clearSession(); }

void SupabaseClient::doRefresh(std::function<void(bool)> then) {
    if (refreshToken_.isEmpty()) {
        then(false);
        return;
    }
    QNetworkRequest req{QUrl(url_ + QStringLiteral("/auth/v1/token?grant_type=refresh_token"))};
    applyHeaders(req, false);
    const QJsonObject body{{QStringLiteral("refresh_token"), refreshToken_}};
    QNetworkReply *r = nam_->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(r, &QNetworkReply::finished, this, [this, r, then] {
        const int st = statusOf(r);
        const QByteArray raw = r->readAll();
        r->deleteLater();
        if (st == 200) {
            storeAuth(QJsonDocument::fromJson(raw).object());
            then(true);
        } else {
            clearSession();
            then(false);
        }
    });
}

void SupabaseClient::ensureToken(std::function<void(bool)> then) {
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (expiresAt_ > 0 && now > expiresAt_ - 60 && !refreshToken_.isEmpty())
        doRefresh(then);
    else
        then(true);
}

// -- data ---------------------------------------------------------------------

void SupabaseClient::insertRows(const QString &table, const QJsonArray &rows, Cb cb) {
    if (rows.isEmpty()) {
        cb(true, QString());
        return;
    }
    ensureToken([this, table, rows, cb](bool ok) {
        if (!ok) {
            cb(false, QStringLiteral("Session expired — sign in again"));
            return;
        }
        QNetworkRequest req{QUrl(url_ + QStringLiteral("/rest/v1/") + table)};
        applyHeaders(req, true);
        req.setRawHeader("Prefer", "return=minimal");
        QNetworkReply *r = nam_->post(req, QJsonDocument(rows).toJson(QJsonDocument::Compact));
        connect(r, &QNetworkReply::finished, this, [r, table, cb] {
            const int st = statusOf(r);
            const QByteArray raw = r->readAll();
            r->deleteLater();
            cb(ok2xx(st), ok2xx(st) ? QString()
                                    : messageFrom(raw, QStringLiteral("Insert into %1 failed").arg(table), st));
        });
    });
}

void SupabaseClient::upsertRow(const QString &table, const QJsonObject &row,
                               const QString &onConflict, Cb cb) {
    ensureToken([this, table, row, onConflict, cb](bool ok) {
        if (!ok) {
            cb(false, QStringLiteral("Session expired — sign in again"));
            return;
        }
        QUrl u(url_ + QStringLiteral("/rest/v1/") + table);
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("on_conflict"), onConflict);
        u.setQuery(q);
        QNetworkRequest req(u);
        applyHeaders(req, true);
        req.setRawHeader("Prefer", "return=minimal,resolution=merge-duplicates");
        QNetworkReply *r = nam_->post(req, QJsonDocument(QJsonArray{row}).toJson(QJsonDocument::Compact));
        connect(r, &QNetworkReply::finished, this, [r, table, cb] {
            const int st = statusOf(r);
            const QByteArray raw = r->readAll();
            r->deleteLater();
            cb(ok2xx(st), ok2xx(st) ? QString()
                                    : messageFrom(raw, QStringLiteral("Upsert into %1 failed").arg(table), st));
        });
    });
}

void SupabaseClient::deleteOlderThan(const QString &table, const QString &isoCutoff, Cb cb) {
    ensureToken([this, table, isoCutoff, cb](bool ok) {
        if (!ok) {
            cb(false, QStringLiteral("Session expired — sign in again"));
            return;
        }
        QUrl u(url_ + QStringLiteral("/rest/v1/") + table);
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("created_at"), QStringLiteral("lt.") + isoCutoff);
        u.setQuery(q);
        QNetworkRequest req(u);
        applyHeaders(req, true);
        req.setRawHeader("Prefer", "return=minimal");
        QNetworkReply *r = nam_->deleteResource(req);
        connect(r, &QNetworkReply::finished, this, [r, table, cb] {
            const int st = statusOf(r);
            const QByteArray raw = r->readAll();
            r->deleteLater();
            cb(ok2xx(st), ok2xx(st) ? QString()
                                    : messageFrom(raw, QStringLiteral("Prune of %1 failed").arg(table), st));
        });
    });
}

void SupabaseClient::count(const QString &table, std::function<void(bool, int)> cb) {
    ensureToken([this, table, cb](bool ok) {
        if (!ok) {
            cb(false, 0);
            return;
        }
        QUrl u(url_ + QStringLiteral("/rest/v1/") + table);
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("select"), QStringLiteral("id"));
        u.setQuery(q);
        QNetworkRequest req(u);
        applyHeaders(req, true);
        req.setRawHeader("Prefer", "count=exact");
        req.setRawHeader("Range", "0-0");
        QNetworkReply *r = nam_->get(req);
        connect(r, &QNetworkReply::finished, this, [r, cb] {
            const int st = statusOf(r);
            const QByteArray range = r->rawHeader("content-range");
            r->deleteLater();
            if (st != 200 && st != 206) {
                cb(false, 0);
                return;
            }
            const int slash = range.indexOf('/');
            cb(true, slash >= 0 ? range.mid(slash + 1).toInt() : 0);
        });
    });
}

void SupabaseClient::select(const QString &table, const Params &params,
                            std::function<void(bool, QJsonArray)> cb) {
    ensureToken([this, table, params, cb](bool ok) {
        if (!ok) {
            cb(false, {});
            return;
        }
        QUrl u(url_ + QStringLiteral("/rest/v1/") + table);
        QUrlQuery q;
        for (const auto &p : params)
            q.addQueryItem(p.first, p.second);
        u.setQuery(q);
        QNetworkRequest req(u);
        applyHeaders(req, true);
        QNetworkReply *r = nam_->get(req);
        connect(r, &QNetworkReply::finished, this, [r, cb] {
            const int st = statusOf(r);
            const QByteArray raw = r->readAll();
            r->deleteLater();
            cb(st == 200, QJsonDocument::fromJson(raw).array());
        });
    });
}

// -- session persistence ------------------------------------------------------

void SupabaseClient::loadSession() {
    QFile f(cfg::sessionPath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    accessToken_ = o.value(QStringLiteral("access_token")).toString();
    refreshToken_ = o.value(QStringLiteral("refresh_token")).toString();
    expiresAt_ = static_cast<qint64>(o.value(QStringLiteral("expires_at")).toDouble());
    userId_ = o.value(QStringLiteral("user_id")).toString();
    email_ = o.value(QStringLiteral("email")).toString();
    isAnonymous_ = o.value(QStringLiteral("is_anonymous")).toBool();
}

void SupabaseClient::saveSession() const {
    cfg::ensureDirs();
    QFile f(cfg::sessionPath());
    if (!f.open(QIODevice::WriteOnly))
        return;
    const QJsonObject o{
        {QStringLiteral("access_token"), accessToken_},
        {QStringLiteral("refresh_token"), refreshToken_},
        {QStringLiteral("expires_at"), static_cast<double>(expiresAt_)},
        {QStringLiteral("user_id"), userId_},
        {QStringLiteral("email"), email_},
        {QStringLiteral("is_anonymous"), isAnonymous_},
    };
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

void SupabaseClient::clearSession() {
    accessToken_.clear();
    refreshToken_.clear();
    expiresAt_ = 0;
    userId_.clear();
    email_.clear();
    isAnonymous_ = false;
    QFile::remove(cfg::sessionPath());
}
