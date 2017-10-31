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

#include <stdio.h> /* printf, sprintf */
#include <stdlib.h> /* exit */
#include <unistd.h> /* read, write, close */
#include <string.h> /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h> /* struct hostent, gethostbyname */

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

/* Initialise the sockets
 * Return 1 on success -1 on failure
 */
int socketInint()
{
    /* first what are we going to send and where are we going to send it? */
    int portno =        80;
    char *host =        "www.omdbapi.com";
    //    char *message_fmt = "POST /apikey=%s&command=%s HTTP/1.0\r\n\r\n";
    //    char *message	="GET /?t=premam&apikey=BanMePlz HTTP/1.0\r\nHost:www.omdbapi.com\r\n\r\n";

    struct hostent *server;
    struct sockaddr_in serv_addr;
    /* create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        qDebug()<<"ERROR opening socket";
        return -1;
    }

    /* lookup the ip address */
    server = gethostbyname(host);
    if (server == NULL)
    {
        qDebug()<<"ERROR, no such host";
        return -1;
    }

    /* fill in the structure */
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    memcpy(&serv_addr.sin_addr.s_addr,server->h_addr,server->h_length);

    /* connect the socket */
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0){
        qDebug()<<"ERROR connecting"<<endl;
        return -1;
    }
    else
        return 1;
}
/* Send API request to the server
 * @param movie name
 * return -1 on any socket error and 1 on success
 */
int sendRequest (char *message)
{
    //    movieMeta tempMetaStruct;
    if(socketInint()>0){
        int bytes, sent, received, total;
        char httpHDR[1024];
        sprintf(httpHDR,"GET /?t=%s&apikey=BanMePlz HTTP/1.0\r\nHost:www.omdbapi.com\r\n\r\n",message);
        printf("%s\n",httpHDR);
        /* send the request */
        total=strlen(httpHDR);
        sent = 0;
        do {
            bytes = write(sockfd,httpHDR+sent,total-sent);
            if (bytes < 0)
                qDebug()<<"ERROR writing message to socket";
            if (bytes == 0)
                break;
            sent+=bytes;
        } while (sent < total);

        /* receive the response */
        response.clear();
        total = 40959;
        received = 0;
        char resp[40960];
        memset(resp,0,40960);
        do {

            //        bytes = read(sockfd,resp+received,total-received);
            bytes=recv(sockfd,resp+received,total-received,8);
            if (bytes < 0)
                qDebug()<<"ERROR reading response from socket";
            if (bytes == 0)
                break;
            received+=bytes;
        } while (received < total);

        if (received == total)
            qDebug()<<"ERROR storing complete response from socket";
        QString respons(resp);
        respons.remove(0,4+respons.indexOf("\r\n\r\n",0,Qt::CaseInsensitive));
        qDebug()<<respons<<endl<<endl;
        QJsonDocument d = QJsonDocument::fromJson(respons.toUtf8());
        QJsonObject sett2 = d.object();

        if(sett2.value(QString("Response")).toString()=="True"){
            /*fill movie meta structure */
            globaldataFetched.mName=sett2.value(QString("Title")).toString();
            globaldataFetched.mYear=sett2.value(QString("Year")).toString();
            globaldataFetched.mRating=sett2.value(QString("imdbRating")).toString();
            globaldataFetched.mRes=1;
        }
        else{
            globaldataFetched.mRes=-1;
        }
        QJsonValue value = sett2.value(QString("Title"));
        qDebug()<<endl<<"Name: "<<value.toString()<<endl;
        qDebug()<<endl<<"rating: "<<sett2.value(QString ("imdbRating")).toString()<<endl;
        ::close(sockfd);
        return 1;
    }
    else
    {
        ::close(sockfd);
        return -1;
    }
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
    mutexProc.unlock();
    this->threadProcPtr.terminate();
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
    ui->label_loader->hide();
    ui->labelStatus->clear();
    ui->tableWidgetDir->hideColumn(5);
    ui->tableWidgetDir->hideColumn(4);
    ui->tableWidgetDir->hideColumn(3);
    ui->tableWidgetDir->hideColumn(2);
}

/*fill the movie list in Ui
 * @param folder directory
 */
