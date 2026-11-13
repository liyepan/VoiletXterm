#include "MainWindow.h"

#include "SessionDialog.h"
#include "SessionStore.h"

#include "../session/CredentialStore.h"
#include "../session/SessionFactory.h"
#include "../session/Transport.h"
#include "../term/TerminalTheme.h"
#include "../term/TerminalWidget.h"

#include "ThemeManager.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QFontDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>

namespace vx {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("VoiletXterm"));
    resize(1100, 700);
    buildUi();
    buildActions();

    QSettings settings;
    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    const QByteArray splitterState = settings.value(QStringLiteral("window/splitter")).toByteArray();
    if (!splitterState.isEmpty())
        m_splitter->restoreState(splitterState);


}

MainWindow::~MainWindow() = default;

QWidget *MainWindow::buildWelcomePage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(60, 60, 60, 60);
    layout->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("VoiletXterm"), page);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 16);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setProperty("role", "title");

    auto *subtitle = new QLabel(tr("SSH / Telnet / 串口，一个窗口搞定所有远程工作"), page);
    subtitle->setProperty("role", "muted");

    auto *hint = new QLabel(tr("快捷键：Ctrl+N 新建 SSH  ·  Ctrl+Shift+C 复制  ·  Ctrl+Shift+V 粘贴  ·  Ctrl+滚轮 缩放"), page);
    hint->setProperty("role", "dim");

    auto *row = new QHBoxLayout;
    auto *sshButton = new QPushButton(tr("新建 SSH 会话"), page);
    sshButton->setDefault(true);
    auto *telnetButton = new QPushButton(tr("新建 Telnet 会话"), page);
    auto *serialButton = new QPushButton(tr("打开串口"), page);
    row->addWidget(sshButton);
    row->addWidget(telnetButton);
    row->addWidget(serialButton);
    row->addStretch(1);

    layout->addStretch(1);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addSpacing(18);
    layout->addLayout(row);
    layout->addSpacing(24);
    layout->addWidget(hint);
    layout->addStretch(2);

    connect(sshButton, &QPushButton::clicked, this, &MainWindow::newSshSession);
    connect(telnetButton, &QPushButton::clicked, this, &MainWindow::newTelnetSession);
    connect(serialButton, &QPushButton::clicked, this, &MainWindow::newSerialSession);
    return page;
}

void MainWindow::buildUi()
{
    m_stack = new QStackedWidget(this);
    m_welcome = buildWelcomePage();
    m_tabs = new QTabWidget(this);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);

    m_stack->addWidget(m_welcome);
    m_stack->addWidget(m_tabs);
    m_stack->setCurrentWidget(m_welcome);

    // Session sidebar: always visible on the left, splitter width is rembered.
    auto *sidebar = new QWidget(this);
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    auto *sidebarTitle = new QLabel(tr("会话"), sidebar);
    sidebarTitle->setProperty("role", "muted");
    sidebarTitle->setContentsMargins(12, 8, 12, 6);
    sidebarLayout->addWidget(sidebarTitle);

    m_sessionList = new QListWidget(sidebar);
    m_sessionList->setFrameShape(QFrame::NoFrame);
    m_sessionList->setContextMenuPolicy(Qt::CustomContextMenu);
    sidebarLayout->addWidget(m_sessionList, 1);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->addWidget(sidebar);
    m_splitter->addWidget(m_stack);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({200, 900});
    setCentralWidget(m_splitter);

    connect(m_sessionList, &QListWidget::itemDoubleClicked, this, &MainWindow::connectSelectedSession);
    connect(m_sessionList, &QListWidget::customContextMenuRequested, this, &MainWindow::showSessionMenu);

    reloadSessions();

    m_statusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_statusLabel);

    connect(m_tabs, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);

    // The UI scheme changes; the terminal stays dark on purpose.
    connect(ThemeManager::instance(), &ThemeManager::schemeChanged, this, [this](bool) {
        const TerminalTheme theme = darkTheme();
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (auto *terminal = qobject_cast<TerminalWidget *>(m_tabs->widget(i)))
                terminal->setTheme(theme);
        }
    });
}

