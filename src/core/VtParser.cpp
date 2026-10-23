#include "VtParser.h"

#include "Screen.h"

#include <QByteArray>
#include <QString>
#include <QVector>

#include <algorithm>

namespace vx {

namespace {

void parseParams(const QByteArray &buf, char &priv, QVector<int> &out)
{
    out.clear();
    int start = 0;
    if (!buf.isEmpty()) {
        const char c = buf.at(0);
        if (c == '?' || c == '>' || c == '=' || c == '<' || c == '!') {
            priv = c;
            start = 1;
        }
    }
    int value = 0;
    bool hasValue = false;
    for (int i = start; i < buf.size(); ++i) {
        const char c = buf.at(i);
        if (c >= '0' && c <= '9') {
            value = value * 10 + (c - '0');
            hasValue = true;
        } else if (c == ';' || c == ':') {
            out.append(hasValue ? value : 0);
            value = 0;
            hasValue = false;
        }
    }
    if (hasValue)
        out.append(value);
}

} // namespace

VtParser::VtParser(Screen *screen)
    : m_screen(screen)
{
}

void VtParser::reset()
{
    m_state = Ground;
    m_paramBuf.clear();
    m_interBuf.clear();
    m_strBuf.clear();
    m_charsetSlot = -1;
    m_stringEsc = false;
    m_utf8Cp = 0;
    m_utf8Need = 0;
}

void VtParser::feed(const char *data, int len)
{
    if (!m_screen)
        return;
    for (int i = 0; i < len; ++i) {
        const uint8_t b = uint8_t(data[i]);
        if (b == 0x18 || b == 0x1A) { // CAN / SUB cancel any sequence
            m_state = Ground;
            m_paramBuf.clear();
            m_interBuf.clear();
            m_strBuf.clear();
            m_utf8Need = 0;
            continue;
        }
        switch (m_state) {
        case Ground:
            handleGround(b);
            break;
        case Escape:
            handleEscape(b);
            break;
        case EscapeInter:
            handleEscapeInter(b);
            break;
        case Csi:
            handleCsi(b);
            break;
        case Osc:
            handleOsc(b);
            break;
        case Dcs:
        case Ignore:
            if (b == 0x1B) {
                m_state = Ignore;
                m_stringEsc = true;
            } else if (m_stringEsc && b == '\\') {
                m_state = Ground;
                m_stringEsc = false;
            } else if (b == 0x07) {
                m_state = Ground;
            }
            break;
        }
    }
}

void VtParser::emitText(char32_t cp)
{
    m_screen->putChar(cp);
}

void VtParser::handleGround(uint8_t b)
{
    if (b == 0x1B) {
        m_state = Escape;
        m_interBuf.clear();
        m_paramBuf.clear();
        m_charsetSlot = -1;
        return;
    }
    if (b < 0x20 || b == 0x7F) {
        handleControl(b);
        return;
    }

    if (m_utf8Need > 0) {
        if ((b & 0xC0) == 0x80) {
            m_utf8Cp = (m_utf8Cp << 6) | char32_t(b & 0x3F);
            if (--m_utf8Need == 0) {
                const char32_t cp = m_utf8Cp;
                m_utf8Cp = 0;
                emitText(cp);
            }
            return;
        }
        m_utf8Need = 0;
        m_utf8Cp = 0;
        emitText(0xFFFD);
    }

    if (b < 0x80) {
        emitText(char32_t(b));
    } else if ((b & 0xE0) == 0xC0) {
        m_utf8Cp = char32_t(b & 0x1F);
        m_utf8Need = 1;
    } else if ((b & 0xF0) == 0xE0) {
        m_utf8Cp = char32_t(b & 0x0F);
        m_utf8Need = 2;
    } else if ((b & 0xF8) == 0xF0) {
        m_utf8Cp = char32_t(b & 0x07);
        m_utf8Need = 3;
    } else {
        emitText(0xFFFD);
    }
}

void VtParser::handleControl(uint8_t b)
{
    switch (b) {
    case 0x07:
        m_screen->ringBell();
        break;
    case 0x08:
        m_screen->backSpace();
        break;
    case 0x09:
        m_screen->tab(1);
        break;
    case 0x0A:
    case 0x0B:
    case 0x0C:
        m_screen->lineFeed();
        break;
    case 0x0D:
        m_screen->carriageReturn();
        break;
    case 0x0E:
        m_screen->selectCharset(1);
        break;
    case 0x0F:
        m_screen->selectCharset(0);
        break;
    default:
        break;
    }
}

void VtParser::handleEscape(uint8_t b)
{
    switch (b) {
    case '[':
        m_state = Csi;
        m_paramBuf.clear();
        m_interBuf.clear();
        return;
    case ']':
        m_state = Osc;
        m_strBuf.clear();
        m_stringEsc = false;
        return;
    case 'P':
        m_state = Dcs;
        m_stringEsc = false;
        return;
    case 'X':
    case '^':
    case '_':
        m_state = Ignore;
        m_stringEsc = false;
        return;
    case '(':
        m_charsetSlot = 0;
        m_state = EscapeInter;
        return;
    case ')':
        m_charsetSlot = 1;
        m_state = EscapeInter;
        return;
    case '*':
    case '+':
        m_charsetSlot = -2;
        m_state = EscapeInter;
        return;
    case '#':
    case '%':
    case ' ':
        m_interBuf = QByteArray(1, char(b));
        m_state = EscapeInter;
        return;
    case '7':
        m_screen->saveCursor();
        m_state = Ground;
        return;
    case '8':
        m_screen->restoreCursor();
        m_state = Ground;
        return;
    case 'D':
        m_screen->lineFeed();
        m_state = Ground;
        return;
    case 'E':
        m_screen->carriageReturn();
        m_screen->lineFeed();
        m_state = Ground;
        return;
    case 'M':
        m_screen->reverseIndex();
        m_state = Ground;
        return;
    case 'H':
        m_state = Ground;
        return;
    case 'c':
        m_screen->reset();
        m_state = Ground;
        return;
    case '=':
        m_state = Ground;
        return;
    case '>':
        m_state = Ground;
        return;
    default:
        m_state = Ground;
        return;
    }
}

void VtParser::handleEscapeInter(uint8_t b)
{
    if (m_charsetSlot >= 0) {
        m_screen->setCharset(m_charsetSlot, char(b));
        m_charsetSlot = -1;
        m_state = Ground;
        return;
    }
    if (m_interBuf == "#" && b == '8') {
        m_screen->alignTest();
        m_state = Ground;
        return;
    }
    m_state = Ground;
}

void VtParser::handleCsi(uint8_t b)
{
    if (b >= 0x30 && b <= 0x3F) {
        m_paramBuf.append(char(b));
        return;
    }
    if (b >= 0x20 && b <= 0x2F) {
        m_interBuf.append(char(b));
        return;
    }
    if (b >= 0x40 && b <= 0x7E) {
        dispatchCsi(b);
        m_state = Ground;
        return;
    }
    m_state = Ground;
}

void VtParser::dispatchCsi(uint8_t finalByte)
{
    char priv = 0;
    QVector<int> params;
    parseParams(m_paramBuf, priv, params);

    const auto P = [&params](int index, int fallback) {
        return index < params.size() ? params.at(index) : fallback;
    };

    switch (finalByte) {
    case '@':
        m_screen->insertChars(std::max(1, P(0, 1)));
        break;
    case 'A':
        m_screen->cursorUp(std::max(1, P(0, 1)));
        break;
    case 'B':
    case 'e':
        m_screen->cursorDown(std::max(1, P(0, 1)));
        break;
    case 'C':
    case 'a':
        m_screen->cursorRight(std::max(1, P(0, 1)));
        break;
    case 'D':
        m_screen->cursorLeft(std::max(1, P(0, 1)));
        break;
    case 'E':
        m_screen->cursorDown(std::max(1, P(0, 1)));
        m_screen->setCursorCol(0);
        break;
    case 'F':
        m_screen->cursorUp(std::max(1, P(0, 1)));
        m_screen->setCursorCol(0);
        break;
    case 'G':
    case '\140':
        m_screen->setCursorCol(std::max(1, P(0, 1)) - 1);
        break;
    case 'H':
    case 'f':
        m_screen->moveTo(std::max(1, P(0, 1)) - 1, std::max(1, P(1, 1)) - 1);
        break;
    case 'I':
        m_screen->tab(std::max(1, P(0, 1)));
        break;
    case 'J':
        m_screen->eraseInDisplay(P(0, 0));
        break;
    case 'K':
        m_screen->eraseInLine(P(0, 0));
        break;
    case 'L':
        m_screen->insertLines(std::max(1, P(0, 1)));
        break;
    case 'M':
        m_screen->deleteLines(std::max(1, P(0, 1)));
        break;
    case 'P':
        m_screen->deleteChars(std::max(1, P(0, 1)));
        break;
    case 'S':
        m_screen->scrollUp(std::max(1, P(0, 1)));
        break;
    case 'T':
        m_screen->scrollDown(std::max(1, P(0, 1)));
        break;
    case 'X':
        m_screen->eraseChars(std::max(1, P(0, 1)));
        break;
    case 'Z':
        m_screen->backTab(std::max(1, P(0, 1)));
        break;
    case 'b':
        m_screen->repeatLastChar(std::max(1, P(0, 1)));
        break;
    case 'c':
        if (priv == 0)
            m_screen->reportDeviceAttributes();
        break;
    case 'd':
        m_screen->setCursorRow(std::max(1, P(0, 1)) - 1);
        break;
    case 'g':
        break;
    case 'h':
    case 'l': {
        const bool on = (finalByte == 'h');
        if (params.isEmpty()) {
            if (priv == '?')
                m_screen->setPrivateMode(0, on);
        }
        for (int i = 0; i < params.size(); ++i) {
            if (priv == '?')
                m_screen->setPrivateMode(params.at(i), on);
            else
                m_screen->setAnsiMode(params.at(i), on);
        }
        break;
    }
    case 'm':
        if (priv == 0)
            m_screen->setSgr(params);
        break;
    case 'n':
        if (priv == '?')
            m_screen->reportStatus(P(0, 0));
        break;
    case 'q':
        break;
    case 'r':
        m_screen->setMargins(std::max(1, P(0, 1)) - 1, std::max(1, P(1, m_screen->rows())) - 1);
        break;
    case 's':
        if (priv == 0)
            m_screen->saveCursor();
        break;
    case 'u':
        if (priv == 0)
            m_screen->restoreCursor();
        break;
    case 't':
        break;
    default:
        break;
    }
}

void VtParser::handleOsc(uint8_t b)
{
    if (m_stringEsc) {
        m_stringEsc = false;
        if (b == '\\') {
            dispatchOsc();
            m_state = Ground;
            return;
        }
        m_strBuf.clear();
        m_state = Ground;
        return;
    }
    if (b == 0x07) {
        dispatchOsc();
        m_state = Ground;
        return;
    }
    if (b == 0x1B) {
        m_stringEsc = true;
        return;
    }
    if (m_strBuf.size() < 8192)
        m_strBuf.append(char(b));
}

void VtParser::dispatchOsc()
{
    const QByteArray payload = m_strBuf;
    m_strBuf.clear();
    const int sep = payload.indexOf(';');
    const int code = sep < 0 ? 0 : payload.left(sep).toInt();
    const QByteArray rest = sep < 0 ? QByteArray() : payload.mid(sep + 1);

    switch (code) {
    case 0:
    case 2:
        m_screen->setTitle(QString::fromUtf8(rest));
        break;
    case 7: {
        QByteArray url = rest;
        if (url.startsWith("file://"))
            url = url.mid(7);
        m_screen->setWorkingDirectory(QString::fromUtf8(url));
        break;
    }
    case 52: {
        const int semi = rest.indexOf(';');
        if (semi >= 0) {
            const QByteArray data = rest.mid(semi + 1);
            m_screen->requestClipboard(QString::fromUtf8(QByteArray::fromBase64(data)));
        }
        break;
    }
    default:
        break;
    }
}

} // namespace vx
