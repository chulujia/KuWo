#include "mainwindow.h"
#include "toplistitem.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QHeaderView>
#include <QDir>
#include <QTextBlock>
#include <QScrollBar>
#include <QDialog>
#include <QStandardPaths>
#include <QFontDialog>
#include <QColorDialog>
#include <QFileDialog>
#include <QSettings>
#include <QShortcut>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowIcon(QIcon(":/icon.png"));
    setWindowFlags(Qt::FramelessWindowHint);
    resize(1000,700);
    move((QApplication::desktop()->width()-width())/2,(QApplication::desktop()->height()-height())/2);
    setStyleSheet("color:white; background-color:#000000;");
    connect(new QShortcut(QKeySequence(Qt::Key_Escape),this), SIGNAL(activated()),this, SLOT(exitFullscreen()));

    QWidget *widget = new QWidget;
    setCentralWidget(widget);
    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->setSpacing(0);
    vbox->setContentsMargins(0,0,0,0);

    titleBar = new TitleBar;
    connect(titleBar->action_search,SIGNAL(triggered(bool)),this,SLOT(preSearch()));
    connect(titleBar->lineEdit_search,SIGNAL(returnPressed()),this,SLOT(preSearch()));
    connect(titleBar->lineEdit_page,SIGNAL(returnPressed()),this,SLOT(search()));
    connect(titleBar->pushButton_lastPage,SIGNAL(clicked(bool)),this,SLOT(lastPage()));
    connect(titleBar->pushButton_nextPage,SIGNAL(clicked(bool)),this,SLOT(nextPage()));
    connect(titleBar->pushButton_minimize,SIGNAL(clicked(bool)),this,SLOT(showMinimized()));
    connect(titleBar->pushButton_maximize,SIGNAL(clicked(bool)),this,SLOT(showNormalMaximize()));
    connect(titleBar->pushButton_close,SIGNAL(clicked(bool)),qApp,SLOT(quit()));
    connect(titleBar,SIGNAL(moveMainWindow(QPoint)),this,SLOT(moveMe(QPoint)));
    vbox->addWidget(titleBar);

    createToplistWidget();

    QHBoxLayout *hbox = new QHBoxLayout;
    navWidget = new NavWidget;
    navWidget->listWidget->setCurrentRow(2);
    connect(navWidget->listWidget,SIGNAL(currentRowChanged(int)),this,SLOT(navPage(int)));
    connect(navWidget->pushButton_albumPic,SIGNAL(pressed()),this,SLOT(swapLyric()));
    hbox->addWidget(navWidget);

    stackedWidget = new QStackedWidget;
    stackedWidget->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    stackedWidget->addWidget(rankScrollArea);

    playlistWidget = new QWidget;
    //playlistWidget->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    QVBoxLayout *vboxPL = new QVBoxLayout;
    vboxPL->setMargin(0);
    label_playlistTitle = new QLabel;
    label_playlistTitle->setFont(QFont("Timers",20,50));
    vboxPL->addWidget(label_playlistTitle);
    tableWidget_playlist = new QTableWidget;
    tableWidget_playlist->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableWidget_playlist->setSelectionMode(QAbstractItemView::SingleSelection);
    tableWidget_playlist->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget_playlist->setColumnCount(6);
    //tableWidget_playlist->setColumnHidden(4,true);
    //tableWidget_playlist->setColumnHidden(5,true);
    QStringList header;
    header << "歌名" << "歌手" << "专辑" << "时长" << "id" << "专辑封面";
    tableWidget_playlist->setHorizontalHeaderLabels(header);
    tableWidget_playlist->horizontalHeader()->setStyleSheet("QHeaderView::section { color:white; background-color:#232326; }");
    tableWidget_playlist->verticalHeader()->setStyleSheet("QHeaderView::section { color:white; background-color:#232326; }");
    tableWidget_playlist->setStyleSheet("QTableView { color:white; selection-background-color:#232326; }");
    connect(tableWidget_playlist,SIGNAL(cellDoubleClicked(int,int)),this,SLOT(playSong(int,int)));
    vboxPL->addWidget(tableWidget_playlist);
    playlistWidget->setLayout(vboxPL);
    stackedWidget->addWidget(playlistWidget);

    hbox->addWidget(stackedWidget);

    textBrowser = new QTextBrowser;
    textBrowser->zoomIn(10);
    stackedWidget->addWidget(textBrowser);

    vbox->addLayout(hbox);

    controlBar = new ControlBar;
    connect(controlBar->pushButton_last,SIGNAL(pressed()),this,SLOT(playLast()));
    connect(controlBar->pushButton_play,SIGNAL(pressed()),this,SLOT(playPause()));
    connect(controlBar->pushButton_next,SIGNAL(pressed()),this,SLOT(playNext()));
    connect(controlBar->pushButton_mute,SIGNAL(pressed()),this,SLOT(mute()));
    connect(controlBar->pushButton_lyric,SIGNAL(clicked(bool)),this,SLOT(showHideLyric(bool)));
    connect(controlBar->pushButton_download,SIGNAL(pressed()),this,SLOT(dialogDownload()));
    connect(controlBar->pushButton_fullscreen,SIGNAL(pressed()),this,SLOT(enterFullscreen()));
    connect(controlBar->slider_progress,SIGNAL(sliderMoved(int)),this,SLOT(sliderProgressMoved(int)));
    connect(controlBar->slider_volume,SIGNAL(sliderMoved(int)),this,SLOT(sliderVolumeMoved(int)));
    vbox->addWidget(controlBar);
    widget->setLayout(vbox);

    player = new QMediaPlayer;
    connect(player,SIGNAL(durationChanged(qint64)),this,SLOT(durationChange(qint64)));
    connect(player,SIGNAL(positionChanged(qint64)),this,SLOT(positionChange(qint64)));
    connect(player,SIGNAL(volumeChanged(int)),this,SLOT(volumeChange(int)));
    //connect(player,SIGNAL(error(QMediaPlayer::Error)),this,SLOT(errorHandle(QMediaPlayer::Error)));
    connect(player,SIGNAL(stateChanged(QMediaPlayer::State)),SLOT(stateChange(QMediaPlayer::State)));

    lyricWidget = new LyricWidget;
    connect(lyricWidget->pushButton_set,SIGNAL(pressed()),this,SLOT(on_action_settings_triggered()));
    connect(lyricWidget->pushButton_close,SIGNAL(pressed()),this,SLOT(hideLyric()));
    QString slx = readSettings(QDir::currentPath() + "/config.ini", "config", "LyricX");
    QString sly = readSettings(QDir::currentPath() + "/config.ini", "config", "LyricY");
    if(slx=="" || sly=="" || slx.toInt()>QApplication::desktop()->width() || sly.toInt()>QApplication::desktop()->height()){
        lyricWidget->move((QApplication::desktop()->width()-lyricWidget->width())/2, QApplication::desktop()->height()-lyricWidget->height());
    }else{
        lyricWidget->move(slx.toInt(),sly.toInt());
    }
    //qDebug() << "歌词坐标" << slx << sly;
    QColor color(readSettings(QDir::currentPath() + "/config.ini", "config", "LyricFontColor"));
    QPalette plt;
    plt.setColor(QPalette::WindowText, color);
    lyricWidget->label_lyric->setPalette(plt);
    QString sfont = readSettings(QDir::currentPath() + "/config.ini", "config", "Font");
    if(sfont!=""){
        QStringList SLFont = sfont.split(",");
        lyricWidget->label_lyric->setFont(QFont(SLFont.at(0),SLFont.at(1).toInt(),SLFont.at(2).toInt(),SLFont.at(3).toInt()));
    }
    //lyricWidget->show();
}