void MainWindow::buildActions()
{
    m_newSshAction = new QAction(tr("新建 SSH 会话..."), this);
    m_newSshAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+N")));
    connect(m_newSshAction, &QAction::triggered, this, &MainWindow::newSshSession);

    m_newTelnetAction = new QAction(tr("新建 Telnet 会话..."), this);
    connect(m_newTelnetAction, &QAction::triggered, this, &MainWindow::newTelnetSession);

    m_newSerialAction = new QAction(tr("打开串口..."), this);
    connect(m_newSerialAction, &QAction::triggered, this, &MainWindow::newSerialSession);

    m_closeTabAction = new QAction(tr("关闭当前标签"), this);
    m_closeTabAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+W")));
    connect(m_closeTabAction, &QAction::triggered, this, &MainWindow::closeCurrentTab);

    m_copyAction = new QAction(tr("复制"), this);
    m_copyAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")));
    connect(m_copyAction, &QAction::triggered, this, [this] {
        if (TerminalWidget *terminal = currentTerminal())
            terminal->copySelection();
    });

    m_pasteAction = new QAction(tr("粘贴"), this);
    m_pasteAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+V")));
    connect(m_pasteAction, &QAction::triggered, this, [this] {
        if (TerminalWidget *terminal = currentTerminal())
            terminal->pasteClipboard();
    });

    m_zoomInAction = new QAction(tr("放大字号"), this);
    m_zoomInAction->setShortcut(QKeySequence(QStringLiteral("Ctrl++")));
    connect(m_zoomInAction, &QAction::triggered, this, [this] {
        if (TerminalWidget *terminal = currentTerminal())
            terminal->zoomIn();
    });

    m_zoomOutAction = new QAction(tr("缩小字号"), this);
    m_zoomOutAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+-")));
    connect(m_zoomOutAction, &QAction::triggered, this, [this] {
        if (TerminalWidget *terminal = currentTerminal())
            terminal->zoomOut();
    });

    auto *fontAction = new QAction(tr("字体..."), this);
    connect(fontAction, &QAction::triggered, this, &MainWindow::chooseFont);

    m_zoomResetAction = new QAction(tr("重置字号"), this);
    m_zoomResetAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    connect(m_zoomResetAction, &QAction::triggered, this, [this] {
        if (TerminalWidget *terminal = currentTerminal())
            terminal->resetZoom();
    });

    auto *quitAction = new QAction(tr("退出"), this);
    quitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Q")));
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    auto *aboutAction = new QAction(tr("关于 VoiletXterm"), this);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    // No Alt mnemonics: Alt+letter belongs to the shell (readline bindings).
    QMenu *fileMenu = menuBar()->addMenu(tr("文件"));
    fileMenu->addAction(m_newSshAction);
    fileMenu->addAction(m_newTelnetAction);
    fileMenu->addAction(m_newSerialAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_closeTabAction);
    fileMenu->addSeparator();
    fileMenu->addAction(quitAction);

    QMenu *editMenu = menuBar()->addMenu(tr("编辑"));
    editMenu->addAction(m_copyAction);
    editMenu->addAction(m_pasteAction);

    QMenu *viewMenu = menuBar()->addMenu(tr("视图"));
    viewMenu->addAction(fontAction);
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addAction(m_zoomResetAction);
    viewMenu->addSeparator();

    QMenu *themeMenu = viewMenu->addMenu(tr("主题"));
    auto *themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);
    const struct {
        ThemeManager::Mode mode;
        QString label;
    } themeItems[] = {
        {ThemeManager::System, tr("跟随系统")},
        {ThemeManager::Schedule, tr("按时间自动（6:00–18:00 浅色）")},
        {ThemeManager::Light, tr("浅色")},
        {ThemeManager::Dark, tr("深色")},
    };
    for (const auto &item : themeItems) {
        auto *action = new QAction(item.label, this);
        action->setCheckable(true);
        action->setData(int(item.mode));
        action->setChecked(ThemeManager::instance()->mode() == item.mode);
        themeGroup->addAction(action);
        themeMenu->addAction(action);
    }
    connect(themeGroup, &QActionGroup::triggered, this, [](QAction *action) {
        ThemeManager::instance()->setMode(ThemeManager::Mode(action->data().toInt()));
    });

    QMenu *helpMenu = menuBar()->addMenu(tr("帮助"));
    helpMenu->addAction(aboutAction);

    QToolBar *toolbar = addToolBar(tr("主工具栏"));
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->addAction(m_newSshAction);
    toolbar->addAction(m_newTelnetAction);
    toolbar->addAction(m_newSerialAction);
    toolbar->addSeparator();
    toolbar->addAction(m_copyAction);
    toolbar->addAction(m_pasteAction);
}

void MainWindow::reloadSessions()
{
    if (!m_sessionList)
        return;

    m_sessionList->clear();
    m_sessions = SessionStore::load();
    for (const SessionSpec &spec : m_sessions) {
        auto *item = new QListWidgetItem(spec.displayName(), m_sessionList);
        item->setToolTip(spec.summary());
    }
    if (m_sessions.isEmpty()) {
        auto *placeholder = new QListWidgetItem(tr("还没有保存的会话"), m_sessionList);
        placeholder->setFlags(Qt::NoItemFlags);
        placeholder->setForeground(palette().color(QPalette::Disabled, QPalette::WindowText));
        placeholder->setToolTip(tr("新建会话时勾选“连接后保存到会话列表”"));
    }
}

