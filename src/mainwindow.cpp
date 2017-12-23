/*
 * Copyright 2017 Alexander Fasching
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>

#include <QDebug>
#include <QDir>
#include <QByteArray>
#include <QHostAddress>
#include <QUrl>
#include <QClipboard>
#include "mainwindow.h"
#include "dht/dht.h"

#include "ui_mainwindow.h"


MainWindow::MainWindow() :
    ui(new Ui::MainWindow),
    s4(-1),
    s6(-1),
    sn4(nullptr),
    sn6(nullptr),
    settings(nullptr),
    myID(nullptr)
{
    ui->setupUi(this);
    ui->peerList->setSelectionMode(QAbstractItemView::ExtendedSelection);

    searchValidator = new HashValidator(this);
    ui->searchInput->setValidator(searchValidator);
    ui->searchButton->setEnabled(false);
    connect(searchValidator, &HashValidator::validityChanged, ui->searchButton, &QPushButton::setEnabled);

    connect(ui->searchButton, &QPushButton::clicked, this, &MainWindow::searchButtonClicked);
    connect(ui->searchList, &QListWidget::currentRowChanged, this, &MainWindow::searchListRowChanged);

    connect(ui->copyButton, &QPushButton::clicked, this, &MainWindow::copyResultsToClipboard);
    connect(ui->refreshButton, &QPushButton::clicked, this, &MainWindow::refreshButtonClicked);
    connect(ui->clearButton, &QPushButton::clicked, this, &MainWindow::clearButtonClicked);

    trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(ui->actionQuit);

    trayIcon = new QSystemTrayIcon(QIcon(":/icons/trayicon.svg"), this);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->show();

    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::iconActivated);
}

MainWindow::~MainWindow()
{
    auto peers = getPeers();
    settings->setValue("nodes", peers);
    settings->sync();

    dht_uninit();

    if(s4 >= 0)
        ::close(s4);

    if(s6 >= 0)
        ::close(s6);

    delete sn4;
    delete sn6;
    delete timer;

    settings->sync();
    delete settings;
    delete myID;
}

/**
 * Initialize the DHT. Returns true on success.
 */
bool MainWindow::init()
{
    /* Get the configuration */
    auto cfgfile = QString("%1/.config/dht-explorer/default.conf").arg(QDir::homePath());
    settings = new QSettings(cfgfile, QSettings::IniFormat, this);

    auto port = settings->value("port", "6881").toInt();
    auto useIPv4 = settings->value("IPv4", "1").toInt();
    auto useIPv6 = settings->value("IPv6", "0").toInt();
    auto id = settings->value("ID", "").toString();
    auto btNodes = settings->value("nodes", QStringList() << "82.221.103.244:6881").toStringList();

    /* Create a new ID if it doesn't exist */
    myID = new unsigned char[20];
    if(id.size() != 40) {
        /* Generate new ID */
        for(int i=0; i<20; i++)
            myID[i] = rand() % 256;
        settings->setValue("ID", QString(QByteArray((char *) myID, 20).toHex()));
    }
    else {
        memcpy(&myID[0], QByteArray::fromHex(id.toLatin1()).data(), 20);
    }
    qDebug() << "Using ID" << id;
    settings->sync();

    /* Initialize the sockets and the DHT */
    int rc;

    sockaddr_in  sin4;
    sockaddr_in6 sin6;

    memset(&sin4, 0, sizeof(sin4));
    sin4.sin_family = AF_INET;

    memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_family = AF_INET6;

    if(useIPv4) {
        s4 = socket(AF_INET, SOCK_DGRAM, 0);
        if(s4 < 0) {
            qCritical("Creation of IPv4 socket failed");
            return false;
        }

        /* Setup the IPv4 socket */
        sin4.sin_port = htons(port);
        rc = bind(s4, (sockaddr*) &sin4, sizeof(sin4));
        if(rc < 0) {
            qCritical("Binding of IPv4 socket failed");
            return false;
        }
    }

    if(useIPv6) {
        s6 = socket(AF_INET6, SOCK_DGRAM, 0);
        if(s6 < 0) {
            qCritical("Creation of IPv6 socket failed");
            return false;
        }

        /* Setup the IPv6 socket */
        int val = 1;
        rc = setsockopt(s6, IPPROTO_IPV6, IPV6_V6ONLY, (char *) &val, sizeof(val));
        if(rc < 0) {
            qCritical("setsockopt() on IPv6 socket failed");
            return false;
        }

        sin6.sin6_port = htons(port);
        rc = bind(s6, (sockaddr*)&sin6, sizeof(sin6));
        if(rc < 0) {
            qCritical("bind() on IPv6 socket failed");
            return false;
        }
    }

    /* Setup the DHT. This sets the sockets to non-blocking. */
    rc = dht_init(s4, s6, myID, (unsigned char *)"AFG\0");
    if(rc < 0) {
        qCritical("dht_init() failed");
        return false;
    }

    /* Bootstrap the DHT */
    for(auto &s : btNodes) {
        /* QUrl requires a scheme */
        QUrl url(QString("http://%1").arg(s));

        if(not url.isValid())
            continue;

        auto ip = url.host();
        auto port = url.port();

        if(QHostAddress(ip).protocol() == QAbstractSocket::IPv4Protocol) {
            sockaddr_in sin;
            memset(&sin, 0, sizeof(sockaddr_in));
            sin.sin_family = AF_INET;
            rc = inet_pton(AF_INET, ip.toLatin1().data(), &sin.sin_addr);
            if(rc == 1) {
                sin.sin_port = htons(port);
                socklen_t salen = sizeof(sockaddr_in);
                rc = dht_ping_node((sockaddr*) &sin, salen);

                if(rc > 0) {
                    qDebug() << "Bootstrapped from" << s;
                }
                else {
                    qDebug() << "Bootstrapping from" << s << "failed";
                }
            }
        }
        else if(QHostAddress(ip).protocol() == QAbstractSocket::IPv6Protocol) {
            qDebug() << "Bootstrapping from IPv6 not implemented";
        }
    }

    /* At this point, the DHT should be initialized. We can now set up
     * the QSocketNotifiers and process the remaining events through
     * the event loop.
     */
    if(useIPv4) {
        sn4 = new QSocketNotifier(s4, QSocketNotifier::Read, this);
        connect(sn4, &QSocketNotifier::activated, this, &MainWindow::socketActivated);
    }

    if(useIPv6) {
        sn6 = new QSocketNotifier(s6, QSocketNotifier::Read, this);
        connect(sn6, &QSocketNotifier::activated, this, &MainWindow::socketActivated);
    }

    timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, &MainWindow::timerActivated);
    timer->start(0);

    return true;
}