MainWindow::~MainWindow()
{
}

void MainWindow::moveMe(QPoint point)
{
    move(point);
}

void MainWindow::createToplistWidget()
{
    rankScrollArea = new QScrollArea;
    QWidget *toplistWidget = new QWidget;
    rankScrollArea->setWidget(toplistWidget);
    rankScrollArea->setWidgetResizable(true);   //关键语句
    QString surl = "http://qukudata.kuwo.cn/q.k?op=query&fmt=json&src=mbox&cont=ninfo&rn=50&pn=0&node=2";
    qDebug() << surl;
    QJsonDocument json = QJsonDocument::fromJson(getReply(surl));
    QGridLayout *gridLayout = new QGridLayout(toplistWidget);
    gridLayout->setSpacing(20);
    QJsonArray list = json.object().value("child").toArray();
    //qDebug() << list;
    for(int i=0; i< list.size(); i++){
        QString rankPicUrl = list[i].toObject().value("pic").toString();
        //qDebug() << rankPicUrl;
        ToplistItem *toplistItem = new ToplistItem;
        toplistItem->setImage(rankPicUrl);
        toplistItem->id = list[i].toObject().value("sourceid").toString();
        toplistItem->name = list[i].toObject().value("name").toString();
        connect(toplistItem,SIGNAL(send(QString,QString)),this,SLOT(createPlaylist(QString,QString)));
        gridLayout->addWidget(toplistItem,i/5,i%5);
    }
}

