#include "remoteevent.h"
#include "socket.h"

#include <QNetworkDatagram>
#include <QThread>
#include <QTime>

const int maxBlockSize = 8000;

Socket::Socket(QObject *parent)
    : QUdpSocket (parent)
{
    connect(this, &QUdpSocket::readyRead, this, &Socket::processRecvData);
}

Socket::~Socket()
{

}

void Socket::finish()
{
    m_destAddr = QHostAddress();
}

void Socket::writeToSocket(const QByteArray &d, qint8 blockType)
{
    static QTime time = QTime::currentTime();
    static int frame = 0;
    if (time.msecsTo(QTime::currentTime()) > 1000)
    {
        qDebug() << "发送速率：" << frame << " / s";
        frame = 0;
        time = QTime::currentTime();;
    }

    if (!m_destAddr.isNull())
    {
        int currentIndex = 0;
        int blockOffset = 0;
        int blockSize = d.size();
        int blockNum = blockSize / maxBlockSize;
        int last = blockSize % maxBlockSize;
        if (last != 0) blockNum++;

        QByteArray data;
        QDataStream out(&data, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_12);

        while (currentIndex < blockNum)
        {
            DataBlock block;
            block.blockType = blockType;
            block.blockIndex = currentIndex + 1;
            block.blockNum = blockNum;
            block.blockSize = blockSize;
            block.data = d.mid(blockOffset, maxBlockSize);

            out.device()->seek(0);
            out << block;
            writeDatagram(data, m_destAddr, 43800);
            QThread::usleep(1);

            data.clear();
            blockOffset += maxBlockSize;
            currentIndex++;
        }
        frame++;
    }
}

void Socket::writeToSocket(const RemoteEvent &event)
{
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_12);
    out << qint32(event.type()) << event.position();;
    writeToSocket(data, EVENT_TYPE);
}

void Socket::processRecvData()
{
    static int currentIndex = 1;
    static int blockSize = 0;
    static QByteArray recvData;

    while (hasPendingDatagrams())
    {
        QNetworkDatagram datagram = receiveDatagram();
        QByteArray data  = datagram.data();

        DataBlock block;
        QDataStream in(&data, QIODevice::ReadOnly);
        in >> block;

        if (currentIndex == 1)
            recvData.resize(block.blockSize);

        if (currentIndex == block.blockIndex)
        {
            currentIndex++;
            blockSize += block.data.size();
            recvData.insert((block.blockIndex - 1) * maxBlockSize, block.data);

            if (block.blockIndex == block.blockNum && blockSize == block.blockSize)
            {
                if (block.blockType == SCREEN_TYPE)
                    emit hasScreenData(recvData);

                currentIndex = 1;
                blockSize = 0;
                recvData.clear();
            }
        }
        else
        {
            currentIndex = 1;
            blockSize = 0;
            recvData.clear();
        }
    }
}