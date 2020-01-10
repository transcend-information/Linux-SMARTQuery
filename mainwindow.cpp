#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <sys/ioctl.h>
#include <QProcess>
#include <QDesktopWidget>
#include <QMessageBox>
#include "ata_io.h"
#include <qdebug.h>
#include "int64.h"
#include <nvme_util.h>
#include <showsmartdialog.h>

const char * format_char_array(char * str, int strsize, const char * chr, int chrsize)
{
  int b = 0;
  while (b < chrsize && chr[b] == ' ')
    b++;
  int n = 0;
  while (b+n < chrsize && chr[b+n])
    n++;
  while (n > 0 && chr[b+n-1] == ' ')
    n--;

  if (n >= strsize)
    n = strsize-1;

  for (int i = 0; i < n; i++) {
    char c = chr[b+i];
    str[i] = (' ' <= c && c <= '~' ? c : '?');
  }

  str[n] = 0;
  return str;
}


static const char * to_str(char (& str)[64], int k)
{
  if (!k) // unsupported?
    str[0] = '-', str[1] = 0;
  else
    snprintf(str, sizeof(str), "%d Celsius", k - 273);
  return str;
}
const char * format_with_thousands_sep(char * str, int strsize, uint64_t val,
                                       const char * thousands_sep /* = 0 */)
{
  if (!thousands_sep) {
    thousands_sep = ",";
#ifdef HAVE_LOCALE_H
    setlocale(LC_ALL, "");
    const struct lconv * currentlocale = localeconv();
    if (*(currentlocale->thousands_sep))
      thousands_sep = currentlocale->thousands_sep;
#endif
  }

  char num[64];
  snprintf(num, sizeof(num), "%" PRIu64, val);
  int numlen = strlen(num);

  int i = 0, j = 0;
  do
    str[j++] = num[i++];
  while (i < numlen && (numlen - i) % 3 != 0 && j < strsize-1);
  str[j] = 0;

  while (i < numlen && j < strsize-1) {
    j += snprintf(str+j, strsize-j, "%s%.3s", thousands_sep, num+i);
    i += 3;
  }

  return str;
}
const char * format_capacity(char * str, int strsize, uint64_t val,
                             const char * decimal_point /* = 0 */)
{
  if (!decimal_point) {
    decimal_point = ".";
#ifdef HAVE_LOCALE_H
    setlocale(LC_ALL, "");
    const struct lconv * currentlocale = localeconv();
    if (*(currentlocale->decimal_point))
      decimal_point = currentlocale->decimal_point;
#endif
  }

  const unsigned factor = 1000; // 1024 for KiB,MiB,...
  static const char prefixes[] = " KMGTP";

  // Find d with val in [d, d*factor)
  unsigned i = 0;
  uint64_t d = 1;
  for (uint64_t d2 = d * factor; val >= d2; d2 *= factor) {
    d = d2;
    if (++i >= sizeof(prefixes)-2)
      break;
  }

  // Print 3 digits
  uint64_t n = val / d;
  if (i == 0)
    snprintf(str, strsize, "%u B", (unsigned)n);
  else if (n >= 100) // "123 xB"
    snprintf(str, strsize, "%" PRIu64 " %cB", n, prefixes[i]);
  else if (n >= 10)  // "12.3 xB"
    snprintf(str, strsize, "%" PRIu64 "%s%u %cB", n, decimal_point,
        (unsigned)(((val % d) * 10) / d), prefixes[i]);
  else               // "1.23 xB"
    snprintf(str, strsize, "%" PRIu64 "%s%02u %cB", n, decimal_point,
        (unsigned)(((val % d) * 100) / d), prefixes[i]);

  return str;
}

static unsigned le16_to_uint(const unsigned char (& val)[2])
{
  return ((val[1] << 8) | val[0]);
}

