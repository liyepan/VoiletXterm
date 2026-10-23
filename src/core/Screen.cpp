#include "Screen.h"

#include "CharWidth.h"

#include <QDebug>
#include <QString>

#include <algorithm>
#include <utility>

namespace vx {

namespace {

// DEC Special Graphics (0x5F .. 0x7E), used by ncurses line drawing fallbacks.
const char32_t kDecGraphics[32] = {
    0x0020, 0x25C6, 0x2592, 0x2409, 0x240C, 0x240D, 0x240A, 0x00B0,
    0x00B1, 0x2424, 0x240B, 0x2518, 0x2510, 0x250C, 0x2514, 0x253C,
    0x23BA, 0x23BB, 0x2500, 0x23BC, 0x23BD, 0x251C, 0x2524, 0x2534,
    0x252C, 0x2502, 0x2264, 0x2265, 0x03C0, 0x2260, 0x00A3, 0x00B7,
};

} // namespace

Screen::Screen(QObject *parent)
    : QObject(parent)
{
    initialize(80, 24);
}

void Screen::initialize(int cols, int rows)
{
    m_cols = std::max(1, cols);
    m_rows = std::max(1, rows);
    m_primary.clear();
    m_primary.resize(m_rows);
    m_alt.clear();
    m_alt.resize(m_rows);
    for (auto &row : m_primary)
        resizeRow(row, m_cols);
    for (auto &row : m_alt)
        resizeRow(row, m_cols);
    m_history.clear();
    m_altActive = false;
    m_cx = m_cy = 0;
    m_pendingWrap = false;
    m_top = 0;
    m_bot = m_rows - 1;
    m_fg = m_bg = kColorDefault;
    m_attr = 0;
    m_autowrap = true;
    m_origin = false;
    m_insert = false;
    m_cursorVisible = true;
    m_cursorKeysApp = false;
    m_keypadApp = false;
    m_reverseVideo = false;
    m_bracketedPaste = false;
    m_charset[0] = m_charset[1] = 0;
    m_activeCharset = 0;
    m_graphemes.clear();
}

void Screen::reset()
{
    const int cols = m_cols;
    const int rows = m_rows;
    initialize(cols, rows);
    emit titleChanged(QString());
}

QVector<QVector<Cell>> &Screen::grid()
{
    return m_altActive ? m_alt : m_primary;
}

const QVector<QVector<Cell>> &Screen::grid() const
{
    return m_altActive ? m_alt : m_primary;
}

Cell Screen::blankCell() const
{
    Cell c;
    c.bg = m_bg;
    return c;
}

void Screen::resizeRow(QVector<Cell> &row, int cols)
{
    row.resize(cols);
}

void Screen::resize(int cols, int rows)
{
    cols = std::max(1, cols);
    rows = std::max(1, rows);
    if (cols == m_cols && rows == m_rows)
        return;

    const int oldRows = m_rows;

    if (cols != m_cols) {
        for (auto &row : m_primary) {
            row.resize(cols);
            // drop a wide tail that lost its head
            if (!row.isEmpty() && row.last().isWideTail())
                row.last() = Cell();
            if (!row.isEmpty() && row.last().isWideHead())
                row.last() = Cell();
        }
        for (auto &row : m_alt) {
            row.resize(cols);
            if (!row.isEmpty() && (row.last().isWideTail() || row.last().isWideHead()))
                row.last() = Cell();
        }
        m_cols = cols;
        if (m_cx >= m_cols)
            m_cx = m_cols - 1;
    }

    if (rows != oldRows) {
        if (!m_altActive) {
            if (rows < oldRows) {
                const int drop = oldRows - rows;
                for (int i = 0; i < drop; ++i) {
                    if (!m_primary.isEmpty())
                        pushHistory(std::move(m_primary[0]));
                    if (!m_primary.isEmpty())
                        m_primary.remove(0);
                }
                m_cy -= drop;
            } else {
                const int add = rows - oldRows;
                int taken = 0;
                while (taken < add && !m_history.empty()) {
                    m_primary.prepend(std::move(m_history.back()));
                    m_history.pop_back();
                    ++taken;
                }
                m_cy += taken;
                m_primary.resize(rows);
                for (int i = 0; i < m_primary.size(); ++i)
                    resizeRow(m_primary[i], m_cols);
            }
        }
        m_alt.resize(rows);
        for (auto &row : m_alt)
            resizeRow(row, m_cols);
        if (m_primary.size() < rows)
            m_primary.resize(rows);
        if (m_primary.size() > rows)
            m_primary.resize(rows);
        m_rows = rows;
        m_cy = std::clamp(m_cy, 0, m_rows - 1);
        m_cx = std::clamp(m_cx, 0, m_cols - 1);
        m_top = 0;
        m_bot = m_rows - 1;
        m_pendingWrap = false;
    }
}

void Screen::clearScrollback()
{
    m_history.clear();
}

void Screen::pushHistory(QVector<Cell> &&row)
{
    m_history.push_back(std::move(row));
    while (int(m_history.size()) > m_historyLimit)
        m_history.pop_front();
}

const QVector<Cell> &Screen::lineVector(int index) const
{
    static const QVector<Cell> kEmpty;
    if (index < 0)
        return kEmpty;
    const int hist = m_altActive ? 0 : int(m_history.size());
    if (index < hist)
        return m_history[std::size_t(index)];
    const int r = index - hist;
    const auto &g = grid();
    if (r < 0 || r >= g.size())
        return kEmpty;
    return g[r];
}

const Cell *Screen::lineData(int index) const
{
    const QVector<Cell> &v = lineVector(index);
    return v.isEmpty() ? nullptr : v.constData();
}

QString Screen::graphemeText(quint16 ref) const
{
    if (ref == 0 || ref > m_graphemes.size())
        return QString();
    return m_graphemes[ref - 1];
}

quint16 Screen::addGrapheme(quint16 previous, char32_t cp)
{
    QString text;
    if (previous != 0 && previous <= m_graphemes.size())
        text = m_graphemes[previous - 1];
    text += QString::fromUcs4(&cp, 1);
    if (m_graphemes.size() >= 65534)
        return previous;
    m_graphemes.append(text);
    return quint16(m_graphemes.size());
}

char32_t Screen::mapCharsetChar(char32_t cp) const
{
    const int set = m_charset[m_activeCharset];
    if (set == '0' && cp >= 0x5F && cp <= 0x7E)
        return kDecGraphics[cp - 0x5F];
    if (set == 'A' && cp == 0x23)
        return 0x00A3;
    return cp;
}

void Screen::putChar(char32_t cp)
{
    if (cp < 0x20)
        return;

    cp = mapCharsetChar(cp);

    int w = charWidth(cp);
    if (w == 0) {
        putCombining(cp);
        return;
    }
    if (w < 0) {
        cp = 0xFFFD;
        w = 1;
    }
    if (m_cols < 2)
        w = 1;

    if (m_pendingWrap) {
        m_cx = 0;
        lineFeed();
        m_pendingWrap = false;
    }

    if (w == 2 && m_cx >= m_cols - 1) {
        if (m_autowrap) {
            m_cx = 0;
            lineFeed();
        } else {
            m_cx = m_cols - 2;
        }
    }

    auto &line = grid()[m_cy];
    if (line.size() != m_cols)
        line.resize(m_cols);

    if (m_insert) {
        const int shift = std::min(w, m_cols - m_cx);
        line.insert(m_cx, shift, blankCell());
        line.resize(m_cols);
    }

    clearCellAt(m_cy, m_cx);
    if (w == 2)
        clearCellAt(m_cy, m_cx + 1);

    Cell c;
    c.ch = cp;
    c.fg = m_fg;
    c.bg = m_bg;
    c.attr = m_attr;
    if (w == 2)
        c.attr |= AttrWide;
    c.extra = 0;
    line[m_cx] = c;

    if (w == 2) {
        Cell tail;
        tail.ch = 0;
        tail.fg = m_fg;
        tail.bg = m_bg;
        tail.attr = quint16(m_attr | AttrWideTail);
        line[m_cx + 1] = tail;
    }

    m_lastChar = cp;
    m_cx += w;
    if (m_cx >= m_cols) {
        m_cx = m_cols - 1;
        m_pendingWrap = m_autowrap;
    }
}

void Screen::putCombining(char32_t cp)
{
    int col = m_cx;
    if (!m_pendingWrap)
        --col;
    if (col < 0)
        col = 0;
    auto &line = grid()[m_cy];
    if (col < line.size() && line[col].isWideTail() && col > 0)
        --col;
    if (col < 0 || col >= line.size())
        return;
    Cell &c = line[col];
    if (c.ch == 0 && c.isWideTail())
        return;
    c.extra = addGrapheme(c.extra, cp);
}

void Screen::repeatLastChar(int count)
{
    for (int i = 0; i < count; ++i)
        putChar(m_lastChar);
}

void Screen::carriageReturn()
{
    m_cx = 0;
    m_pendingWrap = false;
}

void Screen::lineFeed()
{
    m_pendingWrap = false;
    if (m_cy == m_bot)
        scrollUp(1);
    else if (m_cy < m_rows - 1)
        ++m_cy;
}

void Screen::reverseIndex()
{
    m_pendingWrap = false;
    if (m_cy == m_top)
        scrollDown(1);
    else if (m_cy > 0)
        --m_cy;
}

void Screen::backSpace()
{
    m_pendingWrap = false;
    if (m_cx > 0)
        --m_cx;
}

void Screen::tab(int count)
{
    m_pendingWrap = false;
    for (int i = 0; i < count; ++i) {
        const int next = ((m_cx / 8) + 1) * 8;
        m_cx = std::min(next, m_cols - 1);
    }
}

void Screen::backTab(int count)
{
    m_pendingWrap = false;
    for (int i = 0; i < count; ++i) {
        const int prev = ((m_cx + 7) / 8 - 1) * 8;
        m_cx = std::max(prev, 0);
    }
}

void Screen::alignTest()
{
    for (int r = 0; r < m_rows; ++r) {
        auto &line = grid()[r];
        for (int c = 0; c < m_cols; ++c) {
            Cell cell;
            cell.ch = U'E';
            cell.fg = m_fg;
            cell.bg = m_bg;
            line[c] = cell;
        }
    }
    m_cx = 0;
    m_cy = 0;
    m_pendingWrap = false;
}

void Screen::moveTo(int row, int col)
{
    m_pendingWrap = false;
    if (m_origin) {
        row += m_top;
        row = std::clamp(row, m_top, m_bot);
    }
    m_cy = std::clamp(row, 0, m_rows - 1);
    m_cx = std::clamp(col, 0, m_cols - 1);
}

void Screen::cursorUp(int n)
{
    m_pendingWrap = false;
    const int floorRow = (m_cy >= m_top) ? m_top : 0;
    m_cy = std::max(m_cy - n, floorRow);
}

void Screen::cursorDown(int n)
{
    m_pendingWrap = false;
    const int ceilRow = (m_cy <= m_bot) ? m_bot : m_rows - 1;
    m_cy = std::min(m_cy + n, ceilRow);
}

void Screen::cursorLeft(int n)
{
    m_pendingWrap = false;
    m_cx = std::max(m_cx - n, 0);
}

void Screen::cursorRight(int n)
{
    m_pendingWrap = false;
    m_cx = std::min(m_cx + n, m_cols - 1);
}

void Screen::setCursorCol(int col)
{
    m_pendingWrap = false;
    m_cx = std::clamp(col, 0, m_cols - 1);
}

void Screen::setCursorRow(int row)
{
    m_pendingWrap = false;
    if (m_origin)
        row = std::clamp(m_top + row, m_top, m_bot);
    m_cy = std::clamp(row, 0, m_rows - 1);
}

void Screen::clearRow(int row)
{
    if (row < 0 || row >= m_rows)
        return;
    auto &line = grid()[row];
    line.resize(m_cols);
    const Cell blank = blankCell();
    for (int c = 0; c < line.size(); ++c)
        line[c] = blank;
}

void Screen::clearCellAt(int row, int col)
{
    if (row < 0 || row >= m_rows)
        return;
    auto &line = grid()[row];
    if (col < 0 || col >= line.size())
        return;
    if (line[col].isWideHead() && col + 1 < line.size() && line[col + 1].isWideTail())
        line[col + 1] = blankCell();
    else if (line[col].isWideTail() && col > 0 && line[col - 1].isWideHead())
        line[col - 1] = blankCell();
    line[col] = blankCell();
}

void Screen::sanitizeWide(int row)
{
    if (row < 0 || row >= m_rows)
        return;
    auto &line = grid()[row];
    for (int c = 0; c < line.size(); ++c) {
        if (line[c].isWideHead()) {
            if (c + 1 < line.size() && line[c + 1].isWideTail())
                ++c;
            else
                line[c] = blankCell();
        } else if (line[c].isWideTail()) {
            line[c] = blankCell();
        }
    }
}

void Screen::eraseInDisplay(int mode)
{
    switch (mode) {
    case 0:
        eraseInLine(0);
        for (int r = m_cy + 1; r < m_rows; ++r)
            clearRow(r);
        break;
    case 1:
        eraseInLine(1);
        for (int r = 0; r < m_cy; ++r)
            clearRow(r);
        break;
    case 2:
        for (int r = 0; r < m_rows; ++r)
            clearRow(r);
        break;
    case 3:
        clearScrollback();
        break;
    default:
        break;
    }
}

void Screen::eraseInLine(int mode)
{
    int from = 0;
    int to = m_cols;
    if (mode == 0) {
        from = m_cx;
    } else if (mode == 1) {
        to = std::min(m_cx + 1, m_cols);
    } else if (mode == 2) {
        from = 0;
        to = m_cols;
    } else {
        return;
    }
    for (int c = from; c < to; ++c)
        clearCellAt(m_cy, c);
    m_pendingWrap = false;
}

void Screen::eraseChars(int count)
{
    for (int i = 0; i < count && m_cx + i < m_cols; ++i)
        clearCellAt(m_cy, m_cx + i);
}

void Screen::insertChars(int count)
{
    if (count <= 0)
        return;
    count = std::min(count, m_cols - m_cx);
    auto &line = grid()[m_cy];
    line.insert(m_cx, count, blankCell());
    line.resize(m_cols);
    sanitizeWide(m_cy);
    m_pendingWrap = false;
}

void Screen::deleteChars(int count)
{
    if (count <= 0)
        return;
    count = std::min(count, m_cols - m_cx);
    auto &line = grid()[m_cy];
    line.remove(m_cx, count);
    line.append(QVector<Cell>(count, blankCell()));
    sanitizeWide(m_cy);
    m_pendingWrap = false;
}

void Screen::insertLines(int count)
{
    count = std::min(count, m_bot - m_cy + 1);
    for (int i = 0; i < count; ++i) {
        if (m_bot > m_cy)
            grid()[m_bot] = std::move(grid()[m_bot - 1]);
        for (int r = m_bot - 1; r > m_cy; --r)
            grid()[r] = std::move(grid()[r - 1]);
        clearRow(m_cy);
    }
    m_cx = 0;
    m_pendingWrap = false;
}

void Screen::deleteLines(int count)
{
    count = std::min(count, m_bot - m_cy + 1);
    for (int i = 0; i < count; ++i) {
        for (int r = m_cy; r < m_bot; ++r)
            grid()[r] = std::move(grid()[r + 1]);
        clearRow(m_bot);
    }
    m_cx = 0;
    m_pendingWrap = false;
}

void Screen::scrollUpRegion(int top, int bottom, int count)
{
    count = std::min(count, bottom - top + 1);
    const bool toHistory = (top == 0 && bottom == m_rows - 1 && !m_altActive);
    for (int i = 0; i < count; ++i) {
        if (toHistory)
            pushHistory(std::move(grid()[top]));
        for (int r = top; r < bottom; ++r)
            grid()[r] = std::move(grid()[r + 1]);
        clearRow(bottom);
    }
}

void Screen::scrollDownRegion(int top, int bottom, int count)
{
    count = std::min(count, bottom - top + 1);
    for (int i = 0; i < count; ++i) {
        for (int r = bottom; r > top; --r)
            grid()[r] = std::move(grid()[r - 1]);
        clearRow(top);
    }
}

void Screen::scrollUp(int count)
{
    scrollUpRegion(m_top, m_bot, count);
    m_pendingWrap = false;
}

void Screen::scrollDown(int count)
{
    scrollDownRegion(m_top, m_bot, count);
    m_pendingWrap = false;
}

void Screen::setMargins(int top, int bottom)
{
    top = std::clamp(top, 0, m_rows - 1);
    bottom = std::clamp(bottom, 0, m_rows - 1);
    if (top >= bottom)
        return;
    m_top = top;
    m_bot = bottom;
    moveTo(0, 0);
}

void Screen::resetMargins()
{
    m_top = 0;
    m_bot = m_rows - 1;
    m_cx = m_cy = 0;
    m_pendingWrap = false;
}

void Screen::saveCursor()
{
    m_savedCx = m_cx;
    m_savedCy = m_cy;
    m_savedFg = m_fg;
    m_savedBg = m_bg;
    m_savedAttr = m_attr;
    m_savedOrigin = m_origin;
    m_savedAutowrap = m_autowrap;
    m_savedCharset[0] = m_charset[0];
    m_savedCharset[1] = m_charset[1];
}

void Screen::restoreCursor()
{
    m_cx = std::clamp(m_savedCx, 0, m_cols - 1);
    m_cy = std::clamp(m_savedCy, 0, m_rows - 1);
    m_fg = m_savedFg;
    m_bg = m_savedBg;
    m_attr = m_savedAttr;
    m_origin = m_savedOrigin;
    m_autowrap = m_savedAutowrap;
    m_charset[0] = m_savedCharset[0];
    m_charset[1] = m_savedCharset[1];
    m_pendingWrap = false;
}

void Screen::setSgr(const QVector<int> &params)
{
    const int n = params.size();
    if (n == 0) {
        m_fg = m_bg = kColorDefault;
        m_attr = 0;
        return;
    }
    for (int i = 0; i < n; ++i) {
        const int v = params[i];
        if (v == 0) {
            m_fg = m_bg = kColorDefault;
            m_attr = 0;
        } else if (v == 1) {
            m_attr |= AttrBold;
        } else if (v == 2) {
            m_attr |= AttrDim;
        } else if (v == 3) {
            m_attr |= AttrItalic;
        } else if (v == 4) {
            if (i + 1 < n && params[i + 1] == 0) {
                m_attr &= ~quint16(AttrUnderline | AttrDoubleUnderline);
                ++i;
            } else if (i + 1 < n && (params[i + 1] == 2 || params[i + 1] == 3 || params[i + 1] == 4 || params[i + 1] == 5)) {
                m_attr |= AttrUnderline;
                ++i;
            } else {
                m_attr |= AttrUnderline;
            }
        } else if (v == 5 || v == 6) {
            m_attr |= AttrBlink;
        } else if (v == 7) {
            m_attr |= AttrReverse;
        } else if (v == 8) {
            m_attr |= AttrHidden;
        } else if (v == 9) {
            m_attr |= AttrStrike;
        } else if (v == 21) {
            m_attr |= AttrDoubleUnderline;
        } else if (v == 22) {
            m_attr &= ~quint16(AttrBold | AttrDim);
        } else if (v == 23) {
            m_attr &= ~quint16(AttrItalic);
        } else if (v == 24) {
            m_attr &= ~quint16(AttrUnderline | AttrDoubleUnderline);
        } else if (v == 25) {
            m_attr &= ~quint16(AttrBlink);
        } else if (v == 27) {
            m_attr &= ~quint16(AttrReverse);
        } else if (v == 28) {
            m_attr &= ~quint16(AttrHidden);
        } else if (v == 29) {
            m_attr &= ~quint16(AttrStrike);
        } else if (v >= 30 && v <= 37) {
            m_fg = paletteColor(v - 30);
        } else if (v == 38 || v == 48 || v == 58) {
            Color color = kColorDefault;
            bool ok = false;
            if (i + 1 < n) {
                const int kind = params[++i];
                if (kind == 5 && i + 1 < n) {
                    color = paletteColor(params[++i]);
                    ok = true;
                } else if (kind == 2 && i + 3 < n) {
                    color = rgbColor(params[i + 1], params[i + 2], params[i + 3]);
                    i += 3;
                    ok = true;
                } else if (kind == 2 && i + 4 < n) {
                    color = rgbColor(params[i + 2], params[i + 3], params[i + 4]);
                    i += 4;
                    ok = true;
                }
            }
            if (ok) {
                if (v == 38)
                    m_fg = color;
                else if (v == 48)
                    m_bg = color;
            }
        } else if (v == 39) {
            m_fg = kColorDefault;
        } else if (v >= 40 && v <= 47) {
            m_bg = paletteColor(v - 40);
        } else if (v == 49) {
            m_bg = kColorDefault;
        } else if (v == 53) {
            m_attr |= AttrOverline;
        } else if (v == 55) {
            m_attr &= ~quint16(AttrOverline);
        } else if (v >= 90 && v <= 97) {
            m_fg = paletteColor(v - 90 + 8);
        } else if (v >= 100 && v <= 107) {
            m_bg = paletteColor(v - 100 + 8);
        }
    }
}

void Screen::setPrivateMode(int mode, bool on)
{
    switch (mode) {
    case 1:
        m_cursorKeysApp = on;
        break;
    case 5:
        m_reverseVideo = on;
        break;
    case 6:
        m_origin = on;
        moveTo(0, 0);
        break;
    case 7:
        m_autowrap = on;
        if (!on)
            m_pendingWrap = false;
        break;
    case 12:
        break;
    case 25:
        m_cursorVisible = on;
        break;
    case 47:
    case 1047:
    case 1049:
        if (on != m_altActive) {
            if (on) {
                saveCursor();
                m_altActive = true;
                for (auto &row : m_alt)
                    std::fill(row.begin(), row.end(), Cell());
                m_cx = m_cy = 0;
                m_pendingWrap = false;
                m_top = 0;
                m_bot = m_rows - 1;
            } else {
                m_altActive = false;
                restoreCursor();
                m_top = 0;
                m_bot = m_rows - 1;
            }
        }
        break;
    case 1048:
        if (on)
            saveCursor();
        else
            restoreCursor();
        break;
    case 1000:
    case 1002:
    case 1003:
    case 1006:
    case 1015:
        if (on)
            m_mouseMode = mode;
        else if (m_mouseMode == mode)
            m_mouseMode = 0;
        break;
    case 2004:
        m_bracketedPaste = on;
        break;
    default:
        break;
    }
}

void Screen::setAnsiMode(int mode, bool on)
{
    switch (mode) {
    case 4:
        m_insert = on;
        break;
    case 20:
        break;
    default:
        break;
    }
}

void Screen::setCharset(int slot, char designator)
{
    if (slot < 0 || slot > 1)
        return;
    m_charset[slot] = (designator == 'B' || designator == 0) ? 0 : designator;
}

void Screen::selectCharset(int slot)
{
    m_activeCharset = (slot == 0 || slot == 1) ? slot : 0;
}

void Screen::ringBell()
{
    emit bell();
}

void Screen::setTitle(const QString &title)
{
    emit titleChanged(title);
}

void Screen::setWorkingDirectory(const QString &dir)
{
    emit workingDirectoryChanged(dir);
}

void Screen::requestClipboard(const QString &text)
{
    emit clipboardRequested(text);
}

void Screen::reportDeviceAttributes()
{
    emit response(QByteArrayLiteral("\033[?62;22c"));
}

void Screen::reportStatus(int mode)
{
    if (mode == 5)
        emit response(QByteArrayLiteral("\033[0n"));
    else if (mode == 6)
        reportCursorPosition();
}

void Screen::reportCursorPosition()
{
    QByteArray out = "\033[";
    out += QByteArray::number(m_cy + 1);
    out += ';';
    out += QByteArray::number(m_cx + 1);
    out += 'R';
    emit response(out);
}

} // namespace vx
