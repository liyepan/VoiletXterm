#include "SessionDialog.h"

#include "SessionStore.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace vx {

SessionDialog::SessionDialog(QWidget *parent)
    : QDialog(parent)
{
    buildUi();
    reloadSessions();
}

SessionDialog::SessionDialog(const SessionSpec &initial, QWidget *parent)
    : QDialog(parent)
{
    buildUi();
    reloadSessions();
    loadSpecIntoForm(initial);
}

void SessionDialog::buildUi()
{
    setWindowTitle(tr("新建会话"));
    resize(760, 480);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    // ---------------- saved sessions ----------------
    auto *left = new QVBoxLayout;
    left->addWidget(new QLabel(tr("已保存的会话"), this));
    m_sessionList = new QListWidget(this);
    m_sessionList->setMinimumWidth(200);
    left->addWidget(m_sessionList, 1);

    auto *listButtons = new QHBoxLayout;
    m_deleteButton = new QPushButton(tr("删除"), this);
    auto *saveButton = new QPushButton(tr("保存"), this);
    auto *newButton = new QPushButton(tr("清空表单"), this);
    listButtons->addWidget(m_deleteButton);
    listButtons->addWidget(saveButton);
    listButtons->addWidget(newButton);
    listButtons->addStretch(1);
    left->addLayout(listButtons);
    root->addLayout(left, 0);

    // ---------------- form ----------------
    auto *right = new QVBoxLayout;
    auto *nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel(tr("会话名称"), this));
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("留空则使用主机名"));
    nameRow->addWidget(m_nameEdit, 1);
    right->addLayout(nameRow);

    m_tabs = new QTabWidget(this);

    // SSH page
    auto *sshPage = new QWidget(this);
    auto *sshForm = new QFormLayout(sshPage);
    m_hostEdit = new QLineEdit(sshPage);
    m_hostEdit->setPlaceholderText(QStringLiteral("example.com 或 192.168.1.10"));
    m_portSpin = new QSpinBox(sshPage);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(22);
    m_userEdit = new QLineEdit(sshPage);
    m_userEdit->setPlaceholderText(tr("root"));
    auto *keyRow = new QHBoxLayout;
    m_keyEdit = new QLineEdit(sshPage);
    m_keyEdit->setPlaceholderText(tr("可选，默认使用 ssh-agent 或 ~/.ssh/config"));
    auto *browse = new QPushButton(tr("浏览"), sshPage);
    keyRow->addWidget(m_keyEdit, 1);
    keyRow->addWidget(browse);
    m_jumpEdit = new QLineEdit(sshPage);
    m_jumpEdit->setPlaceholderText(tr("可选，例如 user@bastion:22"));
    m_commandEdit = new QLineEdit(sshPage);
    m_commandEdit->setPlaceholderText(tr("可选，登录后立即执行"));
    sshForm->addRow(tr("主机"), m_hostEdit);
    sshForm->addRow(tr("端口"), m_portSpin);
    sshForm->addRow(tr("用户名"), m_userEdit);
    sshForm->addRow(tr("私钥"), keyRow);
    sshForm->addRow(tr("跳板机"), m_jumpEdit);
    sshForm->addRow(tr("远程命令"), m_commandEdit);
    m_tabs->addTab(sshPage, tr("SSH"));

    // Telnet page
    auto *telnetPage = new QWidget(this);
    auto *telnetForm = new QFormLayout(telnetPage);
    m_telnetHostEdit = new QLineEdit(telnetPage);
    m_telnetPortSpin = new QSpinBox(telnetPage);
    m_telnetPortSpin->setRange(1, 65535);
    m_telnetPortSpin->setValue(23);
    telnetForm->addRow(tr("主机"), m_telnetHostEdit);
    telnetForm->addRow(tr("端口"), m_telnetPortSpin);
    m_tabs->addTab(telnetPage, tr("Telnet"));

    // Serial page
    auto *serialPage = new QWidget(this);
    auto *serialForm = new QFormLayout(serialPage);
    auto *portRow = new QHBoxLayout;
    m_serialPortCombo = new QComboBox(serialPage);
    m_serialPortCombo->setEditable(true);
    auto *refresh = new QPushButton(tr("刷新"), serialPage);
    portRow->addWidget(m_serialPortCombo, 1);
    portRow->addWidget(refresh);
    m_baudCombo = new QComboBox(serialPage);
    m_baudCombo->setEditable(true);
    for (int baud : {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600})
        m_baudCombo->addItem(QString::number(baud));
    m_baudCombo->setCurrentText(QStringLiteral("115200"));
    m_dataBitsCombo = new QComboBox(serialPage);
    for (int bits : {5, 6, 7, 8})
        m_dataBitsCombo->addItem(QString::number(bits));
    m_dataBitsCombo->setCurrentText(QStringLiteral("8"));
    m_parityCombo = new QComboBox(serialPage);
    m_parityCombo->addItem(tr("无"), QStringLiteral("none"));
    m_parityCombo->addItem(tr("偶校验"), QStringLiteral("even"));
    m_parityCombo->addItem(tr("奇校验"), QStringLiteral("odd"));
    m_parityCombo->addItem(tr("空格"), QStringLiteral("space"));
    m_parityCombo->addItem(tr("标记"), QStringLiteral("mark"));
    m_stopBitsCombo = new QComboBox(serialPage);
    m_stopBitsCombo->addItem(QStringLiteral("1"), 1);
    m_stopBitsCombo->addItem(QStringLiteral("2"), 2);
    m_flowCombo = new QComboBox(serialPage);
    m_flowCombo->addItem(tr("无"), QStringLiteral("none"));
    m_flowCombo->addItem(tr("硬件 RTS/CTS"), QStringLiteral("hardware"));
    m_flowCombo->addItem(tr("软件 XON/XOFF"), QStringLiteral("software"));
    m_localEchoCheck = new QCheckBox(tr("本地回显"), serialPage);
    serialForm->addRow(tr("串口"), portRow);
    serialForm->addRow(tr("波特率"), m_baudCombo);
    serialForm->addRow(tr("数据位"), m_dataBitsCombo);
    serialForm->addRow(tr("校验"), m_parityCombo);
    serialForm->addRow(tr("停止位"), m_stopBitsCombo);
    serialForm->addRow(tr("流控"), m_flowCombo);
    serialForm->addRow(QString(), m_localEchoCheck);
    m_tabs->addTab(serialPage, tr("串口"));

    right->addWidget(m_tabs, 1);

    m_saveCheck = new QCheckBox(tr("连接后保存到会话列表"), this);
    m_saveCheck->setChecked(true);
    right->addWidget(m_saveCheck);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("连接"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    right->addWidget(buttons);

    root->addLayout(right, 1);

    connect(buttons, &QDialogButtonBox::accepted, this, &SessionDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SessionDialog::reject);
    connect(m_sessionList, &QListWidget::itemDoubleClicked, this, &SessionDialog::onSessionActivated);
    connect(m_sessionList, &QListWidget::itemActivated, this, &SessionDialog::onSessionActivated);
    connect(m_deleteButton, &QPushButton::clicked, this, &SessionDialog::onDeleteClicked);
    connect(saveButton, &QPushButton::clicked, this, &SessionDialog::onSaveClicked);
    connect(newButton, &QPushButton::clicked, this, [this] {
        m_nameEdit->clear();
        m_hostEdit->clear();
        m_userEdit->clear();
        m_keyEdit->clear();
        m_jumpEdit->clear();
        m_commandEdit->clear();
        m_telnetHostEdit->clear();
    });
    connect(browse, &QPushButton::clicked, this, &SessionDialog::onBrowseKey);
    connect(refresh, &QPushButton::clicked, this, &SessionDialog::onRefreshPorts);

    onRefreshPorts();
}

