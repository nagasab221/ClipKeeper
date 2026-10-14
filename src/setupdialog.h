#pragma once

#include <functional>

class Settings;
class QWidget;

// First-run / settings dialog: the two explicit opt-ins (Win+V via the keyboard
// hook, and turning off Windows' own clipboard history). Port of setup_dialog.py.
// onApplied runs after saving, so the caller can (re)bind the Win+V hook.
void showSetupDialog(QWidget *parent, Settings *settings, std::function<void()> onApplied);