QByteArray MainWindow::getReply(QString surl)
{
    QNetworkAccessManager *NAM = new QNetworkAccessManager;
    QNetworkRequest request;
    request.setUrl(QUrl(surl));
    QNetworkReply *reply = NAM->get(request);
    QEventLoop loop;
    connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);
    loop.exec();
    reply->deleteLater();
    return reply->readAll();
}

QByteArray MainWindow::postReply(QString surl,QString spost)
{
    QNetworkAccessManager *NAM = new QNetworkAccessManager;
    QNetworkRequest request;
    request.setUrl(QUrl(surl));
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
    QByteArray BA_post;
    BA_post.append(spost);
    QNetworkReply *reply = NAM->post(request,BA_post);
    QEventLoop loop;
    connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);
    loop.exec();
    reply->deleteLater();
    return reply->readAll();
}

void MainWindow::showNormalMaximize()
{
    //qDebug() << "isMaximized=" << isMaximized();
    if(isMaximized()){
        showNormal();
        titleBar->pushButton_maximize->setIcon(QIcon(":/maximize.svg"));
    }else{
        showMaximized();
        titleBar->pushButton_maximize->setIcon(QIcon(":/normal.svg"));
    }
}

void MainWindow::createPlaylist(QString id, QString name)
{
    navWidget->listWidget->setCurrentRow(0);
    label_playlistTitle->setText(name);
    tableWidget_playlist->setRowCount(0);
    //qDebug() << id;
    QString surl = QString("http://kbangserver.kuwo.cn/ksong.s?from=pc&fmt=json&type=bang&data=content&rn=100&id=%1").arg(id);
    qDebug() << surl;
    QJsonDocument json = QJsonDocument::fromJson(getReply(surl));
    QJsonArray musiclist = json.object().value("musiclist").toArray();
    //qDebug() << musiclist;
    for(int i=0; i<musiclist.size(); i++){
        tableWidget_playlist->insertRow(i);
        tableWidget_playlist->setItem(i,0,new QTableWidgetItem(musiclist[i].toObject().value("name").toString()));
        tableWidget_playlist->setItem(i,1,new QTableWidgetItem(musiclist[i].toObject().value("artist").toString()));
        tableWidget_playlist->setItem(i,2,new QTableWidgetItem(musiclist[i].toObject().value("album").toString()));
        int ds = musiclist[i].toObject().value("duration").toString().toInt();
        tableWidget_playlist->setItem(i,3,new QTableWidgetItem(QString("%1:%2").arg(ds/60,2,10,QLatin1Char(' ')).arg(ds%60,2,10,QLatin1Char('0'))));
        tableWidget_playlist->setItem(i,4,new QTableWidgetItem(musiclist[i].toObject().value("id").toString()));
        //tableWidget_playlist->setItem(i,5,new QTableWidgetItem(musiclist[i].toObject().value("album").toObject().value("picUrl").toString()));
    }
    tableWidget_playlist->resizeColumnsToContents();
}

