#include "TerminalWidget.h"

#include "../core/Cell.h"
#include "../core/CharWidth.h"
#include "../core/Palette.h"
#include "../session/Transport.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace vx {

namespace {

// Preference order matters: the first two families have a 1:2 Latin/CJK advance
// ratio, so full width characters land exactly on two cells.
QString pickDefaultFontFamily()
{
    const QStringList available = QFontDatabase().families();
#if defined(Q_OS_WIN)
    // Cascadia ships with Windows Terminal, Consolas with every Windows.
    const QStringList preferred = {
        QStringLiteral("Cascadia Mono"),
        QStringLiteral("Cascadia Code"),
        QStringLiteral("Consolas"),
        QStringLiteral("Sarasa Mono SC"),
        QStringLiteral("JetBrains Mono"),
        QStringLiteral("Fira Code"),
        QStringLiteral("DejaVu Sans Mono"),
    };
#elif defined(Q_OS_MACOS)
    const QStringList preferred = {
        QStringLiteral("SF Mono"),
        QStringLiteral("Menlo"),
        QStringLiteral("Monaco"),
        QStringLiteral("JetBrains Mono"),
        QStringLiteral("Sarasa Mono SC"),
    };
#else
    const QStringList preferred = {
        QStringLiteral("Sarasa Mono SC"),
        QStringLiteral("JetBrains Mono"),
        QStringLiteral("WenQuanYi Zen Hei Mono"),
        QStringLiteral("DejaVu Sans Mono"),
        QStringLiteral("Noto Sans Mono"),
        QStringLiteral("Liberation Mono"),
    };
#endif
    for (const QString &family : preferred) {
        if (available.contains(family, Qt::CaseInsensitive))
            return family;
    }
    const QFont systemFixed = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    return systemFixed.family().isEmpty() ? QStringLiteral("monospace") : systemFixed.family();
}

bool isWordChar(char32_t c)
{
    if (c > 0x7F)
        return true;
    if (c >= U'0' && c <= U'9')
        return true;
    if ((c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z'))
        return true;
    switch (c) {
    case U'_': case U'-': case U'.': case U'/': case U':':
    case U'@': case U'~': case U'=': case U'+': case U'%':
        return true;
    default:
        return false;
    }
}

} // namespace

TerminalWidget::TerminalWidget(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    viewport()->setCursor(Qt::IBeamCursor);
    viewport()->setAutoFillBackground(false);

    m_theme = darkTheme();
    for (int i = 0; i < 256; ++i)
        m_palette[i] = QColor::fromRgb(QRgb(0xFF000000u | defaultPaletteColor(i)));
    for (int i = 0; i < 16; ++i)
        m_palette[i] = m_theme.ansi[i];

    m_fontFamily = pickDefaultFontFamily();

    m_screen = new Screen(this);
    m_parser = new VtParser(m_screen);
    m_screen->initialize(80, 24);

    connect(m_screen, &Screen::titleChanged, this, &TerminalWidget::titleChanged);
    connect(m_screen, &Screen::workingDirectoryChanged, this, &TerminalWidget::workingDirectoryChanged);
    connect(m_screen, &Screen::bell, this, &TerminalWidget::bell);
    connect(m_screen, &Screen::response, this, [this](const QByteArray &data) { sendBytes(data); });
    connect(m_screen, &Screen::clipboardRequested, this, [this](const QString &text) { copyToClipboards(text); });

    m_blinkTimer.setInterval(530);
    connect(&m_blinkTimer, &QTimer::timeout, this, &TerminalWidget::onBlinkTimer);
    m_blinkTimer.start();

    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &TerminalWidget::onScrollBarMoved);

    updateFonts();
    updateScrollBar();
}

TerminalWidget::~TerminalWidget() = default;

void TerminalWidget::setTransport(Transport *transport)
{
    if (m_transport == transport)
        return;
    releaseTransport();
    m_transport = transport;
    if (!m_transport)
        return;

    connect(m_transport, &Transport::dataReceived, this, &TerminalWidget::onDataReceived);
    connect(m_transport, &Transport::finished, this, &TerminalWidget::onTransportFinished);
    connect(m_transport, &Transport::connected, this, [this] { viewport()->update(); });

    m_transport->start(m_screen->columns(), m_screen->rows());
}

void TerminalWidget::releaseTransport()
{
    if (!m_transport)
        return;
    m_transport->disconnect(this);
    m_transport->deleteLater();
    m_transport = nullptr;
}

void TerminalWidget::setTheme(const TerminalTheme &theme)
{
    m_theme = theme;
    for (int i = 0; i < 256; ++i)
        m_palette[i] = QColor::fromRgb(QRgb(0xFF000000u | defaultPaletteColor(i)));
    for (int i = 0; i < 16; ++i)
        m_palette[i] = m_theme.ansi[i];

    // The UI stylesheet may be light while the terminal stays dark, so the
    // scrollbar living inside the terminal gets its own colours.
    const auto blend = [&theme](qreal ratio) {
        const auto mix = [ratio](int fg, int bg) { return int(fg * ratio + bg * (1.0 - ratio)); };
        return QColor(mix(theme.foreground.red(), theme.background.red()),
                      mix(theme.foreground.green(), theme.background.green()),
                      mix(theme.foreground.blue(), theme.background.blue()));
    };
    verticalScrollBar()->setStyleSheet(
        QStringLiteral("QScrollBar:vertical { background: transparent; width: 11px; margin: 0; }"
                       "QScrollBar::handle:vertical { background: %1; border-radius: 5px; min-height: 24px; }"
                       "QScrollBar::handle:vertical:hover { background: %2; }"
                       "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                       "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
            .arg(blend(0.30).name(), blend(0.50).name()));

    viewport()->update();
}

void TerminalWidget::setBaseFontFamily(const QString &family)
{
    if (family.isEmpty() || family == m_fontFamily)
        return;
    m_fontFamily = family;
    updateFonts();
}

void TerminalWidget::setBaseFontSize(int points)
{
    points = std::clamp(points, 6, 48);
    if (points == m_fontSize)
        return;
    m_fontSize = points;
    updateFonts();
}

void TerminalWidget::setBaseFontWeight(int weight)
{
    weight = std::clamp(weight, int(QFont::Thin), int(QFont::Black));
    if (weight == m_fontWeight)
        return;
    m_fontWeight = weight;
    updateFonts();
}

QSize TerminalWidget::sizeHint() const
{
    return QSize(int(m_cellWidth * 100), int(m_cellHeight * 28));
}

void TerminalWidget::updateFonts()
{
    m_fonts[0] = QFont(m_fontFamily, m_fontSize);
    m_fonts[0].setStyleHint(QFont::Monospace);
    m_fonts[0].setFixedPitch(true);
    m_fonts[0].setWeight(m_fontWeight);
    m_fonts[2] = m_fonts[0];
    m_fonts[2].setItalic(true);
    m_fonts[1] = m_fonts[0];
    m_fonts[1].setWeight(QFont::Bold);
    m_fonts[3] = m_fonts[1];
    m_fonts[3].setItalic(true);

    const QFontMetricsF fm(m_fonts[0]);

    // Snap the grid to whole pixels: with a fractional advance every second
    // glyph lands on a half pixel and Qt falls back to sub-pixel hinting,
    // which is what made the text look soft.
    const qreal advance = fm.horizontalAdvance(QLatin1Char('M'));
    m_cellWidth = std::max<qreal>(1.0, std::round(advance));
    m_cellHeight = std::max<qreal>(1.0, std::round(fm.height()));
    m_ascent = std::round(fm.ascent());

    // Force each glyph to advance exactly one cell, otherwise a run of text
    // drifts away from the grid and stops lining up with the cursor/selection.
    const qreal spacing = m_cellWidth - advance;
    for (QFont &font : m_fonts)
        font.setLetterSpacing(QFont::AbsoluteSpacing, spacing);

    recomputeGeometry();
    viewport()->update();
}

void TerminalWidget::recomputeGeometry()
{
    if (!m_screen || !isVisible())
        return;
    const int cols = int(viewport()->width() / m_cellWidth);
    const int rows = int(viewport()->height() / m_cellHeight);
    // Layout emits transient tiny sizes while a tab is being built; ignoring
    // them keeps the scrollback and cursor stable.
    if (cols < 2 || rows < 2)
        return;
    if (cols != m_screen->columns() || rows != m_screen->rows()) {
        m_screen->resize(cols, rows);
        if (m_transport)
            m_transport->resizeTerminal(cols, rows);
    }
    updateScrollBar();
}

int TerminalWidget::visibleRows() const
{
    return std::max(1, int(viewport()->height() / m_cellHeight));
}

int TerminalWidget::visibleColumns() const
{
    return std::max(1, int(viewport()->width() / m_cellWidth));
}

int TerminalWidget::topLineIndex() const
{
    const int total = m_screen->totalLines();
    const int rows = visibleRows();
    return std::max(0, total - rows - m_scrollOffset);
}

int TerminalWidget::historyVisible() const
{
    return m_screen->historyVisible();
}

void TerminalWidget::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    recomputeGeometry();
}

void TerminalWidget::showEvent(QShowEvent *event)
{
    QAbstractScrollArea::showEvent(event);
    recomputeGeometry();
}

// QWidget::event() runs focus navigation for Tab/Backtab *before* calling
// keyPressEvent(), which made Tab switch widgets instead of reaching the shell.
bool TerminalWidget::focusNextPrevChild(bool next)
{
    Q_UNUSED(next)
    return false;
}

void TerminalWidget::onDataReceived(const QByteArray &data)
{
    m_parser->feed(data);
    m_cursorOn = true;
    if (m_scrollOffset != 0 && m_screen->alternateScreen())
        m_scrollOffset = 0;
    updateScrollBar();
    viewport()->update();
}

void TerminalWidget::onTransportFinished(const QString &reason)
{
    emit sessionFinished(reason);
}

void TerminalWidget::onBlinkTimer()
{
    if (!hasFocus())
        return;
    m_cursorOn = !m_cursorOn;
    viewport()->update();
}

void TerminalWidget::updateScrollBar()
{
    QScrollBar *sb = verticalScrollBar();
    if (!sb)
        return;
    const int hist = m_screen->alternateScreen() ? 0 : m_screen->historyCount();
    if (m_scrollOffset > hist)
        m_scrollOffset = hist;
    m_updatingScrollBar = true;
    sb->setRange(0, hist);
    sb->setPageStep(visibleRows());
    sb->setSingleStep(1);
    sb->setValue(hist - m_scrollOffset);
    m_updatingScrollBar = false;
}

void TerminalWidget::onScrollBarMoved(int value)
{
    if (m_updatingScrollBar)
        return;
    const int hist = m_screen->alternateScreen() ? 0 : m_screen->historyCount();
    m_scrollOffset = hist - value;
    viewport()->update();
}

void TerminalWidget::scrollTo(int offset)
{
    const int hist = m_screen->alternateScreen() ? 0 : m_screen->historyCount();
    m_scrollOffset = std::clamp(offset, 0, hist);
    updateScrollBar();
    viewport()->update();
}

QString TerminalWidget::cellText(const Cell &cell) const
{
    if (cell.ch == 0)
        return QStringLiteral(" ");
    QString text = codePointToString(cell.ch);
    if (cell.extra != 0)
        text += m_screen->graphemeText(cell.extra);
    return text;
}

QColor TerminalWidget::colorFor(Color color, bool foreground) const
{
    if (isDefaultColor(color))
        return foreground ? m_theme.foreground : m_theme.background;
    if (isPaletteColor(color)) {
        const int index = paletteIndexOf(color);
        if (index >= 0 && index < 256)
            return m_palette[index];
    }
    const quint32 value = rgbValueOf(color);
    return QColor(int((value >> 16) & 0xFF), int((value >> 8) & 0xFF), int(value & 0xFF));
}

const QFont &TerminalWidget::fontFor(quint16 attr) const
{
    const bool bold = (attr & AttrBold) != 0;
    const bool italic = (attr & AttrItalic) != 0;
    if (bold && italic)
        return m_fonts[3];
    if (bold)
        return m_fonts[1];
    if (italic)
        return m_fonts[2];
    return m_fonts[0];
}

void TerminalWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), m_theme.background);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    if (m_screen->alternateScreen() && m_scrollOffset != 0)
        m_scrollOffset = 0;

    const int rows = visibleRows();
    const int top = topLineIndex();
    const int total = m_screen->totalLines();

    for (int y = 0; y < rows; ++y) {
        const int absoluteLine = top + y;
        if (absoluteLine < 0 || absoluteLine >= total)
            break;
        drawRow(painter, y, absoluteLine);
    }

    drawCursor(painter);
    drawPreedit(painter);
}

