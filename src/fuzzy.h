#pragma once

#include <QString>

// Typo-tolerant ranking for the popup search — a port of fuzzy.py's behaviour.
// db.py's FTS5 gives fast exact/prefix search; this gives the interactive fuzzy
// feel by scoring the in-memory working set on every keystroke. Approximates
// rapidfuzz's WRatio (Indel ratio + partial_ratio + token_sort) and adds the
// explicit substring bonus so a literal hit always ranks high.
namespace fuzzy {

constexpr int kMinScore = 55;       // _MIN_SCORE
constexpr int kSubstringScore = 92; // _SUBSTRING_SCORE

// Combined match score (0..100), case-insensitive, including the substring bonus.
int score(const QString &query, const QString &text);

}  // namespace fuzzy
