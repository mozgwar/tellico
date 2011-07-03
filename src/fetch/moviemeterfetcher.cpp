/***************************************************************************
    Copyright (C) 2011 Robby Stephenson <robby@periapsis.org>
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

#include <config.h>
#include "moviemeterfetcher.h"
#include "../collections/videocollection.h"
#include "../images/imagefactory.h"
#include "../images/image.h"
#include "../entry.h"
#include "../tellico_debug.h"

#include <KLocale>
#include <KCodecs>

#include <QLabel>
#include <QFile>
#include <QTextStream>
#include <QGridLayout>
#include <QTextCodec>

#ifdef HAVE_KDEPIMLIBS
#include <kxmlrpcclient/client.h>
#endif

namespace {
  static const char* MOVIEMETER_API_KEY = "t80a06uf736d0yd00jpynpdsgea255yk";
  static const char* MOVIEMETER_URL = "http://www.moviemeter.nl/ws";
}

using namespace Tellico;
using Tellico::Fetch::MovieMeterFetcher;

MovieMeterFetcher::MovieMeterFetcher(QObject* parent_)
    : Fetcher(parent_), m_started(false), m_client(0) {
#ifdef HAVE_KDEPIMLIBS
  m_client = new KXmlRpc::Client(KUrl(MOVIEMETER_URL), this);
  m_client->setUserAgent(QLatin1String("Tellico"));
#endif
}

MovieMeterFetcher::~MovieMeterFetcher() {
}

QString MovieMeterFetcher::source() const {
  return m_name.isEmpty() ? defaultName() : m_name;
}

QString MovieMeterFetcher::attribution() const {
//  return i18n("This data is licensed under <a href=""%1"">specific terms</a>.")
//         .arg(QLatin1String("http://filmaster.com/license/"));
//  return QLatin1String("<a href=\"http://www.moviemeter.nl\">MovieMeter</a>");
  return QString();
}

bool MovieMeterFetcher::canSearch(FetchKey k) const {
#ifndef HAVE_KDEPIMLIBS
  return false;
#else
  return k == Person || k == Keyword;
#endif
}

bool MovieMeterFetcher::canFetch(int type) const {
  return type == Data::Collection::Video;
}

void MovieMeterFetcher::readConfigHook(const KConfigGroup&) {
}

void MovieMeterFetcher::search() {
  m_started = true;

#ifdef HAVE_KDEPIMLIBS
  m_client->call(QLatin1String("api.startSession"), QString::fromLatin1(MOVIEMETER_API_KEY),
                 this, SLOT(gotSession(const QList<QVariant>&, const QVariant&)),
                 this, SLOT(gotError(int, const QString&, const QVariant&)));
#else
  stop();
#endif
}

void MovieMeterFetcher::stop() {
  if(!m_started) {
    return;
  }
  m_started = false;
  m_currEntry.clear();
  emit signalDone(this);
}

Tellico::Data::EntryPtr MovieMeterFetcher::fetchEntryHook(uint uid_) {
#ifdef HAVE_KDEPIMLIBS
 // if the entry is not in the hash yet, we need to fetch it
  if(!m_entries.contains(uid_)) {
    if(!m_films.contains(uid_)) {
      myWarning() << "no entry in dict";
      return Data::EntryPtr();
    }
    if(!m_coll) {
      m_coll = new Data::VideoCollection(true);
    }
    m_currEntry = new Data::Entry(m_coll);
    m_coll->addEntries(m_currEntry);
    m_entries.insert(uid_, m_currEntry);
    m_client->call(QLatin1String("film.retrieveDetails"), QVariantList() << m_session << m_films[uid_],
                   this, SLOT(gotFilmDetails(const QList<QVariant>&, const QVariant&)),
                   this, SLOT(gotError(int, const QString&, const QVariant&)));
    m_loop.exec();
    m_client->call(QLatin1String("film.retrieveImage"), QVariantList() << m_session << m_films[uid_],
                   this, SLOT(gotFilmImage(const QList<QVariant>&, const QVariant&)),
                   this, SLOT(gotError(int, const QString&, const QVariant&)));
    m_loop.exec();
  }
#endif
  return m_entries.value(uid_);
}

Tellico::Fetch::FetchRequest MovieMeterFetcher::updateRequest(Data::EntryPtr entry_) {
  const QString title = entry_->field(QLatin1String("title"));
  if(!title.isEmpty()) {
    return FetchRequest(Keyword, title);
  }
  return FetchRequest();
}

void MovieMeterFetcher::gotSession(const QList<QVariant>& args_, const QVariant& result_) {
#ifdef HAVE_KDEPIMLIBS
  Q_UNUSED(result_);
  if(args_.isEmpty()) {
    stop();
    return;
  }
//  myDebug() << args_;
  const QVariantMap map = args_.first().toMap();
  m_session = map.value(QLatin1String("session_key")).toString();
//  myDebug() << m_session;
  const int valid_till = map.value(QLatin1String("valid_till")).toInt();
  m_validTill.setTime_t(valid_till);

  switch(request().key) {
    case Title:
    case Keyword:
      m_client->call(QLatin1String("film.search"), QVariantList() << m_session << request().value,
                     this, SLOT(gotFilmSearch(const QList<QVariant>&, const QVariant&)),
                     this, SLOT(gotError(int, const QString&, const QVariant&)));
      break;

    case Person:
      m_client->call(QLatin1String("director.search"), QVariantList() << m_session << request().value,
                     this, SLOT(gotDirectorSearch(const QList<QVariant>&, const QVariant&)),
                     this, SLOT(gotError(int, const QString&, const QVariant&)));
      break;

    default:
      stop();
      break;
  }
#endif
}

void MovieMeterFetcher::gotFilmSearch(const QList<QVariant>& args_, const QVariant& result_) {
  Q_UNUSED(result_);
  if(args_.isEmpty()) {
    stop();
    return;
  }
  foreach(const QVariant& v, args_.first().toList()) {
    const QVariantMap map = v.toMap();
    if(map.isEmpty()) {
      myDebug() << "empty map";
      break;
    }
    FetchResult* r = new FetchResult(Fetcher::Ptr(this),
                                     map.value(QLatin1String("title")).toString(),
                                     map.value(QLatin1String("year")).toString());
    m_films.insert(r->uid, map.value(QLatin1String("filmId")).toInt());
//    myDebug() << m_films.value(r->uid);
    emit signalResultFound(r);
  }
  stop();
}

void MovieMeterFetcher::gotFilmDetails(const QList<QVariant>& args_, const QVariant& result_) {
  Q_ASSERT(m_coll);
  Q_ASSERT(m_currEntry);
  Q_UNUSED(result_);
  if(args_.isEmpty() || !m_currEntry) {
    m_loop.quit();
    return;
  }

  const QVariantMap map = args_.first().toMap();
  m_currEntry->setField(QLatin1String("title"), map.value(QLatin1String("title")).toString());
  m_currEntry->setField(QLatin1String("year"), map.value(QLatin1String("year")).toString());
  QStringList genres;
  foreach(const QVariant& v, map.value(QLatin1String("genres")).toList()) {
    genres << v.toString();
  }
  m_currEntry->setField(QLatin1String("genre"), genres.join(FieldFormat::delimiterString()));
  QStringList actors;
  foreach(const QVariant& v, map.value(QLatin1String("actors")).toList()) {
    actors << v.toMap().value(QLatin1String("name")).toString();
  }
  m_currEntry->setField(QLatin1String("cast"), actors.join(FieldFormat::rowDelimiterString()));
  QStringList directors;
  foreach(const QVariant& v, map.value(QLatin1String("directors")).toList()) {
    directors << v.toMap().value(QLatin1String("name")).toString();
  }
  m_currEntry->setField(QLatin1String("director"), directors.join(FieldFormat::delimiterString()));
  m_currEntry->setField(QLatin1String("running-time"), map.value(QLatin1String("duration")).toString());
  m_currEntry->setField(QLatin1String("plot"), map.value(QLatin1String("plot")).toString());
  QStringList countries;
  foreach(const QVariant& v, map.value(QLatin1String("countries")).toList()) {
    countries << v.toMap().value(QLatin1String("name")).toString();
  }
  m_currEntry->setField(QLatin1String("nationality"), countries.join(FieldFormat::rowDelimiterString()));

  const QString mm = QLatin1String("moviemeter");
  if(!m_coll->hasField(mm) && optionalFields().contains(mm)) {
    Data::FieldPtr field(new Data::Field(mm, i18n("MovieMeter Link"), Data::Field::URL));
    field->setCategory(i18n("General"));
    m_coll->addField(field);
    m_currEntry->setField(mm, map.value(QLatin1String("url")).toString());
  }
  const QString alttitle = QLatin1String("alttitle");
  if(!m_coll->hasField(alttitle) && optionalFields().contains(alttitle)) {
    Data::FieldPtr field(new Data::Field(alttitle, i18n("Alternative Titles"), Data::Field::Table));
    field->setFormatType(FieldFormat::FormatTitle);
    m_coll->addField(field);
    QStringList alts;
    foreach(const QVariant& v, map.value(QLatin1String("alternative_titles")).toList()) {
      alts << v.toMap().value(QLatin1String("title")).toString();
    }
    m_currEntry->setField(alttitle, alts.join(FieldFormat::rowDelimiterString()));
  }

//  QMapIterator<QString, QVariant> i(map);
//  while(i.hasNext()) {
//    i.next();
//    myDebug() << i.key() << ":" << i.value();
//  }
  m_loop.quit();
}

void MovieMeterFetcher::gotFilmImage(const QList<QVariant>& args_, const QVariant& result_) {
  Q_ASSERT(m_coll);
  Q_ASSERT(m_currEntry);
  Q_UNUSED(result_);
  if(args_.isEmpty() || !m_currEntry) {
    m_loop.quit();
    return;
  }
  const QVariantMap map = args_.first().toMap().value(QLatin1String("image")).toMap();
  const QString base64 = map.value(QLatin1String("base64_encoded_contents")).toString();

  QByteArray ba;
  KCodecs::base64Decode(QByteArray(base64.toLatin1()), ba);
  if(!ba.isEmpty()) {
    QString result = ImageFactory::addImage(ba, QLatin1String("JPG"),
                                            Data::Image::calculateID(ba, QLatin1String("JPG")));
    if(!result.isEmpty()) {
      m_currEntry->setField(QLatin1String("cover"), result);
    }
  }

  m_loop.quit();
}

void MovieMeterFetcher::gotDirectorSearch(const QList<QVariant>& args_, const QVariant& result_) {
#ifdef HAVE_KDEPIMLIBS
  Q_UNUSED(result_);
  if(args_.isEmpty()) {
    stop();
    return;
  }
  foreach(const QVariant& v, args_.first().toList()) {
    m_client->call(QLatin1String("director.retrieveFilms"),
                   QVariantList() << m_session << v.toMap().value(QLatin1String("directorId")).toInt(),
                   this, SLOT(gotDirectorFilms(const QList<QVariant>&, const QVariant&)),
                   this, SLOT(gotError(int, const QString&, const QVariant&)));
    m_loop.exec();
  }
  stop();
#endif
}

void MovieMeterFetcher::gotDirectorFilms(const QList<QVariant>& args_, const QVariant& result_) {
  Q_ASSERT(m_loop.isRunning());
  Q_UNUSED(result_);
  if(args_.isEmpty()) {
    m_loop.quit();
    return;
  }
  foreach(const QVariant& v, args_.first().toList()) {
    const QVariantMap map = v.toMap();
    if(map.isEmpty()) {
      myDebug() << "empty map";
      break;
    }
    FetchResult* r = new FetchResult(Fetcher::Ptr(this),
                                     map.value(QLatin1String("title")).toString(),
                                     map.value(QLatin1String("year")).toString());
    m_films.insert(r->uid, map.value(QLatin1String("filmId")).toInt());
//    myDebug() << m_films.value(r->uid);
    emit signalResultFound(r);
  }
  m_loop.quit();
}

void MovieMeterFetcher::gotError(int err_, const QString& msg_, const QVariant& result_) {
  Q_UNUSED(err_);
  myDebug() << msg_;
  myDebug() << result_;
  if(m_loop.isRunning()) {
    m_loop.quit();
  } else {
    stop();
  }
}

Tellico::Fetch::ConfigWidget* MovieMeterFetcher::configWidget(QWidget* parent_) const {
  return new MovieMeterFetcher::ConfigWidget(parent_, this);
}

QString MovieMeterFetcher::defaultName() {
  return QLatin1String("MovieMeter"); // no translation
}

QString MovieMeterFetcher::defaultIcon() {
  return favIcon("http://www.moviemeter.nl");
}

Tellico::StringHash MovieMeterFetcher::allOptionalFields() {
  StringHash hash;
  hash[QLatin1String("moviemeter")] = i18n("MovieMeter Link");
  hash[QLatin1String("alttitle")]   = i18n("Alternative Titles");
  return hash;
}

MovieMeterFetcher::ConfigWidget::ConfigWidget(QWidget* parent_, const MovieMeterFetcher* fetcher_)
    : Fetch::ConfigWidget(parent_) {
  QVBoxLayout* l = new QVBoxLayout(optionsWidget());
  l->addWidget(new QLabel(i18n("This source has no options."), optionsWidget()));
  l->addStretch();

  // now add additional fields widget
  addFieldsWidget(MovieMeterFetcher::allOptionalFields(), fetcher_ ? fetcher_->optionalFields() : QStringList());
}

void MovieMeterFetcher::ConfigWidget::saveConfigHook(KConfigGroup&) {
}

QString MovieMeterFetcher::ConfigWidget::preferredName() const {
  return MovieMeterFetcher::defaultName();
}

#include "moviemeterfetcher.moc"