void TerminalWidget::drawRow(QPainter &painter, int screenRow, int absoluteLine)
{
    const QVector<Cell> &cells = m_screen->lineVector(absoluteLine);
    const int cols = std::min(m_screen->columns(), cells.size());
    if (cols <= 0)
        return;

    const qreal y = screenRow * m_cellHeight;
    const qreal baseline = y + m_ascent;

    int c = 0;
    while (c < cols) {
        const Cell &cell = cells[c];
        if (cell.isWideTail()) {
            ++c;
            continue;
        }

        const quint16 attrs = quint16(cell.attr & ~(AttrWide | AttrWideTail));
        QColor fg = colorFor(cell.fg, true);
        QColor bg = colorFor(cell.bg, false);
        const bool selected = isSelected(absoluteLine, c);
        if (selected)
            bg = m_theme.selection;
        if (attrs & AttrReverse)
            std::swap(fg, bg);
        if (m_screen->reverseVideo())
            std::swap(fg, bg);
        if (attrs & AttrHidden)
            fg = bg;
        if ((attrs & AttrBold) && isPaletteColor(cell.fg)) {
            const int index = paletteIndexOf(cell.fg);
            if (index < 8)
                fg = m_palette[index + 8];
        }
        if (attrs & AttrDim)
            fg = fg.darker(150);

        const bool wide = cell.isWideHead();
        const int span = wide ? 2 : 1;

        if (bg != m_theme.background || selected)
            painter.fillRect(QRectF(c * m_cellWidth, y, span * m_cellWidth, m_cellHeight), bg);

        if (wide) {
            painter.save();
            painter.setClipRect(QRectF(c * m_cellWidth, y, 2 * m_cellWidth, m_cellHeight));
            painter.setFont(fontFor(attrs));
            painter.setPen(fg);
            painter.drawText(QPointF(c * m_cellWidth, baseline), cellText(cell));
            painter.restore();
            ++c;
            continue;
        }

        int runStart = c;
        QString run;
        while (c < cols) {
            const Cell &next = cells[c];
            if (next.isWideHead() || next.isWideTail())
                break;
            if (next.fg != cell.fg || next.bg != cell.bg)
                break;
            if (quint16(next.attr & ~(AttrWide | AttrWideTail)) != attrs)
                break;
            if (isSelected(absoluteLine, c) != selected)
                break;
            run += cellText(next);
            ++c;
        }
        if (c == runStart)
            ++c;

        painter.setFont(fontFor(attrs));
        painter.setPen(fg);
        painter.drawText(QPointF(runStart * m_cellWidth, baseline), run);

        const qreal runWidth = (c - runStart) * m_cellWidth;
        if (attrs & (AttrUnderline | AttrDoubleUnderline)) {
            painter.fillRect(QRectF(runStart * m_cellWidth, baseline + 1, runWidth, 1), fg);
            if (attrs & AttrDoubleUnderline)
                painter.fillRect(QRectF(runStart * m_cellWidth, baseline + 3, runWidth, 1), fg);
        }
        if (attrs & AttrStrike)
            painter.fillRect(QRectF(runStart * m_cellWidth, y + m_cellHeight * 0.5, runWidth, 1), fg);
        if (attrs & AttrOverline)
            painter.fillRect(QRectF(runStart * m_cellWidth, y, runWidth, 1), fg);
    }
}

