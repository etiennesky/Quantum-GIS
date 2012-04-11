#include "qgsbrowserdockwidget.h"

#include <QHeaderView>
#include <QTreeView>
#include <QMenu>
#include <QSettings>
#include <QToolButton>
#include <QSortFilterProxyModel>

#include "qgsbrowsermodel.h"
#include "qgslogger.h"
#include "qgsmaplayerregistry.h"
#include "qgsrasterlayer.h"
#include "qgsvectorlayer.h"
#include "qgisapp.h"

// browser layer properties dialog
#include "qgsapplication.h"
#include "qgsmapcanvas.h"
#include <ui_qgsbrowserlayerpropertiesbase.h>
#include <ui_qgsbrowserdirectorypropertiesbase.h>


#include <QDragEnterEvent>
/**
Utility class for correct drag&drop handling.

We want to allow user to drag layers to qgis window. At the same time we do not
accept drops of the items on our view - but if we ignore the drag enter action
then qgis application consumes the drag events and it is possible to drop the
items on the tree view although the drop is actually managed by qgis app.
 */
class QgsBrowserTreeView : public QTreeView
{
  public:
    QgsBrowserTreeView( QWidget* parent ) : QTreeView( parent )
    {
      setDragDropMode( QTreeView::DragDrop ); // sets also acceptDrops + dragEnabled
      setSelectionMode( QAbstractItemView::ExtendedSelection );
      setContextMenuPolicy( Qt::CustomContextMenu );
      setHeaderHidden( true );
      setDropIndicatorShown( true );
    }

    void dragEnterEvent( QDragEnterEvent* e )
    {
      // accept drag enter so that our widget will not get ignored
      // and drag events will not get passed to QgisApp
      e->accept();
    }
    void dragMoveEvent( QDragMoveEvent* e )
    {
      // do not accept drops above/below items
      /*if ( dropIndicatorPosition() != QAbstractItemView::OnItem )
      {
        QgsDebugMsg("drag not on item");
        e->ignore();
        return;
      }*/

      QTreeView::dragMoveEvent( e );

      if ( !e->provides( "application/x-vnd.qgis.qgis.uri" ) )
      {
        e->ignore();
        return;
      }
    }
};

class QgsBrowserTreeFilterProxyModel : public QSortFilterProxyModel
{
  public:

    QgsBrowserTreeFilterProxyModel( QObject *parent )
        : QSortFilterProxyModel( parent ), mModel( 0 ),
        mFilter( "" ), mPatternSyntax( QRegExp::Wildcard )
    {
      setDynamicSortFilter( true );
    }

    void setBrowserModel( QgsBrowserModel* model )
    {
      mModel = model;
      setSourceModel( model );
    }

    void setFilterSyntax( const QRegExp::PatternSyntax & syntax )
    {
      QgsDebugMsg( QString( "syntax = %1" ).arg(( int ) mPatternSyntax ) );
      if ( mPatternSyntax == syntax )
        return;
      mPatternSyntax = syntax;
      updateFilter();
    }

    void setFilter( const QString & filter )
    {
      QgsDebugMsg( QString( "filter = %1" ).arg( mFilter ) );
      if ( mFilter == filter )
        return;
      mFilter = filter;
      updateFilter();
    }

    void updateFilter( )
    {
      QgsDebugMsg( QString( "filter = %1 syntax = %2" ).arg( mFilter ).arg(( int ) mPatternSyntax ) );
      mREList.clear();
      if ( mPatternSyntax == QRegExp::Wildcard ||
           mPatternSyntax == QRegExp::WildcardUnix )
      {
        foreach( QString f, mFilter.split( "|" ) )
        {
          QRegExp rx( f.trimmed() );
          rx.setPatternSyntax( mPatternSyntax );
          mREList.append( rx );
        }
      }
      else
      {
        QRegExp rx( mFilter.trimmed() );
        rx.setPatternSyntax( mPatternSyntax );
        mREList.append( rx );
      }
      invalidateFilter();
    }