void MainWindow::fillMovieTable(QString DirLocation)
{
    int rowpos=0;
    QDir folder(DirLocation);
    ui->tableWidgetDir->clear();
    ui->tableWidgetDir->setRowCount(0);
    ui->tableWidgetDir->setColumnCount(6);
    ui->tableWidgetDir->setHorizontalHeaderLabels(QString(" ;Folder Name;Title;Year;Rating;##").split(";"));
    ui->tableWidgetDir->horizontalHeader()->setFixedHeight(35);
    globalFlist.clear();
    globalFlist =folder.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot, QDir::DirsFirst);
    foreach (QFileInfo item,globalFlist)
    {
        ui->tableWidgetDir->insertRow(ui->tableWidgetDir->rowCount());
        QTableWidgetItem*folder=new QTableWidgetItem(item.fileName());
        QTableWidgetItem*checkbox=new QTableWidgetItem();
        checkbox->setFlags(checkbox->flags() & ~Qt::ItemIsEditable); // non editable
        checkbox->setCheckState(Qt::Checked);

        ui->tableWidgetDir->setItem(rowpos,1,folder);
        ui->tableWidgetDir->setItem(rowpos,0,checkbox);
        ui->tableWidgetDir->item(rowpos,0)->setTextAlignment(Qt::AlignCenter);
        ui->tableWidgetDir->item(rowpos,1)->setTextAlignment(Qt::AlignCenter);
        rowpos++;
    }
    ui->tableWidgetDir->resizeColumnToContents(0);
    ui->tableWidgetDir->resizeColumnToContents(1);
    //    ui->tableWidgetDir->resizeColumnToContents(5);
    //    ui->tableWidgetDir->resizeColumnToContents(4);
    //    ui->tableWidgetDir->resizeColumnToContents(3);
    ui->tableWidgetDir->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
}

/*Funtion extract movie name from the folder name
 *@param movie folder name
 *return movie name
 */
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
/*fucntions handles the play button press from table
 *calls folder rename
 * @param row and colm number
 */
void MainWindow::slotTableItemClicked(int row,int col)
{
    if(col==5 && ui->tableWidgetDir->currentItem()!=NULL)/*individual rename*/
    {
        movieMeta tempMeta;
        QString title=ui->tableWidgetDir->item(row,2)->text();
        QString rating=ui->tableWidgetDir->item(row,4)->text();
        QString year=ui->tableWidgetDir->item(row,3)->text();
        QMessageBox::StandardButton replay;
        replay= QMessageBox::information(this,"Confirm renaming folder","Title:"+title+\
                                         "\nRating:"+rating+"\nYear:"+year+"\n\n"+title+"("+year+")"+"-"+rating,QMessageBox::Ok | QMessageBox::Cancel);
        if(replay==QMessageBox::Ok && !title.isEmpty())
        {
            tempMeta.mName=title;
            tempMeta.mYear=year;
            tempMeta.mRating=rating;
            renameDir(ui->tableWidgetDir->currentRow(),tempMeta);
        }
    }
}
/*insert movie meta data from imdb
 *@param row number of the movie
 */
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
    ui->tableWidgetDir->item(row,5)->setTextAlignment(Qt::AlignCenter);
    ui->tableWidgetDir->item(row,2)->setTextAlignment(Qt::AlignCenter);
    ui->tableWidgetDir->item(row,3)->setTextAlignment(Qt::AlignCenter);
    ui->tableWidgetDir->item(row,4)->setTextAlignment(Qt::AlignCenter);
    ui->tableWidgetDir->resizeColumnToContents(2);
    ui->tableWidgetDir->resizeColumnToContents(5);
    ui->tableWidgetDir->setSelectionMode(QTableWidget::NoSelection);

}

/* Funtion rename the folder
 * @param row number and movie meta from table
 */
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
    QString help="#Browse         : click to locate your movies location\n"
                 "#Search          :Click to fetch imdb data\n"
                 "#Rename all   : folders will rename\n"
                 "#In Addition    : you can edit movie folder name from the colmn 1 to fetch proper imdb\n"
                 "#Click 'Play' icon to rename single folder\n"
                 "#Click #Search again:in case movie data is not feteched even when the movie name is proper,\n"
                 "#Recommend good internet conenction for better performance \n"
                 "\n@Developer  :Thejas"
                 "\n@git        :"
                 "\n@Blog       :Thejas"
                 "\n\n your suggestions,error reports and feedbacks through my blog\n"
                 "----------------------------------------Peace!----------------------------------------";
    QMessageBox::information(this,"help",help);
}

/*thread for hndling all the socket functions*/
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
        if(sendRequest(globalListMovie[i].toLatin1().data())==-1)
        {
            emit signalError();
        }
        if(globaldataFetched.mRes)
            emit signalUpdateTable(globalCheckList[i]);
        usleep(200);
    }
    usleep(300);
    emit signalStopSpinner();
}

/*slot for handling spinner */
void MainWindow::stopSpinner()
{
    loaderMv->stop();
    ui->label_loader->hide();
}

/*slot for handling error from thread */
void MainWindow::networkError()
{
    ui->labelStatus->setText("<font color='red'>Network Error: Unable to conenct server! :(</font>");
}

void MainWindow::on_pushButtonCheck_clicked()
{
    if(ui->pushButtonCheck->text()=="uncheck all")
    {
        //uncheck all
        for(int i=0;(i <ui->tableWidgetDir->rowCount());i++)
            ui->tableWidgetDir->item(i,0)->setCheckState(Qt::Unchecked);
        ui->pushButtonCheck->setText("check all");
    }
    else if(ui->pushButtonCheck->text()=="check all")
    {
        //uncheck all
        for(int i=0;(i <ui->tableWidgetDir->rowCount());i++)
            ui->tableWidgetDir->item(i,0)->setCheckState(Qt::Checked);
        ui->pushButtonCheck->setText("uncheck all");
    }
}
