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

#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QSocketNotifier>
#include <QSettings>
#include <QTimer>

#include "hashvalidator.h"


namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow();
    bool init();

private slots:
    void on_actionHide_triggered();
    void on_actionQuit_triggered();
    void iconActivated(QSystemTrayIcon::ActivationReason reason);
    void socketActivated(int s);
    void timerActivated(void);

private:
    static void dhtCallback(void *inst, int event, const unsigned char *info_hash,
                  const void *data, size_t data_len);

    void updatePeers(void);
    QStringList getPeers(void);

    Ui::MainWindow *ui;
    QMenu *trayIconMenu;
    QSystemTrayIcon *trayIcon;
    HashValidator *searchValidator;

    int s4;                 /* Descriptor for IPv4 socket */
    int s6;                 /* Descriptor for IPv6 socket */
    QSocketNotifier *sn4;   /* Socket notifier for IPv4 socket */
    QSocketNotifier *sn6;   /* Socket notifier for IPv6 socket */
    QTimer *timer;          /* Timer to call dht_periodic */

    QSettings *settings;
    unsigned char *myID;
};