void TerminalWidget::drawCursor(QPainter &painter)
{
    if (!m_screen->cursorVisible())
        return;
    if (hasFocus() && !m_cursorOn)
        return;

    const int rows = visibleRows();
    const int top = topLineIndex();
    const int absolute = historyVisible() + m_screen->cursorRow();
    const int screenRow = absolute - top;
    if (screenRow < 0 || screenRow >= rows)
        return;

    const qreal x = m_screen->cursorCol() * m_cellWidth;
    const qreal y = screenRow * m_cellHeight;
    const QRectF rect(x, y, m_cellWidth, m_cellHeight);

    if (!hasFocus()) {
        painter.setPen(QPen(m_theme.cursor, 1));
        painter.drawRect(rect.adjusted(0.5, 0.5, -0.5, -0.5));
        return;
    }

    painter.fillRect(rect, m_theme.cursor);
    const QVector<Cell> &cells = m_screen->lineVector(absolute);
    const int col = m_screen->cursorCol();
    if (col >= 0 && col < cells.size()) {
        const Cell &cell = cells[col];
        painter.setFont(fontFor(cell.attr));
        painter.setPen(m_theme.cursorText);
        painter.drawText(QPointF(x, y + m_ascent), cellText(cell));
    }
}

void TerminalWidget::drawPreedit(QPainter &painter)
{
    if (m_preedit.isEmpty())
        return;
    const int rows = visibleRows();
    const int top = topLineIndex();
    const int absolute = historyVisible() + m_screen->cursorRow();
    const int screenRow = absolute - top;
    if (screenRow < 0 || screenRow >= rows)
        return;

    const qreal x = m_screen->cursorCol() * m_cellWidth;
    const qreal y = screenRow * m_cellHeight;
    painter.setFont(m_fonts[0]);
    painter.setPen(m_theme.foreground);
    painter.drawText(QPointF(x, y + m_ascent), m_preedit);
    const qreal width = QFontMetricsF(m_fonts[0]).horizontalAdvance(m_preedit);
    painter.fillRect(QRectF(x, y + m_ascent + 1, width, 1), m_theme.cursor);
}

