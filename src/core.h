/*
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef CORE_H
#define CORE_H

#include <memory>
#include <QTabWidget>
#include <QObject>
#include <QUrl>
#include "kdenlivecore_export.h"

class MainWindow;
class ProjectManager;
class MonitorManager;
class BinController;
class Bin;
class LibraryWidget;
class ProducerQueue;
class MltConnection;
class ProfileModel;

namespace Mlt
{
    class Repository;
}

#define EXIT_RESTART (42)
#define pCore Core::self()

/**
 * @class Core
 * @brief Singleton that provides access to the different parts of Kdenlive
 *
 * Needs to be initialize before any widgets are created in MainWindow.
 * Plugins should be loaded after the widget setup.
 */

class /*KDENLIVECORE_EXPORT*/ Core : public QObject
{
    Q_OBJECT

public:
    Core(const Core&) = delete;
    Core& operator=( const Core& ) = delete;
    Core(Core&&) = delete;
    Core& operator=(Core&&) = delete;

    virtual ~Core();

    /**
     * @brief Setup the basics of the application, in particular the connection
     * with Mlt
     * @param MltPath (optional) path to MLT environment
     */
    static void build(const QString &MltPath = QString());

    /**
     * @brief Init the GUI part of the app and show the main window
     * @param Url (optional) file to open
     * If Url is present, it will be opened, otherwhise, if openlastproject is
     * set, latest project will be opened. If no file is open after trying this,
     * a default new file will be created. */
    void initGUI(const QUrl &Url);

    /** @brief Returns a pointer to the singleton object. */
    static std::unique_ptr<Core>& self();

    /** @brief Returns a pointer to the main window. */
    MainWindow *window();

    /** @brief Returns a pointer to the project manager. */
    ProjectManager *projectManager();
    /** @brief Returns a pointer to the monitor manager. */
    MonitorManager *monitorManager();
    /** @brief Returns a pointer to the project bin controller. */
    BinController *binController();
    /** @brief Returns a pointer to the project bin. */
    Bin *bin();
    /** @brief Returns a pointer to the producer queue. */
    ProducerQueue *producerQueue();
    /** @brief Returns a pointer to the library. */
    LibraryWidget *library();
    /** @brief Returns a pointer to the timeline. */
    QWidget *timelineTabs();
    /** @brief Add a new timeline tab. */
    void addTimeline(QWidget *timeline, const QString &name);

    /** @brief Returns a pointer to MLT's repository */
    std::unique_ptr<Mlt::Repository>& getMltRepository();


    /** @brief Returns a pointer to the current profile */
    std::unique_ptr<ProfileModel>& getCurrentProfile() const;

private:
    explicit Core();
    static std::unique_ptr<Core> m_self;

    /** @brief Makes sure Qt's locale and system locale settings match. */
    void initLocale();

    MainWindow *m_mainWindow;
    ProjectManager *m_projectManager;
    MonitorManager *m_monitorManager;
    BinController *m_binController;
    ProducerQueue *m_producerQueue;
    Bin *m_binWidget;
    LibraryWidget *m_library;
    QTabWidget *m_timelineTab;

    std::unique_ptr<MltConnection> m_mltConnection;

    QString m_profile;

signals:
    void coreIsReady();
    void updateLibraryPath();
};

#endif
