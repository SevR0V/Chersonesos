#include "video_streamer.h"

VideoStreamer::VideoStreamer(StreamFrameInfo* streamInfo, int port, QObject* parent)
    : QObject(parent), m_streamInfo(streamInfo), m_port(port), m_server(new QTcpServer(this)), m_isStreaming(false) {
    connect(m_server, &QTcpServer::newConnection, this, &VideoStreamer::handleNewConnection);
}

VideoStreamer::~VideoStreamer() {
    stopStreaming();
}

void VideoStreamer::startStreaming() {
    if (m_isStreaming) {
        QString errorMsg = QString("Стриминг уже активен для камеры %1").arg(m_streamInfo->name);
        qDebug() << errorMsg;
        emit errorOccurred("VideoStreamer", errorMsg);
        return;
    }

    if (!m_server->listen(QHostAddress::Any, m_port)) {
        QString errorMsg = QString("Не удалось запустить сервер для камеры %1 на порту %2: %3")
                               .arg(m_streamInfo->name).arg(m_port).arg(m_server->errorString());
        qDebug() << errorMsg;
        emit errorOccurred("VideoStreamer", errorMsg);
        emit streamingFailed(errorMsg);
        return;
    }

    m_isStreaming = true;
    qDebug() << "Стриминг начат для камеры" << m_streamInfo->name << "на порту" << m_port;
    emit streamingStarted();
}

void VideoStreamer::stopStreaming() {
    if (!m_isStreaming) {
        return;
    }

    m_isStreaming = false;
    QMetaObject::invokeMethod(this, "closeAllConnections", Qt::QueuedConnection);
}

void VideoStreamer::closeAllConnections() {
    for (QTcpSocket* client : m_clients) {
        if (client->state() == QAbstractSocket::ConnectedState) {
            client->disconnectFromHost();
        }
        client->deleteLater();
    }
    m_clients.clear();
    m_server->close();
    qDebug() << "Стриминг остановлен для камеры" << m_streamInfo->name;
    emit streamingFinished();
}

void VideoStreamer::handleNewConnection() {
    QTcpSocket* client = m_server->nextPendingConnection();
    if (!client) {
        QString errorMsg = QString("Не удалось получить новое соединение для камеры %1").arg(m_streamInfo->name);
        qDebug() << errorMsg;
        emit errorOccurred("VideoStreamer", errorMsg);
        return;
    }

    connect(client, &QTcpSocket::disconnected, this, [=]() {
        QByteArray disconnectMessage = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nDisconnected from camera stream\r\n";
        client->write(disconnectMessage);
        client->flush();
        m_clients.remove(client);
        client->deleteLater();
        qDebug() << "Клиент отключился от стриминга камеры" << m_streamInfo->name;
    });

    connect(client, &QTcpSocket::readyRead, this, [=]() {
        QByteArray request = client->readAll();
        QString requestStr = QString::fromUtf8(request);
        if (requestStr.contains("GET /" + m_streamInfo->name)) {
            m_clients.insert(client);
            sendMJPEGHeader(client);
            qDebug() << "Клиент подключился к стримингу камеры" << m_streamInfo->name;
            streamFrames(client);
        } else {
            QString errorMsg = QString("Неверный запрос для камеры %1: %2").arg(m_streamInfo->name).arg(requestStr);
            qDebug() << errorMsg;
            emit errorOccurred("VideoStreamer", errorMsg);
            client->write("HTTP/1.1 404 Not Found\r\n\r\n");
            client->disconnectFromHost();
        }
    });

    connect(client, &QAbstractSocket::stateChanged, this, [=](QAbstractSocket::SocketState state) {
        if (state == QAbstractSocket::UnconnectedState && m_clients.contains(client)) {
            m_clients.remove(client);
            client->deleteLater();
        }
    });
}

void VideoStreamer::sendMJPEGHeader(QTcpSocket* client) {
    QByteArray header;
    header += "HTTP/1.1 200 OK\r\n";
    header += "Content-Type: multipart/x-mixed-replace; boundary=--frameboundary\r\n";
    header += "Cache-Control: no-cache\r\n";
    header += "Connection: close\r\n";
    header += "\r\n";
    client->write(header);
    client->flush();
}

void VideoStreamer::streamFrames(QTcpSocket* client) {
    while (m_isStreaming && client->state() == QAbstractSocket::ConnectedState) {
        cv::Mat frame;
        {
            QMutexLocker locker(m_streamInfo->mutex);
            int front = m_streamInfo->frontIndex.load(std::memory_order_acquire);  // Читаем atomic front
            if (!m_streamInfo->buffers[front].empty()) {
                frame = m_streamInfo->buffers[front].clone();  // Clone для модификации
            }
        }

        if (!frame.empty()) {
            try {
                cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
                std::vector<uchar> buffer;
                std::vector<int> compression_params;
                cv::resize(frame, frame, cv::Size(800, 600));
                compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
                compression_params.push_back(70);
                cv::imencode(".jpg", frame, buffer, compression_params);

                QByteArray frameData;
                frameData += "--frameboundary\r\n";
                frameData += "Content-Type: image/jpeg\r\n";
                frameData += "Content-Length: " + QByteArray::number(buffer.size()) + "\r\n";
                frameData += "\r\n";
                frameData += QByteArray::fromRawData(reinterpret_cast<const char*>(buffer.data()), buffer.size());
                frameData += "\r\n";

                client->write(frameData);
                client->flush();
            } catch (const cv::Exception& e) {
                QString errorMsg = QString("Ошибка кодирования кадра для стриминга камеры %1: %2")
                                       .arg(m_streamInfo->name).arg(e.what());
                qDebug() << errorMsg;
                emit errorOccurred("VideoStreamer", errorMsg);
                client->disconnectFromHost();
                return;
            }
        } else {
            qDebug() << "Пропущен пустой кадр для стриминга камеры" << m_streamInfo->name;
        }

        QThread::msleep(50);
    }
}
