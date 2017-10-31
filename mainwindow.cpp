#include <QCoreApplication>
#include <QtCore>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDir>
#include <QFileDialog>
#include <QMovie>
#include <QMessageBox>
#include <QThread>
#include <QMutex>
#include <QTcpSocket>

#define SHOW 1
#define HIDE -1

int sockfd;
int globalRowCount;
movieMeta globaldataFetched;
QString response;
QList<QString>globalListMovie;
QList<int>globalCheckList;
QFileInfoList globalFlist;
Qt::CheckState globalCheckState;
QMutex mutexProc;
QMovie *loaderMv;

int qSendRequest(QString movie)
{
    QTcpSocket *qSocket=new QTcpSocket();
    qSocket->connectToHost("omdbapi.com",80);
    if(!qSocket->waitForConnected(3000))
    {
        qDebug()<<"not connected";
        return -1;
        //terminate thread
    }
    QString tdata=QString("GET /?t=%1&apikey=BanMePlz HTTP/1.0\r\nHost:www.omdbapi.com\r\n\r\n").arg(movie);
    QByteArray data;
    data.clear();
    data.append(tdata);
     qSocket->write(data);
     qSocket->waitForBytesWritten(3000);
     qSocket->waitForReadyRead(4000);
    QString responds;
    responds=(qSocket->readAll());
    responds.remove(0,4+responds.indexOf("\r\n\r\n",0,Qt::CaseInsensitive));
    QJsonDocument d = QJsonDocument::fromJson(responds.toUtf8());
    QJsonObject sett2 = d.object();
    if(sett2.value(QString("Response")).toString()=="True"){
    /*fill movie meta structure */
    globaldataFetched.mName=sett2.value(QString("Title")).toString();
    globaldataFetched.mYear=sett2.value(QString("Year")).toString();
    globaldataFetched.mRating=sett2.value(QString("imdbRating")).toString();
    globaldataFetched.mRes=1;

    QJsonValue value = sett2.value(QString("Title"));
    qDebug()<<endl<<"year: "<<value.toString()<<endl;
    }
    else{
        globaldataFetched.mRes=-1;
    }
    qSocket->close();
    return 1;
}


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    mutexProc.lock();
    connect(ui->tableWidgetDir,SIGNAL(cellClicked(int,int)),this,SLOT(slotTableItemClicked(int,int)));
    connect(&(this->threadProcPtr),SIGNAL(signalStopSpinner()),this,SLOT(stopSpinner()));
    connect(&(this->threadProcPtr),SIGNAL(signalError()),this,SLOT(networkError()));
    connect(&(this->threadProcPtr),SIGNAL(signalUpdateTable(int)),this,SLOT(insertIntoTable(int)));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButtonBrowse_clicked()
{
    QString DirLocation = QFileDialog::QFileDialog::getExistingDirectory(this, tr("Find Files"), QDir::currentPath());
    DirLocation.append("/");
    if(DirLocation!=NULL)
    {
        ui->labelDir->setText("<font color='green'>"+DirLocation+"</font>");
        ui->labelDir->setWordWrap(true);
    }
    fillMovieTable(DirLocation);
    ui->tableWidgetDir->hideColumn(5);
    ui->tableWidgetDir->hideColumn(4);
    ui->tableWidgetDir->hideColumn(3);
    ui->tableWidgetDir->hideColumn(2);
}

void MainWindow::fillMovieTable(QString DirLocation)
{
    int rowpos=0;
    QDir folder(DirLocation);
    ui->tableWidgetDir->clear();
    ui->tableWidgetDir->setRowCount(0);
    ui->tableWidgetDir->setColumnCount(6);
    ui->tableWidgetDir->setHorizontalHeaderLabels(QString("check;Folder Name;Title;Year;Rating;Action").split(";"));

    globalFlist.clear();
    globalFlist =folder.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot, QDir::DirsFirst);
    foreach (QFileInfo item,globalFlist)
    {
        ui->tableWidgetDir->insertRow(ui->tableWidgetDir->rowCount());
        QTableWidgetItem*folder=new QTableWidgetItem(item.fileName());
        QTableWidgetItem*checkbox=new QTableWidgetItem();
        checkbox->setCheckState(Qt::Checked);

        ui->tableWidgetDir->setItem(rowpos,1,folder);
        ui->tableWidgetDir->setItem(rowpos,0,checkbox);
        ui->tableWidgetDir->item(rowpos,0)->setTextAlignment(Qt::AlignCenter);
        ui->tableWidgetDir->item(rowpos,1)->setTextAlignment(Qt::AlignCenter);
        rowpos++;
    }
    ui->tableWidgetDir->resizeColumnToContents(0);
    ui->tableWidgetDir->resizeColumnToContents(1);
    ui->tableWidgetDir->resizeColumnToContents(5);
    ui->tableWidgetDir->resizeColumnToContents(4);
    ui->tableWidgetDir->resizeColumnToContents(3);
    ui->tableWidgetDir->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
}


QString MainWindow::formatTitle(QString movName)
{
    if(movName.contains("("))
    movName.remove(movName.indexOf("("),movName.count());
    if(movName.contains("["))
    movName.remove(movName.indexOf("["),movName.count());
    if(movName.contains("DVD"))
    movName.remove(movName.indexOf("DVD"),movName.count());
    movName.replace(".","+");
    movName.replace(" ","+");
    movName.replace("-","+");
    movName.replace("_","+");
    return movName;
}

