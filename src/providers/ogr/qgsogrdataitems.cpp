/***************************************************************************
                             qgsogrdataitems.cpp
                             -------------------
    begin                : 2011-04-01
    copyright            : (C) 2011 Radim Blazek
    email                : radim dot blazek at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsogrdataitems.h"

#include "qgslogger.h"
#include "qgsmessagelog.h"

#include <QFileInfo>
#include <QTextStream>
#include <QSettings>

#include <ogr_srs_api.h>
#include <cpl_error.h>
#include <cpl_conv.h>

// these are defined in qgsogrprovider.cpp
QGISEXTERN QStringList fileExtensions();
QGISEXTERN QStringList wildcards();


QgsOgrLayerItem::QgsOgrLayerItem( QgsDataItem* parent, QString name, QString path,
                                  QString uri, LayerType layerType, QString layerName )
    : QgsLayerItem( parent, name, path, uri, layerType, "ogr" ), mLayerName( layerName )
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

QgsOgrLayerItem::~QgsOgrLayerItem()
{
}

QgsLayerItem::Capability QgsOgrLayerItem::capabilities()
{
  QgsDebugMsg( "mPath = " + mPath );
  OGRRegisterAll();
  OGRSFDriverH hDriver;
  OGRDataSourceH hDataSource = OGROpen( TO8F( mPath ), true, &hDriver );

  if ( !hDataSource )
    return NoCapabilities;

  QString  driverName = OGR_Dr_GetName( hDriver );
  OGR_DS_Destroy( hDataSource );

  if ( driverName == "ESRI Shapefile" )
    return SetCrs;

  return NoCapabilities;
}

bool QgsOgrLayerItem::setCrs( QgsCoordinateReferenceSystem crs )
{
  QgsDebugMsg( "mPath = " + mPath );
  OGRRegisterAll();
  OGRSFDriverH hDriver;
  OGRDataSourceH hDataSource = OGROpen( TO8F( mPath ), true, &hDriver );

  if ( !hDataSource )
    return false;

  QString  driverName = OGR_Dr_GetName( hDriver );
  OGR_DS_Destroy( hDataSource );

  // we are able to assign CRS only to shapefiles :-(
  if ( driverName == "ESRI Shapefile" )
  {
    QString layerName = mPath.left( mPath.indexOf( ".shp", Qt::CaseInsensitive ) );
    QString wkt = crs.toWkt();

    // save ordinary .prj file
    OGRSpatialReferenceH hSRS = OSRNewSpatialReference( wkt.toLocal8Bit().data() );
    OSRMorphToESRI( hSRS ); // this is the important stuff for shapefile .prj
    char* pszOutWkt = NULL;
    OSRExportToWkt( hSRS, &pszOutWkt );
    QFile prjFile( layerName + ".prj" );
    if ( prjFile.open( QIODevice::WriteOnly ) )
    {
      QTextStream prjStream( &prjFile );
      prjStream << pszOutWkt << endl;
      prjFile.close();
    }
    else
    {
      QgsMessageLog::logMessage( tr( "Couldn't open file %1.prj" ).arg( layerName ), tr( "OGR" ) );
      return false;
    }
    OSRDestroySpatialReference( hSRS );
    CPLFree( pszOutWkt );

    // save qgis-specific .qpj file (maybe because of better wkt compatibility?)
    QFile qpjFile( layerName + ".qpj" );
    if ( qpjFile.open( QIODevice::WriteOnly ) )
    {
      QTextStream qpjStream( &qpjFile );
      qpjStream << wkt.toLocal8Bit().data() << endl;
      qpjFile.close();
    }
    else
    {
      QgsMessageLog::logMessage( tr( "Couldn't open file %1.qpj" ).arg( layerName ), tr( "OGR" ) );
      return false;
    }

    return true;
  }

  // It it is impossible to assign a crs to an existing layer
  // No OGR_L_SetSpatialRef : http://trac.osgeo.org/gdal/ticket/4032
  return false;
}

// -------


static QgsOgrLayerItem* dataItemForLayer( QgsDataItem* parentItem, QgsOgrDataSource* dataSource, const QString& layerName )
{
  QgsDataSourceLayer* dataLayer = dataSource->dataSourceLayer( layerName );
  if ( ! dataLayer )
    return 0;

  QStringList info = dataLayer->info.split( ":" );
  QString layerTypeName = info.count() >= 3 ? info.at( 3 ) : "";
  QgsLayerItem::LayerType layerType =  QgsOgrProvider::getGeomType( layerTypeName );

  QgsDebugMsgLevel( QString( "adding QgsOgrLayerItem [%1] [%2] [%3] [%4] " ).arg( dataLayer->name ).arg( dataLayer->path ).arg( dataLayer->uri ).arg( layerType ), 2 );

  return new QgsOgrLayerItem( parentItem, dataLayer->name, dataLayer->path, dataLayer->uri, layerType );
}

static QgsOgrLayerItem* dataItemForLayer( QgsDataItem* parentItem, QgsOgrDataSource* dataSource, int layerId )
{
  return dataItemForLayer( parentItem, dataSource, dataSource->layerNames().at( layerId ) );
}


// ----

QgsOgrDataCollectionItem::QgsOgrDataCollectionItem( QgsDataItem* parent, QString name, QString path, QgsOgrDataSource* dataSource )
    : QgsDataCollectionItem( parent, name, path ), mDataSource( dataSource )
{
  Q_ASSERT( mDataSource != 0 );
}

QgsOgrDataCollectionItem::~QgsOgrDataCollectionItem()
{
  delete mDataSource;
}

QVector<QgsDataItem*> QgsOgrDataCollectionItem::createChildren()
{
  QgsDebugMsg( "Entered, path=" + path() );

  QVector<QgsDataItem*> children;
  for ( int i = 0; i < mDataSource->layerNames().count(); i++ )
  {
    QgsOgrLayerItem* item = dataItemForLayer( this, mDataSource, i );
    children.append( item );
  }

  return children;
}

// ---------------------------------------------------------------------------

QGISEXTERN int dataCapabilities()
{
  return  QgsDataProvider::File | QgsDataProvider::Dir;
}

QGISEXTERN QgsDataItem * dataItem( QString thePath, QgsDataItem* parentItem )
{
  if ( thePath.isEmpty() )
    return 0;

  QgsDebugMsgLevel( "thePath: " + thePath, 2 );

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
  if ( settings.value( "/qgis/scanItemsInBrowser2",
                       "extension" ).toString() == "extension" )
    scanExtSetting = true;
  else if ( settings.value( "/qgis/scanItemsFastScanUris",
                            QStringList() ).toStringList().contains( parentItem->path() ) )
    scanExtSetting = true;

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

  QStringList myExtensions = fileExtensions();

  // skip *.aux.xml files (GDAL auxilary metadata files) and .shp.xml files (ESRI metadata)
  // unless that extension is in the list (*.xml might be though)
  if ( thePath.endsWith( ".aux.xml", Qt::CaseInsensitive ) &&
       !myExtensions.contains( "aux.xml" ) )
    return 0;
  if ( thePath.endsWith( ".shp.xml", Qt::CaseInsensitive ) &&
       !myExtensions.contains( "shp.xml" ) )
    return 0;

  // We have to filter by extensions, otherwise e.g. all Shapefile files are displayed
  // because OGR drive can open also .dbf, .shx.
  if ( myExtensions.indexOf( suffix ) < 0 )
  {
    bool matches = false;
    foreach ( QString wildcard, wildcards() )
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

  // .dbf should probably appear if .shp is not present
  if ( suffix == "dbf" )
  {
    QString pathShp = thePath.left( thePath.count() - 4 ) + ".shp";
    if ( QFileInfo( pathShp ).exists() )
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
  // zipfile and scan zip == "Basic scan"
  // not zipfile and scanExtSetting=true
  if ((( is_vsizip || is_vsitar ) && scanZipSetting == "basic" ) ||
      ( !is_vsizip && !is_vsitar && scanExtSetting ) )
  {
    // if this is a VRT file make sure it is vector VRT to avoid duplicates
    if ( suffix == "vrt" )
    {
      OGRSFDriverH hDriver = OGRGetDriverByName( "VRT" );
      if ( hDriver )
      {
        // do not print errors, but write to debug
        CPLPushErrorHandler( CPLQuietErrorHandler );
        CPLErrorReset();
        OGRDataSourceH hDataSource = OGR_Dr_Open( hDriver, thePath.toLocal8Bit().constData(), 0 );
        CPLPopErrorHandler();
        if ( ! hDataSource )
        {
          QgsDebugMsgLevel( "Skipping VRT file because root is not a OGR VRT", 2 );
          return 0;
        }
        OGR_DS_Destroy( hDataSource );
      }
    }
    // add the item
    // TODO: how to handle collections?
    QgsLayerItem * item = new QgsOgrLayerItem( parentItem, name, thePath, thePath, QgsLayerItem::Vector );
    if ( item )
      return item;
  }

  QgsDataItem* item = 0;
  QgsOgrDataSource * dataSource = new QgsOgrDataSource( thePath );

  if ( dataSource && dataSource->isValid() )
  {
    if ( dataSource->layerUris().count() == 1 )
    {
      item = dataItemForLayer( parentItem, dataSource, 0 );
      delete dataSource;
    }
    else if ( dataSource->layerUris().count() > 1 )
    {
      item = new QgsOgrDataCollectionItem( parentItem, name, thePath, dataSource );
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