void MainWindow::playSong(int row, int column)
{
    Q_UNUSED(column);
    QString rid = tableWidget_playlist->item(row,4)->text();
    QString surl = "http://antiserver.kuwo.cn/anti.s?type=convert_url&format=aac|mp3&response=url&rid=" + rid;
    qDebug() << surl;
    QString songurl = getReply(surl);
    qDebug() << songurl;
    player->setMedia(QUrl(songurl));
    player->play();
    navWidget->label_songname->setText(tableWidget_playlist->item(row,0)->text() + "\n" + tableWidget_playlist->item(row,1)->text());
    lyricWidget->label_lyric->setText(tableWidget_playlist->item(row,0)->text() + " - " + tableWidget_playlist->item(row,1)->text());
    getLyric(rid);
    //歌手图片列表，http://artistpicserver.kuwo.cn/pic.web?type=big_artist_pic&pictype=url&content=list&&id=0&from=pc&name=于文文
    //歌手信息，http://search.kuwo.cn/r.s?stype=artistinfo&artistid=118640
    //MV，http://mobi.kuwo.cn/mobi.s?f=kuwo&q=VH0YmBjTIqOcgA13rwnO1X7lUHE/58mfHbUI3IEkHufDgtSE0LwjffcBeI2LwQtkxQskru/aVoIUso/mnMHPsU0utlE6RQAvCVXIpBpBZG1E4P9lovYYvxOs8l3FpNHd18NsawlyRySZ3PRcWHjDeL6pWBIIDdvQDKikSAJaHLb03shhRoJW8G5N1nfqQWjJGD2cNW7nHDlKcjfFHvkS2IxoP/XOo3YeQVhu+04UpFuMsvIfgZLb5RT6LrMEzKeH
    //MVURL，http://ar.player.ri01.sycdn.kuwo.cn/resource/m1/78/40/1301233427.mp4
    //歌手列表，http://artistlistinfo.kuwo.cn/mb.slist?stype=artistlist&order=hot&rn=50&category=0&pn=0

//    QPixmap pixmap;
//    pixmap.loadFromData(getReply(tableWidget_playlist->item(row,5)->text()));
//    navWidget->pushButton_albumPic->setIcon(QIcon(pixmap));
//    pixmap.save(QDir::currentPath() + "/cover.jpg");
}

void MainWindow::durationChange(qint64 d)
{
    controlBar->slider_progress->setMaximum(d);
    QTime t(0,0,0);
    t = t.addMSecs(d);
    controlBar->label_song_duration->setText(t.toString("mm:ss"));
}

void MainWindow::positionChange(qint64 p)
{
    //qDebug() << "position =" << p;
    if(!controlBar->slider_progress->isSliderDown())controlBar->slider_progress->setValue(p);
    QTime t(0,0,0);
    t = t.addMSecs(p);
    controlBar->label_song_timeNow->setText(t.toString("mm:ss"));

    // 歌词选行
    int hl=0;
    // 非最后一句
    for(int i=0; i<lyrics.size()-1; i++){
        if(t>lyrics.at(i).time && t<lyrics.at(i+1).time){
            lyricWidget->label_lyric->setText(lyrics.at(i).sentence);
            hl=i;
            break;
        }
    }
    //最后一句
    if(lyrics.size()>0){
        int j = lyrics.size()-1;
        if(t>lyrics.at(j).time){
            lyricWidget->label_lyric->setText(lyrics.at(j).sentence);
            hl=j;
        }
    }

    // 歌词文本着色
    for(int a=0; a<lyrics.size(); a++){
        QTextCursor cursor(textBrowser->document()->findBlockByLineNumber(a));
        QTextBlockFormat TBF = cursor.blockFormat();
        TBF.setForeground(QBrush(Qt::white));
        //TBF.setBackground(QBrush(Qt::transparent));
        TBF.clearBackground();
        cursor.setBlockFormat(TBF);
    }
    if(lyrics.size()>0){
        QTextCursor cursor1(textBrowser->document()->findBlockByLineNumber(hl));
        QTextBlockFormat TBF1 = cursor1.blockFormat();
        TBF1.setForeground(QBrush(Qt::green));
        TBF1.setBackground(QBrush(QColor(255,255,255,80)));
        cursor1.setBlockFormat(TBF1);
        //textBrowser->setTextCursor(cursor1);
        QScrollBar *scrollBar = textBrowser->verticalScrollBar();
        //qDebug() << "scrollBar" << scrollBar->maximum() << scrollBar->maximum()*hl/(lyrics.size()) ;
        scrollBar->setSliderPosition(scrollBar->maximum()*hl/(lyrics.size()));
    }
}

void MainWindow::volumeChange(int v)
{
    if(!controlBar->slider_volume->isSliderDown()) controlBar->slider_volume->setValue(v);
    controlBar->slider_volume->setToolTip(QString::number(v));
}

void MainWindow::stateChange(QMediaPlayer::State state)
{
    //qDebug() << state;
    if(state == QMediaPlayer::PlayingState){
        controlBar->pushButton_play->setIcon(QIcon(":/pause.svg"));
    }
    if(state == QMediaPlayer::PausedState){
        controlBar->pushButton_play->setIcon(QIcon(":/play.svg"));
    }
    if(state == QMediaPlayer::StoppedState){
        controlBar->pushButton_play->setIcon(QIcon(":/play.svg"));
    }
}

