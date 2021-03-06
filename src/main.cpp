/*
 * Copyright (C) 2014 Red Hat
 * Copyright (C) 2016 by Lubomír Carik <Lubomir.Carik@gmail.com>
 *
 * This file is part of openconnect-gui.
 *
 * openconnect-gui is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common.h"
#include "config.h"
#include "dialog/MyInputDialog.h"
#include "dialog/mainwindow.h"
#include "openconnect-gui.h"

#include "logger.h"
#include "FileLogger.h"

extern "C" {
#include <gnutls/pkcs11.h>
#include <openconnect.h>
}

#include <QApplication>
#if !defined(_WIN32) && !defined(PROJ_GNUTLS_DEBUG)
#include <QMessageBox>
#endif
#ifdef PROJ_INI_SETTINGS
#include <QSettings>
#endif

#include <csignal>
#include <cstdio>

static void log_callback(int level, const char* str)
{
    Logger::instance().addMessage(QString(str).trimmed(),
                                  Logger::MessageType::DEBUG,
                                  Logger::ComponentType::GNUTLS
                                  );
}

int pin_callback(void* userdata, int attempt, const char* token_url,
    const char* token_label, unsigned flags, char* pin,
    size_t pin_max)
{
    QString type = QObject::tr("user");
    if (flags & GNUTLS_PIN_SO) {
        type = QObject::tr("security officer");
    }

    QString outtext = QObject::tr("Please enter the ") + type + QObject::tr(" PIN for ") + QLatin1String(token_label) + ".";
    if (flags & GNUTLS_PKCS11_PIN_FINAL_TRY) {
        outtext += QObject::tr(" This is the FINAL try!");
    }
    if (flags & GNUTLS_PKCS11_PIN_COUNT_LOW) {
        outtext += QObject::tr(" Only few tries before token lock!");
    }

    MainWindow* w = (MainWindow*)userdata;
    MyInputDialog dialog(w, QLatin1String(token_url), outtext, QLineEdit::Password);
    dialog.show();

    QString text;
    bool ok = dialog.result(text);
    if (ok == false) {
        return -1;
    }

    snprintf(pin, pin_max, "%s", text.toLatin1().data());
    return 0;
}

int main(int argc, char* argv[])
{
    qRegisterMetaType<Logger::Message>();

#ifdef PROJ_INI_SETTINGS
    QSettings::setDefaultFormat(QSettings::IniFormat);
#endif

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName(appDescription);
    app.setApplicationDisplayName(appDescriptionLong);
    app.setApplicationVersion(appVersion);
    app.setOrganizationName(appOrganizationName);
    app.setOrganizationDomain(appOrganizationDomain);

    auto fileLog = std::make_unique<FileLogger>();
    Logger::instance().addMessage(QString("%1 (%2) logging started...").arg(app.applicationDisplayName()).arg(app.applicationVersion()));

#if !defined(_WIN32) && !defined(PROJ_GNUTLS_DEBUG)
    if (getuid() != 0) {
        QMessageBox msgBox;

        msgBox.setText(QObject::tr("This program requires root privileges to fully function."));
        msgBox.setInformativeText(QObject::tr("VPN connection establishment would fail."));
        msgBox.exec();
    }
#endif

    gnutls_global_init();
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    openconnect_init_ssl();

    MainWindow mainWindow;

#ifdef PROJ_PKCS11
    gnutls_pkcs11_set_pin_function(pin_callback, &mainWindow);
#endif
    gnutls_global_set_log_function(log_callback);
#ifdef PROJ_GNUTLS_DEBUG
    gnutls_global_set_log_level(3);
#endif

    mainWindow.show();
    return app.exec();
}