void SessionDialog::onRefreshPorts()
{
    const QString current = m_serialPortCombo->currentText();
    m_serialPortCombo->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        QString label = info.systemLocation();
        if (!info.description().isEmpty())
            label += QStringLiteral(" - %1").arg(info.description());
        m_serialPortCombo->addItem(label, info.systemLocation());
    }
    if (m_serialPortCombo->count() == 0)
        m_serialPortCombo->addItem(QStringLiteral("/dev/ttyUSB0"), QStringLiteral("/dev/ttyUSB0"));
    if (!current.isEmpty())
        m_serialPortCombo->setCurrentText(current);
    syncPortDisplay();
}

void SessionDialog::syncPortDisplay()
{
    m_serialPortCombo->setEditText(m_serialPortCombo->currentData().toString().isEmpty()
                                       ? m_serialPortCombo->currentText()
                                       : m_serialPortCombo->currentData().toString());
}

void SessionDialog::onBrowseKey()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("选择私钥"),
                                                      QDir::homePath() + QStringLiteral("/.ssh"));
    if (!file.isEmpty())
        m_keyEdit->setText(file);
}

void SessionDialog::reloadSessions()
{
    m_sessionList->clear();
    const auto sessions = SessionStore::load();
    for (const SessionSpec &spec : sessions) {
        auto *item = new QListWidgetItem(spec.displayName(), m_sessionList);
        item->setData(Qt::UserRole, int(spec.type));
        item->setToolTip(spec.summary());
    }
}