static const char * le128_to_str(char (& str)[64], uint64_t hi, uint64_t lo, unsigned bytes_per_unit)
{
  if (!hi) {
    // Up to 64-bit, print exact value
    format_with_thousands_sep(str, sizeof(str)-16, lo);

    if (lo && bytes_per_unit && lo < 0xffffffffffffffffULL / bytes_per_unit) {
      int i = strlen(str);
      str[i++] = ' '; str[i++] = '[';
      format_capacity(str+i, (int)sizeof(str)-i-1, lo * bytes_per_unit);
      i = strlen(str);
      str[i++] = ']'; str[i] = 0;
    }
  }
  else {
    // More than 64-bit, print approximate value, prepend ~ flag
    snprintf(str, sizeof(str), "~%.0f",
             hi * (0xffffffffffffffffULL + 1.0) + lo);
  }

  return str;
}
// Format 128 bit LE integer for printing.
// Add value with SI prefixes if BYTES_PER_UNIT is specified.
static const char * le128_to_str(char (& str)[64], const unsigned char (& val)[16],
  unsigned bytes_per_unit = 0)
{
  uint64_t hi = val[15];
  for (int i = 15-1; i >= 8; i--) {
    hi <<= 8; hi += val[i];
  }
  uint64_t lo = val[7];
  for (int i =  7-1; i >= 0; i--) {
    lo <<= 8; lo += val[i];
  }
  return le128_to_str(str, hi, lo, bytes_per_unit);
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    sel_row = -1;
    moving = false;

    UI_init();
    scanDevice();

    connect(ui->tableView, SIGNAL(clicked(QModelIndex)), this, SLOT(ontableclicked(QModelIndex)));

    connect(this,  SIGNAL(fw_test_signal(QStringList)), this, SLOT(fw_test_ret(QStringList)));

}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::UI_init()
{
    //setWindowFlags(Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint);
    setWindowFlags(Qt::CustomizeWindowHint);
    QString title="   ";
    title.append(TITLE);
    QDesktopWidget *desktop=QApplication::desktop();
    desktopHeight=desktop->screenGeometry().height();
    desktopWidth=desktop->screenGeometry().width();

    if(desktopWidth>RETINA_WIDTH ||desktopHeight>RETINA_HEIGHT)
    {
        setGeometry(0,0,desktopWidth/FRAME_WIDTH_RATIO,desktopHeight/FRAME_HEIGHT_RATIO);

        ui->tableView->setGeometry(QRect(desktopWidth/TABLEVIEW_X_OFFSET_RATIO, desktopHeight/TABLEVIEW_Y_OFFSET_RATIO, desktopWidth/TABLEVIEW_WIDTH_RATIO, desktopHeight/TABLEVIEW_HEIGHT_RATIO));
        ui->btn_Update->setGeometry(desktopWidth/UPDATEBUTTON_X_OFFSET_RATIO, ui->tableView->y() + ui->tableView->height() + desktopHeight/UPDATEBUTTON_Y_ADDOFFSET_RATIO, desktopWidth/UPDATEBUTTON_WIDTH_RATIO, desktopHeight/UPDATEBUTTON_HEIGHT_RATIO);
    }
    else
    {
        setGeometry(0,0,720,370);

        ui->tableView->setGeometry(QRect(20, 59, 680, 250));
        ui->btn_Update->setGeometry(620, ui->tableView->y() + ui->tableView->height() + 20, 80, 25);
    }

    ui->btn_close->setParent(ui->label_Title);
    ui->btn_re->setParent(ui->label_Title);
    ui->btn_info->setParent(ui->label_Title);
    ui->btn_min->setParent(ui->label_Title);

    if(desktopWidth>RETINA_WIDTH ||desktopHeight>RETINA_HEIGHT)
    {
        ui->label_Title->setGeometry(0, 0, desktopWidth/FRAME_WIDTH_RATIO, desktopHeight/TITLE_HEIGHT_RATIO);
    }
    else
    {
        ui->label_Title->setGeometry(0, 0, 720, 39);
    }
    QString spaceTitle = "  " + QString(TITLE);
    ui->label_Title->setText(spaceTitle);

    if(desktopWidth>RETINA_WIDTH ||desktopHeight>RETINA_HEIGHT)
    {
        ui->btn_close->setGeometry(this->width() - desktopWidth/TITLEBUTTON_WIDTH_RATIO, 0, desktopWidth/TITLEBUTTON_WIDTH_RATIO, desktopHeight/TITLE_HEIGHT_RATIO);
        ui->btn_info->setGeometry(ui->btn_close->x() - ui->btn_close->width(), 0, desktopWidth/TITLEBUTTON_WIDTH_RATIO, desktopHeight/TITLE_HEIGHT_RATIO);
        ui->btn_re->setGeometry(ui->btn_info->x() - ui->btn_info->width(), 0, desktopWidth/TITLEBUTTON_WIDTH_RATIO, desktopHeight/TITLE_HEIGHT_RATIO);
    }
    else
    {
        ui->btn_close->setGeometry(this->width() - ui->btn_close->width(), 0, 41, 39);
        ui->btn_info->setGeometry(ui->btn_close->x() - ui->btn_close->width(), 0, 41, 39);
        ui->btn_re->setGeometry(ui->btn_info->x() - ui->btn_info->width(), 0, 41, 39);
    }
    ui->btn_min->setVisible(false);

    //adjust font size
    QFont font1=ui->label_Title->font();
    QFont font2=ui->btn_Update->font();
    if(desktopWidth>RETINA_WIDTH ||desktopHeight>RETINA_HEIGHT)
    {
        font1.setPointSize(desktopWidth/TITLEFONT_SIZE_RATIO);
        font2.setPointSize(desktopWidth/TABLEFONT_SIZE_RATIO);
    }
    else
    {
        font1.setPointSize(14);
        font2.setPointSize(11);
    }

    ui->label_Title->setFont(font1);
    ui->btn_Update->setFont(font2);
    ui->tableView->setFont(font2);
    //

}

