#pragma once

#include <QAbstractScrollArea>
#include <QColor>
#include <QElapsedTimer>
#include <QFont>
#include <QPoint>
#include <QTimer>
#include <QVector>

#include "../core/Screen.h"
#include "../core/VtParser.h"
#include "TerminalTheme.h"

class QScrollBar;

namespace vx {

class Transport;

class TerminalWidget : public QAbstractScrollArea
{
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget() override;

    Screen *screen() const { return m_screen; }
    VtParser *parser() const { return m_parser; }

    void setTransport(Transport *transport);
    Transport *transport() const { return m_transport; }
    void releaseTransport();

    void setTheme(const TerminalTheme &theme);
    const TerminalTheme &theme() const { return m_theme; }

    void setBaseFontFamily(const QString &family);
    QString baseFontFamily() const { return m_fontFamily; }
    void setBaseFontSize(int points);
    int baseFontSize() const { return m_fontSize; }

    QSize sizeHint() const override;

signals:
    void titleChanged(const QString &title);
    void workingDirectoryChanged(const QString &dir);
    void bell();
    void sessionFinished(const QString &reason);
    void userActivity();

public slots:
    void copySelection();
    void pasteClipboard();
    void selectAll();
    void clearScrollback();
    void clearScreen();
    void zoomIn();
    void zoomOut();
    void resetZoom();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onDataReceived(const QByteArray &data);
    void onTransportFinished(const QString &reason);
    void onBlinkTimer();
    void onScrollBarMoved(int value);

private:
    int visibleRows() const;
    int visibleColumns() const;
    int topLineIndex() const;
    int historyVisible() const;
    void recomputeGeometry();
    void updateScrollBar();
    void updateFonts();

    QString cellText(const Cell &cell) const;
    QColor colorFor(Color color, bool foreground) const;
    const QFont &fontFor(quint16 attr) const;

    void drawRow(QPainter &painter, int screenRow, int absoluteLine);
    void drawCursor(QPainter &painter);
    void drawPreedit(QPainter &painter);

    bool hasSelection() const;
    bool isSelected(int absoluteLine, int column) const;
    void clearSelection();
    void selectionRange(int &startLine, int &startCol, int &endLine, int &endCol) const;
    QString selectedText() const;
    void copyToClipboards(const QString &text);

    QPoint cellAt(const QPoint &pos) const;
    int absoluteLineAt(int y) const;
    void startSelection(const QPoint &pos, int mode);
    void extendSelection(const QPoint &pos);
    void setSelection(int startLine, int startCol, int endLine, int endCol);
    void selectWordAt(const QPoint &pos);
    void selectLineAt(const QPoint &pos);

    void sendText(const QString &text);
    void sendBytes(const QByteArray &bytes);
    void sendKey(QKeyEvent *event);

    void scrollTo(int offset);

    Screen *m_screen = nullptr;
    VtParser *m_parser = nullptr;
    Transport *m_transport = nullptr;

    TerminalTheme m_theme;
    QColor m_palette[256];

    QString m_fontFamily;
    int m_fontSize = 11;
    QFont m_fonts[4];   // normal, bold, italic, bold+italic
    qreal m_cellWidth = 8;
    qreal m_cellHeight = 16;
    qreal m_ascent = 12;

    QTimer m_blinkTimer;
    bool m_cursorOn = true;

    int m_scrollOffset = 0;
    bool m_updatingScrollBar = false;

    // selection (absolute line indices, inclusive)
    bool m_selecting = false;
    bool m_hasSelection = false;
    int m_selStartLine = 0;
    int m_selStartCol = 0;
    int m_selEndLine = 0;
    int m_selEndCol = 0;
    int m_selectMode = 0;   // 0 char, 1 word, 2 line
    QElapsedTimer m_clickTimer;
    int m_clickCount = 0;

    QString m_preedit;
    bool m_showCursor = true;
};

} // namespace vx