  protected:

    QgsBrowserModel* mModel;
    QString mFilter; //filter string provided
    QVector<QRegExp> mREList; //list of filters, seperated by "|"
    QRegExp::PatternSyntax mPatternSyntax;

    bool filterAcceptsString( const QString & value ) const
    {
      // return ( filterRegExp().exactMatch( fileInfo.fileName() ) );
      if ( mPatternSyntax == QRegExp::Wildcard ||
           mPatternSyntax == QRegExp::WildcardUnix )
      {
        foreach( QRegExp rx, mREList )
        {
          QgsDebugMsg( QString( "value: [%1] rx: [%2] match: %3" ).arg( value ).arg( rx.pattern() ).arg( rx.exactMatch( value ) ) );
          if ( rx.exactMatch( value ) )
            return true;
        }
      }
      else
      {
        foreach( QRegExp rx, mREList )
        {
          QgsDebugMsg( QString( "value: [%1] rx: [%2] match: %3" ).arg( value ).arg( rx.pattern() ).arg( rx.indexIn( value ) ) );
          QRegExp rx2( "\\b(mail|letter|correspondence)\\b" );
          QgsDebugMsg( QString( "value: [%1] rx2: [%2] match: %3" ).arg( value ).arg( rx2.pattern() ).arg( rx2.indexIn( value ) ) );
          QgsDebugMsg( QString( "T1 %1" ).arg( rx2.indexIn( "I sent you an email" ) ) );     // returns -1 (no match)
          QgsDebugMsg( QString( "T2 %2" ).arg( rx2.indexIn( "Please write the letter" ) ) );     // returns -1 (no match)
          QgsDebugMsg( QString( "T3 %2" ).arg( rx.indexIn( "Please write the letter" ) ) );     // returns -1 (no match)

          if ( rx.indexIn( value ) != -1 )
            return true;
        }
      }
      return false;
    }

