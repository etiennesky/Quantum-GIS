/***************************************************************************
    qgssublayersdialog.cpp  - dialog for selecting sublayers
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

#include "qgssublayersdialog.h"

#include "qgslogger.h"

#include <QSettings>
#include <QTableWidgetItem>
#include <QPushButton>


QgsSublayersDialog::QgsSublayersDialog( QgsDataSource* dataSource,
                                        QWidget* parent, Qt::WFlags fl )
    : QDialog( parent, fl )
{
  setupUi( this );

  if ( ! dataSource )
    return;

  mName = dataSource->providerKey();
  mDataSource = dataSource;

  if ( mDataSource->dataSourceType() == QgsDataSource::VectorDataSource )
    setWindowTitle( tr( "Select vector layers to add..." ) );
  else if ( mDataSource->dataSourceType() == QgsDataSource::RasterDataSource )
    setWindowTitle( tr( "Select raster layers to add..." ) );
  else
    setWindowTitle( tr( "Select layers to add..." ) );

  layersTable->setHeaderLabels( mDataSource->layerInfoHeaders() );

  // populate table, add uri as tooltip
  populateLayerTable( mDataSource->layerInfo() );
  for ( int i = 0; i < layersTable->topLevelItemCount(); i++ )
    layersTable->invisibleRootItem()->child( i )->setToolTip( 1, mDataSource->layerUris().at( i ) );

  // add a "Select All" button - would be nicer with an icon
  QPushButton* button = new QPushButton( tr( "Select All" ) );
  buttonBox->addButton( button, QDialogButtonBox::ActionRole );
  connect( button, SIGNAL( pressed() ), layersTable, SLOT( selectAll() ) );
  // connect( pbnSelectNone, SIGNAL( pressed() ), SLOT( layersTable->selectNone() ) );

  QSettings settings;
  restoreGeometry( settings.value( "/Windows/" + mName + "SubLayers/geometry" ).toByteArray() );
}

QgsSublayersDialog::~QgsSublayersDialog()
{
  QSettings settings;
  settings.setValue( "/Windows/" + mName + "SubLayers/geometry", saveGeometry() );
  settings.setValue( "/Windows/" + mName + "SubLayers/headerState",
                     layersTable->header()->saveState() );
}

QStringList QgsSublayersDialog::selectionNames()
{
  QStringList list;
  for ( int i = 0; i < layersTable->selectedItems().size(); i++ )
  {
    list << layersTable->selectedItems().at( i )->text( 1 );
  }
  return list;
}

QList<int> QgsSublayersDialog::selectionIndexes()
{
  QList<int> list;
  for ( int i = 0; i < layersTable->selectedItems().size(); i++ )
  {
    list << layersTable->selectedItems().at( i )->text( 0 ).toInt();
  }
  return list;
}

QStringList QgsSublayersDialog::selectionUris()
{
  QStringList list;
  QStringList uris;
  if ( mDataSource )
    uris = mDataSource->layerUris();
  for ( int i = 0; i < layersTable->selectedItems().size(); i++ )
  {
    list << uris[ layersTable->selectedItems().at( i )->text( 0 ).toInt()];
  }
  return list;
}

void QgsSublayersDialog::populateLayerTable( QStringList theList, QString delim )
{
  foreach ( QString item, theList )
  {
    layersTable->addTopLevelItem( new QTreeWidgetItem( item.split( delim ) ) );
  }

  // resize columns
  QSettings settings;
  QByteArray ba = settings.value( "/Windows/" + mName + "SubLayers/headerState" ).toByteArray();
  if ( ! ba.isNull() )
  {
    layersTable->header()->restoreState( ba );
  }
  else
  {
    for ( int i = 0; i < layersTable->columnCount(); i++ )
      layersTable->resizeColumnToContents( i );
    layersTable->setColumnWidth( 1, layersTable->columnWidth( 1 ) + 10 );
  }
}

// override exec() instead of using showEvent()
// because in some case we don't want the dialog to appear (depending on user settings)
// TODO alert the user when dialog is not opened
int QgsSublayersDialog::exec()
{
  QSettings settings;
  QString promptLayers = settings.value( "/qgis/promptForSublayers", 1 ).toString();

  QgsDebugMsg( QString( "promptLayers=%1 count=%2" ).arg( promptLayers ).arg( mDataSource ? mDataSource->layerNames().count() : 0 ) );

  // make sure three are any sublayers
  if ( mDataSource && mDataSource->layerNames().isEmpty() )
    return QDialog::Rejected;

  // check promptForSublayers settings - perhaps this should be in QgsDataSource instead?
  if ( promptLayers == "no" )
    return QDialog::Rejected;
  else if ( promptLayers == "all" )
  {
    layersTable->selectAll();
    return QDialog::Accepted;
  }

  // if there is only 1 sublayer (probably the main layer), just select that one and return
  if ( mDataSource && mDataSource->layerNames().count() == 1 )
  {
    layersTable->selectAll();
    return QDialog::Accepted;
  }

  // if we got here, disable override cursor, open dialog and return result
  // TODO add override cursor where it is missing (e.g. when opening via "Add Raster")
  QCursor cursor;
  bool override = ( QApplication::overrideCursor() != 0 ); 
  if ( override )
  {
    cursor = QCursor( * QApplication::overrideCursor() );
    QApplication::restoreOverrideCursor();
  }
  int ret = QDialog::exec();
  if ( override )
    QApplication::setOverrideCursor( cursor );
  return ret;
}

QList<QgsMapLayer *> QgsSublayersDialog::addDataSourceLayers( QWidget* parentWidget, QString baseUri,
    QgsMapLayer::LayerType type, QStringList providerKeys )
{
  QgsDebugMsg( QString( "Trying to create a QgsDataSource from %1 with providers {%2}" ).arg( baseUri, providerKeys.join( " " ) ) );

  QList<QgsMapLayer *> list;
  QgsDataSource *dataSource = QgsDataSource::open( baseUri, type, providerKeys );

  if ( dataSource )
  {
    QgsDebugMsg( QString( "got dataSource with %1 sublayers : {%2}" ).arg( dataSource->layerNames().count() ).arg( dataSource->layerNames().join( " " ) ) );
    QgsDebugMsg( QString( "sublayer uris : {%1}" ).arg( dataSource->layerUris().join( " " ) ) );

    QgsSublayersDialog chooseSublayersDialog( dataSource, parentWidget );
    if ( chooseSublayersDialog.exec() == QDialog::Accepted	)
    {
      list = dataSource->addLayers( chooseSublayersDialog.selectionNames() );
    }
    // // if empty, add a NULL element so we know it wasn't because of a invalid datasource
    // if ( list.isEmpty() )
    //   list << NULL;
    delete dataSource;
  }

  return list;
}

