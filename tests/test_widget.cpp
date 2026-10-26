// Renders a TerminalWidget offscreen and inspects the produced pixels, so the
// painting path can be verified without a display.
#include <QApplication>
#include <QImage>
#include <QScrollBar>
#include <QtTest/QTest>
#include <cstdio>

#include "session/Transport.h"
#include "ui/Style.h"

#include "core/Screen.h"
#include "core/VtParser.h"
#include "term/TerminalTheme.h"
#include "term/TerminalWidget.h"

using namespace vx;

static int g_failures = 0;

static void check(bool ok, const char *what, int line)
{
    if (!ok) {
        std::printf("FAIL %s:%d  %s\n", __FILE__, line, what);
        ++g_failures;
    }
}
#define CHECK(cond) check((cond), #cond, __LINE__)

class RecordingTransport : public Transport
{
public:
    QByteArray written;
    bool start(int, int) override { return true; }
    void write(const QByteArray &data) override { written += data; }
    QString description() const override { return QStringLiteral("recording"); }
};

static bool nearColor(const QColor &a, const QColor &b, int tolerance = 24)
{
    return qAbs(a.red() - b.red()) <= tolerance && qAbs(a.green() - b.green()) <= tolerance
        && qAbs(a.blue() - b.blue()) <= tolerance;
}

static int countNear(const QImage &image, const QRect &rect, const QColor &color, int tolerance = 24)
{
    int count = 0;
    const QRect r = rect.intersected(image.rect());
    for (int y = r.top(); y <= r.bottom(); ++y) {
        for (int x = r.left(); x <= r.right(); ++x) {
            if (nearColor(image.pixelColor(x, y), color, tolerance))
                ++count;
        }
    }
    return count;
}

