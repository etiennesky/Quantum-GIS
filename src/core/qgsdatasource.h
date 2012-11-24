/***************************************************************************
                qgsdatasource.h - DataSource Interface class
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

#ifndef QQGSDATASOURCE_H
#define QQGSDATASOURCE_H

#include "qgsmaplayer.h"

#include <QStringList>

/** \ingroup core
 */

// perhaps this class needs to be exported?
class QgsDataSourceLayer
{
    //friend class QgsDataSource;

  public:

    QgsDataSourceLayer() { valid = false; }
    QgsDataSourceLayer( const QString& n, const QString& ln, const QString& u, const QString& p,
                        QgsMapLayer::LayerType t, const QString& pK, const QString& i )
        : name( n ), layerName( ln ), uri( u ), path( p ), type( t ), providerKey( pK ), info( i ), valid( true )
    {}

    //protected:

    QString name; //name (coming from provider, with extension)
    QString layerName; //layer name (without extension)
    QString uri;
    QString path; // this for qgsdataitem, not sure of the reason for uri/path
    QgsMapLayer::LayerType type;
    QString providerKey;
    QString info;
    bool valid;
};

class CORE_EXPORT QgsDataSource
{
  public:

    /** Layers enum defining the types of layers that can be added to a map */
    enum DataSourceType
    {
      VectorDataSource,
      RasterDataSource,
      VsifileDataSource,
      OtherDataSource
    };

    QgsDataSource( QString baseUri, QString providerKey )
        : mBaseUri( baseUri ), mProviderKey( providerKey ),
        mDataSourceType( OtherDataSource ), mValid( false )
    { }
    ~QgsDataSource();

    /* static method to create a QgsDataSource object from all available providers */
    static QgsDataSource* open( QString baseUri, QgsMapLayer::LayerType type )
    { return open( baseUri, type, QStringList() ); }
    /* static method to create a QgsDataSource object from the given provider */
    static QgsDataSource* open( QString baseUri, QgsMapLayer::LayerType type, QString providerKey )
    { return open( baseUri, type, QStringList( providerKey ) ); }
    /* static method to create a QgsDataSource object, trying the given providers one by one */
    static QgsDataSource* open( QString baseUri, QgsMapLayer::LayerType type, QStringList providerKeys );

    QString providerKey() const { return mProviderKey; }
    DataSourceType dataSourceType() const { return mDataSourceType; }

    // get list of sub-layers
    virtual QStringList layerNames() const { return mLayerNames; }// = 0;
    // get list of sub-layer uris
    virtual QStringList layerUris() const; // = 0;
    // return full provider URI for a particular sub-layer
    virtual QString uriForLayer( const QString& layerName ) const; // = 0;
    QgsDataSourceLayer* dataSourceLayer( const QString& layerName ) const;
    QgsDataSourceLayer* dataSourceLayer( int layerId ) const;
    // get layer info headers
    virtual QStringList layerInfoHeaders( ) const;
    // get layer info for QgsSublayersDialog, separated by ":"
    virtual QStringList layerInfo( ) const; // = 0;
    virtual bool isValid() { return mValid; }

    /* creates layers from URIs and optionally adds them to map registry
     * does not update/freeze the canvas */
    QList<QgsMapLayer *> addLayers( const QStringList& layers, bool addToRegistry = true ) const;


  protected:

    QString mBaseUri;
    QString mProviderKey;
    DataSourceType mDataSourceType;
    QStringList mLayerNames;
    QMap< QString, QgsDataSourceLayer* > mLayers;
    bool mValid;
};


/* A wrapper for the gdal/ogr vsifile mechanism */
// TODO limit QgsVsifileDataSource to specific providers
/* Perhaps should be in its own provider, but that would be a bit overkill */
class QgsVsifileDataSource : public QgsDataSource
{
  public:

    QgsVsifileDataSource( QString baseUri, QgsMapLayer::LayerType type );
    virtual ~QgsVsifileDataSource();

    virtual QStringList layerInfoHeaders( ) const;

    // returns a vsifile prefix (/vsizip/ , /vsitar/ or /vsigzip/ ) corresponding to the given uri
    static QString vsiPrefix( QString uri );

    static QgsVsifileDataSource * dataSource( const QString& uri, QgsMapLayer::LayerType type );

  protected:
    bool loadZipItems();

    QgsMapLayer::LayerType mLayerType;
};

#endif
