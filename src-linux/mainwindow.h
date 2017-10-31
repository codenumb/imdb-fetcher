#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>

typedef struct MovieMetaStructure
{
    QString mName;
    QString mYear;
    QString mRating;
    int mRes;
}movieMeta;
class threadImdb : public QThread
{
    Q_OBJECT
signals:
    void signalError();
    void signalUpdateTable(int row);
    void signalStopSpinner();
//    void signalShowColms();
public slots:

public:
    void run();

};


namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    threadImdb threadProcPtr;
    explicit MainWindow(QWidget *parent = 0);    
    void fillMovieTable(QString DirLocation);
    int sendRequest(char*message);
    QString formatTitle(QString movName);
    void renameDir(int row,movieMeta meta);
    Ui::MainWindow *ui;
    ~MainWindow();
signals:
    void signalDirPath(QString);

private slots:
    void on_pushButtonBrowse_clicked();
    int  on_pushButtonStart_clicked();
    void insertIntoTable(int row);
    void slotTableItemClicked(int row,int col);
    void on_pushButtonApply_clicked();
    void stopSpinner();
    void on_pushButtonHelp_clicked();
    void networkError();
    void on_pushButtonCheck_clicked();

private:
};

#endif // MAINWINDOW_H