void MainWindow::set_tabview()
{
    //Set table view attribute
    ui->tableView->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);

    char cssstr[128];
    if(desktopWidth>RETINA_WIDTH ||desktopHeight>RETINA_HEIGHT)
    {
        ui->tableView->setColumnWidth(0,desktopWidth/TABLEVIEW_COLUME0_WIDTH_RATIO);
        ui->tableView->setColumnWidth(1,desktopWidth/TABLEVIEW_COLUME1_WIDTH_RATIO);
        ui->tableView->setColumnWidth(2,desktopWidth/TABLEVIEW_COLUME2_WIDTH_RATIO);
        ui->tableView->setColumnWidth(3,desktopWidth/TABLEVIEW_COLUME3_WIDTH_RATIO);
        sprintf(cssstr,"QHeaderView { font-size: %dpt; }",desktopWidth/TITLEFONT_SIZE_RATIO);
    }
    else
    {
        ui->tableView->setColumnWidth(0,100);
        ui->tableView->setColumnWidth(1,170);
        ui->tableView->setColumnWidth(2,120);
        ui->tableView->setColumnWidth(3,170);
        sprintf(cssstr,"QHeaderView { font-size: 11pt; }");
    }
    ui->tableView->setStyleSheet(cssstr);
    ui->tableView->horizontalHeader()->setSectionResizeMode(4,QHeaderView::Stretch);
    ui->tableView->verticalHeader()->setVisible(false);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView->horizontalHeader()->setSectionsClickable(false);
}

void MainWindow::ontableclicked(QModelIndex index)
{
    if(index.isValid())
    {
        sel_row = index.row();
        sel_dev = data[sel_row][0];
        sel_model = data[sel_row][1];
        sel_fw = data[sel_row][2];
        QString sn = data[sel_row][3];
        sel_wo = sn.left(6);
    }
}

void MainWindow::scanDevice()
{
    int fd;
    QByteArray device_str_byte;
    QStandardItemModel  *model = new QStandardItemModel();

    QByteArray model_str_byte, serialno_str_byte, fwrev_str_byte;

    ui->tableView->reset();
    //Add table view header
    model->setColumnCount(5);    
    model->setHeaderData(0,Qt::Horizontal,QString::fromLocal8Bit("Device"));
    model->setHeaderData(1,Qt::Horizontal,QString::fromLocal8Bit("Model"));
    model->setHeaderData(2,Qt::Horizontal,QString::fromLocal8Bit("FW Version"));
    model->setHeaderData(3,Qt::Horizontal,QString::fromLocal8Bit("Serial No."));
    model->setHeaderData(4,Qt::Horizontal,QString::fromLocal8Bit("Capacity (GB)"));    
    ui->tableView->setModel(model);

    set_tabview();


    int cnt = 0;
    nvme_Device * nvmeDev;

    for(int x = 0; x<10; x++)
    {
        for(int y = 0; y<10; y++)
        {
            QString tmp_str = "/dev/nvme" + QString::number(x) + "n" + QString::number(y);
            device_str_byte = tmp_str.toLatin1();

            nvmeDev = new nvme_Device(device_str_byte, "", 0);
            fd = nvmeDev->myOpen();

            if(fd>=0)
            {
                nvme_Device::nvme_id_ctrl id_ctrl;

                char buf[64];
                nvmeDev->nvme_read_id_ctrl(id_ctrl);
                model_str_byte = format_char_array(buf, id_ctrl.mn);
                serialno_str_byte  = format_char_array(buf, id_ctrl.sn);
                fwrev_str_byte = format_char_array(buf, id_ctrl.fr);

                double local_capacity = GetDeviceCapacity(device_str_byte);

                int int_capacity = static_cast<int>(local_capacity);

                qDebug() << "Product: " << model_str_byte << " SN: " << serialno_str_byte << " FW: " << fwrev_str_byte<< " device: " << device_str_byte<< " capacity: " << int_capacity;


                model->setItem(cnt,0,new QStandardItem(QString::fromLocal8Bit(device_str_byte)));
                model->setItem(cnt,1,new QStandardItem(QString(model_str_byte)));
                model->setItem(cnt,2,new QStandardItem(QString(fwrev_str_byte)));
                model->setItem(cnt,3,new QStandardItem(QString(serialno_str_byte)));
                model->setItem(cnt,4,new QStandardItem(QString::number(int_capacity)));
                device[cnt] = QString::fromLocal8Bit(device_str_byte);
                modelname[cnt] = QString(model_str_byte);
                fw[cnt] = QString(fwrev_str_byte);
                sn[cnt] = QString(serialno_str_byte);
                capacity[cnt] = QString::number(int_capacity);
                data[cnt] << device[cnt] << modelname[cnt] << fw[cnt] << sn[cnt] << capacity[cnt];
                cnt++;


            }
            ::close(fd);

        }
    }

}