    bool filterAcceptsRow( int sourceRow,
                           const QModelIndex &sourceParent ) const
    {
      // if ( filterRegExp().pattern() == QString( "" ) ) return true;
      if ( mFilter == "" ) return true;

      QModelIndex index = sourceModel()->index( sourceRow, 0, sourceParent );
      QgsDataItem* item = mModel->dataItem( index );
      QgsDataItem* parentItem = mModel->dataItem( sourceParent );

      // accept "invalid" items and data collections
      if ( ! item )
        return true;
      if ( qobject_cast<QgsDataCollectionItem*>( item ) )
        return true;

      // filter layer items - this could be delegated to the providers but a little overkill
      if ( parentItem && qobject_cast<QgsLayerItem*>( item ) )
      {
        // filter normal files by extension
        if ( qobject_cast<QgsDirectoryItem*>( parentItem ) )
        {
          QFileInfo fileInfo( item->path() );
          return filterAcceptsString( fileInfo.fileName() );
        }
        // filter other items (postgis, etc.) by name
        else if ( qobject_cast<QgsDataCollectionItem*>( parentItem ) )
        {
          return filterAcceptsString( item->name() );
        }
      }

      // accept anything else
      return true;
    }

};
QgsBrowserDockWidget::QgsBrowserDockWidget( QWidget * parent ) :
    QDockWidget( parent ), mModel( NULL ), mProxyModel( NULL )
{
  setupUi( this );

  setWindowTitle( tr( "Browser" ) );

  mBrowserView = new QgsBrowserTreeView( this );
  mLayoutBrowser->addWidget( mBrowserView );

  mBtnRefresh->setIcon( QgisApp::instance()->getThemeIcon( "mActionRefresh.png" ) );
  mBtnAddLayers->setIcon( QgisApp::instance()->getThemeIcon( "mActionAdd.png" ) );
  mBtnCollapse->setIcon( QgisApp::instance()->getThemeIcon( "mActionCollapseTree.png" ) );

  mWidgetFilter->hide();
  // icons from http://www.fatcow.com/free-icons License: CC Attribution 3.0
  mBtnFilterShow->setIcon( QgisApp::instance()->getThemeIcon( "mActionFilter.png" ) );
  mBtnFilter->setIcon( QgisApp::instance()->getThemeIcon( "mActionFilter.png" ) );
  mBtnFilterClear->setIcon( QgisApp::instance()->getThemeIcon( "mActionFilterDelete.png" ) );

  QMenu* menu = new QMenu( this );
  menu->setSeparatorsCollapsible( false );
  mBtnFilterOptions->setMenu( menu );
  QActionGroup* group = new QActionGroup( menu );
  QAction* action = new QAction( tr( "Filter Pattern Syntax" ), group );
  action->setSeparator( true );
  menu->addAction( action );
  // group->addAction( action );
  action = new QAction( tr( "Wildcard(s)" ), group );
  action->setData( QVariant(( int ) QRegExp::Wildcard ) );
  action->setCheckable( true );
  action->setChecked( true );
  menu->addAction( action );
  // group->addAction( action );
  // menu->addSeparator()->setText( tr( "Pattern Syntax" ) );
  action = new QAction( tr( "Regular Expression" ), group );
  action->setData( QVariant(( int ) QRegExp::RegExp ) );
  action->setCheckable( true );
  menu->addAction( action );
  // group->addAction( action );

  connect( mBtnRefresh, SIGNAL( clicked() ), this, SLOT( refresh() ) );
  connect( mBtnAddLayers, SIGNAL( clicked() ), this, SLOT( addSelectedLayers() ) );
  connect( mBtnCollapse, SIGNAL( clicked() ), mBrowserView, SLOT( collapseAll() ) );
  connect( mBtnFilterShow, SIGNAL( toggled( bool ) ), this, SLOT( showFilterWidget( bool ) ) );
  connect( mBtnFilter, SIGNAL( clicked() ), this, SLOT( setFilter() ) );
  connect( mLeFilter, SIGNAL( returnPressed() ), this, SLOT( setFilter() ) );
  connect( mBtnFilterClear, SIGNAL( clicked() ), this, SLOT( clearFilter() ) );
  connect( group, SIGNAL( triggered( QAction * ) ), this, SLOT( setFilterSyntax( QAction * ) ) );

  connect( mBrowserView, SIGNAL( customContextMenuRequested( const QPoint & ) ), this, SLOT( showContextMenu( const QPoint & ) ) );
  connect( mBrowserView, SIGNAL( doubleClicked( const QModelIndex& ) ), this, SLOT( addLayerAtIndex( const QModelIndex& ) ) );

}

void QgsBrowserDockWidget::showEvent( QShowEvent * e )
{
  // delayed initialization of the model
  if ( mModel == NULL )
  {
    mModel = new QgsBrowserModel( mBrowserView );

    bool useFilter = true;
    if ( useFilter ) // enable proxy model
    {
      // mBrowserView->setModel( mModel );
      mProxyModel = new QgsBrowserTreeFilterProxyModel( this );
      mProxyModel->setBrowserModel( mModel );
      mBrowserView->setModel( mProxyModel );
    }
    else
    {
      mBrowserView->setModel( mModel );
    }
    // provide a horizontal scroll bar instead of using ellipse (...) for longer items
    mBrowserView->setTextElideMode( Qt::ElideNone );
    mBrowserView->header()->setResizeMode( 0, QHeaderView::ResizeToContents );
    mBrowserView->header()->setStretchLastSection( false );
  }

  QDockWidget::showEvent( e );
}

