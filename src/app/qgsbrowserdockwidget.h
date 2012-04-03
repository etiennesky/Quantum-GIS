#ifndef QGSBROWSERDOCKWIDGET_H
#define QGSBROWSERDOCKWIDGET_H

#include <QDockWidget>
#include <ui_qgsbrowserdockwidgetbase.h>

class QgsBrowserModel;
class QModelIndex;
class QTreeView;
class QgsLayerItem;

class QgsBrowserDockWidget : public QDockWidget, private Ui::QgsBrowserDockWidgetBase
{
    Q_OBJECT
  public:
    explicit QgsBrowserDockWidget( QWidget *parent = 0 );

  signals:

  public slots:
    void addLayerAtIndex( const QModelIndex& index );
    void showContextMenu( const QPoint & );

    void addFavourite();
    void removeFavourite();

    void refresh();

    // layer menu items
    void addCurrentLayer();
    void addSelectedLayers();
    void showProperties();

  protected:

    void refreshModel( const QModelIndex& index );

    void showEvent( QShowEvent * event );

    void addLayer( QgsLayerItem *layerItem );

    QTreeView* mBrowserView;
    QgsBrowserModel* mModel;
};

#endif // QGSBROWSERDOCKWIDGET_H