int MainWindow::on_pushButtonStart_clicked()
{
    if(globalFlist.isEmpty())
    {
        ui->labelStatus->setText("<font color='blue'>please browse a directory</font>");
    }
    else {
        ui->labelStatus->setText("fetching information from imdb....");
        loaderMv=new QMovie(":/Images/Load.gif");
        ui->label_loader->setMovie(loaderMv);
        loaderMv->start();
        ui->label_loader->show();
        globalListMovie.clear();
        globalCheckList.clear();
        globalRowCount=ui->tableWidgetDir->rowCount();
        for(int i=0;(i <globalRowCount);i++)
        {
            if(ui->tableWidgetDir->item(i,0)->checkState()==Qt::Checked){
              globalListMovie.append(formatTitle(ui->tableWidgetDir->item(i,1)->text()));
              globalCheckList.append(i);}
        }
        mutexProc.unlock();
        this->threadProcPtr.start();
        ui->tableWidgetDir->showColumn(5);
        ui->tableWidgetDir->showColumn(4);
        ui->tableWidgetDir->showColumn(3);
        ui->tableWidgetDir->showColumn(2);
        return 0;
   }
}

void MainWindow::slotTableItemClicked(int row,int col)
{
    if(col==5 && ui->tableWidgetDir->currentItem()!=NULL)/*individual rename*/
    {
        movieMeta tempMeta;
        QString title=ui->tableWidgetDir->item(row,2)->text();
        QString rating=ui->tableWidgetDir->item(row,4)->text();
        QString year=ui->tableWidgetDir->item(row,3)->text();
        QMessageBox::StandardButton replay;
        replay= QMessageBox::information(this,"Confirm renaming","Title:"+title+"\n\nRating:"+rating+"\n\nYear:"+year,QMessageBox::Ok | QMessageBox::Cancel);
        if(replay==QMessageBox::Ok && !title.isEmpty())
        {
            tempMeta.mName=title;
            tempMeta.mYear=year;
            tempMeta.mRating=rating;
            renameDir(ui->tableWidgetDir->currentRow(),tempMeta);
        }
    }
}

void MainWindow::insertIntoTable(int row)
{
    QIcon playicon(QPixmap(":/Images/play.png"));
    QTableWidgetItem*title=new QTableWidgetItem(globaldataFetched.mName);
    ui->tableWidgetDir->setItem(row,2,title);
    QTableWidgetItem*year=new QTableWidgetItem(globaldataFetched.mYear);
    ui->tableWidgetDir->setItem(row,3,year);
    QTableWidgetItem*rating=new QTableWidgetItem(globaldataFetched.mRating);
    ui->tableWidgetDir->setItem(row,4,rating);
    QTableWidgetItem *nodeRemoveBtn = new QTableWidgetItem();
    nodeRemoveBtn->setIcon(playicon);
    ui->tableWidgetDir->setItem(row,5,nodeRemoveBtn);
    ui->labelStatus->setText(globaldataFetched.mName);
}
void MainWindow::renameDir(int row,movieMeta meta)
{
    QDir dirName(globalFlist[row].absoluteFilePath());
    QString newName=globalFlist[row].absolutePath().append("/").append(meta.mName).append("("+meta.mYear+")-"+meta.mRating);
    QString oldName=globalFlist[row].absoluteFilePath();
    if(dirName.rename(oldName,newName))
        ui->labelStatus->setText(ui->tableWidgetDir->item(row,1)->text()+" renamed");
    /*update new data in ui and glovalFlist*/
    globalFlist.replace(row,newName);
    ui->tableWidgetDir->item(row,1)->setText(globalFlist[row].fileName());
}


void MainWindow::on_pushButtonApply_clicked()
{
    int rowcount=ui->tableWidgetDir->rowCount();
    if(rowcount!=0)
    {
        movieMeta tempMeta;
        QString title;
        QString rating;
        QString year;
        for(int i=0;i<rowcount;i++)
        {
            if(ui->tableWidgetDir->item(i,0)->checkState()==Qt::Checked&&!ui->tableWidgetDir->item(i,2)->text().isEmpty())
            {
                title=ui->tableWidgetDir->item(i,2)->text();
                rating=ui->tableWidgetDir->item(i,4)->text();
                year=ui->tableWidgetDir->item(i,3)->text();
                tempMeta.mName=title;
                tempMeta.mYear=year;
                tempMeta.mRating=rating;
                renameDir(i,tempMeta);
            }
        }
    }
}

void MainWindow::on_pushButtonHelp_clicked()
{
    QString help="Browse:click to locate your movies location\nSearch:Click to fetch imdb data\n"
                 "Rename all:folders will rename\n"
                  "In Addition,you can edit movie folder name from the colmn 1 to fetch proper imdb\n"
                  "Click 'Play' icon to rename single folder\n"
                  "\n@Developer:Thejas"
                  "\n@git:"
                  "\n@Blog:Thejas"
                  "\n\n expecting Suggestions and feedbacks";
    QMessageBox::information(this,"help",help);
}
void threadImdb::run()
{
    qDebug()<<"threadImdb start";
    mutexProc.lock();
    qDebug()<<"threadImdb continue";
    for(int i=0;(i < globalListMovie.count());i++)
    {
        globaldataFetched.mName.clear();
        globaldataFetched.mRating.clear();
        globaldataFetched.mYear.clear();
        if(qSendRequest(globalListMovie[i])==-1)
        {
            emit signalError();
        }
        if(globaldataFetched.mRes)
        emit signalUpdateTable(globalCheckList[i]);
        usleep(200);
    }
    usleep(500);
    emit signalStopSpinner();
}
void MainWindow::stopSpinner()
{
    loaderMv->stop();
    ui->label_loader->hide();
}
void MainWindow::networkError()
{
    ui->labelStatus->setText("<font color='red'>Network Error: Check Connection</font>");
}
