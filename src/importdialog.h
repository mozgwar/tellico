/***************************************************************************
    Copyright (C) 2003-2009 Robby Stephenson <robby@periapsis.org>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 ***************************************************************************/

#ifndef TELLICO_IMPORTDIALOG_H
#define TELLICO_IMPORTDIALOG_H

#include "translators/translators.h"
#include "datavectors.h"

#include <QDialog>
#include <QUrl>
#include <QShowEvent>

class QRadioButton;
class QCheckBox;
class QShowEvent;
class QButtonGroup;

namespace Tellico {
  namespace Import {
    class Importer;
  }

/**
 * @author Robby Stephenson
 */
class ImportDialog : public QDialog {
Q_OBJECT

public:
  ImportDialog(Import::Format format, const QList<QUrl>& urls, QWidget* parent);
  ~ImportDialog();

  Data::CollPtr collection();
  QString statusMessage() const;
  Import::Action action() const;

  static QString fileFilter(Import::Format format);
  static Import::Target importTarget(Import::Format format);
  static QString startDir(Import::Format format);

  static Import::Importer* importer(Import::Format format, const QList<QUrl>& urls);
  static Data::CollPtr importURL(Import::Format format, const QUrl& url);
  static Data::CollPtr importText(Import::Format format, const QString& text);

private Q_SLOTS:
  virtual void slotOk();
  void slotUpdateAction();

private:
  // TODO: update to use every importer that allows text importing
  static Import::Importer* importerForText(Import::Format format, const QString& text);
  Data::CollPtr m_coll;
  Import::Importer* m_importer;
  QRadioButton* m_radioAppend;
  QRadioButton* m_radioReplace;
  QRadioButton* m_radioMerge;
  QButtonGroup* m_buttonGroup;
};

} // end namespace
#endif