void* MainWindow::fw_test(void* param)
{

    MainWindow* pDlg = (MainWindow*)param;

    QStringList smSL = get_SMART_Data_NVMe(pDlg->sel_dev.toStdString().c_str());

    emit(pDlg->fw_test_signal(smSL));
}


void MainWindow::fw_test_ret(QStringList smartList)
{
    if(!smartList.isEmpty())
    {
        ShowSmartDialog *show = new ShowSmartDialog(smartList);
        show->exec();
    }
    else
    {
        QString msg = "Get Device SMART Fail!";
        QMessageBox okbox(QMessageBox::NoIcon,"","",QMessageBox::Ok,this);
        QString stylestr;

        if(desktopWidth>RETINA_WIDTH ||desktopHeight>RETINA_HEIGHT)
        {
            msg=QString("<font size = %1>%2</font>").arg(desktopWidth/MBOX_FONT_RATIO).arg(msg);
            stylestr.append(QString("QPushButton {font:%1pt;min-width:%2px;min-height:%3px;}").arg(desktopWidth/MBOX_BUTTON_FONT_RATIO).arg(desktopWidth/MBOX_BUTTON_WIDTH_RATIO).arg(desktopHeight/MBOX_BUTTON_HEIGHT_RATIO));
        }
        else
        {
        }
        okbox.setStyleSheet(stylestr);
        okbox.setText(msg);
        okbox.setWindowTitle(TITLE);
        okbox.exec();
    }



    for(int i = 0 ; i < 16 ; i++)
    {
        device[i] = "";
        modelname[i] = "";
        fw[i] = "";
        sn[i] = "";
        capacity[i] = "";
        data[i].clear();
    }
    scanDevice();

    ui->btn_Update->setEnabled(true);
    ui->btn_re->setEnabled(true);
    ui->btn_close->setEnabled(true);
    ui->tableView->setEnabled(true);
    sel_row = -1;
}

void MainWindow::on_btn_Update_clicked()
{
    if(sel_row == -1)
    {
        QMessageBox msgbox(QMessageBox::NoIcon,"","",QMessageBox::Ok,this);
        QString msg = "Please select the device you want to get SMART info.";
        QString stylestr;
        if(desktopWidth>RETINA_WIDTH ||desktopHeight>RETINA_HEIGHT)
        {
            msg=QString("<font size = %1>%2</font>").arg(desktopWidth/MBOX_FONT_RATIO).arg(msg);
            stylestr.append(QString("QPushButton {font:%1pt;min-width:%2px;min-height:%3px;}").arg(desktopWidth/MBOX_BUTTON_FONT_RATIO).arg(desktopWidth/MBOX_BUTTON_WIDTH_RATIO).arg(desktopHeight/MBOX_BUTTON_HEIGHT_RATIO));
        }
        else
        {
        }
        msgbox.setText(msg);
        msgbox.setWindowTitle(TITLE);
        msgbox.setStyleSheet(stylestr);
        msgbox.exec();
    }
    else
    {

        if(!pthread_create(&thread_id,NULL,&fw_test, this))
        {
            ui->btn_Update->setEnabled(false);
            ui->btn_re->setEnabled(false);
            ui->btn_close->setEnabled(false);
            ui->tableView->setEnabled(false);
            //QApplication::setOverrideCursor(Qt::WaitCursor);
        }
    }
}