void MainWindow::connectSelectedSession()
{
    const int row = m_sessionList->currentRow();
    if (row < 0 || row >= m_sessions.size())
        return;
    openSession(m_sessions.at(row));
}

void MainWindow::showSessionMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_sessionList->itemAt(pos);
    if (!item || !(item->flags() & Qt::ItemIsSelectable))
        return;

    QMenu menu(this);
    QAction *connectAction = menu.addAction(tr("连接"));
    QAction *deleteAction = menu.addAction(tr("删除会话"));
    QAction *chosen = menu.exec(m_sessionList->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;

    const int row = m_sessionList->row(item);
    if (row < 0 || row >= m_sessions.size())
        return;

    if (chosen == connectAction) {
        m_sessionList->setCurrentRow(row);
        connectSelectedSession();
    } else if (chosen == deleteAction) {
        const SessionSpec &spec = m_sessions.at(row);
        SessionStore::removeByName(spec.name.isEmpty() ? spec.displayName() : spec.name);
        reloadSessions();
    }
}

TerminalWidget *MainWindow::currentTerminal() const
{
    return qobject_cast<TerminalWidget *>(m_tabs->currentWidget());
}

void MainWindow::openSpec(const SessionSpec &spec)
{
    openSession(spec);
}

void MainWindow::newSshSession()
{
    SessionDialog dialog(this);
    dialog.setType(SessionSpec::Ssh);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (dialog.shouldSave()) {
        SessionStore::upsert(dialog.spec());
        reloadSessions();
    }
    openSession(dialog.spec());
}

void MainWindow::newTelnetSession()
{
    SessionDialog dialog(this);
    dialog.setType(SessionSpec::Telnet);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (dialog.shouldSave()) {
        SessionStore::upsert(dialog.spec());
        reloadSessions();
    }
    openSession(dialog.spec());
}

void MainWindow::newSerialSession()
{
    SessionDialog dialog(this);
    dialog.setType(SessionSpec::Serial);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (dialog.shouldSave()) {
        SessionStore::upsert(dialog.spec());
        reloadSessions();
    }
    openSession(dialog.spec());
}

void MainWindow::openSession(const SessionSpec &spec)
{
    SessionSpec resolved = spec;
    if (resolved.type == SessionSpec::Ssh && resolved.password.isEmpty() && resolved.savePassword)
        resolved.password = CredentialStore::load(resolved.credentialKey());

    auto *terminal = new TerminalWidget(this);
    TerminalWidget *widget = terminal;
    widget->setTheme(darkTheme());

    QSettings fontSettings;
    const QString storedFamily = fontSettings.value(QStringLiteral("font/family")).toString();
    if (!storedFamily.isEmpty())
        widget->setBaseFontFamily(storedFamily);
    const int storedSize = fontSettings.value(QStringLiteral("font/size"), 0).toInt();
    if (storedSize > 0)
        widget->setBaseFontSize(storedSize);
    const int storedWeight = fontSettings.value(QStringLiteral("font/weight"), 0).toInt();
    if (storedWeight > 0)
        widget->setBaseFontWeight(storedWeight);

    const int index = m_tabs->addTab(widget, resolved.displayName());
    m_tabs->setCurrentIndex(index);
    m_stack->setCurrentWidget(m_tabs);

    Transport *transport = createTransport(resolved, widget);
    if (!transport) {
        delete widget;
        return;
    }

    wireSession(widget, resolved);

    widget->setTransport(transport);
    widget->setFocus();
    updateStatus();
}

void MainWindow::wireSession(TerminalWidget *widget, const SessionSpec &spec)
{
    m_specs.insert(widget, spec);

    connect(widget, &TerminalWidget::sessionFinished, this, [this, widget](const QString &reason) {
        widget->parser()->feed(QStringLiteral("\r\n\033[38;5;203m[会话已结束: %1]\033[0m\r\n"
                                             "\033[38;5;245m[按 r 重连]\033[0m\r\n")
                                   .arg(reason)
                                   .toUtf8());
        widget->viewport()->update();
        const int tabIndex = m_tabs->indexOf(widget);
        if (tabIndex >= 0) {
            QString title = m_tabs->tabText(tabIndex);
            if (!title.endsWith(tr(" (已断开)")))
                m_tabs->setTabText(tabIndex, title + tr(" (已断开)"));
            if (tabIndex == m_tabs->currentIndex())
                updateStatus();
        }
        widget->releaseTransport();
        m_statusLabel->setText(reason);
    });
    connect(widget, &TerminalWidget::reconnectRequested, this, [this, widget] {
        reconnectSession(widget);
    });
    connect(widget, &TerminalWidget::bell, this, [] { QApplication::beep(); });
    connect(widget, &TerminalWidget::titleChanged, this, [this, widget](const QString &title) {
        if (title.isEmpty())
            return;
        const int tabIndex = m_tabs->indexOf(widget);
        if (tabIndex < 0)
            return;
        m_tabs->setTabText(tabIndex, title);
        if (tabIndex == m_tabs->currentIndex())
            updateStatus();
    });
    connect(widget, &QObject::destroyed, this, [this, widget] { m_specs.remove(widget); });
}