// ---------------------------------------------------------------- selection

bool TerminalWidget::hasSelection() const
{
    return m_hasSelection && (m_selStartLine != m_selEndLine || m_selStartCol != m_selEndCol);
}

void TerminalWidget::clearSelection()
{
    m_hasSelection = false;
    m_selecting = false;
    viewport()->update();
}

void TerminalWidget::setSelection(int startLine, int startCol, int endLine, int endCol)
{
    if (startLine > endLine || (startLine == endLine && startCol > endCol)) {
        std::swap(startLine, endLine);
        std::swap(startCol, endCol);
    }
    m_selStartLine = startLine;
    m_selStartCol = startCol;
    m_selEndLine = endLine;
    m_selEndCol = endCol;
    m_hasSelection = true;
    viewport()->update();
}

void TerminalWidget::selectionRange(int &startLine, int &startCol, int &endLine, int &endCol) const
{
    startLine = m_selStartLine;
    startCol = m_selStartCol;
    endLine = m_selEndLine;
    endCol = m_selEndCol;
    if (startLine > endLine || (startLine == endLine && startCol > endCol)) {
        std::swap(startLine, endLine);
        std::swap(startCol, endCol);
    }
}

bool TerminalWidget::isSelected(int absoluteLine, int column) const
{
    if (!m_hasSelection)
        return false;
    int sl, sc, el, ec;
    selectionRange(sl, sc, el, ec);
    if (absoluteLine < sl || absoluteLine > el)
        return false;
    if (sl == el)
        return column >= sc && column <= ec;
    if (absoluteLine == sl)
        return column >= sc;
    if (absoluteLine == el)
        return column <= ec;
    return true;
}