void MainWindow::playPause()
{
    //qDebug() << "state=" << player->state();
    if(player->state() == QMediaPlayer::PlayingState){
        player->pause();
    }else if(player->state() == QMediaPlayer::PausedState){
        player->play();
    }else if(player->state() == QMediaPlayer::StoppedState){
        player->play();
    }
}

void MainWindow::navPage(int row)
{
    qDebug() << "nav" << row;
    switch (row) {
    case 0:
        stackedWidget->setCurrentWidget(playlistWidget);
        break;
    case 2:
        stackedWidget->setCurrentWidget(rankScrollArea);
        break;
    case 3:
        textBrowser->setStyleSheet("QTextBrowser{color:white; border-image:url(cover.jpg);}");
        stackedWidget->setCurrentWidget(textBrowser);
        break;
    }
}

void MainWindow::sliderProgressMoved(int p)
{
    player->setPosition(p);
}


void MainWindow::sliderVolumeMoved(int v)
{
    player->setVolume(v);
}

void MainWindow::mute()
{
    if(player->isMuted()){
        player->setMuted(false);
        controlBar->pushButton_mute->setIcon(QIcon(":/volume.svg"));
        controlBar->slider_volume->setValue(volume);
    }else{
        volume = player->volume();
        player->setMuted(true);
        controlBar->pushButton_mute->setIcon(QIcon(":/mute.svg"));
        controlBar->slider_volume->setValue(0);
    }
}

void MainWindow::preSearch()
{
    titleBar->lineEdit_page->setText("1");
    search();
}

void MainWindow::search()
{
    if(titleBar->lineEdit_search->text() != ""){
        label_playlistTitle->setText("搜索：" + titleBar->lineEdit_search->text());
        navWidget->listWidget->setCurrentRow(0);
        int rn = 20;
        QString surl = "http://search.kuwo.cn/r.s?ft=music&itemset=web_2013&client=kt&rformat=json&encoding=utf8&all=" + titleBar->lineEdit_search->text() + "&pn=" + QString::number(titleBar->lineEdit_page->text().toInt()-1) + "&rn=" + QString::number(rn);
        qDebug() << surl;
        //stackedWidget->setCurrentIndex(1);
        tableWidget_playlist->setRowCount(0);
        QJsonDocument json = QJsonDocument::fromJson(getReply(surl).replace("'","\"").replace("&nbsp;"," "));
        //qDebug() << json;
        QJsonArray songs = json.object().value("abslist").toArray();
        //qDebug() << songs;
        for(int i=0; i<songs.size(); i++){
            tableWidget_playlist->insertRow(i);
            tableWidget_playlist->setItem(i,0,new QTableWidgetItem(songs[i].toObject().value("SONGNAME").toString()));
            tableWidget_playlist->setItem(i,1,new QTableWidgetItem(songs[i].toObject().value("ARTIST").toString()));
            tableWidget_playlist->setItem(i,2,new QTableWidgetItem(songs[i].toObject().value("ALBUM").toString()));
            //int ds = songs[i].toObject().value("duration").toInt()/1000;
            //tableWidget_playlist->setItem(i,3,new QTableWidgetItem(QString("%1:%2").arg(ds/60,2,10,QLatin1Char(' ')).arg(ds%60,2,10,QLatin1Char('0'))));
            tableWidget_playlist->setItem(i,4,new QTableWidgetItem(songs[i].toObject().value("MUSICRID").toString()));
            //tableWidget_playlist->setItem(i,5,new QTableWidgetItem(songs[i].toObject().value("album").toObject().value("picUrl").toString()));
        }
        tableWidget_playlist->resizeColumnsToContents();
    }
}

void MainWindow::lastPage()
{
    int page = titleBar->lineEdit_page->text().toInt();
    if(page > 1){
        titleBar->lineEdit_page->setText(QString::number(page-1));
        search();
    }
}

void MainWindow::nextPage()
{
    int page = titleBar->lineEdit_page->text().toInt();
    if(page < 99){
        titleBar->lineEdit_page->setText(QString::number(page+1));
        search();
    }
}

