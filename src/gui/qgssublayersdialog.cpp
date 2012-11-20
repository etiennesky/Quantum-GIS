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

#include <QSettings>
#include <QTableWidgetItem>

QgsSublayersDialog::QgsSublayersDialog( QWidget* parent, SublayersType type,
                                        QString name, Qt::WFlags fl )
    : QDialog( parent, fl ), mType( type ), mName( name )
{
  setupUi( this );

  QStringList headers;
  if ( mType == Ogr )
    headers << tr( "Layer ID" ) << tr( "Layer name" ) << tr( "# feat." ) << tr( "Geometry type" );
  else if ( mType == Zip )
    headers << tr( "Layer ID" ) << tr( "Layer name" ) << tr( "Data type" );
  else
    headers << tr( "Layer ID" ) << tr( "Layer name" );
  layersTable->setHeaderLabels( headers );

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

QStringList QgsSublayersDialog::getSelection()
{
  QStringList list;
  for ( int i = 0; i < layersTable->selectedItems().size(); i++ )
  {
    list << layersTable->selectedItems().at( i )->text( 1 );
  }
  return list;
}

QList<int> QgsSublayersDialog::getSelectionIndexes()
{
  QList<int> list;
  for ( int i = 0; i < layersTable->selectedItems().size(); i++ )
  {
    list << layersTable->selectedItems().at( i )->text( 0 ).toInt();
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
