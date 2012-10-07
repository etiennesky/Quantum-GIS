
/**  Raster file writer */

class QgsRasterFileWriter
{
%TypeHeaderCode
#include <qgsrasterfilewriter.h>
#include <qgsrasterdataprovider.h>
%End
  public:
    enum WriterError
    {
      NoError = 0,
      SourceProviderError = 1,
      DestProviderError = 2,
      CreateDatasourceError = 3,
      WriteError = 4,
      // Internal error if a value used for 'no data' was found in input
      NoDataConflict = 5
    };

    QgsRasterFileWriter( const QString& outputUrl );
    ~QgsRasterFileWriter();

    /**Write raster file
        @param pipe raster pipe
        @param nCols number of output columns
        @param nRows number of output rows (or -1 to automatically calculate row number to have square pixels)
        @param outputExtent extent to output
        @param crs crs to reproject to
        @param p dialog to show progress in */
    WriterError writeRaster( const QgsRasterPipe* pipe, int nCols, int nRows, QgsRectangle outputExtent,
                             const QgsCoordinateReferenceSystem& crs, QProgressDialog* p = 0 );

    QString outputProviderKey() const;
    void setOutputProviderKey( const QString& key );

    QString outputFormat() const;
    void setOutputFormat( const QString& format );

    QStringList createOptions() const;
    void setCreateOptions( const QStringList& list );

    bool tiledMode() const;
    void setTiledMode( bool t );

    int maxTileWidth() const;
    void setMaxTileWidth( int w );

    int maxTileHeight() const;
    void setMaxTileHeight( int h );

    QgsRasterDataProvider::RasterBuildPyramids buildPyramidsFlag() const;
    void setBuildPyramidsFlag( QgsRasterDataProvider::RasterBuildPyramids f );

    QList< int > pyramidsList() const;
    void setPyramidsList( const QList< int > & list );

    QString pyramidsResampling() const;
    void setPyramidsResampling( const QString & str );

    QgsRasterDataProvider::RasterPyramidsFormat pyramidsFormat() const;
    void setPyramidsFormat( QgsRasterDataProvider::RasterPyramidsFormat f );

    QStringList pyramidsConfigOptions() const;
    void setPyramidsConfigOptions( const QStringList& list );
};

