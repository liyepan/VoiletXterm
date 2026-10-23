#pragma once

#include "../session/SessionSpec.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTabWidget;

namespace vx {

class SessionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SessionDialog(QWidget *parent = nullptr);
    SessionDialog(const SessionSpec &initial, QWidget *parent = nullptr);

    SessionSpec spec() const { return m_spec; }
    bool shouldSave() const;

public slots:
    void setType(int type);

private slots:
    void onAccept();
    void onSaveClicked();
    void onDeleteClicked();
    void onSessionActivated();
    void onBrowseKey();
    void onRefreshPorts();

private:
    void buildUi();
    void reloadSessions();
    void loadSpecIntoForm(const SessionSpec &spec);
    void applyPageToSpec();
    void syncPortDisplay();

    SessionSpec m_spec;

    QLineEdit *m_nameEdit = nullptr;
    QTabWidget *m_tabs = nullptr;
    QListWidget *m_sessionList = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QCheckBox *m_saveCheck = nullptr;

    // SSH
    QLineEdit *m_hostEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QLineEdit *m_userEdit = nullptr;
    QLineEdit *m_keyEdit = nullptr;
    QLineEdit *m_jumpEdit = nullptr;
    QLineEdit *m_commandEdit = nullptr;

    // Telnet
    QLineEdit *m_telnetHostEdit = nullptr;
    QSpinBox *m_telnetPortSpin = nullptr;

    // Serial
    QComboBox *m_serialPortCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_dataBitsCombo = nullptr;
    QComboBox *m_parityCombo = nullptr;
    QComboBox *m_stopBitsCombo = nullptr;
    QComboBox *m_flowCombo = nullptr;
    QCheckBox *m_localEchoCheck = nullptr;
};

} // namespace vx
