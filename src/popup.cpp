#include "popup.h"

#include <windows.h>
#include <dwmapi.h>

#include <QCheckBox>
#include <QCursor>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPalette>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>

#include "autostart.h"
#include "fuzzy.h"
#include "guard.h"
#include "historymodel.h"
#include "rowdelegate.h"
#include "settings.h"
#include "theme.h"
#include "winclip.h"
#include "winhistory.h"

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

namespace {
constexpr int kWinW = 370;
constexpr int kWinH = 520;
constexpr int kMargin = 12;
constexpr int kWheelStepPx = 48;  // pixels scrolled per wheel notch (lower = slower)
}  // namespace

Popup::Popup(db::Database *db, Settings *settings, QWidget *parent)
    : QWidget(parent), db_(db), settings_(settings) {
    buildUi();
    refreshData();
    applyFilter();
}

void Popup::buildUi() {
    setWindowTitle(QStringLiteral("ClipKeeper"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setFixedSize(kWinW, kWinH);
    setStyleSheet(theme::styleSheet());
    setAutoFillBackground(true);
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, theme::OUTLINE);  // hairline border
        setPalette(pal);
    }

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(1, 1, 1, 1);
    outer->setSpacing(0);
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("Card"));
    outer->addWidget(card);

    auto *v = new QVBoxLayout(card);
    v->setContentsMargins(10, 10, 10, 8);
    v->setSpacing(6);

    // Search row — hidden until the user types (History view only).
    searchRow_ = new QFrame;
    auto *sr = new QHBoxLayout(searchRow_);
    sr->setContentsMargins(0, 0, 0, 0);
    sr->setSpacing(5);
    search_ = new QLineEdit;
    search_->setObjectName(QStringLiteral("Search"));
    search_->setPlaceholderText(QStringLiteral("Search clipboard…"));
    clearBtn_ = new QPushButton(QString(QChar(0x2715)));
    clearBtn_->setObjectName(QStringLiteral("Ghost"));
    clearBtn_->setFixedSize(28, 28);
    clearBtn_->setFocusPolicy(Qt::NoFocus);
    sr->addWidget(search_);
    sr->addWidget(clearBtn_);
    v->addWidget(searchRow_);
    searchRow_->hide();

    // History list — natively virtualized, smooth pixel scrolling.
    model_ = new HistoryModel(this);
    delegate_ = new RowDelegate(this);
    list_ = new QListView;
    list_->setObjectName(QStringLiteral("List"));
    list_->setModel(model_);
    list_->setItemDelegate(delegate_);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_->setMouseTracking(true);
    list_->setUniformItemSizes(false);

    morePanel_ = buildMorePanel();

    stack_ = new QStackedWidget;
    stack_->addWidget(list_);       // 0 = History
    stack_->addWidget(morePanel_);  // 1 = More
    v->addWidget(stack_, 1);

    // Footer: hint (left) | count | compact More/Back toggle (right).
    auto *footer = new QHBoxLayout;
    hint_ = new QLabel;
    hint_->setObjectName(QStringLiteral("Hint"));
    count_ = new QLabel;
    count_->setObjectName(QStringLiteral("Count"));
    moreBtn_ = new QPushButton(QStringLiteral("More"));
    moreBtn_->setObjectName(QStringLiteral("More"));
    moreBtn_->setFocusPolicy(Qt::NoFocus);
    moreBtn_->setCursor(Qt::PointingHandCursor);
    footer->addWidget(hint_);
    footer->addStretch(1);
    footer->addWidget(count_);
    footer->addSpacing(8);
    footer->addWidget(moreBtn_);
    v->addLayout(footer);

    connect(search_, &QLineEdit::textChanged, this, [this] { applyFilter(); });
    connect(clearBtn_, &QPushButton::clicked, this, [this] { hideSearch(); applyFilter(); });
    connect(moreBtn_, &QPushButton::clicked, this, [this] { setView(!moreView_); });
    connect(list_, &QListView::clicked, this, [this](const QModelIndex &i) { pasteHistory(i); });
    connect(delegate_, &RowDelegate::pinClicked, this, [this](const QModelIndex &i) { pinIndex(i); });
    connect(delegate_, &RowDelegate::deleteClicked, this, [this](const QModelIndex &i) { deleteIndex(i); });

    list_->installEventFilter(this);
    list_->viewport()->installEventFilter(this);  // slow the wheel scroll
    search_->installEventFilter(this);
    updateFooter();
}