void MainWindow::reconnectSession(TerminalWidget *widget)
{
    const auto it = m_specs.constFind(widget);
    if (it == m_specs.constEnd())
        return;

    const int tabIndex = m_tabs->indexOf(widget);
    if (tabIndex >= 0) {
        QString title = m_tabs->tabText(tabIndex);
        title.remove(tr(" (已断开)"));
        m_tabs->setTabText(tabIndex, title);
    }

    widget->clearSelection();
    widget->parser()->feed(QStringLiteral("\r\n\033[38;5;114m[正在重连…]\033[0m\r\n").toUtf8());

    // The old transport released itself when it finished; a fresh one picks up
    // the same session spec, including the saved password.
    Transport *transport = createTransport(it.value(), widget);
    if (!transport) {
        widget->parser()->feed(QStringLiteral("\033[38;5;203m[重连失败]\033[0m\r\n").toUtf8());
        widget->viewport()->update();
        return;
    }

    widget->setTransport(transport);
    widget->setFocus();
    updateStatus();
}

void MainWindow::chooseFont()
{
    QSettings settings;
    QFont initial(settings.value(QStringLiteral("font/family")).toString(),
                  settings.value(QStringLiteral("font/size"), 11).toInt());
    if (initial.family().isEmpty()) {
        if (TerminalWidget *terminal = currentTerminal())
            initial = QFont(terminal->baseFontFamily(), terminal->baseFontSize());
    }

    bool accepted = false;
    const QFont chosen = QFontDialog::getFont(&accepted, initial, this, tr("选择终端字体"));
    if (!accepted)
        return;

    const int size = chosen.pointSize() > 0 ? chosen.pointSize() : 11;
    settings.setValue(QStringLiteral("font/family"), chosen.family());
    settings.setValue(QStringLiteral("font/size"), size);
    settings.setValue(QStringLiteral("font/weight"), int(chosen.weight()));

    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto *terminal = qobject_cast<TerminalWidget *>(m_tabs->widget(i))) {
            terminal->setBaseFontFamily(chosen.family());
            terminal->setBaseFontSize(size);
            terminal->setBaseFontWeight(int(chosen.weight()));
        }
    }
}

void MainWindow::closeCurrentTab()
{
    if (m_tabs->count() == 0)
        return;
    onTabCloseRequested(m_tabs->currentIndex());
}

void MainWindow::onTabCloseRequested(int index)
{
    QWidget *page = m_tabs->widget(index);
    if (!page)
        return;
    m_tabs->removeTab(index);
    page->deleteLater();
    if (m_tabs->count() == 0) {
        m_stack->setCurrentWidget(m_welcome);
        m_statusLabel->clear();
        setWindowTitle(QStringLiteral("VoiletXterm"));
    }
}

void MainWindow::onCurrentTabChanged(int)
{
    updateStatus();
    if (TerminalWidget *terminal = currentTerminal())
        terminal->setFocus();
}

void MainWindow::updateStatus()
{
    TerminalWidget *terminal = currentTerminal();
    if (!terminal) {
        m_statusLabel->clear();
        setWindowTitle(QStringLiteral("VoiletXterm"));
        return;
    }
    Transport *transport = terminal->transport();
    if (transport)
        m_statusLabel->setText(transport->description());
    const int index = m_tabs->currentIndex();
    setWindowTitle(QStringLiteral("VoiletXterm — %1").arg(m_tabs->tabText(index)));
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("关于 VoiletXterm"),
                       tr("<h3>VoiletXterm</h3>"
                          "<p>一个轻量的 SSH / Telnet / 串口终端工作台。</p>"
                          "<p>Qt %1 · 自研 VT100/xterm 终端内核</p>"
                          "<p>终端字体：JetBrains Mono NL（SIL Open Font License 1.1）</p>")
                           .arg(QString::fromLatin1(qVersion())));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/splitter"), m_splitter->saveState());
    event->accept();
}

} // namespace vx
