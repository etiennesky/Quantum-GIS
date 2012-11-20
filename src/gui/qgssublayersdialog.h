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

class QgsSublayersDialog : public QDialog, private Ui::QgsSublayersDialogBase
{
    Q_OBJECT
  public:

    enum SublayersType
    {
      Ogr = 0,
      Gdal = 1,
      Zip = 2,
      Other = 3
    };

    QgsSublayersDialog( QWidget* parent = 0, SublayersType type = Ogr,
                        QString name = "OGR", Qt::WFlags fl = 0 );
    ~QgsSublayersDialog();
    void populateLayerTable( QStringList theList, QString delim = ":" );
    QStringList getSelection();
    QList<int> getSelectionIndexes();

  public slots:
    void on_buttonBox_helpRequested() { QgsContextHelp::run( metaObject()->className() ); }

  protected:
    SublayersType mType;
    QString mName;
};

#endif
