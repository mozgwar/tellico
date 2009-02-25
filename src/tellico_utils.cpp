/***************************************************************************
    copyright            : (C) 2003-2008 by Robby Stephenson
    email                : robby@periapsis.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of version 2 of the GNU General Public License as  *
 *   published by the Free Software Foundation;                            *
 *                                                                         *
 ***************************************************************************/

#include "tellico_utils.h"
#include "tellico_kernel.h"
#include "tellico_debug.h"

#include <kapplication.h>
#include <klocale.h>
#include <kglobal.h>
#include <klibrary.h>
#include <kstandarddirs.h>
#include <kcharsets.h>
#include <krandom.h>

#include <QRegExp>
#include <QDir>
#include <QCursor>
#include <QTextCodec>

namespace {
  static const int STRING_STORE_SIZE = 997; // too big, too small?
}

QString Tellico::decodeHTML(const QByteArray& data_) {
  return decodeHTML(fromHtmlData(data_));
}

QString Tellico::decodeHTML(QString text) {
  QRegExp rx(QLatin1String("&(.+);"));
  rx.setMinimal(true);
  int pos = rx.indexIn(text);
  while(pos > -1) {
    QChar c = KCharsets::fromEntity(rx.cap(1));
    if(!c.isNull()) {
      text.replace(pos, rx.matchedLength(), c);
    }
    pos = rx.indexIn(text, pos+1);
  }
  return text;
}

QString Tellico::uid(int l, bool prefix) {
  QString uid;
  if(prefix) {
    uid = QLatin1String("Tellico");
  }
  uid.append(KRandom::randomString(qMax(l - uid.length(), 0)));
  return uid;
}

uint Tellico::toUInt(const QString& s, bool* ok) {
  if(s.isEmpty()) {
    if(ok) {
      *ok = false;
    }
    return 0;
  }

  int idx = 0;
  while(idx < s.length() && s[idx].isDigit()) {
    ++idx;
  }
  if(idx == 0) {
    if(ok) {
      *ok = false;
    }
    return 0;
  }
  return s.left(idx).toUInt(ok);
}

QString Tellico::i18nReplace(QString text) {
  // Because QDomDocument sticks in random newlines, go ahead and grab them too
  static QRegExp rx(QLatin1String("(?:\\n+ *)*<i18n>([^<]*)</i18n>(?: *\\n+)*"));
  int pos = rx.indexIn(text);
  while(pos > -1) {
    text.replace(pos, rx.matchedLength(), i18n(rx.cap(1).toUtf8()));
    pos = rx.indexIn(text, pos+rx.matchedLength());
  }
  return text;
}

QString Tellico::removeAcceleratorMarker(const QString& label_) {
#if KDE_IS_VERSION(4,2,0)
  return KGlobal::locale()->removeAcceleratorMarker(label_);
#else
  QString label = label_;

  int p = 0;
  while (true) {
      p = label.indexOf('&', p);
      if (p < 0 || p + 1 == label.length()) {
          break;
      }

      if (label[p + 1].isLetterOrNumber()) {
          // Valid accelerator.
          label = label.left(p) + label.mid(p + 1);

          // May have been an accelerator in style of
          // "(<marker><alnum>)" at the start or end of text.
          if (   p > 0 && p + 1 < label.length()
              && label[p - 1] == '(' && label[p + 1] == ')')
          {
              // Check if at start or end, ignoring non-alphanumerics.
              int len = label.length();
              int p1 = p - 2;
              while (p1 >= 0 && !label[p1].isLetterOrNumber()) {
                  --p1;
              }
              ++p1;
              int p2 = p + 2;
              while (p2 < len && !label[p2].isLetterOrNumber()) {
                  ++p2;
              }
              --p2;

              if (p1 == 0) {
                  label = label.left(p - 1) + label.mid(p2 + 1);
              } else if (p2 + 1 == len) {
                  label = label.left(p1) + label.mid(p + 2);
              }
          }
      } else if (label[p + 1] == '&') {
          // Escaped accelerator marker.
          label = label.left(p) + label.mid(p + 1);
      }

      ++p;
  }

  return label;
#endif
}

QStringList Tellico::findAllSubDirs(const QString& dir_) {
  if(dir_.isEmpty()) {
    return QStringList();
  }

  // TODO: build in symlink checking, for now, prohibit
  QDir dir(dir_, QString(), QDir::Name | QDir::IgnoreCase, QDir::Dirs | QDir::Readable | QDir::NoSymLinks);

  QStringList allSubdirs; // the whole list

  // find immediate sub directories
  const QStringList subdirs = dir.entryList();
  for(QStringList::ConstIterator subdir = subdirs.begin(); subdir != subdirs.end(); ++subdir) {
    if((*subdir).isEmpty() || *subdir == QLatin1String(".") || *subdir == QLatin1String("..")) {
      continue;
    }
    QString absSubdir = dir.absoluteFilePath(*subdir);
    allSubdirs += findAllSubDirs(absSubdir);
    allSubdirs += absSubdir;
  }
  return allSubdirs;
}

// Based on QGDict's hash functions, Copyright (C) 1992-2000 Trolltech AS
// and used from Juk, Copyright (C) 2003 - 2004 by Scott Wheeler
int Tellico::stringHash(const QString& str) {
  uint h = 0;
  uint g = 0;
  for(int i = 0; i < str.length(); ++i) {
    h = (h << 4) + str.unicode()[i].cell();
    if((g = h & 0xf0000000)) {
      h ^= g >> 24;
    }
    h &= ~g;
  }

  int index = h;
  return index < 0 ? -index : index;
}

QString Tellico::shareString(const QString& str) {
  static QString stringStore[STRING_STORE_SIZE];

  const int hash = stringHash(str) % STRING_STORE_SIZE;
  if(stringStore[hash] != str) {
    stringStore[hash] = str;
  }
  return stringStore[hash];
}

KLibrary* Tellico::openLibrary(const QString& libName_) {
  KLibrary* library = new KLibrary(libName_);
  if(!library->load()) {
    myWarning() << "Could not load library'" << libName_ << "'";
    myWarning() << "ERROR:" << library->errorString();
    return 0;
  }

  return library;
}

QString Tellico::minutes(int seconds) {
  int min = seconds / 60;
  seconds = seconds % 60;
  return QString::number(min) + QLatin1Char(':') + QString::number(seconds).rightJustified(2, QLatin1Char('0'));
}

QString Tellico::saveLocation(const QString& dir_) {
  return KGlobal::dirs()->saveLocation("appdata", dir_, true);
}

QString Tellico::fromHtmlData(const QByteArray& data_) {
  QTextCodec* codec = QTextCodec::codecForHtml(data_);
  return codec->toUnicode(data_);
}

Tellico::GUI::CursorSaver::CursorSaver(const QCursor& cursor_) : m_restored(false) {
  kapp->setOverrideCursor(cursor_);
}

Tellico::GUI::CursorSaver::~CursorSaver() {
  if(!m_restored) {
    kapp->restoreOverrideCursor();
  }
}

void Tellico::GUI::CursorSaver::restore() {
  kapp->restoreOverrideCursor();
  m_restored = true;
}