QWidget *Popup::buildMorePanel() {
    auto *area = new QScrollArea;
    area->setObjectName(QStringLiteral("MoreArea"));
    area->setWidgetResizable(true);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget;
    area->setWidget(content);
    auto *l = new QVBoxLayout(content);
    l->setContentsMargins(12, 12, 12, 12);
    l->setSpacing(7);

    auto section = [&](const QString &text) {
        auto *lbl = new QLabel(text);
        lbl->setObjectName(QStringLiteral("Section"));
        l->addWidget(lbl);
    };
    auto hint = [&](const QString &text) {
        auto *lbl = new QLabel(text);
        lbl->setObjectName(QStringLiteral("Hint"));
        lbl->setWordWrap(true);
        l->addWidget(lbl);
    };

    // --- Settings ---
    section(QStringLiteral("SETTINGS"));
    cbWinV_ = new QCheckBox(QStringLiteral("Use Win+V to open ClipKeeper"));
    cbHistory_ = new QCheckBox(QStringLiteral("Turn off Windows' own clipboard history"));
    cbAutostart_ = new QCheckBox(QStringLiteral("Start with Windows"));
    cbTray_ = new QCheckBox(QStringLiteral("Show tray icon"));
    l->addWidget(cbWinV_);
    l->addWidget(cbHistory_);
    l->addWidget(cbAutostart_);
    l->addWidget(cbTray_);
    hint(QStringLiteral("If you hide the tray icon, reopen ClipKeeper by searching for it in "
                        "Windows, or with your hotkey."));

    l->addSpacing(6);

    // --- Snippets ---
    section(QStringLiteral("SNIPPETS"));
    auto *form = new QFrame;
    form->setObjectName(QStringLiteral("SnipForm"));
    auto *fl = new QHBoxLayout(form);
    fl->setContentsMargins(7, 7, 7, 7);
    fl->setSpacing(5);
    snipKw_ = new QLineEdit;
    snipKw_->setObjectName(QStringLiteral("SnipKw"));
    snipKw_->setPlaceholderText(QStringLiteral("keyword"));
    snipKw_->setFixedWidth(78);
    snipText_ = new QLineEdit;
    snipText_->setObjectName(QStringLiteral("SnipText"));
    snipText_->setPlaceholderText(QStringLiteral("expansion text"));
    auto *save = new QPushButton(QStringLiteral("Save"));
    save->setObjectName(QStringLiteral("Primary"));
    save->setFocusPolicy(Qt::NoFocus);
    fl->addWidget(snipKw_);
    fl->addWidget(snipText_, 1);
    fl->addWidget(save);
    l->addWidget(form);
    auto *snipList = new QWidget;
    snipListLayout_ = new QVBoxLayout(snipList);
    snipListLayout_->setContentsMargins(0, 0, 0, 0);
    snipListLayout_->setSpacing(2);
    l->addWidget(snipList);

    l->addSpacing(6);

    // --- Cloud sync ---
    section(QStringLiteral("CLOUD SYNC"));
    auto *acct = new QPushButton(QStringLiteral("Account & sync…"));
    acct->setObjectName(QStringLiteral("Ghost"));
    acct->setFocusPolicy(Qt::NoFocus);
    l->addWidget(acct, 0, Qt::AlignLeft);
    hint(QStringLiteral("Copied text syncs to the cloud in plain text. Images stay on this PC."));

    l->addStretch(1);

    connect(save, &QPushButton::clicked, this, [this] { saveSnippet(); });
    connect(acct, &QPushButton::clicked, this, [this] { emit accountRequested(); });
    connect(cbWinV_, &QCheckBox::toggled, this, [this](bool on) {
        if (settings_)
            settings_->set(QStringLiteral("use_win_v"), on);
        emit applyWinVRequested();
    });
    connect(cbHistory_, &QCheckBox::toggled, this, [this](bool on) {
        if (settings_)
            settings_->set(QStringLiteral("disable_windows_history"), on);
        winhistory::setEnabled(!on);
    });
    connect(cbAutostart_, &QCheckBox::toggled, this, [](bool on) {
        if (on)
            autostart::enable();
        else
            autostart::disable();
    });
    connect(cbTray_, &QCheckBox::toggled, this, [this](bool showIcon) {
        if (settings_)
            settings_->set(QStringLiteral("hide_tray_icon"), !showIcon);
        emit hideTrayChanged(!showIcon);
    });
    return area;
}

void Popup::refreshData() { allItems_ = db_->search(QString(), 2000); }

