/***************************************************************************
    qgsgdaldataitems.h
    ---------------------
    begin                : October 2011
    copyright            : (C) 2011 by Martin Dobias
    email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSGDALDATAITEMS_H
#define QGSGDALDATAITEMS_H

#include "qgsdataitem.h"
#include "qgsgdalprovider.h"

class QgsGdalLayerItem : public QgsLayerItem
{
  public:
    QgsGdalLayerItem( QgsDataItem* parent, QString name,
                      QString path, QString uri, QString layerName = QString() );
    ~QgsGdalLayerItem();

    bool setCrs( QgsCoordinateReferenceSystem crs );
    Capability capabilities();
    QString layerName() const { return mLayerName; }

  protected:
    QString mLayerName;
};

class QgsGdalDataCollectionItem : public QgsDataCollectionItem
{
  public:
    QgsGdalDataCollectionItem( QgsDataItem* parent, QString name, QString path, QgsGdalDataSource* dataSource );
    ~QgsGdalDataCollectionItem();

    int realChildCount() const { return mDataSource->layerNames().count(); }
    QVector<QgsDataItem*> createChildren();

  protected:
    QgsGdalDataSource* mDataSource;
};

#endif // QGSGDALDATAITEMS_H
