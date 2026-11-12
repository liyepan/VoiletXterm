#pragma once

#include "../session/SessionSpec.h"

#include <QMainWindow>
#include <QVector>

class QLabel;
class QListWidget;
class QSplitter;
class QStackedWidget;
class QTabWidget;

namespace vx {

class TerminalWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    void newSshSession();
    void newTelnetSession();
    void newSerialSession();
    void openSpec(const SessionSpec &spec);

private slots:
    void closeCurrentTab();
    void chooseFont();
    void reloadSessions();
    void connectSelectedSession();
    void showSessionMenu(const QPoint &pos);
    void onCurrentTabChanged(int index);
    void onTabCloseRequested(int index);
    void showAbout();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void buildActions();
    QWidget *buildWelcomePage();
    void openSession(const SessionSpec &spec);
    TerminalWidget *currentTerminal() const;
    void updateStatus();

    QSplitter *m_splitter = nullptr;
    QListWidget *m_sessionList = nullptr;
    QVector<SessionSpec> m_sessions;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_welcome = nullptr;
    QTabWidget *m_tabs = nullptr;
    QLabel *m_statusLabel = nullptr;

    QAction *m_newSshAction = nullptr;
    QAction *m_newTelnetAction = nullptr;
    QAction *m_newSerialAction = nullptr;
    QAction *m_closeTabAction = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_pasteAction = nullptr;
    QAction *m_zoomInAction = nullptr;
    QAction *m_zoomOutAction = nullptr;
    QAction *m_zoomResetAction = nullptr;
};

} // namespace vx
