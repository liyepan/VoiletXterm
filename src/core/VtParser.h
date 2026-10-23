#pragma once

#include <QByteArray>

#include <cstdint>

namespace vx {

class Screen;

// Incremental VT100/xterm escape sequence parser. Bytes are decoded from UTF-8
// and pushed into Screen; no rendering happens here.
class VtParser
{
public:
    explicit VtParser(Screen *screen);

    void setScreen(Screen *screen) { m_screen = screen; }
    Screen *screen() const { return m_screen; }

    void feed(const char *data, int len);
    void feed(const QByteArray &data) { feed(data.constData(), data.size()); }
    void reset();

private:
    enum State {
        Ground,
        Escape,
        EscapeInter,
        Csi,
        Osc,
        Dcs,
        Ignore,
    };

    void handleGround(uint8_t b);
    void handleEscape(uint8_t b);
    void handleEscapeInter(uint8_t b);
    void handleCsi(uint8_t b);
    void handleOsc(uint8_t b);
    void handleControl(uint8_t b);
    void dispatchCsi(uint8_t finalByte);
    void dispatchOsc();
    void emitText(char32_t cp);

    Screen *m_screen = nullptr;
    State m_state = Ground;
    QByteArray m_paramBuf;
    QByteArray m_interBuf;
    QByteArray m_strBuf;
    int m_charsetSlot = -1;
    bool m_stringEsc = false;
    char32_t m_utf8Cp = 0;
    int m_utf8Need = 0;
};

} // namespace vx
