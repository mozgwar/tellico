/***************************************************************************
    copyright            : (C) 2003-2004 by Robby Stephenson
    email                : robby@periapsis.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of version 2 of the GNU General Public License as  *
 *   published by the Free Software Foundation;                            *
 *                                                                         *
 ***************************************************************************/

#include "bibtexmlimporter.h"
#include "bibtexhandler.h"
#include "../collections/bibtexcollection.h"
#include "../latin1literal.h"

#include <klocale.h>
#include <kdebug.h>

extern const char* loadError;

using Bookcase::Import::BibtexmlImporter;

Bookcase::Data::Collection* BibtexmlImporter::collection() {
  if(!m_coll) {
    loadDomDocument();
  }
  return m_coll;
}

void BibtexmlImporter::loadDomDocument() {
  QDomElement root = domDocument().documentElement();
  if(root.tagName() != Latin1Literal("file")) {
    setStatusMessage(i18n(loadError).arg(url().fileName()));
    return;
  }

  QString ns = BibtexHandler::s_bibtexmlNamespace;
  m_coll = new Data::BibtexCollection(true);

  QDomNodeList entryelems = root.elementsByTagNameNS(ns, QString::fromLatin1("entry"));
//  kdDebug() << "BibtexmlImporter::loadDomDocument - found " << entryelems.count() << " entries" << endl;

  unsigned count = entryelems.count();
  for(unsigned j = 0; j < entryelems.count(); ++j) {
    readEntry(entryelems.item(j));

    if(j%s_stepSize == 0) {
      emit signalFractionDone(static_cast<float>(j)/static_cast<float>(count));
    }
  } // end unit loop
}

void BibtexmlImporter::readEntry(const QDomNode& entryNode_) {
  QDomNode node = const_cast<QDomNode&>(entryNode_);

  Data::Entry* unit = new Data::Entry(m_coll);

/* The Bibtexml format looks like
  <entry id="...">
   <book>
    <authorlist>
     <author>...</author>
    </authorlist>
    <publisher>...</publisher> */

  QString type = node.firstChild().toElement().tagName();
  unit->setField(QString::fromLatin1("entry-type"), type);
  QString id = node.toElement().attribute(QString::fromLatin1("id"));
  unit->setField(QString::fromLatin1("bibtex-key"), id);

  QString name, value;
  // field values are first child of first child of entry node
  for(QDomNode n = node.firstChild().firstChild(); !n.isNull(); n = n.nextSibling()) {
    // n could be something like authorlist, with multiple authors, or just
    // a plain element with a single text child...
    // second case first
    if(n.firstChild().isText()) {
      name = n.toElement().tagName();
      value = n.toElement().text();
    } else {
      // is either titlelist, authorlist, editorlist, or keywords
      QString parName = n.toElement().tagName();
      if(parName == Latin1Literal("titlelist")) {
        for(QDomNode n2 = node.firstChild(); !n2.isNull(); n2 = n2.nextSibling()) {
          name = n2.toElement().tagName();
          value = n2.toElement().text();
          if(!name.isEmpty() && !value.isEmpty()) {
            BibtexHandler::setFieldValue(unit, name, value.simplifyWhiteSpace());
          }
        }
        name.truncate(0);
        value.truncate(0);
      } else {
        name = n.firstChild().toElement().tagName();
        if(name == Latin1Literal("keyword")) {
          name = QString::fromLatin1("keywords");
        }
        value.truncate(0);
        for(QDomNode n2 = n.firstChild(); !n2.isNull(); n2 = n2.nextSibling()) {
          // n2 could have first, middle, lastname elements...
          if(name == Latin1Literal("person")) {
            QStringList names;
            names << QString::fromLatin1("initials") << QString::fromLatin1("first")
                  << QString::fromLatin1("middle") << QString::fromLatin1("prelast")
                  << QString::fromLatin1("last") << QString::fromLatin1("lineage");
            for(QStringList::ConstIterator it = names.begin(); it != names.end(); ++it) {
              QDomNodeList list = n2.toElement().elementsByTagName(*it);
              if(list.count() > 1) {
                value += list.item(0).toElement().text();
              }
              if(*it != names.last()) {
                value += QString::fromLatin1(" ");
              }
            }
          }
          for(QDomNode n3 = n2.firstChild(); !n3.isNull(); n3 = n3.nextSibling()) {
            if(n3.isElement()) {
              value += n3.toElement().text();
            } else if(n3.isText()) {
              value += n3.toText().data();
            }
            if(n3 != n2.lastChild()) {
              value += QString::fromLatin1(" ");
            }
          }
          if(n2 != n.lastChild()) {
            value += QString::fromLatin1("; ");
          }
        }
      }
    }
    if(!name.isEmpty() && !value.isEmpty()) {
      BibtexHandler::setFieldValue(unit, name, value.simplifyWhiteSpace());
    }
  }

  m_coll->addEntry(unit);
}

#include "bibtexmlimporter.moc"
