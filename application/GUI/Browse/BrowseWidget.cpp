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
  
#include <Browse/BrowseWidget.h>
#include <ui_BrowseWidget.h>
using namespace GUI;

#include <QMenu>
#include <QPainter>
#include <QIcon>
#include <QUrl>
#include <QShowEvent>

#include <Common/ProtoHelper.h>

#include <Log.h>
#include <Utils.h>

void BrowseDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
   QStyleOptionViewItem newOption(option);
   newOption.state = option.state & (~QStyle::State_HasFocus);
   QStyledItemDelegate::paint(painter, newOption, index);
}

/////

BrowseWidget::BrowseWidget(QSharedPointer<RCC::ICoreConnection> coreConnection, const PeerListModel& peerListModel, const DirListModel& sharedDirsModel, const Common::Hash& peerID, QWidget* parent) :
   QWidget(parent),
   ui(new Ui::BrowseWidget),
   downloadMenu(sharedDirsModel),
   coreConnection(coreConnection),
   peerID(peerID),
   browseModel(coreConnection, sharedDirsModel, peerID),
   tryingToReachEntryToBrowse(false)
{
   this->ui->setupUi(this);

   this->ui->treeView->setModel(&this->browseModel);
   this->ui->treeView->setItemDelegate(&this->browseDelegate);
   this->ui->treeView->header()->setVisible(false);
   this->ui->treeView->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
   this->ui->treeView->header()->setSectionResizeMode(1, QHeaderView::Stretch);

   this->ui->treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
   this->ui->treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

   this->ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);
   connect(this->ui->treeView, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(displayContextMenuDownload(const QPoint&)));
   connect(this->ui->treeView, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(entryDoubleClicked(const QModelIndex&)));

   if (this->coreConnection->getRemoteID() == this->peerID)
      this->ui->butDownload->hide();
   else
      connect(this->ui->butDownload, SIGNAL(clicked()), this, SLOT(download()));

   connect(&this->downloadMenu, SIGNAL(download()), this, SLOT(download()));
   connect(&this->downloadMenu, SIGNAL(downloadTo()), this, SLOT(downloadTo()));
   connect(&this->downloadMenu, SIGNAL(downloadTo(const QString&, const Common::Hash&)), this, SLOT(downloadTo(const QString&, const Common::Hash&)));

   connect(&this->browseModel, SIGNAL(loadingResultFinished()), this, SLOT(tryToReachEntryToBrowse()));

   this->setWindowTitle(peerListModel.getNick(this->peerID));
}

BrowseWidget::~BrowseWidget()
{
    delete this->ui;
}

Common::Hash BrowseWidget::getPeerID() const
{
   return this->peerID;
}

void BrowseWidget::browseTo(const Protos::Common::Entry& remoteEntry)
{
   this->tryingToReachEntryToBrowse = true;
   this->remoteEntryToBrowse = remoteEntry;

   if (!this->browseModel.isWaitingResult())
      this->tryToReachEntryToBrowse();
}

void BrowseWidget::refresh()
{
   this->browseModel.refresh();
}

void BrowseWidget::changeEvent(QEvent* event)
{
   if (event->type() == QEvent::LanguageChange)
      this->ui->retranslateUi(this);

   QWidget::changeEvent(event);
}

void BrowseWidget::keyPressEvent(QKeyEvent* event)
{
   // Return key -> open all selected files.
   if (event->key() == Qt::Key_Return)
   {
      const QModelIndexList& selectedRows = this->ui->treeView->selectionModel()->selectedRows();
      for (QListIterator<QModelIndex> i(selectedRows); i.hasNext();)
         this->openFile(i.next());
   }
   else
      QWidget::keyPressEvent(event);
}

void BrowseWidget::displayContextMenuDownload(const QPoint& point)
{
   QPoint globalPosition = this->ui->treeView->mapToGlobal(point);
   if (this->coreConnection->getRemoteID() == this->peerID)
   {
      if (this->coreConnection->isLocal())
      {
         QMenu menu;
         menu.addAction(QIcon(":/icons/ressources/explore_folder.png"), tr("Open location"), this, SLOT(openLocation()));
         menu.exec(globalPosition);
      }
   }
   else
   {
      this->downloadMenu.show(globalPosition);
   }
}