void MainWindow::getLyric(QString rid)
{
    QString surl = "http://player.kuwo.cn/webmusic/st/getNewMuiseByRid?rid=";
    if(!rid.contains("MUSIC_")) surl += "MUSIC_";
    surl +=rid;
    qDebug() << surl;
    QString xml = getReply(surl);
    //qDebug() << xml.indexOf("<lyric>")+7 << xml.indexOf("</lyric>")-xml.indexOf("<lyric>")-7;
    QString hash = xml.mid(xml.indexOf("<lyric>")+7,xml.indexOf("</lyric>")-xml.indexOf("<lyric>")-7);
    qDebug() << "lyric hash" << hash;
    surl = "http://newlyric.kuwo.cn/newlyric.lrc?" + hash;
    qDebug() << surl;
    QString txt = getReply(surl);
    qDebug() << txt;

//    QString slyric = json.object().value("lrc").toObject().value("lyric").toString();
//    textBrowser->setText("");

//    lyrics.clear();
//    QStringList line = slyric.split("\n");
//    for(int i=0; i<line.size(); i++){
//        if(line.at(i).contains("]")){
//            QStringList strlist = line.at(i).split("]");
//            //qDebug() << strlist.at(0).mid(1);
//            Lyric lyric;
//            QString stime = strlist.at(0).mid(1);
//            //qDebug() << stime.length() << stime.indexOf(".");
//            if((stime.length()-stime.indexOf("."))==3) stime += "0";
//            lyric.time = QTime::fromString(stime, "mm:ss.zzz");
//            lyric.sentence = strlist.at(1);
//            lyrics.append(lyric);
//        }
//    }
//    for(int i=0; i<lyrics.size(); i++){
//        textBrowser->insertPlainText(lyrics.at(i).sentence + "\n");
//    }
//    textBrowser->selectAll();
//    textBrowser->setAlignment(Qt::AlignCenter);
//    QTextCursor cursor = textBrowser->textCursor();
//    cursor.setPosition(0, QTextCursor::MoveAnchor);
//    textBrowser->setTextCursor(cursor);
}

void MainWindow::swapLyric()
{
//    if(navWidget->listWidget->currentRow()==3){
//        navWidget->listWidget->setCurrentRow(2);
//    }else{
//        navWidget->listWidget->setCurrentRow(3);
//    }
}

void MainWindow::hideLyric()
{
    lyricWidget->hide();
    controlBar->pushButton_lyric->setChecked(false);
}

void MainWindow::showHideLyric(bool on)
{
    if(on){
        lyricWidget->show();
    }else{
        lyricWidget->hide();
    }
}

void MainWindow::on_action_settings_triggered()
{
    QDialog *dialog_settings = new QDialog(this);
    dialog_settings->setWindowTitle("设置");
    dialog_settings->setFixedSize(300,200);
    dialog_settings->setStyleSheet("QLineEdit{border:1px solid black;}");
    QVBoxLayout *vbox = new QVBoxLayout;
    QHBoxLayout *hbox = new QHBoxLayout;
    QLabel *label = new QLabel("歌词");
    hbox->addWidget(label);
    QPushButton *pushButton_font = new QPushButton;
    QString sfont = lyricWidget->label_lyric->font().family() + "," + QString::number(lyricWidget->label_lyric->font().pointSize()) + "," + lyricWidget->label_lyric->font().weight() + "," + lyricWidget->label_lyric->font().italic();
    pushButton_font->setText(sfont);
    pushButton_font->setFocusPolicy(Qt::NoFocus);
    //pushButton_font->setFlat(true);
    connect(pushButton_font,SIGNAL(pressed()),this,SLOT(chooseFont()));
    hbox->addWidget(pushButton_font);
    pushButton_fontcolor = new QPushButton;
    pushButton_fontcolor->setText("■");
    pushButton_fontcolor->setFocusPolicy(Qt::NoFocus);
    //pushButton_fontcolor->setFlat(true);
    QPalette plt = lyricWidget->label_lyric->palette();
    QBrush brush = plt.color(QPalette::WindowText);
    plt.setColor(QPalette::ButtonText, brush.color());
    pushButton_fontcolor->setPalette(plt);
    connect(pushButton_fontcolor,SIGNAL(pressed()),this,SLOT(chooseFontColor()));
    hbox->addWidget(pushButton_fontcolor);
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    label = new QLabel("保存路径");
    hbox->addWidget(label);
    lineEdit_downloadPath = new QLineEdit;
    downloadPath = readSettings(QDir::currentPath() + "/config.ini", "config", "DownloadPath");
    if(downloadPath==""){
        lineEdit_downloadPath->setText(QStandardPaths::standardLocations(QStandardPaths::MusicLocation).first());
    }else{
        lineEdit_downloadPath->setText(downloadPath);
    }
    hbox->addWidget(lineEdit_downloadPath);
    QPushButton *pushButton_downloadPath = new QPushButton("选择路径");
    pushButton_downloadPath->setFocusPolicy(Qt::NoFocus);
    //pushButton_downloadPath->setFlat(true);
    connect(pushButton_downloadPath,SIGNAL(pressed()),this,SLOT(chooseDownloadPath()));
    hbox->addWidget(pushButton_downloadPath);
    vbox->addLayout(hbox);
    dialog_settings->setLayout(vbox);
    dialog_settings->show();
}