void SessionDialog::onSessionActivated()
{
    QListWidgetItem *item = m_sessionList->currentItem();
    if (!item)
        return;
    const auto sessions = SessionStore::load();
    const int row = m_sessionList->row(item);
    if (row < 0 || row >= sessions.size())
        return;
    loadSpecIntoForm(sessions.at(row));
}

void SessionDialog::loadSpecIntoForm(const SessionSpec &spec)
{
    m_nameEdit->setText(spec.name);
    m_spec = spec;
    switch (spec.type) {
    case SessionSpec::Ssh:
        m_tabs->setCurrentIndex(0);
        m_hostEdit->setText(spec.host);
        m_portSpin->setValue(spec.port > 0 ? spec.port : 22);
        m_userEdit->setText(spec.user);
        m_keyEdit->setText(spec.keyFile);
        m_jumpEdit->setText(spec.jumpHost);
        m_commandEdit->setText(spec.remoteCommand);
        break;
    case SessionSpec::Telnet:
        m_tabs->setCurrentIndex(1);
        m_telnetHostEdit->setText(spec.host);
        m_telnetPortSpin->setValue(spec.port > 0 ? spec.port : 23);
        break;
    case SessionSpec::Serial:
        m_tabs->setCurrentIndex(2);
        m_serialPortCombo->setEditText(spec.serialPort);
        m_baudCombo->setCurrentText(QString::number(spec.baudRate));
        m_dataBitsCombo->setCurrentText(QString::number(spec.dataBits));
        m_stopBitsCombo->setCurrentText(QString::number(spec.stopBits));
        for (int i = 0; i < m_parityCombo->count(); ++i) {
            if (m_parityCombo->itemData(i).toString() == spec.parity) {
                m_parityCombo->setCurrentIndex(i);
                break;
            }
        }
        for (int i = 0; i < m_flowCombo->count(); ++i) {
            if (m_flowCombo->itemData(i).toString() == spec.flowControl) {
                m_flowCombo->setCurrentIndex(i);
                break;
            }
        }
        m_localEchoCheck->setChecked(spec.localEcho);
        break;
    }
}

void SessionDialog::setType(int type)
{
    m_tabs->setCurrentIndex(qBound(0, type, 2));
}

void SessionDialog::applyPageToSpec()
{
    const int page = m_tabs->currentIndex();
    if (page == 0) {
        m_spec.type = SessionSpec::Ssh;
        m_spec.host = m_hostEdit->text().trimmed();
        m_spec.port = m_portSpin->value();
        m_spec.user = m_userEdit->text().trimmed();
        m_spec.keyFile = m_keyEdit->text().trimmed();
        m_spec.jumpHost = m_jumpEdit->text().trimmed();
        m_spec.remoteCommand = m_commandEdit->text().trimmed();
    } else if (page == 1) {
        m_spec.type = SessionSpec::Telnet;
        m_spec.host = m_telnetHostEdit->text().trimmed();
        m_spec.port = m_telnetPortSpin->value();
    } else {
        m_spec.type = SessionSpec::Serial;
        const QString data = m_serialPortCombo->currentData().toString();
        m_spec.serialPort = data.isEmpty() ? m_serialPortCombo->currentText().trimmed() : data;
        m_spec.baudRate = m_baudCombo->currentText().toInt();
        m_spec.dataBits = m_dataBitsCombo->currentText().toInt();
        m_spec.stopBits = m_stopBitsCombo->currentData().toInt();
        m_spec.parity = m_parityCombo->currentData().toString();
        m_spec.flowControl = m_flowCombo->currentData().toString();
        m_spec.localEcho = m_localEchoCheck->isChecked();
    }
    m_spec.name = m_nameEdit->text().trimmed();
}

void SessionDialog::onAccept()
{
    applyPageToSpec();
    if (m_spec.type == SessionSpec::Serial) {
        if (m_spec.serialPort.isEmpty()) {
            QMessageBox::warning(this, tr("参数不完整"), tr("请选择或填写串口设备。"));
            return;
        }
    } else if (m_spec.host.isEmpty()) {
        QMessageBox::warning(this, tr("参数不完整"), tr("请填写主机地址。"));
        return;
    }

    if (m_spec.name.isEmpty())
        m_spec.name = m_spec.displayName();
    accept();
}

bool SessionDialog::shouldSave() const
{
    return m_saveCheck->isChecked();
}

void SessionDialog::onSaveClicked()
{
    applyPageToSpec();
    if (m_spec.name.isEmpty())
        m_spec.name = m_spec.displayName();
    SessionStore::upsert(m_spec);
    reloadSessions();
}

void SessionDialog::onDeleteClicked()
{
    QListWidgetItem *item = m_sessionList->currentItem();
    if (!item)
        return;
    SessionStore::removeByName(item->text());
    reloadSessions();
}

} // namespace vx