void Popup::applyFilter() {
    QString query = search_->text().trimmed();
    if (searchVisible_ && query.isEmpty()) {
        hideSearch();
        query.clear();
    }
    QList<db::Item> results;
    if (query.isEmpty()) {
        results = allItems_;
    } else {
        struct Scored { int pinned; int score; const db::Item *item; };
        QList<Scored> scored;
        for (const db::Item &it : allItems_) {
            const QString hay = it.preview + QLatin1Char(' ') + it.content;
            const int sc = fuzzy::score(query, hay);
            if (sc >= fuzzy::kMinScore)
                scored.append({it.pinned ? 1 : 0, sc, &it});
        }
        std::stable_sort(scored.begin(), scored.end(), [](const Scored &a, const Scored &b) {
            if (a.pinned != b.pinned) return a.pinned > b.pinned;
            return a.score > b.score;
        });
        for (const Scored &s : scored)
            results.append(*s.item);
    }
    model_->setHistory(results);
    clearBtn_->setVisible(!query.isEmpty());
    if (model_->rowCount() > 0)
        list_->setCurrentIndex(model_->index(0, 0));
    updateFooter();
}

void Popup::setView(bool more) {
    moreView_ = more;
    if (more) {
        // Reflect current settings/registry without firing the toggle slots.
        const QSignalBlocker b1(cbWinV_), b2(cbHistory_), b3(cbAutostart_), b4(cbTray_);
        cbWinV_->setChecked(settings_ && settings_->getBool(QStringLiteral("use_win_v")));
        cbHistory_->setChecked(winhistory::state() == 0);
        cbAutostart_->setChecked(autostart::isEnabled());
        cbTray_->setChecked(!(settings_ && settings_->getBool(QStringLiteral("hide_tray_icon"))));
        refreshSnippets();
        if (searchVisible_)
            hideSearch();
        stack_->setCurrentIndex(1);
        moreBtn_->setText(QStringLiteral("‹ Back"));
    } else {
        stack_->setCurrentIndex(0);
        moreBtn_->setText(QStringLiteral("More"));
        list_->setFocus();
    }
    updateFooter();
}

void Popup::refreshSnippets() {
    while (QLayoutItem *item = snipListLayout_->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }
    const QList<db::Snippet> snips = db_->allSnippets();
    if (snips.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("No snippets yet — add one above."));
        empty->setObjectName(QStringLiteral("Hint"));
        snipListLayout_->addWidget(empty);
        return;
    }
    for (const db::Snippet &s : snips) {
        auto *row = new QWidget;
        auto *rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(4);
        auto *btn = new QPushButton(s.triggerKeyword + QStringLiteral("  —  ") + s.expansionText);
        btn->setObjectName(QStringLiteral("SnipItem"));
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setCursor(Qt::PointingHandCursor);
        auto *del = new QPushButton(QString(QChar(0x2715)));
        del->setObjectName(QStringLiteral("Ghost"));
        del->setFixedSize(24, 24);
        del->setFocusPolicy(Qt::NoFocus);
        rl->addWidget(btn, 1);
        rl->addWidget(del);
        snipListLayout_->addWidget(row);
        const qint64 id = s.id;
        const QString exp = s.expansionText;
        connect(btn, &QPushButton::clicked, this, [this, exp] { pasteText(exp); });
        connect(del, &QPushButton::clicked, this, [this, id] {
            db_->deleteSnippet(id);
            refreshSnippets();
        });
    }
}

void Popup::saveSnippet() {
    const QString kw = snipKw_->text().trimmed();
    const QString text = snipText_->text().trimmed();
    if (kw.isEmpty() || text.isEmpty())
        return;
    db_->addSnippet(kw, text);
    snipKw_->clear();
    snipText_->clear();
    refreshSnippets();
}

void Popup::showSearch() {
    searchRow_->show();
    searchVisible_ = true;
    search_->setFocus();
    updateFooter();
}

void Popup::hideSearch() {
    const QSignalBlocker block(search_);
    search_->clear();
    searchRow_->hide();
    searchVisible_ = false;
    if (shown_ && !moreView_)
        list_->setFocus();
    updateFooter();
}

void Popup::onEscape() {
    if (searchVisible_) {
        hideSearch();
        applyFilter();
    } else if (moreView_) {
        setView(false);
    } else {
        hidePopup();
    }
}

void Popup::moveSelection(int delta) {
    const int n = model_->rowCount();
    if (n == 0)
        return;
    const int cur = list_->currentIndex().isValid() ? list_->currentIndex().row() : 0;
    const QModelIndex idx = model_->index(std::clamp(cur + delta, 0, n - 1), 0);
    list_->setCurrentIndex(idx);
    list_->scrollTo(idx, QAbstractItemView::EnsureVisible);
}

void Popup::pasteCurrent() {
    if (moreView_)
        return;
    QModelIndex idx = list_->currentIndex();
    if (!idx.isValid() && model_->rowCount() > 0)
        idx = model_->index(0, 0);
    if (idx.isValid())
        pasteHistory(idx);
}