void BrowseWidget::entryDoubleClicked(const QModelIndex& index)
{
   this->openFile(index);
}

void BrowseWidget::download()
{
   if (this->browseModel.nbSharedDirs() == 0)
   {
      QStringList dirs = Utils::askForDirectoriesToDownloadTo(this->coreConnection);
      if (!dirs.isEmpty())
         this->downloadTo(dirs.first(), Common::Hash());
      return;
   }

   QModelIndexList selectedRows = this->ui->treeView->selectionModel()->selectedRows();
   for (QListIterator<QModelIndex> i(selectedRows); i.hasNext();)
      this->coreConnection->download(this->peerID, this->browseModel.getEntry(i.next()));
}

void BrowseWidget::downloadTo()
{
   QStringList dirs = Utils::askForDirectoriesToDownloadTo(this->coreConnection);
   if (!dirs.isEmpty())
      this->downloadTo(dirs.first());
}

void BrowseWidget::downloadTo(const QString& path, const Common::Hash& sharedDirID)
{
   QModelIndexList selectedRows = this->ui->treeView->selectionModel()->selectedRows();

   for (QListIterator<QModelIndex> i(selectedRows); i.hasNext();)
      this->coreConnection->download(this->peerID, this->browseModel.getEntry(i.next()), sharedDirID, path);
}

void BrowseWidget::openLocation()
{
   QModelIndexList selectedRows = this->ui->treeView->selectionModel()->selectedRows();

   QSet<QString> locations;
   for (QListIterator<QModelIndex> i(selectedRows); i.hasNext();)
      locations.insert(this->browseModel.getPath(i.next(), true));

   Utils::openLocations(locations.values());
}

/**
  * Try to select an entry from a remote peer in the browse tab.
  * The entry to browse is set in 'this->remoteEntryToBrowse'.
  */
void BrowseWidget::tryToReachEntryToBrowse()
{
   if (!this->tryingToReachEntryToBrowse)
      return;

   // First we search for the shared directory of the entry.
   for (int r = 0; r < this->browseModel.rowCount(); r++)
   {
      QModelIndex currentIndex = this->browseModel.index(r, 0);
      Protos::Common::Entry root = this->browseModel.getEntry(currentIndex);
      if (root.has_shared_dir() && this->remoteEntryToBrowse.has_shared_dir() && root.shared_dir().id().hash() == this->remoteEntryToBrowse.shared_dir().id().hash())
      {
         // Then we try to match each folder name. If a folder cannot be reached then we ask to expand the last folder.
         // After the folder entries are loaded, 'tryToReachEntryToBrowse()' will be recalled via the signal 'BrowseModel::loadingResultFinished()'.
         const QStringList& path = Common::ProtoHelper::getStr(this->remoteEntryToBrowse, &Protos::Common::Entry::path).append(Common::ProtoHelper::getStr(this->remoteEntryToBrowse, &Protos::Common::Entry::name)).split('/', QString::SkipEmptyParts);
         for (QStringListIterator i(path); i.hasNext();)
         {
            QModelIndex childIndex = this->browseModel.searchChild(i.next(), currentIndex);
            if (!childIndex.isValid())
            {
               this->ui->treeView->expand(currentIndex);
               return;
            }
            currentIndex = childIndex;

            // We reach the last entry name (file or directory), we just have to show and select it.
            if (!i.hasNext())
            {
               this->ui->treeView->scrollTo(currentIndex);
               this->ui->treeView->selectionModel()->select(currentIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            }
         }
      }
   }

   this->tryingToReachEntryToBrowse = false;
}

void BrowseWidget::openFile(const QModelIndex& index) const
{
   if (this->coreConnection->getRemoteID() == this->peerID && !this->browseModel.isDir(index))
      Utils::openFile(this->browseModel.getPath(index));
}