void QgsBrowserDockWidget::showContextMenu( const QPoint & pt )
{
  QModelIndex idx = mBrowserView->indexAt( pt );
  QgsDataItem* item = dataItem( idx );
  if ( !item )
    return;

  QMenu* menu = new QMenu( this );

  if ( item->type() == QgsDataItem::Directory )
  {
    QSettings settings;
    QStringList favDirs = settings.value( "/browser/favourites" ).toStringList();
    bool inFavDirs = favDirs.contains( item->path() );

    if ( item->parent() != NULL && !inFavDirs )
    {
      // only non-root directories can be added as favourites
      menu->addAction( tr( "Add as a favourite" ), this, SLOT( addFavourite() ) );
    }
    else if ( inFavDirs )
    {
      // only favourites can be removed
      menu->addAction( tr( "Remove favourite" ), this, SLOT( removeFavourite() ) );
    }
    menu->addAction( tr( "Properties" ), this, SLOT( showProperties( ) ) );
  }

  else if ( item->type() == QgsDataItem::Layer )
  {
    menu->addAction( tr( "Add Layer" ), this, SLOT( addCurrentLayer( ) ) );
    menu->addAction( tr( "Add Selected Layers" ), this, SLOT( addSelectedLayers() ) );
    menu->addAction( tr( "Properties" ), this, SLOT( showProperties( ) ) );
  }

  QList<QAction*> actions = item->actions();
  if ( !actions.isEmpty() )
  {
    if ( !menu->actions().isEmpty() )
      menu->addSeparator();
    // add action to the menu
    menu->addActions( actions );
  }

  if ( menu->actions().count() == 0 )
  {
    delete menu;
    return;
  }

  menu->popup( mBrowserView->mapToGlobal( pt ) );
}

void QgsBrowserDockWidget::addFavourite()
{
  QgsDataItem* item = dataItem( mBrowserView->currentIndex() );
  if ( !item )
    return;
  if ( item->type() != QgsDataItem::Directory )
    return;

  QString newFavDir = item->path();

  QSettings settings;
  QStringList favDirs = settings.value( "/browser/favourites" ).toStringList();
  favDirs.append( newFavDir );
  settings.setValue( "/browser/favourites", favDirs );

  // reload the browser model so that the newly added favourite directory is shown
  mModel->reload();
}

void QgsBrowserDockWidget::removeFavourite()
{
  QgsDataItem* item = dataItem( mBrowserView->currentIndex() );

  if ( !item )
    return;
  if ( item->type() != QgsDataItem::Directory )
    return;

  QString favDir  = item->path();

  QSettings settings;
  QStringList favDirs = settings.value( "/browser/favourites" ).toStringList();
  favDirs.removeAll( favDir );
  settings.setValue( "/browser/favourites", favDirs );

  // reload the browser model so that the favourite directory is not shown anymore
  mModel->reload();
}

void QgsBrowserDockWidget::refresh()
{
  QApplication::setOverrideCursor( Qt::WaitCursor );
  refreshModel( QModelIndex() );
  QApplication::restoreOverrideCursor();
}

void QgsBrowserDockWidget::refreshModel( const QModelIndex& index )
{
  QgsDebugMsg( "Entered" );
  if ( index.isValid() )
  {
    QgsDataItem *item = dataItem( index );
    if ( item )
    {
      QgsDebugMsg( "path = " + item->path() );
    }
    else
    {
      QgsDebugMsg( "invalid item" );
    }
  }

  mModel->refresh( index );

  for ( int i = 0 ; i < mModel->rowCount( index ); i++ )
  {
    QModelIndex idx = mModel->index( i, 0, index );
    if ( mBrowserView->isExpanded( idx ) || !mModel->hasChildren( idx ) )
    {
      refreshModel( idx );
    }
  }
}