void MainWindow::on_btn_re_clicked()
{
    sel_row = -1;
    for(int i = 0 ; i < 16 ; i++)
    {
        device[i] = "";
        modelname[i] = "";
        fw[i] = "";
        sn[i] = "";
        capacity[i] = "";
        data[i].clear();
    }

    scanDevice();
}

void MainWindow::on_btn_info_clicked()
{    
    QString msg="";
    QString stylestr="";
    QMessageBox msgbox(QMessageBox::NoIcon,"","",QMessageBox::Ok,this);

    if(desktopWidth>RETINA_WIDTH ||desktopHeight>RETINA_HEIGHT)
    {
        msg.append(QString("<font size = %1>%2</font>").arg(desktopWidth/MBOX_FONT_RATIO).arg(TITLE));
        stylestr.append(QString("QPushButton {font:%1pt;min-width:%2px;min-height:%3px;}").arg(desktopWidth/MBOX_BUTTON_FONT_RATIO).arg(desktopWidth/MBOX_BUTTON_WIDTH_RATIO).arg(desktopHeight/MBOX_BUTTON_HEIGHT_RATIO));
    }
    else
    {
        msg.append(QString("%1").arg(TITLE));
        msg.append(QString("  %1").arg(VERSION));
    }

    msgbox.setText(msg);
    msgbox.setWindowTitle(TITLE);
    msgbox.setStyleSheet(stylestr);
    msgbox.exec();
}

void MainWindow::on_btn_min_clicked()
{
    showMinimized();
}

