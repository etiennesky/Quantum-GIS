/***************************************************************************
                qgsdatasource.cpp - DataSource Interface class
                     --------------------------------------
    Date                 : 21-Nov-2012
    Copyright            : (C) 2012 by Etienne Tourigny
    email                : e.tourigny at gmail.com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsdatasource.h"

#include "qgis.h"
#include "qgsproviderregistry.h"
#include "qgslogger.h"
#include "qgsmaplayerregistry.h"
#include "qgsrasterlayer.h"
#include "qgsvectorlayer.h"
#include "qgsdataitem.h"

#include <QLibrary>

// -----------------------------------------------------------------------------
// QgsDataSource class
// -----------------------------------------------------------------------------

QgsDataSource::~QgsDataSource()
{
  foreach ( QgsDataSourceLayer* layer, mLayers.values() )
    delete layer;
}

QString QgsDataSource::uriForLayer( const QString& layerName ) const
{
  return mLayerNames.contains( layerName ) ? mLayers.value( layerName )->uri : QString();
}

QgsDataSourceLayer* QgsDataSource::dataSourceLayer( const QString& layerName ) const
{
  return mLayerNames.contains( layerName ) ? mLayers.value( layerName ) : NULL;
}

QgsDataSourceLayer* QgsDataSource::dataSourceLayer( int layerId ) const
{
  return mLayerNames.count() >= layerId ? mLayers.value( mLayerNames.at( layerId ) ) : NULL;
}

QStringList QgsDataSource::layerUris( ) const
{
  QStringList list;
  foreach ( QString layerName, mLayerNames )
  {
    list << mLayers.value( layerName )->uri;
  }
  return list;
}

QStringList QgsDataSource::layerInfoHeaders( ) const
{
  return QStringList() << QObject::tr( "ID" ) << QObject::tr( "Layer name" );
}

QStringList QgsDataSource::layerInfo( ) const
{
  QStringList list;
  foreach ( QString layerName, mLayerNames )
  {
    list << mLayers.value( layerName )->info;
  }
  return list;
}

QList<QgsMapLayer *> QgsDataSource::addLayers( const QStringList& layers, bool addToRegistry ) const
{
  // TODO add gui warning
  if ( !layers.isEmpty() )
    QgsDebugMsg( QString( "passed %1 layers: {%2}" ).arg( layers.count() ).arg( layers.join( " " ) ) );

  QList<QgsMapLayer *> myList;
  QString name;

  // add layers in reverse order so they appear in the right order in the layer dock
  // for( int i = 0; i < layers.count(); i++ )
  for ( int i = layers.count() - 1 ; i >= 0; i-- )
  {
    if ( ! mLayerNames.contains( layers[i] ) )
      continue;

    QgsDataSourceLayer* sourceLayer = mLayers.value( layers[i] );
    // depending in type/provider, add raster or vector layer
    // if ( mProviderKey == "gdal" )
    if ( sourceLayer && sourceLayer->type == QgsMapLayer::RasterLayer )
    {
      QgsRasterLayer *layer = new QgsRasterLayer( sourceLayer->uri, sourceLayer->layerName, sourceLayer->providerKey );
      if ( ! layer )
      {
        // XXX insert meaningful whine to the user here; although be
        // XXX mindful that a null layer may mean exhausted memory resources
        continue;
      }
      if ( layer->isValid() )
      {
        myList << layer;
      }
      else
      {
        // TODO fix this - provider is not deleted in ~QgsRasterLayer()
        delete layer->dataProvider();
        delete layer;
      }
    }
    if ( sourceLayer && sourceLayer->type == QgsMapLayer::VectorLayer )
    {
      // create the layer
      QgsVectorLayer *layer = new QgsVectorLayer( sourceLayer->uri, sourceLayer->layerName, sourceLayer->providerKey );

      if ( layer->isValid() )
      {
        myList << layer;
      }
      else
      {
        delete layer;
      }
      // statusBar()->showMessage( mMapCanvas->extent().toString( 2 ) );
    }
  }

  if ( addToRegistry )
    QgsMapLayerRegistry::instance()->addMapLayers( myList );
  return myList;
}

// -----------------------------------------------------------------------------
// QgsVsifileDataSource class
// -----------------------------------------------------------------------------

QgsVsifileDataSource::QgsVsifileDataSource( QString baseUri, QgsMapLayer::LayerType type )
    : QgsDataSource( baseUri, "vsifile" ), mLayerType( type )
{
  mDataSourceType = VsifileDataSource;

  QgsDebugMsg( QString( "baseUri= %1 providerKey= %2" ).arg( baseUri ) );

  mValid = loadZipItems();
}

// copied from QgisApp::askUserForZipItemLayers
bool QgsVsifileDataSource::loadZipItems()
{
  bool ok = false;
  QVector<QgsDataItem*> childItems;
  QgsZipItem *zipItem = 0;
  // QSettings settings;
  // QString promptLayers = settings.value( "/qgis/promptForRasterSublayers", "yes" ).toString();

  zipItem = new QgsZipItem( 0, mBaseUri, mBaseUri );
  if ( ! zipItem )
    return false;

  zipItem->populate();
  QgsDebugMsg( QString( "baseUri= %1 got zipitem with %2 children" ).arg( mBaseUri ).arg( zipItem->rowCount() ) );

  // if 1 or 0 child found, exit so a normal item is created by gdal or ogr provider
  if ( zipItem->rowCount() <= 1 )
  {
    delete zipItem;
    return false;
  }

  // TODO optimize when promptLayers = "all" or "no" or scanZipInBrowser2= "no"
  // if scanZipBrowser == no: skip to the next file
  // if ( settings.value( "/qgis/scanZipInBrowser2", "basic" ).toString() == "no" )
  // {
  //   return false;
  // }
  // // if promptLayers=Load all, load all layers without prompting
  // if ( promptLayers == "all" )
  // {
  //   childItems = zipItem->children();
  // }
  // exit if promptLayers=no
  // else if ( promptLayers == "no" )
  // {
  //   delete zipItem;
  //   return false;
  // }

  //   childItems = zipItem->children();

  QStringList layers;
  QString info;
  QgsMapLayer::LayerType type;
  for ( int i = 0; i < zipItem->children().size(); i++ )
  {
    QgsDataItem *item = zipItem->children()[i];
    QgsLayerItem *layerItem = dynamic_cast<QgsLayerItem *>( item );
    QgsDebugMsgLevel( QString( "item path=%1 provider=%2" ).arg( item->path() ).arg( layerItem->providerKey() ), 2 );
    if ( ! item || ! layerItem )
      continue;

    if ( layerItem->providerKey() == "gdal" && mLayerType != QgsMapLayer::VectorLayer )
    {
      info = QString( "%1:%2:%3" ).arg( i ).arg( item->name() ).arg( QObject::tr( "Raster" ) );
      type = QgsMapLayer::RasterLayer;
    }
    else if ( layerItem->providerKey() == "ogr" && mLayerType != QgsMapLayer::RasterLayer )
    {
      info = QString( "%1:%2:%3" ).arg( i ).arg( item->name() ).arg( QObject::tr( "Vector" ) );
      type = QgsMapLayer::VectorLayer;
    }
    else
    {
      continue; //print an error?
    }
    mLayerNames << item->name();
    // create QgsDataSourceLayer, with baseName as layerName
    mLayers[ item->name()] =
      new QgsDataSourceLayer( item->name(), QFileInfo( item->name() ).baseName(),
                              item->path(), item->path(), type, layerItem->providerKey(), info );
  }

  if ( childItems.isEmpty() )
  {
    // TODO make sure dialog doesn't popup again (#6225) - hopefully this doesn't create other trouble
    ok = true;
  }

  delete zipItem;

  if ( !mLayerNames.isEmpty() )
    ok = true;
  return ok;
}

QgsVsifileDataSource::~QgsVsifileDataSource()
{}

QStringList QgsVsifileDataSource::layerInfoHeaders( ) const
{
  return QStringList() << QObject::tr( "ID" ) << QObject::tr( "Layer name" ) << QObject::tr( "Data type" );
}

QgsVsifileDataSource * QgsVsifileDataSource::dataSource( const QString& uri, QgsMapLayer::LayerType type )
{
  QgsVsifileDataSource* dataSource = new QgsVsifileDataSource( uri, type );
  if ( ! dataSource->isValid() )
  {
    delete dataSource;
    dataSource = 0;
  }
  return dataSource;
}


QString QgsVsifileDataSource::vsiPrefix( QString path )
{
  if ( path.startsWith( "/vsizip/", Qt::CaseInsensitive ) ||
       path.endsWith( ".zip", Qt::CaseInsensitive ) )
    return "/vsizip/";
  else if ( path.startsWith( "/vsitar/", Qt::CaseInsensitive ) ||
            path.endsWith( ".tar", Qt::CaseInsensitive ) ||
            path.endsWith( ".tar.gz", Qt::CaseInsensitive ) ||
            path.endsWith( ".tgz", Qt::CaseInsensitive ) )
    return "/vsitar/";
  else if ( path.startsWith( "/vsigzip/", Qt::CaseInsensitive ) ||
            path.endsWith( ".gz", Qt::CaseInsensitive ) )
    return "/vsigzip/";
  else
    return "";
}

// -----------------------------------------------------------------------------
// static functions
// -----------------------------------------------------------------------------

// typedef for the QgsDataProvider class factory
typedef QgsDataSource * dataSourceFunction_t( const QString & );

// private function which does the actual work for one provider
QgsDataSource* _open( QString baseUri, QgsMapLayer::LayerType type, QString providerKey )
{
  QgsDebugMsg( QString( "baseUri= %1 type= %2 providerKey= %3" ).arg( baseUri ).arg(( int ) type ).arg( providerKey ) );

  // if vsifile "provider" try QgsVsifileDataSource, it's not a real provider
  if ( providerKey == "vsifile" )
  {
    return QgsVsifileDataSource::dataSource( baseUri, type );
  }

  // skip provider if incompatible type passed
  // TODO add infomation if providers support raster/vector in QgsProviderMetadata or QgsDataProvider
  if ( type == QgsMapLayer::RasterLayer )
  {
    if ( providerKey == "ogr" )
      return 0;
  }
  else if ( type == QgsMapLayer::VectorLayer )
  {
    if ( providerKey == "gdal" )
      return 0;
  }

  // load the plugin
  QString lib = QgsProviderRegistry::instance()->library( providerKey );

  // load the data provider
  QLibrary* myLib = new QLibrary( lib );

  QgsDebugMsg( "Library name is " + myLib->fileName() );

  bool loaded = myLib->load();

  if ( loaded )
  {
    QgsDebugMsg( "Loaded data provider library" );
    QgsDebugMsg( "Attempting to resolve the dataSource function" );

    dataSourceFunction_t * dataSourceFunc =
      ( dataSourceFunction_t * ) cast_to_fptr( myLib->resolve( "dataSource" ) );

    if ( dataSourceFunc )
    {
      QgsDebugMsg( "Getting pointer to a dataSource object from the library" );

      //XXX - This was a dynamic cast but that kills the Windows
      //      version big-time with an abnormal termination error
      //      QgsDataProvider* dataProvider = (QgsDataProvider*)
      //      (dataSource((const char*)(dataSource.utf8())));

      QgsDataSource * dataSource = ( *dataSourceFunc )( baseUri );

      if ( dataSource )
      {
        QgsDebugMsg( "Instantiated the dataSource object" );
        // QgsDebugMsg( "provider name: " + dataProvider->name() );
        return dataSource;
      }
    }
  }
  return NULL;
}

QgsDataSource* QgsDataSource::open( QString baseUri, QgsMapLayer::LayerType type, QStringList providerKeys )
{
  QgsDebugMsg( QString( "baseUri= %1 type= %2 providerKeys= {%3}" ).arg( baseUri ).arg(( int ) type ).arg( providerKeys.join( " " ) ) );

  // first try vsifile prefix, if it fails try actual provider(s)
  if ( ! providerKeys.contains( "vsifile" ) )
  {
    // test if file should be treated as a /vsizip/ or /vsitar/
    QString vsiPrefix = QgsVsifileDataSource::vsiPrefix( baseUri );
    if ( vsiPrefix == "/vsizip/" || vsiPrefix == "/vsitar/" )
    {
      QgsDataSource * dataSource = QgsVsifileDataSource::dataSource( baseUri, type );
      if ( dataSource )
        return dataSource;
    }
  }

  // loop over all available providers if none are given
  if ( providerKeys.isEmpty() ||
       ( providerKeys.count() == 1 && providerKeys[0].isEmpty() ) )
  {
    providerKeys = QgsProviderRegistry::instance()->providerList();
    QgsDebugMsg( QString( "providerKeys was empty, now providerKeys= {%1}" ).arg( providerKeys.join( " " ) ) );
  }

  // move gdal and ogr to front (if available)
  // not sure if this is good practice, needs further testing
  foreach ( QString provider, QStringList() << "ogr" << "gdal" )
  {
    int index = providerKeys.indexOf( provider );
    if ( index != -1 )
    {
      providerKeys.removeAt( index );
      providerKeys.prepend( provider );
    }
  }
  // add bogus vsifile provider
  // providerKeys.prepend( "vsifile" );

  QgsDebugMsg( QString( "now %1 providerKeys: {%2}" ).arg( providerKeys.count() ).arg( providerKeys.join( " " ) ) );
  // loop over all providers, return first valid dataSource
  QgsDataSource* dataSource = 0;
  foreach ( QString providerKey, providerKeys )
  {
    if ( providerKey.isEmpty() )
      continue;
    QgsDebugMsg( QString( "trying with providerKey= %1" ).arg( providerKey ) );

    // call private _open function
    dataSource = _open( baseUri, type, providerKey );
    if ( dataSource && dataSource->isValid() )
      return dataSource;
    else if ( dataSource )
      delete dataSource;
  }

  return NULL;
}