QString TerminalWidget::selectedText() const
{
    if (!m_hasSelection)
        return QString();
    int sl, sc, el, ec;
    selectionRange(sl, sc, el, ec);
    QString out;
    for (int line = sl; line <= el; ++line) {
        const QVector<Cell> &cells = m_screen->lineVector(line);
        const int from = (line == sl) ? sc : 0;
        const int to = (line == el) ? std::min(ec, cells.size() - 1) : cells.size() - 1;
        QString row;
        for (int c = from; c <= to && c < cells.size(); ++c) {
            const Cell &cell = cells[c];
            if (cell.isWideTail())
                continue;
            row += cellText(cell);
        }
        while (row.endsWith(QLatin1Char(' ')))
            row.chop(1);
        out += row;
        if (line < el)
            out += QLatin1Char('\n');
    }
    return out;
}

void TerminalWidget::copySelection()
{
    const QString text = selectedText();
    if (!text.isEmpty())
        copyToClipboards(text);
}

void TerminalWidget::copyToClipboards(const QString &text)
{
    if (text.isEmpty())
        return;
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(text, QClipboard::Clipboard);
    if (clipboard->supportsSelection())
        clipboard->setText(text, QClipboard::Selection);
}

void TerminalWidget::selectAll()
{
    const int total = m_screen->totalLines();
    if (total <= 0)
        return;
    setSelection(0, 0, total - 1, std::max(0, m_screen->columns() - 1));
}

void TerminalWidget::pasteClipboard()
{
    QString text = QApplication::clipboard()->text();
    if (text.isEmpty())
        return;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\r"));
    text.replace(QLatin1Char('\n'), QLatin1Char('\r'));
    if (m_screen->bracketedPaste())
        text = QStringLiteral("\033[200~") + text + QStringLiteral("\033[201~");
    sendText(text);
    scrollTo(0);
}

void TerminalWidget::clearScrollback()
{
    m_screen->clearScrollback();
    scrollTo(0);
    updateScrollBar();
    viewport()->update();
}

void TerminalWidget::clearScreen()
{
    m_screen->eraseInDisplay(2);
    m_screen->moveTo(0, 0);
    viewport()->update();
}

