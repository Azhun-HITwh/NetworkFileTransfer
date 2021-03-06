#include "mysocket.h"
#include <QDataStream>
#include "protocolcommand.h"
#include <QDir>
#include <QDirIterator>
Mysocket::Mysocket(int socket, int ID,QObject *parent)
    :QTcpSocket(parent), socketDescriptor(socket)
{
    socketID = ID;
    transferData.payloadSize = 1024 * 64;//64k
    clearVariation();
#if defined __DEBUG__
    qDebug()<<"Client ID is"<<socketID;
#endif
    QObject::connect(this,&Mysocket::readyRead,this,&Mysocket::reveiveData);
}

Mysocket::~Mysocket()
{

}

void Mysocket::sendFile(QString path)
{
#if defined __DEBUG__
    qDebug()<<"Socket send file name (full path) is"<<path;
#endif
    /*初始化发送字节为0*/
    transferData.bytesWritten = 0;
    transferData.fileName = path;
    transferData.localFile = new QFile(path);
    if(!transferData.localFile->open(QFile::ReadOnly))
    {
        qDebug()<<tr("open file error!")<<endl;
        return;
    }
    transferData.totalBytes  = transferData.localFile->size();//获取文件大小
#if defined __DEBUG__
    qDebug()<<"content size:"<<transferData.totalBytes;
#endif
    QDataStream sendOut(&transferData.inOrOutBlock,QIODevice::WriteOnly);

    sendOut.setVersion(QDataStream::Qt_5_7); //设置版本
    /*获得文件名称*/
    QString currentFilename = transferData.fileName.right(transferData.fileName.size()
                                                -transferData.fileName.lastIndexOf('/')-1);
#if defined __DEBUG__
    qDebug()<<"file sent name:"<<currentFilename;
#endif

    /*保留总大小信息空间、命令、文件名大小信息空间、然后输入文件名*/
    sendOut << qint64(0) << qint64(0) << qint64(0)<< currentFilename;
    /*总的大小*/
    transferData.totalBytes += transferData.inOrOutBlock.size();
#if defined __DEBUG__
    qDebug()<<"total size incl. header:"<<transferData.totalBytes
            <<"size of header:"<<transferData.inOrOutBlock.size()
           <<"size of file name:"<<qint64((transferData.inOrOutBlock.size()-(sizeof(qint64)*3)));
    qDebug()<<transferData.inOrOutBlock;
#endif
    sendOut.device()->seek(0);
    /*填充实际的存储空间*/
    sendOut << transferData.totalBytes<<_TRANSFER_FILE_
            <<qint64((transferData.inOrOutBlock.size()-(sizeof(qint64)*3)));

    qint64 sum = write(transferData.inOrOutBlock);
#if defined __DEBUG__
    qDebug()<<"sum:"<<sum;
#endif
    transferData.bytesToWrite = transferData.totalBytes - sum;
#if defined __DEBUG__
    qDebug()<<"size of file:"<<transferData.bytesToWrite;
#endif
    //transferData.inOrOutBlock.resize(0);
    /*表示数据没有发送完*/
    if(transferData.bytesToWrite>0)
    {
        /*发送文件内容部分*/
        transferData.inOrOutBlock = (transferData.localFile->readAll());
        write(transferData.inOrOutBlock);
        transferData.inOrOutBlock.resize(0);
    }
    transferData.localFile->close();
}

void Mysocket::sendMSG(QString msg, qint64 cmd)
{
    /*确保连接依然有效*/
    if(!isValid())
    {
        qDebug()<<"Already disconnected!";
        return;
    }
    transferData.totalBytes = 0;
    /*使用数据流写入数据*/
    QDataStream outPut(&transferData.inOrOutBlock,QIODevice::WriteOnly);
    outPut.setVersion(QDataStream::Qt_5_2);
    transferData.totalBytes = msg.toUtf8().size();

#if defined __DEBUG__
    qDebug()<<"Message total size is"<<transferData.totalBytes;
#endif

    /*将命令的长度、命令的类型和文件名字写入到数据流中，*/
    outPut<<qint64(0)<<qint64(cmd)<<msg;

#if defined __DEBUG__
    qDebug()<<"Message total size is"<<qint64(transferData.totalBytes)
           <<"Command size is"<<qint64(cmd);
#endif
    /*消息的总大小*/
    transferData.totalBytes += transferData.inOrOutBlock.size();
    /*定位到数据流的开始位置*/
    outPut.device()->seek(0);
    /*填充所有数据的大小msg+cmd+qint64*/
    outPut<<transferData.totalBytes;
    /*发送命令内容*/
    write(transferData.inOrOutBlock);
    transferData.inOrOutBlock.resize(0);
}

