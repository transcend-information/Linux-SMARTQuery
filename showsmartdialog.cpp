#include "showsmartdialog.h"
#include "ui_showsmartdialog.h"

ShowSmartDialog::ShowSmartDialog(QStringList smartList, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ShowSmartDialog)
{
    ui->setupUi(this);

    ui->tableWidget->setColumnCount(3);
    m_tableHeader<<"Bytes"<<"Property"<<"Value";
    ui->tableWidget->setHorizontalHeaderLabels(m_tableHeader);
    ui->tableWidget->verticalHeader()->setVisible(false);
    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);



    for(int i =0; i<smartList.length(); i++)
    {
        QString content = smartList[i];
        QString attByte = content.split("/").at(0);
        QString attr = content.split("/").at(1);
        QString val = content.split("/").at(2);
        ui->tableWidget->insertRow(i);
        ui->tableWidget->setItem(i,0,new QTableWidgetItem(attByte));
        ui->tableWidget->setItem(i,1,new QTableWidgetItem(attr));
        ui->tableWidget->setItem(i,2,new QTableWidgetItem(val));

    }

    ui->tableWidget->resizeColumnsToContents();
    QHeaderView* header = ui->tableWidget->horizontalHeader();
    header->setStretchLastSection(true);

}

ShowSmartDialog::~ShowSmartDialog()
{
    delete ui;
}

void ShowSmartDialog::on_pushButton_clicked()
{
    close();
}
