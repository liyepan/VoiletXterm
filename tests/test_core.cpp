// Minimal self checks for the terminal core (no GUI required).
#include <QByteArray>
#include <QString>
#include <cstdio>

#include "core/Screen.h"
#include "core/VtParser.h"

using namespace vx;

static int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

static void checkEq(const QString &got, const QString &want, const char *what, int line)
{
    if (got != want) {
        std::printf("FAIL %s:%d  %s\n  got: [%s]\n  exp: [%s]\n",
                    __FILE__, line, what, qPrintable(got), qPrintable(want));
        ++g_failures;
    }
}
#define CHECK_EQ(got, want) checkEq((got), (want), #got, __LINE__)

struct Fixture {
    Screen screen;
    VtParser parser;

    explicit Fixture(int cols = 20, int rows = 5)
        : parser(&screen)
    {
        screen.initialize(cols, rows);
    }

    void feed(const char *text) { parser.feed(QByteArray(text)); }

    const Cell *row(int r) const { return screen.lineData(screen.historyCount() + r); }

    QString dump() const
    {
        QString out;
        for (int r = 0; r < screen.rows(); ++r) {
            const Cell *cells = row(r);
            QString line;
            for (int c = 0; c < screen.columns(); ++c) {
                if (cells[c].isWideTail())
                    continue;
                line += codePointToString(cells[c].ch);
                line += screen.graphemeText(cells[c].extra);
            }
            while (line.endsWith(QLatin1Char(' ')))
                line.chop(1);
            out += line;
            if (r + 1 < screen.rows())
                out += QLatin1Char('\n');
        }
        while (out.endsWith(QLatin1Char('\n')))
            out.chop(1);
        return out;
    }
};

static void testPlainText()
{
    Fixture f;
    f.feed("hello");
    CHECK_EQ(f.dump(), QStringLiteral("hello"));
    CHECK(f.screen.cursorCol() == 5);
    CHECK(f.screen.cursorRow() == 0);
}

static void testNewline()
{
    Fixture f;
    f.feed("abc\r\ndef");
    CHECK_EQ(f.dump(), QStringLiteral("abc\ndef"));
    CHECK(f.screen.cursorRow() == 1);
    CHECK(f.screen.cursorCol() == 3);
}

static void testCursorPosition()
{
    Fixture f;
    f.feed("\x1b[2;3HX");
    CHECK_EQ(f.dump(), QStringLiteral("\n  X"));
    CHECK(f.screen.cursorRow() == 1);
    CHECK(f.screen.cursorCol() == 3);
}

static void testSgrColors()
{
    Fixture f;
    f.feed("\x1b[1;31mR\x1b[0mN");
    const Cell *cells = f.row(0);
    CHECK(cells[0].fg == paletteColor(1));
    CHECK((cells[0].attr & AttrBold) != 0);
    CHECK(cells[1].fg == kColorDefault);
    CHECK(cells[1].attr == 0);
}

static void testTrueColor()
{
    Fixture f;
    f.feed("\x1b[38;2;10;20;30mX");
    CHECK(f.row(0)[0].fg == rgbColor(10, 20, 30));
}

static void testErase()
{
    Fixture f;
    f.feed("abc\r\ndef");
    f.feed("\x1b[2J");
    CHECK_EQ(f.dump(), QString());
}

static void testWideChars()
{
    Fixture f;
    f.feed("\xe4\xb8\xad\xe6\x96\x87");
    const Cell *cells = f.row(0);
    CHECK(cells[0].ch == 0x4E2D);
    CHECK(cells[0].isWideHead());
    CHECK(cells[1].isWideTail());
    CHECK(cells[2].ch == 0x6587);
    CHECK(f.screen.cursorCol() == 4);
    CHECK(cells[0].attr & AttrWide);
}

static void testWrap()
{
    Fixture f(20, 5);
    QString text;
    for (int i = 0; i < 25; ++i)
        text += QLatin1Char('x');
    f.feed(text.toUtf8().constData());
    CHECK(f.screen.cursorRow() == 1);
    CHECK(f.screen.cursorCol() == 5);
    const Cell *cells = f.row(0);
    CHECK(cells[19].ch == U'x');
    CHECK(f.row(1)[4].ch == U'x');
}

static void testScrollback()
{
    Fixture f(20, 5);
    for (int i = 0; i < 10; ++i)
        f.feed(QByteArray::number(i).append("\r\n").constData());
    CHECK(f.screen.historyCount() == 6);
    CHECK(f.screen.totalLines() == 11);
    const Cell *first = f.screen.lineData(0);
    CHECK(first[0].ch == U'0');
}

static void testAltScreen()
{
    Fixture f;
    f.feed("main");
    f.feed("\x1b[?1049h");
    f.feed("alt");
    CHECK_EQ(f.dump(), QStringLiteral("alt"));
    CHECK(f.screen.alternateScreen());
    f.feed("\x1b[?1049l");
    CHECK_EQ(f.dump(), QStringLiteral("main"));
    CHECK(!f.screen.alternateScreen());
}

static void testDecGraphics()
{
    Fixture f;
    f.feed("\x1b(0q\x1b(Bq");
    CHECK(f.row(0)[0].ch == 0x2500);
    CHECK(f.row(0)[1].ch == U'q');
}

static void testInsertDeleteChars()
{
    Fixture f;
    f.feed("abcdef\x1b[1;3H");
    f.feed("\x1b[2@");
    CHECK_EQ(f.dump(), QStringLiteral("ab  cdef"));
    f.feed("\x1b[2P");
    CHECK_EQ(f.dump(), QStringLiteral("abcdef"));
}

static void testScrollRegion()
{
    Fixture f(10, 5);
    f.feed("a\r\nb\r\nc\r\nd\r\ne");
    f.feed("\x1b[2;4r");
    f.feed("\x1b[4;1H\r\n");
    // line 1 ("b") scrolled out of the region
    CHECK(f.row(1)[0].ch == U'c');
    CHECK(f.row(0)[0].ch == U'a');
}

static void testReverseIndex()
{
    Fixture f(10, 4);
    f.feed("\x1b[1;1H\x1bM");
    CHECK_EQ(f.dump(), QString());
}

int main()
{
    testPlainText();
    testNewline();
    testCursorPosition();
    testSgrColors();
    testTrueColor();
    testErase();
    testWideChars();
    testWrap();
    testScrollback();
    testAltScreen();
    testDecGraphics();
    testInsertDeleteChars();
    testScrollRegion();
    testReverseIndex();

    if (g_failures == 0)
        std::printf("all core tests passed\n");
    else
        std::printf("%d core test(s) failed\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