void Mysocket::reveiveData()
{
    /*标志变量：下载标志变量  同步文件列表标志变量  传输文件标志变量*/
    int downflag = 0  ,  synfilelistflag = 0,   transferfileflag = 0;
    qint32 temp;
    /*如果没有数据可读，就直接返回*/
    if(bytesAvailable()<=0)
    {
        return;
    }
#if defined __DEBUG__
    qDebug()<<"data size in socket="<<bytesAvailable();
#endif

    QDataStream in(this);
    in.setVersion(QDataStream::Qt_5_7);
#if defined __DEBUG__
    qDebug()<<"transferData.bytesReceiced ="<<transferData.bytesReceived
            <<"transferData.fileNameSize ="<<transferData.fileNameSize;
#endif
    /*提取出总的大小、命令、文件名字的大小、文件名信息*/
    if(transferData.bytesReceived <= sizeof(qint64)*3)
    {
#if defined __DEBUG__
        qDebug()<<"ava. size in socket"<<bytesAvailable();
#endif
        if(bytesAvailable()>=sizeof(qint64)*3
                &&(transferData.fileNameSize==0))
        {
            /*从套接字中提取总大小信息、命令、文件名字大小*/
            in >> transferData.totalBytes >> transferData.command
               >> transferData.fileNameSize >> temp;
            transferData.bytesReceived += sizeof(qint64)*3;
#if defined __DEBUG__
            qDebug()<< "total size:"<<transferData.totalBytes
                    << "command:"<<transferData.command
                    << "receive file name size:"<<transferData.fileNameSize;
#endif
#if defined __DEBUG__
            if(bytesAvailable())
            {
                qDebug()<<"socket have data!";
            }
#endif
        }
        if(bytesAvailable()>=transferData.fileNameSize
                &&transferData.fileNameSize!=0)
        {
            /*提取接收的文件名字*/
            in >> transferData.fileName;
            transferData.bytesReceived += transferData.fileNameSize;
#if defined __DEBUG__
            qDebug()<<"receive file name is"<<transferData.fileName;
            qDebug()<<"transferData.bytesReceive is"<<transferData.bytesReceived;
#endif

#if defined __DEBUG__
            if(bytesAvailable())
            {
                qDebug()<<"socket have file content data!";
            }
#endif
        }
    }
    switch(transferData.command)
    {
        case _TRANSFER_FILE_ :
        {
            transferfileflag = 1;
            if(transferData.fileNameSize != 0)
            {
                QDir dir;
                QString curdir = dir.currentPath() + "/ReceiveFile/";
//                QString fpath = curdir + "FILELIST.txt";
                if(!dir.exists(curdir))
                    dir.mkpath(curdir);
                QString tempfilename(curdir);
                tempfilename += transferData.fileName;
                qDebug()<<tempfilename;
                /*创建本地文件*/
                transferData.localFile = new QFile(tempfilename);
                /*no need to open here because no content is written into it*/
//                if(!transferData.localFile->open(QFile::WriteOnly))
//                {
//                    qDebug()<<"open local file error!";
//                    return;
//                }
            }
        break;
        }
        case _TRANSFER_LIST_ :
        {
            synfilelistflag = 1;
            clearVariation();
            QDir dir;
            QString curdir = dir.currentPath() + "/FileList/";
            curdir += "FILELIST.txt";
            sendFile(curdir);
        break;
        }
        case _DOWNLOAD_FILE_ :
        {
            downflag = 1;
            QString downloadFilePath;
            clearVariation();
            QDir dir;
            QString curdir = dir.currentPath() + "/ServerFile/";
            if(!dir.exists(curdir))
                dir.mkpath(curdir);
            downloadFilePath = this->findDownloadFile(curdir,transferData.fileName);
            sendFile(downloadFilePath);
        break;
        }
        case _TRANSFER_ACK_ :
        {
            qDebug()<<"Send file success!";
        break;
        }
        default:
            qDebug()<<"Receive command nulity!";
        break;
    }
#if defined __DEBUG__
        qDebug()<<transferData.bytesReceived<<transferData.totalBytes;
#endif
    if(transferData.bytesReceived < transferData.totalBytes)
    {
        transferData.bytesReceived += bytesAvailable();
        transferData.inOrOutBlock = readAll();
        if(!transferData.localFile->open(QFile::WriteOnly|QFile::Append))
        {
            qDebug()<<"open local file error!";
            return;
        }
        transferData.localFile->write(transferData.inOrOutBlock);
        transferData.inOrOutBlock.resize(0);
    }
#if defined __DEBUG__
        qDebug()<<transferData.bytesReceived<<transferData.totalBytes;
#endif
    if(transferData.bytesReceived == transferData.totalBytes)
    {
        if(transferData.bytesReceived != 0)
        {
            clearVariation();
            if(transferfileflag == 1)
            {
                transferfileflag = 0;
                transferData.localFile->close();
                qDebug()<<"Receive file success!";
            }
            else if(synfilelistflag == 1)
            {
                synfilelistflag = 0;
                qDebug()<<"Request file list success!";
            }
            else if(downflag == 1)
            {
                downflag = 0;
                qDebug()<<"Download file success!";
            }
        } else
            downflag = 0;
            qDebug()<<"Download file failed and no content is sent!";
    }
}

void Mysocket::clientDisconnectSlot()
{
    this->disconnectFromHost();
}

void Mysocket::clearVariation()
{
    transferData.totalBytes = 0;
    transferData.bytesReceived = 0;
    transferData.fileNameSize = 0;
    transferData.inOrOutBlock.resize(0);
}

QString Mysocket::findDownloadFile(QString path, QString fileName)
{
    if(path.isEmpty()||fileName.isEmpty())
        return QString();
    QDir dir;
    QStringList filters;
    filters << fileName;
    dir.setPath(path);
    dir.setNameFilters(filters);
    QDirIterator iter(dir,QDirIterator::Subdirectories);
    while(iter.hasNext())
    {
        iter.next();
        QFileInfo info = iter.fileInfo();
        if(info.isFile())
        {
#ifdef __DEBUG__
            qDebug()<<"Download file path:"<<info.absoluteFilePath();
#endif
            return info.absoluteFilePath();
        }
    }
#ifdef __DEBUG__
            qDebug()<<"No file found in ServerFile folder !!!";
#endif
    return QString();
}