void QgsBrowserDockWidget::addLayer( QgsLayerItem *layerItem )
{
  if ( layerItem == NULL )
    return;

  QString uri = layerItem->uri();
  if ( uri.isEmpty() )
    return;

  QgsMapLayer::LayerType type = layerItem->mapLayerType();
  QString providerKey = layerItem->providerKey();

  QgsDebugMsg( providerKey + " : " + uri );
  if ( type == QgsMapLayer::VectorLayer )
  {
    QgisApp::instance()->addVectorLayer( uri, layerItem->name(), providerKey );
  }
  if ( type == QgsMapLayer::RasterLayer )
  {
    // This should go to WMS provider
    QStringList URIParts = uri.split( "|" );
    QString rasterLayerPath = URIParts.at( 0 );
    QStringList layers;
    QStringList styles;
    QString format;
    QString crs;
    for ( int i = 1 ; i < URIParts.size(); i++ )
    {
      QString part = URIParts.at( i );
      int pos = part.indexOf( "=" );
      QString field = part.left( pos );
      QString value = part.mid( pos + 1 );

      if ( field == "layers" )
        layers = value.split( "," );
      if ( field == "styles" )
        styles = value.split( "," );
      if ( field == "format" )
        format = value;
      if ( field == "crs" )
        crs = value;
    }
    QgsDebugMsg( "rasterLayerPath = " + rasterLayerPath );
    QgsDebugMsg( "layers = " + layers.join( " " ) );

    QgisApp::instance()->addRasterLayer( rasterLayerPath, layerItem->name(), providerKey, layers, styles, format, crs );
  }
}

void QgsBrowserDockWidget::addLayerAtIndex( const QModelIndex& index )
{
  QgsDataItem *item = dataItem( index );

  if ( item != NULL && item->type() == QgsDataItem::Layer )
  {
    QgsLayerItem *layerItem = qobject_cast<QgsLayerItem*>( item );
    if ( layerItem != NULL )
    {
      QApplication::setOverrideCursor( Qt::WaitCursor );
      addLayer( layerItem );
      QApplication::restoreOverrideCursor();
    }
  }
}

void QgsBrowserDockWidget::addCurrentLayer( )
{
  addLayerAtIndex( mBrowserView->currentIndex() );
}

void QgsBrowserDockWidget::addSelectedLayers()
{
  QApplication::setOverrideCursor( Qt::WaitCursor );

  // get a sorted list of selected indexes
  QModelIndexList list = mBrowserView->selectionModel()->selectedIndexes();
  qSort( list );

  // add items in reverse order so they are in correct order in the layers dock
  for ( int i = list.size() - 1; i >= 0; i-- )
  {
    QModelIndex index = list[i];
    QgsDataItem *item = dataItem( index );
    if ( item && item->type() == QgsDataItem::Layer )
    {
      QgsLayerItem *layerItem = qobject_cast<QgsLayerItem*>( item );
      if ( layerItem )
        addLayer( layerItem );
    }
  }

  QApplication::restoreOverrideCursor();
}