void Popup::pasteHistory(const QModelIndex &index) {
    if (!index.isValid())
        return;
    const db::Item it = model_->itemAt(index.row());
    void *target = targetHwnd_;
    hidePopup();
    if (it.contentType == QLatin1String("text")) {
        guard::markSelfText(it.content);
        winclip::setText(it.content);
    } else if (it.contentType == QLatin1String("files")) {
        guard::markSelfFiles(it.content);
        winclip::setFiles(it.content.split(QLatin1Char('\n')));
    } else {  // image
        guard::markSelfImage();
        winclip::setImageFromPng(it.content);
    }
    QTimer::singleShot(90, this, [target] { winclip::pasteInto(target); });
}

void Popup::pasteText(const QString &text) {
    void *target = targetHwnd_;
    hidePopup();
    guard::markSelfText(text);
    winclip::setText(text);
    QTimer::singleShot(90, this, [target] { winclip::pasteInto(target); });
}

void Popup::pinIndex(const QModelIndex &index) {
    const db::Item it = model_->itemAt(index.row());
    db_->setPinned(it.id, !it.pinned);
    refreshData();
    applyFilter();
}

void Popup::deleteIndex(const QModelIndex &index) {
    db_->deleteItem(model_->itemAt(index.row()).id);
    refreshData();
    applyFilter();
}

void Popup::updateFooter() {
    if (moreView_) {
        hint_->setText(QStringLiteral("Settings · snippets · sync"));
        count_->clear();
        return;
    }
    const int total = allItems_.size();
    if (!search_->text().trimmed().isEmpty())
        count_->setText(QStringLiteral("%1 of %2").arg(model_->rowCount()).arg(total));
    else
        count_->setText(QStringLiteral("%1 item%2").arg(total).arg(total == 1 ? QString() : QStringLiteral("s")));
    hint_->setText(searchVisible_ ? QStringLiteral("Enter paste  ·  ↑↓ move  ·  Esc clear")
                                  : QStringLiteral("Start typing to search"));
}

void Popup::positionBottomRight() {
    QScreen *scr = QGuiApplication::screenAt(QCursor::pos());
    if (!scr)
        scr = QGuiApplication::primaryScreen();
    const QRect a = scr->availableGeometry();
    int x = a.right() - kWinW - kMargin + 1;
    int y = a.bottom() - kWinH - kMargin + 1;
    x = std::max(a.left() + kMargin, x);
    y = std::max(a.top() + kMargin, y);
    move(x, y);
}

void Popup::roundCorners() {
    if (cornersDone_)
        return;
    const auto hwnd = reinterpret_cast<HWND>(winId());
    const DWORD pref = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
    cornersDone_ = true;
}

void Popup::toggle(void *targetHwnd) {
    if (shown_)
        hidePopup();
    else
        showPopup(targetHwnd);
}

void Popup::showPopup(void *targetHwnd) {
    targetHwnd_ = targetHwnd;
    shown_ = true;
    ready_ = false;
    refreshData();
    moreView_ = false;
    stack_->setCurrentIndex(0);
    moreBtn_->setText(QStringLiteral("More"));
    hideSearch();
    applyFilter();
    positionBottomRight();
    show();
    raise();
    activateWindow();
    roundCorners();
    list_->setFocus();
    if (model_->rowCount() > 0)
        list_->setCurrentIndex(model_->index(0, 0));
    QTimer::singleShot(60, this, [this] { ready_ = true; });
}

void Popup::hidePopup() {
    shown_ = false;
    ready_ = false;
    hide();
}

bool Popup::event(QEvent *e) {
    if (e->type() == QEvent::WindowDeactivate && shown_ && ready_)
        hidePopup();
    return QWidget::event(e);
}

bool Popup::eventFilter(QObject *obj, QEvent *e) {
    if (obj == list_->viewport() && e->type() == QEvent::Wheel) {
        auto *we = static_cast<QWheelEvent *>(e);
        const int dy = we->angleDelta().y();
        if (dy != 0) {
            QScrollBar *sb = list_->verticalScrollBar();
            sb->setValue(sb->value() - (dy * kWheelStepPx) / 120);
            return true;  // consume: we scrolled by our own (slower) step
        }
    }
    if (e->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(e);
        const int key = ke->key();
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            pasteCurrent();
            return true;
        }
        if (key == Qt::Key_Escape) {
            onEscape();
            return true;
        }
        if (obj == search_) {
            if (key == Qt::Key_Up) { moveSelection(-1); return true; }
            if (key == Qt::Key_Down) { moveSelection(1); return true; }
            return false;
        }
        if (obj == list_) {
            const QString t = ke->text();
            const bool mod = ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier);
            if (!moreView_ && !searchVisible_ && !mod && !t.isEmpty() && t.at(0).isPrint() &&
                t.at(0) != QLatin1Char(' ')) {
                showSearch();
                search_->setText(search_->text() + t);
                return true;
            }
            return false;
        }
    }
    return QWidget::eventFilter(obj, e);
}