static int countNonBackground(const QImage &image, const QRect &rect, const QColor &bg)
{
    int count = 0;
    const QRect r = rect.intersected(image.rect());
    for (int y = r.top(); y <= r.bottom(); ++y) {
        for (int x = r.left(); x <= r.right(); ++x) {
            if (!nearColor(image.pixelColor(x, y), bg, 6))
                ++count;
        }
    }
    return count;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    TerminalWidget widget;
    widget.resize(820, 420);
    widget.show();
    QCoreApplication::processEvents();

    const TerminalTheme theme = widget.theme();
    const int cols = widget.screen()->columns();
    const int rows = widget.screen()->rows();
    CHECK(cols > 40);
    CHECK(rows > 10);

    const qreal cellW = qreal(widget.viewport()->width()) / cols;
    const qreal cellH = qreal(widget.viewport()->height()) / rows;
    CHECK(cellW > 4 && cellH > 8);

    const auto cellRect = [&](int row, int col, int span = 1) {
        return QRect(int(col * cellW), int(row * cellH), int(span * cellW), int(cellH));
    };

    // --- empty screen is uniformly background ---
    QImage blank = widget.grab().toImage();
    CHECK(blank.width() > 0);
    // The home position shows a block cursor, so start checking below it.
    QRect belowCursor(0, int(cellH), widget.viewport()->width(),
                      widget.viewport()->height() - int(cellH));
    CHECK(countNonBackground(blank, belowCursor, theme.background) == 0);

    // --- plain text draws ink ---
    widget.parser()->feed("hello world");
    QCoreApplication::processEvents();
    QImage textImage = widget.grab().toImage();
    CHECK(countNonBackground(textImage, textImage.rect(), theme.background) > 200);

    // --- SGR color lands on the right cell ---
    widget.parser()->feed("\r\n\033[31mR\033[0m");
    QCoreApplication::processEvents();
    QImage colorImage = widget.grab().toImage();
    CHECK(countNear(colorImage, cellRect(1, 0), theme.ansi[1]) > 5);

    // --- wide CJK glyph occupies two cells ---
    widget.parser()->feed("\r\n\346\226\207");  // 文
    QCoreApplication::processEvents();
    QImage cjkImage = widget.grab().toImage();
    CHECK(countNonBackground(cjkImage, cellRect(2, 0), theme.background) > 10);
    CHECK(countNonBackground(cjkImage, cellRect(2, 1), theme.background) > 10);

    // --- reverse video paints the cell background ---
    widget.parser()->feed("\r\n\033[7mZ\033[0m");
    QCoreApplication::processEvents();
    QImage reverseImage = widget.grab().toImage();
    CHECK(countNear(reverseImage, cellRect(3, 0), theme.foreground, 40) > 40);

    // --- the cursor is painted where the shell left it ---
    QImage cursorImage = widget.grab().toImage();
    CHECK(countNear(cursorImage, cursorImage.rect(), theme.cursor, 30) > 30);

    // --- selection uses the highlight color ---
    widget.selectAll();
    QCoreApplication::processEvents();
    QImage selectionImage = widget.grab().toImage();
    CHECK(countNear(selectionImage, selectionImage.rect(), theme.selection, 30) > 500);

    // --- the light scheme paints a light background with dark ink ---
    {
        widget.setTheme(lightVioletTheme());
        widget.parser()->feed("\r\nlight theme sample");
        QCoreApplication::processEvents();
        const QImage lightImage = widget.grab().toImage();
        const QColor corner = lightImage.pixelColor(4, 4);
        CHECK(corner.lightness() > 180);
        int darkPixels = 0;
        for (int y = 0; y < lightImage.height(); y += 3)
            for (int x = 0; x < lightImage.width(); x += 3)
                if (lightImage.pixelColor(x, y).lightness() < 90)
                    ++darkPixels;
        CHECK(darkPixels > 20);
        widget.setTheme(darkVioletTheme());
    }

    // --- keyboard input is encoded the way a shell expects ---
    {
        RecordingTransport *recorder = new RecordingTransport;
        widget.setTransport(recorder);

        QTest::keyClicks(&widget, "hi");
        CHECK(recorder->written == QByteArray("hi"));
        recorder->written.clear();

        QTest::keyClick(&widget, Qt::Key_Return);
        CHECK(recorder->written == QByteArray("\r"));
        recorder->written.clear();

        QTest::keyClick(&widget, Qt::Key_Backspace);
        CHECK(recorder->written == QByteArray("\x7f"));
        recorder->written.clear();

        QTest::keyClick(&widget, Qt::Key_Up);
        CHECK(recorder->written == QByteArray("\033[A"));
        recorder->written.clear();

        QTest::keyClick(&widget, Qt::Key_C, Qt::ControlModifier);
        CHECK(recorder->written == QByteArray(1, '\x03'));
        recorder->written.clear();

        QTest::keyClick(&widget, Qt::Key_A, Qt::AltModifier);
        CHECK(recorder->written == QByteArray("\033a", 2));
        recorder->written.clear();

        widget.screen()->setPrivateMode(1, true); // application cursor keys
        QTest::keyClick(&widget, Qt::Key_Left);
        CHECK(recorder->written == QByteArray("\033OD", 3));
        recorder->written.clear();

        widget.releaseTransport();
    }

    // --- scrolling fills the scrollbar range ---
    widget.screen()->clearScrollback();
    for (int i = 0; i < 80; ++i)
        widget.parser()->feed("line\r\n");
    QCoreApplication::processEvents();
    CHECK(widget.screen()->historyCount() > 0);

    // --- the application palette really switches between schemes ---
    {
        applyStyle(true);
        CHECK(qApp->palette().color(QPalette::Window).lightness() < 80);
        CHECK(!qApp->styleSheet().contains(QLatin1Char('@'))); // no unresolved tokens

        applyStyle(false);
        CHECK(qApp->palette().color(QPalette::Window).lightness() > 180);
        CHECK(!qApp->styleSheet().contains(QLatin1Char('@')));
        CHECK(qApp->styleSheet().contains(QStringLiteral("#FFFFFF")));
    }

    if (g_failures == 0)
        std::printf("widget render checks passed (%dx%d cells, %.1fx%.1f px)\n", cols, rows,
                    double(cellW), double(cellH));
    else
        std::printf("%d widget check(s) failed\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