void MainWindow::chooseFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, lyricWidget->label_lyric->font(), this, "选择字体");
    if(ok){
       lyricWidget->label_lyric->setFont(font);
       QString sfont = font.family() + "," + QString::number(font.pointSize()) + "," + font.weight() + "," + font.italic();
       writeSettings(QDir::currentPath() + "/config.ini", "config", "Font", sfont);
       lyricWidget->label_lyric->adjustSize();
       //qDebug() << "label_after" << desktopLyric->ui->label_lyric->size();
       lyricWidget->resize(lyricWidget->label_lyric->size());
       //qDebug() << "window" << desktopLyric->size();
    }
}

void MainWindow::chooseFontColor()
{
    QPalette plt = lyricWidget->label_lyric->palette();
    QBrush brush = plt.color(QPalette::WindowText);
    QColor color = QColorDialog::getColor(brush.color(), this);
    if(color.isValid()){
        plt.setColor(QPalette::WindowText, color);
        lyricWidget->label_lyric->setPalette(plt);
        plt.setColor(QPalette::ButtonText, color);
        pushButton_fontcolor->setPalette(plt);
        writeSettings(QDir::currentPath() + "/config.ini", "config", "LyricFontColor", color.name());
    }
}

void MainWindow::chooseDownloadPath()
{
    downloadPath = QFileDialog::getExistingDirectory(this,"保存路径",downloadPath, QFileDialog::ShowDirsOnly |QFileDialog::DontResolveSymlinks);
    if(downloadPath != ""){
        QObject *object = sender();
        qDebug() << object->objectName() << downloadPath;
        if(object->objectName() == "SettingDialogChooseDownloadPath"){
            lineEdit_downloadPath->setText(downloadPath);
        }
        if(object->objectName() == "DownloadDialogPath"){
            pushButton_path->setText(downloadPath);
            pushButton_path->setToolTip(downloadPath);
        }
        writeSettings(QDir::currentPath() + "/config.ini", "config", "DownloadPath", downloadPath);
    }
}

QString MainWindow::readSettings(QString path, QString group, QString key)
{
    QSettings setting(path, QSettings::IniFormat);
    setting.beginGroup(group);
    QString value = setting.value(key).toString();
    return value;
}

void MainWindow::writeSettings(QString path, QString group, QString key, QString value)
{
    QSettings *config = new QSettings(path, QSettings::IniFormat);
    config->beginGroup(group);
    config->setValue(key, value);
    config->endGroup();
}

void MainWindow::playLast()
{
    int row = tableWidget_playlist->currentRow();
    qDebug() << row;
    if (row != -1) {
        if (row > 0) {
            row--;
            playSong(row,0);
            tableWidget_playlist->setCurrentCell(row,0);
        }
    }
}

void MainWindow::playNext()
{
    int row = tableWidget_playlist->currentRow();
    int rc = tableWidget_playlist->rowCount();
    qDebug() << row << rc;
    if (row != -1) {
        if (row < rc-1) {
            row++;
            playSong(row,0);
            tableWidget_playlist->setCurrentCell(row,0);
        }
    }
}

void MainWindow::enterFullscreen()
{
    if(navWidget->listWidget->currentRow()==3){
        showFullScreen();
        titleBar->hide();
        navWidget->hide();
        controlBar->hide();
        lyricWidget->hide();
    }
}

