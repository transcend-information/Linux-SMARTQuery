#ifndef SHOWSMARTDIALOG_H
#define SHOWSMARTDIALOG_H

#include <QDialog>

namespace Ui {
class ShowSmartDialog;
}

class ShowSmartDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShowSmartDialog(QStringList smartList, QWidget *parent = 0);
    ~ShowSmartDialog();

private slots:
    void on_pushButton_clicked();

private:
    Ui::ShowSmartDialog *ui;
    QStringList m_tableHeader;
};

#endif // SHOWSMARTDIALOG_H