void QgsBrowserDockWidget::showProperties( )
{
  QgsDebugMsg( "Entered" );
  QModelIndex index = mBrowserView->currentIndex();
  QgsDataItem* item = dataItem( index );
  if ( ! item )
    return;

  if ( item->type() == QgsDataItem::Layer )
  {
    QgsLayerItem *layerItem = qobject_cast<QgsLayerItem*>( item );
    if ( layerItem != NULL )
    {
      QgsMapLayer::LayerType type = layerItem->mapLayerType();
      QString layerMetadata = tr( "Error" );
      QgsCoordinateReferenceSystem layerCrs;
      QString notice;

      // temporarily override /Projections/defaultBehaviour to avoid dialog prompt
      QSettings settings;
      QString defaultProjectionOption = settings.value( "/Projections/defaultBehaviour", "prompt" ).toString();
      if ( settings.value( "/Projections/defaultBehaviour", "prompt" ).toString() == "prompt" )
      {
        settings.setValue( "/Projections/defaultBehaviour", "useProject" );
      }

      // find root item
      // we need to create a temporary layer to get metadata
      // we could use a provider but the metadata is not as complete and "pretty"  and this is easier
      QgsDebugMsg( QString( "creating temporary layer using path %1" ).arg( layerItem->path() ) );
      if ( type == QgsMapLayer::RasterLayer )
      {
        QgsDebugMsg( "creating raster layer" );
        // should copy code from addLayer() to split uri ?
        QgsRasterLayer* layer = new QgsRasterLayer( 0, layerItem->uri(), layerItem->uri(), layerItem->providerKey() );
        if ( layer != NULL )
        {
          layerCrs = layer->crs();
          layerMetadata = layer->metadata();
          delete layer;
        }
      }
      else if ( type == QgsMapLayer::VectorLayer )
      {
        QgsDebugMsg( "creating vector layer" );
        QgsVectorLayer* layer = new QgsVectorLayer( layerItem->uri(), layerItem->name(), layerItem->providerKey() );
        if ( layer != NULL )
        {
          layerCrs = layer->crs();
          layerMetadata = layer->metadata();
          delete layer;
        }
      }

      // restore /Projections/defaultBehaviour
      if ( defaultProjectionOption == "prompt" )
      {
        settings.setValue( "/Projections/defaultBehaviour", defaultProjectionOption );
      }

      // initialize dialog
      QDialog *dialog = new QDialog( this );
      Ui::QgsBrowserLayerPropertiesBase ui;
      ui.setupUi( dialog );

      dialog->setWindowTitle( tr( "Layer Properties" ) );
      ui.leName->setText( layerItem->name() );
      ui.leSource->setText( layerItem->path() );
      ui.leProvider->setText( layerItem->providerKey() );
      QString myStyle = QgsApplication::reportStyleSheet();
      ui.txtbMetadata->document()->setDefaultStyleSheet( myStyle );
      ui.txtbMetadata->setHtml( layerMetadata );

      // report if layer was set to to project crs without prompt (may give a false positive)
      if ( defaultProjectionOption == "prompt" )
      {
        QgsCoordinateReferenceSystem defaultCrs =
          QgisApp::instance()->mapCanvas()->mapRenderer()->destinationCrs();
        if ( layerCrs == defaultCrs )
          ui.lblNotice->setText( "NOTICE: Layer srs set from project (" + defaultCrs.authid() + ")" );
      }

      dialog->show();
    }
  }
  else if ( item->type() == QgsDataItem::Directory )
  {
    // initialize dialog
    QDialog *dialog = new QDialog( this );
    Ui::QgsBrowserDirectoryPropertiesBase ui;
    ui.setupUi( dialog );

    dialog->setWindowTitle( tr( "Directory Properties" ) );
    ui.leSource->setText( item->path() );
    QgsDirectoryParamWidget *paramWidget = new QgsDirectoryParamWidget( item->path(), dialog );
    ui.lytWidget->addWidget( paramWidget );

    dialog->show();
  }
}

void QgsBrowserDockWidget::showFilterWidget( bool visible )
{
  mWidgetFilter->setVisible( visible );
  if ( ! visible )
    clearFilter();
}

void QgsBrowserDockWidget::setFilter( )
{
  QString filter = mLeFilter->text();
  if ( mProxyModel )
    mProxyModel->setFilter( filter );
}

void QgsBrowserDockWidget::setFilterSyntax( QAction * action )
{
  if ( !action || ! mProxyModel )
    return;
  mProxyModel->setFilterSyntax(( QRegExp::PatternSyntax ) action->data().toInt() );
}

void QgsBrowserDockWidget::clearFilter( )
{
  mLeFilter->setText( "" );
  setFilter();
}

QgsDataItem* QgsBrowserDockWidget::dataItem( const QModelIndex& index )
{
  if ( ! mProxyModel )
    return mModel->dataItem( index );
  else
    return mModel->dataItem( mProxyModel->mapToSource( index ) );
}