void TerminalWidget::zoomIn()
{
    setBaseFontSize(m_fontSize + 1);
}

void TerminalWidget::zoomOut()
{
    setBaseFontSize(m_fontSize - 1);
}

void TerminalWidget::resetZoom()
{
    setBaseFontSize(11);
}

// ------------------------------------------------------------------- mouse

QPoint TerminalWidget::cellAt(const QPoint &pos) const
{
    const int col = std::clamp(int(pos.x() / m_cellWidth), 0, std::max(0, m_screen->columns() - 1));
    int line = topLineIndex() + int(pos.y() / m_cellHeight);
    line = std::clamp(line, 0, std::max(0, m_screen->totalLines() - 1));
    return QPoint(col, line);
}

int TerminalWidget::absoluteLineAt(int y) const
{
    int line = topLineIndex() + int(y / m_cellHeight);
    return std::clamp(line, 0, std::max(0, m_screen->totalLines() - 1));
}

void TerminalWidget::startSelection(const QPoint &pos, int mode)
{
    const QPoint cell = cellAt(pos);
    m_selectMode = mode;
    m_selecting = true;
    setSelection(cell.y(), cell.x(), cell.y(), cell.x());
}

void TerminalWidget::extendSelection(const QPoint &pos)
{
    const QPoint cell = cellAt(pos);
    if (m_selectMode == 2) {
        setSelection(m_selStartLine, 0, cell.y(), std::max(0, m_screen->columns() - 1));
        return;
    }
    if (m_selectMode == 1) {
        const QVector<Cell> &cells = m_screen->lineVector(cell.y());
        int from = cell.x();
        int to = cell.x();
        while (from > 0 && from - 1 < cells.size() && isWordChar(cells[from - 1].ch))
            --from;
        while (to + 1 < cells.size() && isWordChar(cells[to + 1].ch))
            ++to;
        setSelection(cell.y(), from, cell.y(), to);
        return;
    }
    setSelection(m_selStartLine, m_selStartCol, cell.y(), cell.x());
}

void TerminalWidget::selectWordAt(const QPoint &pos)
{
    const QPoint cell = cellAt(pos);
    const QVector<Cell> &cells = m_screen->lineVector(cell.y());
    if (cell.x() >= cells.size())
        return;
    int from = cell.x();
    int to = cell.x();
    if (!isWordChar(cells[from].ch)) {
        setSelection(cell.y(), from, cell.y(), to);
        return;
    }
    while (from > 0 && isWordChar(cells[from - 1].ch))
        --from;
    while (to + 1 < cells.size() && isWordChar(cells[to + 1].ch))
        ++to;
    setSelection(cell.y(), from, cell.y(), to);
}

void TerminalWidget::selectLineAt(const QPoint &pos)
{
    const int line = cellAt(pos).y();
    setSelection(line, 0, line, std::max(0, m_screen->columns() - 1));
}

void TerminalWidget::mousePressEvent(QMouseEvent *event)
{
    setFocus();
    if (event->button() == Qt::MiddleButton) {
        const QString text = QApplication::clipboard()->text(QClipboard::Selection);
        if (!text.isEmpty()) {
            QString filtered = text;
            filtered.replace(QStringLiteral("\r\n"), QStringLiteral("\r"));
            filtered.replace(QLatin1Char('\n'), QLatin1Char('\r'));
            sendText(filtered);
        }
        return;
    }
    if (event->button() != Qt::LeftButton) {
        QAbstractScrollArea::mousePressEvent(event);
        return;
    }

    if (m_clickTimer.isValid() && m_clickTimer.elapsed() < QApplication::doubleClickInterval())
        ++m_clickCount;
    else
        m_clickCount = 1;
    m_clickTimer.restart();

    if (m_clickCount >= 3) {
        selectLineAt(event->pos());
        m_selecting = true;
        m_selectMode = 2;
        return;
    }
    if (m_clickCount == 2) {
        selectWordAt(event->pos());
        m_selecting = true;
        m_selectMode = 1;
        return;
    }

    if (event->modifiers() & Qt::ShiftModifier && m_hasSelection) {
        extendSelection(event->pos());
        m_selecting = true;
        return;
    }
    clearSelection();
    startSelection(event->pos(), 0);
}

void TerminalWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_selecting)
        return;

    const int y = event->pos().y();
    if (y < 0) {
        scrollTo(m_scrollOffset + 1);
    } else if (y > viewport()->height()) {
        scrollTo(m_scrollOffset - 1);
    }
    extendSelection(event->pos());
}

void TerminalWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_selecting) {
        m_selecting = false;
        if (hasSelection())
            copySelection();
    }
}

void TerminalWidget::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const int delta = event->angleDelta().y();
        if (delta > 0)
            zoomIn();
        else if (delta < 0)
            zoomOut();
        event->accept();
        return;
    }
    const int steps = event->angleDelta().y() / 40;
    if (steps != 0)
        scrollTo(m_scrollOffset + steps);
    event->accept();
}

void TerminalWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *copyAction = menu.addAction(tr("复制"));
    copyAction->setEnabled(hasSelection());
    QAction *pasteAction = menu.addAction(tr("粘贴"));
    pasteAction->setEnabled(m_transport != nullptr);
    menu.addSeparator();
    QAction *selectAllAction = menu.addAction(tr("全选"));
    QAction *clearAction = menu.addAction(tr("清空回滚缓冲区"));

    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == copyAction)
        copySelection();
    else if (chosen == pasteAction)
        pasteClipboard();
    else if (chosen == selectAllAction)
        selectAll();
    else if (chosen == clearAction)
        clearScrollback();
}

// ----------------------------------------------------------------- keyboard

void TerminalWidget::sendText(const QString &text)
{
    if (text.isEmpty())
        return;
    sendBytes(text.toUtf8());
}

void TerminalWidget::sendBytes(const QByteArray &bytes)
{
    if (m_transport)
        m_transport->write(bytes);
    scrollTo(0);
}

void TerminalWidget::keyPressEvent(QKeyEvent *event)
{
    const Qt::KeyboardModifiers mods = event->modifiers();
    const bool ctrl = mods.testFlag(Qt::ControlModifier);
    const bool shift = mods.testFlag(Qt::ShiftModifier);
    const bool alt = mods.testFlag(Qt::AltModifier);

    if (ctrl && shift) {
        switch (event->key()) {
        case Qt::Key_C:
            copySelection();
            return;
        case Qt::Key_V:
            pasteClipboard();
            return;
        case Qt::Key_A:
            selectAll();
            return;
        default:
            break;
        }
    }
    if (ctrl && event->key() == Qt::Key_Insert) {
        copySelection();
        return;
    }
    if (shift && event->key() == Qt::Key_Insert) {
        pasteClipboard();
        return;
    }
    if (ctrl && shift && event->key() == Qt::Key_F) {
        return;
    }
    Q_UNUSED(alt);
    sendKey(event);
}

