/***************************************************************************
    qgsgdaldataitems.cpp
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
#include "qgsgdaldataitems.h"

#include "qgslogger.h"

#include <QFileInfo>
#include <QSettings>

// defined in qgsgdalprovider.cpp
void buildSupportedRasterFileFilterAndExtensions( QString & theFileFiltersString, QStringList & theExtensions, QStringList & theWildcards );


QgsGdalLayerItem::QgsGdalLayerItem( QgsDataItem* parent, QString name,
                                    QString path, QString uri, QString layerName )
    : QgsLayerItem( parent, name, path, uri, QgsLayerItem::Raster, "gdal" ), mLayerName( layerName )
{
  mToolTip = uri;
  mPopulated = true; // children are not expected
  if ( layerName.isEmpty() )
  {
    QFileInfo info( name );
    if ( info.suffix() == "gz" )
      mLayerName = info.baseName();
    else
      mLayerName = info.completeBaseName();
  }
}

QgsGdalLayerItem::~QgsGdalLayerItem()
{
}

QgsLayerItem::Capability QgsGdalLayerItem::capabilities()
{
  // Check if data source can be opened for update
  QgsDebugMsg( "mPath = " + mPath );
  GDALAllRegister();
  GDALDatasetH hDS = GDALOpen( TO8F( mPath ), GA_Update );

  if ( !hDS )
    return NoCapabilities;

  return SetCrs;
}

bool QgsGdalLayerItem::setCrs( QgsCoordinateReferenceSystem crs )
{
  QgsDebugMsg( "mPath = " + mPath );
  GDALAllRegister();
  GDALDatasetH hDS = GDALOpen( TO8F( mPath ), GA_Update );

  if ( !hDS )
    return false;

  QString wkt = crs.toWkt();
  if ( GDALSetProjection( hDS, wkt.toLocal8Bit().data() ) != CE_None )
  {
    QgsDebugMsg( "Could not set CRS" );
    return false;
  }
  GDALClose( hDS );
  return true;
}

// -------


static QgsGdalLayerItem* dataItemForLayer( QgsDataItem* parentItem, QgsGdalDataSource* dataSource, const QString& layerName )
{
  QgsDataSourceLayer* dataLayer = dataSource->dataSourceLayer( layerName );
  if ( ! dataLayer )
    return 0;

  return new QgsGdalLayerItem( parentItem, dataLayer->name, dataLayer->path, dataLayer->uri, dataLayer->layerName );
}

static QgsGdalLayerItem* dataItemForLayer( QgsDataItem* parentItem, QgsGdalDataSource* dataSource, int layerId )
{
  return dataItemForLayer( parentItem, dataSource, dataSource->layerNames().at( layerId ) );
}

// ----

QgsGdalDataCollectionItem::QgsGdalDataCollectionItem( QgsDataItem* parent, QString name, QString path, QgsGdalDataSource* dataSource )
    : QgsDataCollectionItem( parent, name, path ), mDataSource( dataSource )
{
  Q_ASSERT( mDataSource != 0 );
  mIcon = QgsLayerItem::iconRaster(); // collection, but use raster icon
  // if there are sublayers, set populated=false so item can be populated on demand
  if ( dataSource && dataSource->layerNames().count() > 0 )
  {
    mPopulated = false;
  }
  else
    mPopulated = true;
}

QgsGdalDataCollectionItem::~QgsGdalDataCollectionItem()
{
  delete mDataSource;
}

QVector<QgsDataItem*> QgsGdalDataCollectionItem::createChildren( )
{
  QgsDebugMsg( "Entered, path=" + path() );

  QVector<QgsDataItem*> children;
  foreach ( QString layerName, mDataSource->layerNames() )
  {
    QgsGdalLayerItem* item = dataItemForLayer( this, mDataSource, layerName );
    children.append( item );
  }

  return children;
}

// ---------------------------------------------------------------------------

static QString filterString;
static QStringList extensions = QStringList();
static QStringList wildcards = QStringList();

QGISEXTERN int dataCapabilities()
{
  return QgsDataProvider::File | QgsDataProvider::Dir | QgsDataProvider::Net;
}

QGISEXTERN QgsDataItem * dataItem( QString thePath, QgsDataItem* parentItem )
{
  if ( thePath.isEmpty() )
    return 0;

  QgsDebugMsgLevel( "thePath = " + thePath, 2 );

  // zip settings + info
  QSettings settings;
  QString scanZipSetting = settings.value( "/qgis/scanZipInBrowser2", "basic" ).toString();
  QString vsiPrefix = QgsVsifileDataSource::vsiPrefix( thePath );
  bool is_vsizip = ( vsiPrefix == "/vsizip/" );
  bool is_vsigzip = ( vsiPrefix == "/vsigzip/" );
  bool is_vsitar = ( vsiPrefix == "/vsitar/" );

  // should we check ext. only?
  // check if scanItemsInBrowser2 == extension or parent dir in scanItemsFastScanUris
  // TODO - do this in dir item, but this requires a way to inform which extensions are supported by provider
  // maybe a callback function or in the provider registry?
  bool scanExtSetting = false;
  if (( settings.value( "/qgis/scanItemsInBrowser2",
                        "extension" ).toString() == "extension" ) ||
      ( settings.value( "/qgis/scanItemsFastScanUris",
                        QStringList() ).toStringList().contains( parentItem->path() ) ) ||
      (( is_vsizip || is_vsitar ) && parentItem && parentItem->parent() &&
       settings.value( "/qgis/scanItemsFastScanUris",
                       QStringList() ).toStringList().contains( parentItem->parent()->path() ) ) )
  {
    scanExtSetting = true;
  }

  // get suffix, removing .gz if present
  QString tmpPath = thePath; //path used for testing, not for layer creation
  if ( is_vsigzip )
    tmpPath.chop( 3 );
  QFileInfo info( tmpPath );
  QString suffix = info.suffix().toLower();
  // extract basename with extension
  info.setFile( thePath );
  QString name = info.fileName();

  QgsDebugMsgLevel( "thePath= " + thePath + " tmpPath= " + tmpPath + " name= " + name
                    + " suffix= " + suffix + " vsiPrefix= " + vsiPrefix, 3 );

  // allow only normal files or VSIFILE items to continue
  if ( !info.isFile() && vsiPrefix == "" )
    return 0;

  // get supported extensions
  if ( extensions.isEmpty() )
  {
    buildSupportedRasterFileFilterAndExtensions( filterString, extensions, wildcards );
    QgsDebugMsgLevel( "extensions: " + extensions.join( " " ), 2 );
    QgsDebugMsgLevel( "wildcards: " + wildcards.join( " " ), 2 );
  }

  // skip *.aux.xml files (GDAL auxilary metadata files)
  // unless that extension is in the list (*.xml might be though)
  if ( thePath.endsWith( ".aux.xml", Qt::CaseInsensitive ) &&
       !extensions.contains( "aux.xml" ) )
    return 0;

  // Filter files by extension
  if ( !extensions.contains( suffix ) )
  {
    bool matches = false;
    foreach ( QString wildcard, wildcards )
    {
      QRegExp rx( wildcard, Qt::CaseInsensitive, QRegExp::Wildcard );
      if ( rx.exactMatch( info.fileName() ) )
      {
        matches = true;
        break;
      }
    }
    if ( !matches )
      return 0;
  }

  // fix vsifile path and name
  if ( vsiPrefix != "" )
  {
    // add vsiPrefix to path if needed
    if ( !thePath.startsWith( vsiPrefix ) )
      thePath = vsiPrefix + thePath;
    // if this is a /vsigzip/path_to_zip.zip/file_inside_zip remove the full path from the name
    if (( is_vsizip || is_vsitar ) && ( thePath != vsiPrefix + parentItem->path() ) )
    {
      name = thePath;
      name = name.replace( vsiPrefix + parentItem->path() + "/", "" );
    }
  }

  // return item without testing if:
  // scanExtSetting == true
  // or zipfile and scan zip == "Basic scan"
  if ( scanExtSetting ||
       (( is_vsizip || is_vsitar ) && scanZipSetting == "basic" ) )
  {
    // if this is a VRT file make sure it is raster VRT to avoid duplicates
    if ( suffix == "vrt" )
    {
      // do not print errors, but write to debug
      CPLPushErrorHandler( CPLQuietErrorHandler );
      CPLErrorReset();
      if ( ! GDALIdentifyDriver( thePath.toLocal8Bit().constData(), 0 ) )
      {
        QgsDebugMsgLevel( "Skipping VRT file because root is not a GDAL VRT", 2 );
        CPLPopErrorHandler();
        return 0;
      }
      CPLPopErrorHandler();
    }
    // add the item
    QgsDebugMsgLevel( QString( "adding item name=%1 thePath=%2" ).arg( name ).arg( thePath ), 2 );
    QgsLayerItem * item = new QgsGdalLayerItem( parentItem, name, thePath, thePath, name );
    if ( item )
      return item;
  }

  QgsDataItem* item = 0;
  QgsGdalDataSource * dataSource = new QgsGdalDataSource( thePath );

  if ( dataSource && dataSource->isValid() )
  {
    if ( dataSource->layerUris().count() == 1 )
    {
      item = dataItemForLayer( parentItem, dataSource, 0 );
      delete dataSource;
    }
    else if ( dataSource->layerUris().count() > 1 )
    {
      item = new QgsGdalDataCollectionItem( parentItem, name, thePath, dataSource );
    }
    else
    {
      QgsDebugMsg( "data source is valid but has no layers" );
      delete dataSource;
    }
  }
  else if ( dataSource )
    delete dataSource;

  return item;
}

