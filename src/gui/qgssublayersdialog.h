/***************************************************************************
    qgssublayersdialog.h  - dialog for selecting sublayers
    ---------------------
    begin                : January 2009
    copyright            : (C) 2009 by Florian El Ahdab
    email                : felahdab at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSSUBLAYERSDIALOG_H
#define QGSSUBLAYERSDIALOG_H

#include <QDialog>
#include <ui_qgssublayersdialogbase.h>
#include "qgscontexthelp.h"
#include "qgsdatasource.h"


class QgsSublayersDialog : public QDialog, private Ui::QgsSublayersDialogBase
{
    Q_OBJECT
  public:

    QgsSublayersDialog( QgsDataSource* dataSource, QWidget* parent = 0, Qt::WFlags fl = 0 );
    ~QgsSublayersDialog();

    void populateLayerTable( QStringList theList, QString delim = ":" );
    QStringList selectionNames();
    QList<int> selectionIndexes();
    QStringList selectionUris();

    /* tries to load a QgsDataSource for the given uri and providers(s)
     * if the dataSource contains many layers, prompts user to select which ones to add
     * and then adds selection to the map registry
     * does NOT update the mapCanvas
     * returns a list of layers created, an empty list if user declined */
    static QList<QgsMapLayer *> addDataSourceLayers( QWidget* parent, QString baseUri, QgsMapLayer::LayerType type, QStringList providerKeys );
    static QList<QgsMapLayer *> addDataSourceLayers( QWidget* parent, QString baseUri, QgsMapLayer::LayerType type = QgsMapLayer::PluginLayer )
    { return addDataSourceLayers( parent, baseUri, type, QStringList() ); }

  public slots:
    void on_buttonBox_helpRequested() { QgsContextHelp::run( metaObject()->className() ); }
    int exec();

  protected:
    QString mName;
    QgsDataSource* mDataSource;
    QStringList mSelectedSubLayers;
};

#endif