void MainWindow::on_btn_close_clicked()
{
    close();
    //system("poweroff");
}

 void MainWindow::mouseMoveEvent( QMouseEvent * event )
 {
     if( event->buttons().testFlag(Qt::LeftButton) && moving)
     {
         this->move(this->pos() + (event->globalPos() - offset));
         offset = event->globalPos();
      }
 }

 void MainWindow::mousePressEvent(QMouseEvent *event)
 {
     if((event->button() == Qt::LeftButton) /*&& !actionAt(event->pos())*/) {
         moving = true;
         offset = event->globalPos();
     }
 }

 void MainWindow::mouseReleaseEvent(QMouseEvent *event)
 {
     if(event->button() == Qt::LeftButton) {
         moving = false;
     }
 }

 double MainWindow::GetDeviceCapacity( QByteArray device_str_byte)
 {
     QProcess *cmd_Process = new QProcess(this);
     QString res;
     QStringList resList;

     double Total_G = 0;

     QString cmd = "fdisk -l";

     cmd_Process->start(cmd);
     cmd_Process->waitForFinished();
     res = cmd_Process->readAllStandardOutput();

     resList = ToResList(res);

     for(int i=0; i<resList.length(); i++)
     {
         if(resList[i].contains(device_str_byte + ":"))
         {
             Total_G = resList.at(i+1).toDouble();
             break;
         }
     }
     return Total_G;

 }

 QStringList MainWindow::ToResList(QString res)
 {
     QRegExp rx("(\\ |\\n)");
     QStringList tmpList = res.split(rx);
     QStringList resList;
     foreach(QString s, tmpList)
     {
         if(s!="")
         {
             resList.append(s);
         }
     }
     return resList;
 }


 QStringList MainWindow::get_Identify_Data_NVMe(const char * devName)
 {
     QStringList nvme_IDData;

     const char* type = "";
     unsigned nsid = 0; // invalid namespace id -> use default

     nvme_Device * nvmeDev;
     nvmeDev = new nvme_Device(devName, type, nsid);
     int fd = nvmeDev->myOpen();

     nvme_Device::nvme_id_ctrl id_ctrl;
     nvmeDev->nvme_read_id_ctrl(id_ctrl);

     nvme_Device::nvme_id_ns id_ns;
     memset(&id_ns, 0, sizeof(id_ns));

     nsid = nvmeDev->m_nsid;

     if (nsid == 0xffffffffU) {
       // Broadcast namespace
       if (id_ctrl.nn == 1) {
         // No namespace management, get size from single namespace
         nsid = 1;
         if (!nvmeDev->nvme_read_id_ns(nsid, id_ns))
           nsid = 0;
       }
     }
     else {
         // Identify current namespace
         if (!nvmeDev->nvme_read_id_ns(nsid, id_ns)) {
           //pout("Read NVMe Identify Namespace 0x%x failed: %s\n", nsid, device->get_errmsg());
           return nvme_IDData;
         }
     }

     char buf[64];
     QString element;

     element.sprintf("Model Number: %s", format_char_array(buf, id_ctrl.mn));
     nvme_IDData.append(element);
     element.sprintf("Serial Number: %s", format_char_array(buf, id_ctrl.sn));
     nvme_IDData.append(element);
     element.sprintf("Firmware Version: %s", format_char_array(buf, id_ctrl.fr));
     nvme_IDData.append(element);
     element.sprintf("PCI Vendor ID: 0x%04x", id_ctrl.vid);
     nvme_IDData.append(element);
     //element.sprintf(("PCI Vendor Subsystem ID: 0x%04x", id_ctrl.ssvid));
     //nvme_IDData.append(element);
     element.sprintf("Total NVM Capacity: %s", le128_to_str(buf, id_ctrl.tnvmcap, 1));
     nvme_IDData.append(element);
     element.sprintf("Unallocated NVM Capacity: %s", le128_to_str(buf, id_ctrl.unvmcap, 1));
     nvme_IDData.append(element);
     element.sprintf("Controller ID: %d", id_ctrl.cntlid);
     nvme_IDData.append(element);

     ::close(fd);

     return nvme_IDData;
 }

 QStringList MainWindow::get_SMART_Data_NVMe(const char * devName)
 {
     QStringList nvme_SMARTData;

     const char* type = "";
     unsigned nsid = 0; // invalid namespace id -> use default

     nvme_Device * nvmeDev;
     nvmeDev = new nvme_Device(devName, type, nsid);
     int fd = nvmeDev->myOpen();
     if(fd>=0)
     {
         nvme_Device::nvme_smart_log smart_log;

         nvmeDev->nvme_read_smart_log(smart_log);


         char buf[64];
         QString value;

         value.sprintf(" 0/ Critical Warning/ 0x%02x", smart_log.critical_warning);
         nvme_SMARTData.append(value);
         value.sprintf(" 2:1/ Temperature/ %s", to_str(buf, le16_to_uint(smart_log.temperature)));
         nvme_SMARTData.append(value);
         value.sprintf(" 3/ Available Spare/ %u%%", smart_log.avail_spare);
         nvme_SMARTData.append(value);
         value.sprintf(" 4/ Available Spare Threshold/ %u%%", smart_log.spare_thresh);
         nvme_SMARTData.append(value);
         value.sprintf(" 5/ Percentage Used/ %u%%", smart_log.percent_used);
         nvme_SMARTData.append(value);
         value.sprintf(" 47:32/ Data Units Read/ %s", le128_to_str(buf, smart_log.data_units_read, 1000*512));
         nvme_SMARTData.append(value);
         value.sprintf(" 63:48/ Data Units Written/ %s", le128_to_str(buf, smart_log.data_units_written, 1000*512));
         nvme_SMARTData.append(value);
         value.sprintf(" 79:64/ Host Read Commands/ %s", le128_to_str(buf, smart_log.host_reads));
         nvme_SMARTData.append(value);
         value.sprintf(" 95:80/ Host Write Commands/ %s", le128_to_str(buf, smart_log.host_writes));
         nvme_SMARTData.append(value);
         value.sprintf(" 111:96/ Controller Busy Time/ %s", le128_to_str(buf, smart_log.ctrl_busy_time));
         nvme_SMARTData.append(value);
         value.sprintf(" 127:112/ Power Cycles/ %s", le128_to_str(buf, smart_log.power_cycles));
         nvme_SMARTData.append(value);
         value.sprintf(" 143:128/ Power On Hours/ %s", le128_to_str(buf, smart_log.power_on_hours));
         nvme_SMARTData.append(value);
         value.sprintf(" 159:144/ Unsafe Shutdowns/ %s", le128_to_str(buf, smart_log.unsafe_shutdowns));
         nvme_SMARTData.append(value);
         value.sprintf(" 175:160/ Media and Data Integrity Errors/ %s", le128_to_str(buf, smart_log.media_errors));
         nvme_SMARTData.append(value);
         value.sprintf(" 191:176/ Error Information Log Entries/ %s", le128_to_str(buf, smart_log.num_err_log_entries));
         nvme_SMARTData.append(value);
         value.sprintf(" 195:192  / Warning  Comp. Temperature Time          / %d", smart_log.warning_temp_time);
         nvme_SMARTData.append(value);
         value.sprintf(" 199:196/ Critical Comp. Temperature Time/ %d", smart_log.critical_comp_time);
         nvme_SMARTData.append(value);
     }
     ::close(fd);
     return nvme_SMARTData;
 }


