#include "fuzzy.h"

#include <QRegularExpression>
#include <QStringList>
#include <algorithm>
#include <vector>

namespace fuzzy {
namespace {

// Longest common subsequence length (basis of rapidfuzz's Indel ratio).
int lcsLen(const QString &a, const QString &b) {
    const int n = a.size(), m = b.size();
    if (n == 0 || m == 0)
        return 0;
    std::vector<int> prev(m + 1, 0), cur(m + 1, 0);
    for (int i = 1; i <= n; ++i) {
        const QChar ca = a[i - 1];
        for (int j = 1; j <= m; ++j) {
            if (ca == b[j - 1])
                cur[j] = prev[j - 1] + 1;
            else
                cur[j] = std::max(prev[j], cur[j - 1]);
        }
        std::swap(prev, cur);
    }
    return prev[m];
}

// Normalized Indel similarity, 0..100 (rapidfuzz fuzz.ratio).
double ratio(const QString &a, const QString &b) {
    const int total = a.size() + b.size();
    if (total == 0)
        return 100.0;
    return 100.0 * 2.0 * lcsLen(a, b) / total;
}

// Best ratio of the shorter string against equal-length windows of the longer.
double partialRatio(const QString &a, const QString &b) {
    const QString &sh = a.size() <= b.size() ? a : b;
    const QString &lo = a.size() <= b.size() ? b : a;
    if (sh.isEmpty())
        return 0.0;
    double best = 0.0;
    for (int i = 0; i + sh.size() <= lo.size(); ++i) {
        best = std::max(best, ratio(sh, lo.mid(i, sh.size())));
        if (best >= 100.0)
            break;
    }
    return best;
}

QString tokenSort(const QString &s) {
    static const QRegularExpression ws(QStringLiteral("\\s+"));
    QStringList toks = s.split(ws, Qt::SkipEmptyParts);
    toks.sort();
    return toks.join(QLatin1Char(' '));
}

// Approximation of rapidfuzz WRatio: for similar-length strings use the plain and
// token-sorted ratios; for very different lengths lean on partial_ratio, scaled
// down the way WRatio does (0.9, or 0.6 when the length gap is large).
int wratio(const QString &query, const QString &text) {
    const QString q = query.toLower().trimmed();
    const QString t = text.toLower().trimmed();
    if (q.isEmpty() || t.isEmpty())
        return 0;

    const int lenMax = std::max(q.size(), t.size());
    const int lenMin = std::min(q.size(), t.size());
    const double lenRatio = lenMin ? double(lenMax) / lenMin : 0.0;

    double score = ratio(q, t);
    if (lenRatio < 1.5) {
        score = std::max(score, ratio(tokenSort(q), tokenSort(t)) * 0.95);
    } else {
        const double scale = lenRatio > 8.0 ? 0.6 : 0.9;
        score = std::max(score, partialRatio(q, t) * scale);
    }
    return int(score + 0.5);
}

}  // namespace

int score(const QString &query, const QString &text) {
    int s = wratio(query, text);
    if (text.toLower().contains(query.toLower().trimmed()))
        s = std::max(s, kSubstringScore);
    return s;
}

}  // namespace fuzzy
