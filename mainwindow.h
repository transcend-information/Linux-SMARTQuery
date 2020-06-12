
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QMouseEvent>
#include <QSettings>
#include <QTimer>

#define TITLE "Linux_SMARTQuery"
#define VERSION "V1.0"
#define RETINA_WIDTH 2230
#define RETINA_HEIGHT 1238

#define FRAME_HEIGHT_RATIO 2.9
#define FRAME_WIDTH_RATIO 2.6
#define TITLE_HEIGHT_RATIO 27.69
#define TITLEBUTTON_WIDTH_RATIO 46.82927
#define TABLEVIEW_X_OFFSET_RATIO 96
#define TABLEVIEW_Y_OFFSET_RATIO 18.3
#define TABLEVIEW_WIDTH_RATIO 2.8
#define TABLEVIEW_HEIGHT_RATIO 4.32
#define UPDATEBUTTON_X_OFFSET_RATIO 3.1
#define UPDATEBUTTON_Y_ADDOFFSET_RATIO 54
#define UPDATEBUTTON_WIDTH_RATIO 24
#define UPDATEBUTTON_HEIGHT_RATIO 43.2
#define TABLEVIEW_COLUME0_WIDTH_RATIO 19.2
#define TABLEVIEW_COLUME1_WIDTH_RATIO 11.29
#define TABLEVIEW_COLUME2_WIDTH_RATIO 16
#define TABLEVIEW_COLUME3_WIDTH_RATIO 11.29
#define TITLEFONT_SIZE_RATIO 137
#define TABLEFONT_SIZE_RATIO 150
#define MBOX_BUTTON_WIDTH_RATIO 24
#define MBOX_BUTTON_HEIGHT_RATIO 45
#define MBOX_BUTTON_FONT_RATIO 150
#define MBOX_FONT_RATIO 384

const char * format_char_array(char * str, int strsize, const char * chr, int chrsize);

template<size_t STRSIZE, size_t CHRSIZE>
inline const char * format_char_array(char (& str)[STRSIZE], const char (& chr)[CHRSIZE])
{ return format_char_array(str, (int)STRSIZE, chr, (int)CHRSIZE); }

const char * format_with_thousands_sep(char * str, int strsize, uint64_t val,
                                       const char * thousands_sep = 0);

const char * format_capacity(char * str, int strsize, uint64_t val,
                             const char * decimal_point = 0);

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void UI_init();
    void scanDevice();
    void set_tabview();
    void addProduct(QStandardItemModel *UImodel,QString path, QString model, QString fwver, QString SN, QByteArray drive_type,int cnt);
    static void* fw_test(void*);

    double GetDeviceCapacity( QByteArray device_str_byte);
    QStringList ToResList(QString res);
    static QStringList get_SMART_Data_ATA(const char * devName);
    static QStringList get_SMART_Data_NVMe(const char * devName);
    static QStringList  get_SMART_Attributes();

private slots:    

    void ontableclicked(QModelIndex index);
    void fw_test_ret(QStringList smartList);
    void on_btn_Smart_clicked();
    void on_btn_re_clicked();
    void on_btn_info_clicked();
    void on_btn_min_clicked();
    void on_btn_close_clicked();

private:

    Ui::MainWindow *ui;
    pthread_t thread_id;
    QString device[16];
    QString modelname[16];
    QString fw[16];
    QString sn[16];
    QString capacity[16];
    QStringList data[16];
    int sel_row;
    QString sel_dev;
    QString sel_model;
    QString sel_fw;
    QString sel_wo;
    int desktopWidth;
    int desktopHeight;

protected:
    void mouseMoveEvent( QMouseEvent * event );
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    int mouseXCord;
    int mouseYCord;
    QPoint offset;
    bool moving;


signals:
    void fw_test_signal(QStringList smartList);
};

#endif // MAINWINDOW_H
