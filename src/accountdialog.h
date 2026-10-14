#pragma once

#include <functional>

class SupabaseClient;
class SyncEngine;
class Settings;
class QWidget;

// Account window: sign in / out of Supabase sync, upgrade an anonymous identity to
// an email account, pause syncing. Port of account_dialog.py. onChanged fires
// after any account/state change so the caller can kick the sync engine.
void showAccountDialog(QWidget *parent, SupabaseClient *client, SyncEngine *engine,
                       Settings *settings, std::function<void()> onChanged);
