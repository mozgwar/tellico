/***************************************************************************
    copyright            : (C) 2002-2004 by Robby Stephenson
    email                : robby@periapsis.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of version 2 of the GNU General Public License as  *
 *   published by the Free Software Foundation;                            *
 *                                                                         *
 ***************************************************************************/

#ifndef FINDDIALOG_H
#define FINDDIALOG_H

class KComboBox;
class KHistoryCombo;
class KPushButton;

class QCheckBox;

#include <kdialogbase.h>

namespace Bookcase {

/**
 * The find dialog allows the user to search for a string in the document.
 *
 * @author Robby Stephenson
 * @version $Id: finddialog.h 691 2004-06-09 02:48:54Z robby $
 */
class FindDialog : public KDialogBase {
Q_OBJECT

public:
  /**
   * The constructor sets up the dialog.
   *
   * @param parent A pointer to the parent widget, a Bookcase object
   * @param name The widget name
   */
  FindDialog(QWidget* parent, const char* name=0);

  /**
   * Update the field list. This is needed since the parent Bookcase app
   * doesn't delete the object once it's created. That's to retain the history list.
   */
  void updateFieldList();

public slots:
  /**
   * Find the next match
   */
  void slotFindNext();
  /**
   * Find the previous match
   */
  void slotFindPrev();

protected slots:
  /**
   * Called when the Find button is clicked.
   */
  void slotUser1();
  /**
   * Called when the search pattern changes
   *
   * @param text The text in the pattern
   */
  void slotPatternChanged(const QString& text);
  void showEvent(QShowEvent* e);
  void slotEditRegExp();

private:
  KHistoryCombo* m_pattern;
  KComboBox* m_fields;
  QCheckBox* m_caseSensitive;
  QCheckBox* m_findBackwards;
  QCheckBox* m_asRegExp;
  QCheckBox* m_wholeWords;
  QCheckBox* m_fromBeginning;

  KPushButton* m_editRegExp;
  QDialog* m_editRegExpDialog;
};

} // end namespace
#endif
