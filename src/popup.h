#pragma once

#include <QList>
#include <QWidget>

#include "db.h"

class HistoryModel;
class RowDelegate;
class Settings;
class QLineEdit;
class QListView;
class QPushButton;
class QCheckBox;
class QLabel;
class QFrame;
class QStackedWidget;
class QVBoxLayout;
class QModelIndex;

// The popup (the main event): frameless dark rounded window anchored bottom-right,
// like Win+V. A History view (virtualized QListView + custom delegate, search
// hidden until you type) and a compact "More" view (settings, snippets, cloud) are
// swapped by a small bottom-right toggle — no tab bar, to stay compact.
class Popup : public QWidget {
    Q_OBJECT
public:
    explicit Popup(db::Database *db, Settings *settings = nullptr, QWidget *parent = nullptr);

    void toggle(void *targetHwnd);
    void showPopup(void *targetHwnd);
    void hidePopup();
    bool isShown() const { return shown_; }

signals:
    void applyWinVRequested();       // use_win_v changed — re-apply the hook
    void hideTrayChanged(bool hide); // show/hide the tray icon live
    void accountRequested();         // open the Account dialog

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    bool event(QEvent *event) override;

private:
    void buildUi();
    QWidget *buildMorePanel();
    void refreshData();
    void applyFilter();
    void setView(bool more);
    void showSearch();
    void hideSearch();
    void onEscape();
    void moveSelection(int delta);
    void pasteCurrent();
    void pasteHistory(const QModelIndex &index);
    void pasteText(const QString &text);
    void pinIndex(const QModelIndex &index);
    void deleteIndex(const QModelIndex &index);
    void refreshSnippets();
    void saveSnippet();
    void positionBottomRight();
    void roundCorners();
    void updateFooter();

    db::Database *db_;
    Settings *settings_ = nullptr;
    HistoryModel *model_ = nullptr;
    RowDelegate *delegate_ = nullptr;

    QFrame *searchRow_ = nullptr;
    QLineEdit *search_ = nullptr;
    QPushButton *clearBtn_ = nullptr;
    QStackedWidget *stack_ = nullptr;
    QListView *list_ = nullptr;
    QWidget *morePanel_ = nullptr;
    QLineEdit *snipKw_ = nullptr;
    QLineEdit *snipText_ = nullptr;
    QVBoxLayout *snipListLayout_ = nullptr;
    QCheckBox *cbWinV_ = nullptr;
    QCheckBox *cbHistory_ = nullptr;
    QCheckBox *cbTray_ = nullptr;
    QCheckBox *cbAutostart_ = nullptr;
    QLabel *hint_ = nullptr;
    QLabel *count_ = nullptr;
    QPushButton *moreBtn_ = nullptr;

    QList<db::Item> allItems_;
    bool moreView_ = false;

    void *targetHwnd_ = nullptr;
    bool shown_ = false;
    bool ready_ = false;
    bool searchVisible_ = false;
    bool cornersDone_ = false;
};