void TerminalWidget::sendKey(QKeyEvent *event)
{
    const int key = event->key();
    const Qt::KeyboardModifiers mods = event->modifiers();
    const bool ctrl = mods.testFlag(Qt::ControlModifier);
    const bool alt = mods.testFlag(Qt::AltModifier);
    const bool shift = mods.testFlag(Qt::ShiftModifier);

    int modCode = 0;
    if (shift)
        modCode |= 1;
    if (alt)
        modCode |= 2;
    if (ctrl)
        modCode |= 4;
    if (modCode != 0)
        modCode += 1;

    const auto arrow = [&](char finalByte) -> QByteArray {
        if (modCode != 0)
            return QByteArray("\033[1;") + QByteArray::number(modCode) + finalByte;
        if (m_screen->applicationCursorKeys())
            return QByteArray("\033O") + finalByte;
        return QByteArray("\033[") + finalByte;
    };
    const auto tilde = [&](int code) -> QByteArray {
        QByteArray out = "\033[";
        out += QByteArray::number(code);
        if (modCode != 0) {
            out += ';';
            out += QByteArray::number(modCode);
        }
        out += '~';
        return out;
    };
    const auto ss3 = [&](char finalByte) -> QByteArray {
        if (modCode != 0)
            return QByteArray("\033[1;") + QByteArray::number(modCode) + finalByte;
        return QByteArray("\033O") + finalByte;
    };

    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        sendBytes(alt ? QByteArray("\033\r") : QByteArray("\r"));
        return;
    case Qt::Key_Backspace:
        sendBytes(alt ? QByteArray("\033\177") : QByteArray("\177"));
        return;
    case Qt::Key_Tab:
        if (shift)
            sendBytes("\033[Z");
        else
            sendBytes("\t");
        return;
    case Qt::Key_Backtab:
        sendBytes("\033[Z");
        return;
    case Qt::Key_Escape:
        sendBytes("\033");
        return;
    case Qt::Key_Up:
        sendBytes(arrow('A'));
        return;
    case Qt::Key_Down:
        sendBytes(arrow('B'));
        return;
    case Qt::Key_Right:
        sendBytes(arrow('C'));
        return;
    case Qt::Key_Left:
        sendBytes(arrow('D'));
        return;
    case Qt::Key_Home:
        sendBytes(modCode ? QByteArray("\033[1;") + QByteArray::number(modCode) + "H" : QByteArray("\033[H"));
        return;
    case Qt::Key_End:
        sendBytes(modCode ? QByteArray("\033[1;") + QByteArray::number(modCode) + "F" : QByteArray("\033[F"));
        return;
    case Qt::Key_Insert:
        sendBytes(tilde(2));
        return;
    case Qt::Key_Delete:
        sendBytes(tilde(3));
        return;
    case Qt::Key_PageUp:
        sendBytes(tilde(5));
        return;
    case Qt::Key_PageDown:
        sendBytes(tilde(6));
        return;
    case Qt::Key_F1:
        sendBytes(ss3('P'));
        return;
    case Qt::Key_F2:
        sendBytes(ss3('Q'));
        return;
    case Qt::Key_F3:
        sendBytes(ss3('R'));
        return;
    case Qt::Key_F4:
        sendBytes(ss3('S'));
        return;
    case Qt::Key_F5:
        sendBytes(tilde(15));
        return;
    case Qt::Key_F6:
        sendBytes(tilde(17));
        return;
    case Qt::Key_F7:
        sendBytes(tilde(18));
        return;
    case Qt::Key_F8:
        sendBytes(tilde(19));
        return;
    case Qt::Key_F9:
        sendBytes(tilde(20));
        return;
    case Qt::Key_F10:
        sendBytes(tilde(21));
        return;
    case Qt::Key_F11:
        sendBytes(tilde(23));
        return;
    case Qt::Key_F12:
        sendBytes(tilde(24));
        return;
    default:
        break;
    }

    if (ctrl) {
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            const char code = char(key - Qt::Key_A + 1);
            QByteArray out(1, code);
            sendBytes(alt ? QByteArray("\033") + out : out);
            return;
        }
        if (key == Qt::Key_Space || key == Qt::Key_At) {
            sendBytes(alt ? QByteArray("\033\0", 3) : QByteArray(1, '\0'));
            return;
        }
        switch (key) {
        case Qt::Key_BracketLeft:
            sendBytes("\033");
            return;
        case Qt::Key_Backslash:
            sendBytes("\034");
            return;
        case Qt::Key_BracketRight:
            sendBytes("\035");
            return;
        case Qt::Key_AsciiCircum:
        case Qt::Key_Underscore:
            sendBytes("\036");
            return;
        case Qt::Key_Slash:
            sendBytes("\037");
            return;
        default:
            break;
        }
    }

    const QString text = event->text();
    if (!text.isEmpty() && !ctrl) {
        if (alt)
            sendText(QString(QChar(0x1B)) + text);
        else
            sendText(text);
        return;
    }

    QAbstractScrollArea::keyPressEvent(event);
}

void TerminalWidget::inputMethodEvent(QInputMethodEvent *event)
{
    if (!event->commitString().isEmpty()) {
        m_preedit.clear();
        sendText(event->commitString());
    } else {
        m_preedit = event->preeditString();
    }
    viewport()->update();
    event->accept();
}

QVariant TerminalWidget::inputMethodQuery(Qt::InputMethodQuery query) const
{
    if (query == Qt::ImEnabled)
        return QVariant(true);
    if (query == Qt::ImCursorRectangle) {
        const int screenRow = historyVisible() + m_screen->cursorRow() - topLineIndex();
        return QRectF(m_screen->cursorCol() * m_cellWidth, screenRow * m_cellHeight,
                      m_cellWidth, m_cellHeight);
    }
    if (query == Qt::ImFont)
        return m_fonts[0];
    if (query == Qt::ImCursorPosition)
        return 0;
    if (query == Qt::ImSurroundingText)
        return QString();
    return QAbstractScrollArea::inputMethodQuery(query);
}

void TerminalWidget::focusInEvent(QFocusEvent *event)
{
    QAbstractScrollArea::focusInEvent(event);
    m_cursorOn = true;
    viewport()->update();
}

void TerminalWidget::focusOutEvent(QFocusEvent *event)
{
    QAbstractScrollArea::focusOutEvent(event);
    m_cursorOn = true;
    viewport()->update();
}

} // namespace vx