/**
 * Show/Hide the mainwindow when the user clicks the icon.
 */
void MainWindow::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger) {
        this->setVisible(not this->isVisible());
    }
}

/**
 * Hide window when requested through File-menu
 */
void MainWindow::on_actionHide_triggered()
{
    this->hide();
}

/**
 * Close the application when requested through File-menu
 */
void MainWindow::on_actionQuit_triggered()
{
    this->close();
}

/**
 * Activity on a socket. This is triggered by the socket nofiiers.
 */
void MainWindow::socketActivated(int s)
{
    if(s == s4)
        qDebug() << "IPv4 socket activated";
    else if(s == s6)
        qDebug() << "IPv6 socket activated";

    timer->stop();

    /* Receive and process datagrams */
    int rc;
    char buffer[4096];
    sockaddr_storage from;
    socklen_t fromlen = sizeof(from);
    time_t tosleep;

    rc = recvfrom(s, buffer, sizeof(buffer) - 1, 0, (sockaddr *) &from, &fromlen);
    if(rc > 0) {
        buffer[rc] = '\0';
        rc = dht_periodic(buffer, rc, (sockaddr*)&from, fromlen,
                          &tosleep, this->dhtCallback, this);
    }
    else {
        rc = dht_periodic(nullptr, 0, nullptr, 0, &tosleep, this->dhtCallback, this);
    }

    if(rc < 0) {
        tosleep = 1;
    }

    timer->start(tosleep * 1000);

    updatePeers();
}

/**
 * Timeout triggered.
 */
void MainWindow::timerActivated(void)
{
    qDebug() << "Timer activated";

    int rc;
    time_t tosleep = 0;

    rc = dht_periodic(nullptr, 0, nullptr, 0, &tosleep, this->dhtCallback, this);
    timer->start(tosleep * 1000);

    updatePeers();
}

/**
 * Called by dht_periodic.
 * inst is a pointer to a CallbackInfo structure.
 */
void MainWindow::dhtCallback(void *win, int event,
                             const unsigned char *info_hash,
                             const void *data, size_t data_len)
{
    auto window = static_cast<MainWindow *>(win);
    QByteArray hash((char *) info_hash, 20);

    /* Find the SearchInfo structure for the callback */
    SearchInfo *info = window->findSearchInfo(hash);

    if(info) {
        if(event == DHT_EVENT_SEARCH_DONE) {
            qDebug() << "Search for" << hash.toHex() << "completed";
            emit info->searchDone();
        }
        else if(event == DHT_EVENT_VALUES) {
            qDebug() << "Received" << data_len / 6 << "values for" << hash.toHex();

            for(int i=0; i<data_len; i+=6) {
                int port = (((const unsigned char *) data)[i+4] << 8) | ((const unsigned char *) data)[i+5];

                auto addr = QString("%1.%2.%3.%4:%5")
                    .arg(((unsigned char *) data)[i+0])
                    .arg(((unsigned char *) data)[i+1])
                    .arg(((unsigned char *) data)[i+2])
                    .arg(((unsigned char *) data)[i+3])
                    .arg(port);

                info->results.insert(addr);
            }
            emit info->searchUpdate();
        }
        else if(event == DHT_EVENT_VALUES6) {
            qDebug() << "Receiving IPv6 nodes not supported";
        }
    }
    else {
        qDebug() << "Callback executed for unknown hash" << hash.toHex();
    }
}

/**
 * Update peerlist and systray icon
 */
