/***************************************************************************
                             songcollection.cpp
                             -------------------
    begin                : Tue Mar 4 2003
    copyright            : (C) 2003 by Robby Stephenson
    email                : robby@periapsis.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of version 2 of the GNU General Public License as  *
 *   published by the Free Software Foundation;                            *
 *                                                                         *
 ***************************************************************************/

#include "songcollection.h"

SongCollection::SongCollection(int id_, const QString& title_ /* = i18n("My Songs")*/)
   : BCCollection(id_, title_, QString::fromLatin1("song"), i18n("Song")) {

  BCAttribute* att;

  att = new BCAttribute(QString::fromLatin1("artist"), i18n("Artist"));
  att->setCategory(i18n("General"));
  att->setFlags(BCAttribute::AllowGrouped);
  addAttribute(att);
  setDefaultGroupAttribute(att->name());

  att = new BCAttribute(QString::fromLatin1("album"), i18n("Album"));
  att->setCategory(i18n("General"));
  att->setFlags(BCAttribute::AllowGrouped);
  addAttribute(att);

  att = new BCAttribute(QString::fromLatin1("year"), i18n("Year"));
  att->setCategory(i18n("General"));
  att->setFlags(BCAttribute::NoComplete | BCAttribute::AllowGrouped);
  addAttribute(att);

  att = new BCAttribute(QString::fromLatin1("genre"), i18n("Genre"));
  att->setCategory(i18n("General"));
  att->setFlags(BCAttribute::AllowMultiple | BCAttribute::AllowGrouped);
  addAttribute(att);

  QStringList list;
  list << i18n("CD") << i18n("Cassette");
  att = new BCAttribute(QString::fromLatin1("medium"), i18n("Medium"), list);
  att->setCategory(i18n("General"));
  att->setFlags(BCAttribute::AllowGrouped);
  addAttribute(att);

  att = new BCAttribute(QString::fromLatin1("comments"), i18n("Comments"));
  att->setCategory(i18n("General"));
  att->setFlags(BCAttribute::NoComplete);
  addAttribute(att);
  
  // have to do this at end since addAttribute() sets it to true;
  m_isCustom = false;
}