void MainWindow::exitFullscreen()
{
    if(isFullScreen()){
        showNormal();
        titleBar->show();
        navWidget->show();
        controlBar->show();
        if(controlBar->pushButton_lyric->isChecked())lyricWidget->show();
    }
}

void MainWindow::dialogDownload()
{
    QDialog *dialog = new QDialog(this);
    dialog->setFixedWidth(200);
    dialog->setWindowTitle("下载");
    dialog->setStyleSheet("QLineEdit { border:1px solid gray; }"
                          "QToolTip { border:1px solid black; background-color: black; }");
    QGridLayout *gridLayout = new QGridLayout;
    QLabel *label = new QLabel("歌名");
    gridLayout->addWidget(label,0,0,1,1);
    QLineEdit *lineEdit_songname = new QLineEdit;
    lineEdit_songname->setText(navWidget->label_songname->text().replace("\n"," - "));
    gridLayout->addWidget(lineEdit_songname,0,1,1,1);
    label = new QLabel("下载地址");
    gridLayout->addWidget(label,1,0,1,1);
    QLineEdit *lineEdit_url = new QLineEdit;
    lineEdit_url->setText(player->media().canonicalUrl().toString());
    gridLayout->addWidget(lineEdit_url,1,1,1,1);
    label = new QLabel("保存路径");
    gridLayout->addWidget(label,2,0,1,1);
    pushButton_path = new QPushButton;
    pushButton_path->setObjectName("DownloadDialogPath");
    pushButton_path->setFocusPolicy(Qt::NoFocus);
    downloadPath = readSettings(QDir::currentPath() + "/config.ini", "config", "DownloadPath");
    if(downloadPath=="") downloadPath = QStandardPaths::standardLocations(QStandardPaths::MusicLocation).first();
    pushButton_path->setText(downloadPath);
    pushButton_path->setToolTip(downloadPath);
    connect(pushButton_path,SIGNAL(pressed()),this,SLOT(chooseDownloadPath()));
    gridLayout->addWidget(pushButton_path,2,1,1,1);
    dialog->setLayout(gridLayout);
    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addStretch();
    QPushButton *pushButton_confirm = new QPushButton("确定");
    connect(pushButton_confirm, SIGNAL(clicked()), dialog, SLOT(accept()));
    hbox->addWidget(pushButton_confirm);
    QPushButton *pushButton_cancel = new QPushButton("取消");
    connect(pushButton_cancel, SIGNAL(clicked()), dialog, SLOT(reject()));
    hbox->addWidget(pushButton_cancel);
    hbox->addStretch();
    gridLayout->addLayout(hbox,3,0,1,2);
    int result = dialog->exec();
    if (result == QDialog::Accepted) {
        download(lineEdit_url->text(), pushButton_path->text() + "/" + lineEdit_songname->text() + "." + QFileInfo(lineEdit_url->text()).suffix());
    }else
    if (result == QDialog::Rejected) {
        dialog->close();
    }
}

void MainWindow::download(QString surl, QString filepath)
{
    controlBar->pushButton_download->setEnabled(false);
    qDebug() <<  "download -> " << surl << "->" << filepath;
    QUrl url = QString(surl);
    QNetworkAccessManager *NAM = new QNetworkAccessManager;
    QNetworkRequest request(url);
    QNetworkReply *reply = NAM->get(request);
    QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();
    QFile file(filepath);
    file.open(QIODevice::WriteOnly);
    file.write(reply->readAll());
    file.close();
    //ui->pushButton_download->setText("↓");
    controlBar->pushButton_download->setEnabled(true);
}

void MainWindow::updateProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    //ui->pushButton_download->setText(QString("%1%").arg(bytesReceived*100/bytesTotal));
    float p = (float)bytesReceived/bytesTotal;
    controlBar->pushButton_download->setStyleSheet(QString("QPushButton { background-color: qlineargradient(spread:pad, x1:0, y1:1, x2:0, y2:0,"
                                                   "stop:0 rgba(48, 194, 124, 255), stop:%1 rgba(48, 194, 124, 255),"
                                                   "stop:%2 rgba(255, 255, 255, 255), stop:1 rgba(255, 255, 255, 255)); }")
                                      .arg(p-0.001)
                                      .arg(p));
    qDebug() << p << controlBar->pushButton_download->styleSheet();
}