void MainWindow::updatePeers(void)
{
    int good4, good6;

    dht_nodes(AF_INET, &good4, nullptr, nullptr, nullptr);
    dht_nodes(AF_INET6, &good6, nullptr, nullptr, nullptr);

    trayIcon->setToolTip(QString("Peers: %1+%2").arg(good4).arg(good6));

    auto peers = getPeers();
    peers.sort();

    ui->peerList->clear();
    for(auto &p : peers) {
        ui->peerList->addItem(p);
    }
}

/**
 * Return a list of all active peers
 */
QStringList MainWindow::getPeers(void)
{
    int num4 = 1024;
    sockaddr_in sin4[num4];

    int num6 = 1024;
    sockaddr_in6 sin6[num6];

    dht_get_nodes(&sin4[0], &num4, &sin6[0], &num6);

    QStringList nodes;
    char buffer[128];
    for(int i=0; i<num4; i++) {
        inet_ntop(AF_INET, &sin4[i].sin_addr, buffer, 128);
        nodes.append(QString("%1:%2").arg(buffer).arg(ntohs(sin4[i].sin_port)));
    }
    for(int i=0; i<num6; i++) {
        inet_ntop(AF_INET6, &sin6[i].sin6_addr, buffer, 128);
        nodes.append(QString("[%1]:%2").arg(buffer).arg(ntohs(sin4[i].sin_port)));
    }

    return nodes;
}

/**
 * Start a new search
 */
void MainWindow::searchButtonClicked(bool unused)
{
    auto hash = QByteArray::fromHex(ui->searchInput->text().toLatin1());
    SearchInfo *info = findSearchInfo(hash);

    bool exists = true;
    if(not info) {
        info = new SearchInfo(hash, this);
        exists = false;
    }

    if(exists) {
        qDebug() << "Restart search for" << info->hash.toHex();
    }
    else {
        qDebug() << "Start a search for" << info->hash.toHex();

        ui->searchList->addItem(info->hash.toHex());
        if(ui->searchList->count() == 1) {
            ui->searchList->setCurrentRow(0);
        }
        ui->searchInput->clear();

        activeSearches.append(info);
        connect(info, &SearchInfo::searchDone, this, &MainWindow::searchDone);
        connect(info, &SearchInfo::searchUpdate, this, &MainWindow::searchUpdate);
    }
    ui->searchInput->clear();

    dht_search((unsigned char *) info->hash.data(), 0, AF_INET, &MainWindow::dhtCallback, this);
}

/**
 * Called when a search is completed
 */
void MainWindow::searchDone(void)
{
    updateSearchResults();
}

/**
 * Called when a search is updated
 */
void MainWindow::searchUpdate(void)
{
    updateSearchResults();
}

/**
 * Called when the user selects another hash in the
 * searchList widget.
 */
void MainWindow::searchListRowChanged(int row)
{
    updateSearchResults();
}

/**
 * Update the search results widget
 */
void MainWindow::updateSearchResults(void)
{
    auto item = ui->searchList->currentItem();
    if(item) {
        QByteArray hash = QByteArray::fromHex(item->text().toLatin1());
        SearchInfo *info = findSearchInfo(hash);

        if(info) {
            ui->searchResults->clear();
            for(auto &r : info->results) {
                ui->searchResults->addItem(r);
            }
        }
    }
}

/**
 * Copy button pressed
 */
void MainWindow::copyResultsToClipboard(void)
{
    auto item = ui->searchList->currentItem();
    if(item) {
        QByteArray hash = QByteArray::fromHex(item->text().toLatin1());
        SearchInfo *info = findSearchInfo(hash);

        if(info) {
            QString results = QStringList(info->results.toList()).join("\n");
            QClipboard *clipboard = QApplication::clipboard();
            clipboard->setText(results);
        }
    }
}

/**
 * Restart search, but keep results
 */
void MainWindow::refreshButtonClicked(bool unused)
{
    auto item = ui->searchList->currentItem();
    if(item) {
        QByteArray hash = QByteArray::fromHex(item->text().toLatin1());
        SearchInfo *info = findSearchInfo(hash);

        if(info) {
            qDebug() << "Restarting search for" << hash.toHex();
            dht_search((unsigned char *) info->hash.data(), 0, AF_INET, &MainWindow::dhtCallback, this);
        }
    }
}

/**
 * Clear search results
 */
void MainWindow::clearButtonClicked(bool unused)
{
    auto item = ui->searchList->currentItem();
    if(item) {
        QByteArray hash = QByteArray::fromHex(item->text().toLatin1());
        SearchInfo *info = findSearchInfo(hash);

        if(info) {
            activeSearches.removeAll(info);
            delete ui->searchList->currentItem();

            if(ui->searchList->count() == 0) {
                ui->searchResults->clear();
            }
        }
    }
}

/**
 * Find the SearchInfo structure belonging to a hash
 */
SearchInfo * MainWindow::findSearchInfo(QByteArray &hash)
{
    SearchInfo *info = nullptr;
    for(auto &p : activeSearches) {
        if(p->hash == hash) {
            return p;
        }
    }
    return nullptr;
}
