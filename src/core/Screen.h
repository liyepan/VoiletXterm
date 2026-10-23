#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include <deque>

#include "Cell.h"

namespace vx {

// Holds the character grid, the scrollback and the terminal pen/modes.
// Byte stream parsing lives in VtParser, which drives this class.
class Screen : public QObject
{
    Q_OBJECT
public:
    explicit Screen(QObject *parent = nullptr);

    void initialize(int cols, int rows);

    int columns() const { return m_cols; }
    int rows() const { return m_rows; }

    int totalLines() const { return historyVisible() + m_rows; }
    int historyCount() const { return int(m_history.size()); }
    // history exposed to the view (the alternate screen has no scrollback)
    int historyVisible() const { return m_altActive ? 0 : int(m_history.size()); }
    // index 0 == oldest scrollback line, totalLines() - 1 == last screen line
    const Cell *lineData(int index) const;
    const QVector<Cell> &lineVector(int index) const;

    QString graphemeText(quint16 ref) const;

    int cursorRow() const { return m_cy; }
    int cursorCol() const { return m_cx; }
    bool cursorVisible() const { return m_cursorVisible; }
    bool alternateScreen() const { return m_altActive; }
    bool bracketedPaste() const { return m_bracketedPaste; }
    bool applicationCursorKeys() const { return m_cursorKeysApp; }
    bool applicationKeypad() const { return m_keypadApp; }
    bool reverseVideo() const { return m_reverseVideo; }
    Color defaultForeground() const { return m_defaultFg; }
    Color defaultBackground() const { return m_defaultBg; }

    void resize(int cols, int rows);
    void reset();
    void clearScrollback();

    void putChar(char32_t cp);
    void putCombining(char32_t cp);
    void repeatLastChar(int count);

    void carriageReturn();
    void lineFeed();
    void reverseIndex();
    void backSpace();
    void tab(int count = 1);
    void backTab(int count = 1);
    void alignTest();

    void moveTo(int row, int col);
    void cursorUp(int n);
    void cursorDown(int n);
    void cursorLeft(int n);
    void cursorRight(int n);
    void setCursorCol(int col);
    void setCursorRow(int row);

    void eraseInDisplay(int mode);
    void eraseInLine(int mode);
    void eraseChars(int count);
    void insertChars(int count);
    void deleteChars(int count);
    void insertLines(int count);
    void deleteLines(int count);
    void scrollUp(int count);
    void scrollDown(int count);
    void setMargins(int top, int bottom);
    void resetMargins();

    void saveCursor();
    void restoreCursor();

    void setSgr(const QVector<int> &params);
    void setPrivateMode(int mode, bool on);
    void setAnsiMode(int mode, bool on);
    void setCharset(int slot, char designator);
    void selectCharset(int slot);

    void ringBell();
    void setTitle(const QString &title);
    void setWorkingDirectory(const QString &dir);
    void requestClipboard(const QString &text);

    void reportDeviceAttributes();
    void reportStatus(int mode);
    void reportCursorPosition();

signals:
    void titleChanged(const QString &title);
    void workingDirectoryChanged(const QString &dir);
    void response(const QByteArray &data);
    void bell();
    void clipboardRequested(const QString &text);

private:
    QVector<QVector<Cell>> &grid();
    const QVector<QVector<Cell>> &grid() const;
    Cell blankCell() const;
    void clearRow(int row);
    void clearCellAt(int row, int col);
    void sanitizeWide(int row);
    void pushHistory(QVector<Cell> &&row);
    void scrollUpRegion(int top, int bottom, int count);
    void scrollDownRegion(int top, int bottom, int count);
    static void resizeRow(QVector<Cell> &row, int cols);
    char32_t mapCharsetChar(char32_t cp) const;
    quint16 addGrapheme(quint16 previous, char32_t cp);

    int m_cols = 80;
    int m_rows = 24;
    QVector<QVector<Cell>> m_primary;
    QVector<QVector<Cell>> m_alt;
    bool m_altActive = false;
    std::deque<QVector<Cell>> m_history;
    int m_historyLimit = 5000;

    int m_cx = 0;
    int m_cy = 0;
    bool m_pendingWrap = false;
    int m_top = 0;
    int m_bot = 23;

    Color m_fg = kColorDefault;
    Color m_bg = kColorDefault;
    Color m_defaultFg = kColorDefault;
    Color m_defaultBg = kColorDefault;
    quint16 m_attr = 0;

    bool m_autowrap = true;
    bool m_origin = false;
    bool m_insert = false;
    bool m_cursorVisible = true;
    bool m_cursorKeysApp = false;
    bool m_keypadApp = false;
    bool m_reverseVideo = false;
    bool m_bracketedPaste = false;
    int m_mouseMode = 0;

    // saved cursor (DECSC)
    int m_savedCx = 0;
    int m_savedCy = 0;
    Color m_savedFg = kColorDefault;
    Color m_savedBg = kColorDefault;
    quint16 m_savedAttr = 0;
    bool m_savedOrigin = false;
    bool m_savedAutowrap = true;
    int m_savedCharset[2] = {0, 0};

    int m_charset[2] = {0, 0};   // 0 == ASCII, '0' == DEC special graphics, 'A' == UK
    int m_activeCharset = 0;
    char32_t m_lastChar = U' ';

    QVector<QString> m_graphemes;
};

} // namespace vx
