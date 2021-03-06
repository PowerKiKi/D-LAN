/**
  * D-LAN - A decentralized LAN file sharing software.
  * Copyright (C) 2010-2012 Greg Burri <greg.burri@gmail.com>
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
  */
  
#include <Chat/RoomsDock.h>
#include <ui_RoomsDock.h>
using namespace GUI;

#include <Common/Settings.h>

#include <QKeyEvent>
#include <QMenu>

RoomsDock::RoomsDock(QSharedPointer<RCC::ICoreConnection> coreConnection, QWidget *parent) :
   QDockWidget(parent),
   ui(new Ui::RoomsDock),
   coreConnection(coreConnection),
   roomsModel(this->coreConnection)
{
   this->ui->setupUi(this);

   this->roomsModel.setSortType(static_cast<Protos::GUI::Settings::RoomSortType>(SETTINGS.get<quint32>("room_sort_type")));

   this->ui->txtRoomName->installEventFilter(this);

   this->ui->tblRooms->setModel(&this->roomsModel);
   this->ui->tblRooms->setItemDelegate(&this->roomsDelegate);
   this->ui->tblRooms->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
   this->ui->tblRooms->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
   this->ui->tblRooms->horizontalHeader()->setVisible(false);
   this->ui->tblRooms->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
   this->ui->tblRooms->verticalHeader()->setDefaultSectionSize(QApplication::fontMetrics().height() + 4);
   this->ui->tblRooms->verticalHeader()->setVisible(false);
   this->ui->tblRooms->setSelectionBehavior(QAbstractItemView::SelectRows);
   this->ui->tblRooms->setSelectionMode(QAbstractItemView::ExtendedSelection);
   this->ui->tblRooms->setShowGrid(false);
   this->ui->tblRooms->setAlternatingRowColors(false);
   this->ui->tblRooms->setContextMenuPolicy(Qt::CustomContextMenu);

   connect(this->ui->tblRooms, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(displayContextMenuRooms(QPoint)));
   connect(this->ui->tblRooms, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(roomDoubleClicked(QModelIndex)));

   connect(this->ui->butJoinRoom, SIGNAL(clicked()), this, SLOT(joinRoom()));

   connect(this->coreConnection.data(), SIGNAL(connected()), this, SLOT(coreConnected()));
   connect(this->coreConnection.data(), SIGNAL(disconnected(bool)), this, SLOT(coreDisconnected(bool)));

   this->coreDisconnected(false); // Initial state.
}

RoomsDock::~RoomsDock()
{
   delete ui;
}

void RoomsDock::changeEvent(QEvent* event)
{
   if (event->type() == QEvent::LanguageChange)
      this->ui->retranslateUi(this);

   QDockWidget::changeEvent(event);
}

bool RoomsDock::eventFilter(QObject* obj, QEvent* event)
{
   if (obj == this->ui->txtRoomName && event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Return)
   {
      this->joinRoom();
   }

   return QDockWidget::eventFilter(obj, event);
}

void RoomsDock::displayContextMenuRooms(const QPoint& point)
{
   QMenu menu;
   menu.addAction(QIcon(":/icons/ressources/join_chat_room.png"), tr("Join"), this, SLOT(joinSelectedRoom()));

   menu.addSeparator();

   QAction* sortByNbPeersAction = menu.addAction(tr("Sort by number of peers"), this, SLOT(sortByNbPeers()));
   QAction* sortByNameAction = menu.addAction(tr("Sort alphabetically"), this, SLOT(sortByName()));

   QActionGroup sortGroup(this);
   sortGroup.setExclusive(true);
   sortByNbPeersAction->setCheckable(true);
   sortByNbPeersAction->setChecked(this->roomsModel.getSortType() == Protos::GUI::Settings::BY_NB_PEERS);
   sortByNameAction->setCheckable(true);
   sortByNameAction->setChecked(this->roomsModel.getSortType() == Protos::GUI::Settings::BY_NAME);
   sortGroup.addAction(sortByNbPeersAction);
   sortGroup.addAction(sortByNameAction);

   menu.exec(this->ui->tblRooms->mapToGlobal(point));
}

void RoomsDock::roomDoubleClicked(const QModelIndex& index)
{
   this->joinRoom(this->roomsModel.getRoomName(index));
}

void RoomsDock::joinSelectedRoom()
{
   QString roomName = this->roomsModel.getRoomName(this->ui->tblRooms->currentIndex());
   this->joinRoom(roomName);
}

void RoomsDock::joinRoom()
{
   this->joinRoom(this->ui->txtRoomName->text());
}

void RoomsDock::sortByNbPeers()
{
   this->roomsModel.setSortType(Protos::GUI::Settings::BY_NB_PEERS);
   SETTINGS.set("room_sort_type", static_cast<quint32>(Protos::GUI::Settings::BY_NB_PEERS));
   SETTINGS.save();
}

void RoomsDock::sortByName()
{
   this->roomsModel.setSortType(Protos::GUI::Settings::BY_NAME);
   SETTINGS.set("room_sort_type", static_cast<quint32>(Protos::GUI::Settings::BY_NAME));
   SETTINGS.save();

}

void RoomsDock::coreConnected()
{
   this->ui->butJoinRoom->setDisabled(false);
   this->ui->txtRoomName->setDisabled(false);
   this->ui->tblRooms->setDisabled(false);
}

void RoomsDock::coreDisconnected(bool force)
{
   this->ui->butJoinRoom->setDisabled(true);
   this->ui->txtRoomName->setDisabled(true);
   this->ui->tblRooms->setDisabled(true);
}

void RoomsDock::joinRoom(const QString& roomName)
{
   const QString cleanedName = roomName.trimmed().toLower();

   if (!cleanedName.isEmpty())
   {
      this->coreConnection->joinRoom(cleanedName);
      emit roomJoined(cleanedName);
   }
}
