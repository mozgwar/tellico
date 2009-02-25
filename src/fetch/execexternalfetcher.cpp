/***************************************************************************
    copyright            : (C) 2005-2008 by Robby Stephenson
    email                : robby@periapsis.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of version 2 of the GNU General Public License as  *
 *   published by the Free Software Foundation;                            *
 *                                                                         *
 ***************************************************************************/

#include "execexternalfetcher.h"
#include "messagehandler.h"
#include "fetchmanager.h"
#include "../collection.h"
#include "../entry.h"
#include "../importdialog.h"
#include "../translators/tellicoimporter.h"
#include "../tellico_debug.h"
#include "../gui/combobox.h"
#include "../gui/lineedit.h"
#include "../gui/collectiontypecombo.h"
#include "../tellico_utils.h"
//#include "../newstuff/manager.h"

#include <klocale.h>
#include <kprocess.h>
#include <kurlrequester.h>
#include <kacceleratormanager.h>
#include <kshell.h>
#include <KConfigGroup>

#include <QLabel>
#include <QRegExp>
#include <QGroupBox>
#include <QGridLayout>

using Tellico::Fetch::ExecExternalFetcher;

QStringList ExecExternalFetcher::parseArguments(const QString& str_) {
  // matching escaped quotes is too hard... :(
//  QRegExp quotes(QLatin1String("[^\\\\](['\"])(.*[^\\\\])\\1"));
  QRegExp quotes(QLatin1String("(['\"])(.*)\\1"));
  quotes.setMinimal(true);
  QRegExp spaces(QLatin1String("\\s+"));
  spaces.setMinimal(true);

  QStringList args;
  int pos = 0;
  for(int nextPos = quotes.indexIn(str_); nextPos > -1; pos = nextPos+1, nextPos = quotes.indexIn(str_, pos)) {
    // a non-quotes arguments runs from pos to nextPos
    args += str_.mid(pos, nextPos-pos).split(spaces, QString::SkipEmptyParts);
    // move nextpos marker to end of match
    pos = quotes.pos(2); // skip quotation mark
    nextPos += quotes.matchedLength();
    args += str_.mid(pos, nextPos-pos-1);
  }
  // catch the end stuff
  args += str_.mid(pos).split(spaces, QString::SkipEmptyParts);

#if 0
  for(QStringList::ConstIterator it = args.begin(); it != args.end(); ++it) {
    myDebug() << *it << endl;
  }
#endif

  return args;
}

ExecExternalFetcher::ExecExternalFetcher(QObject* parent_) : Fetcher(parent_),
    m_started(false), m_collType(-1), m_formatType(-1), m_canUpdate(false), m_process(0), m_deleteOnRemove(false) {
}

ExecExternalFetcher::~ExecExternalFetcher() {
  stop();
}

QString ExecExternalFetcher::defaultName() {
  return i18n("External Application");
}

QString ExecExternalFetcher::source() const {
  return m_name;
}

bool ExecExternalFetcher::canFetch(int type_) const {
  return m_collType == -1 ? false : m_collType == type_;
}

void ExecExternalFetcher::readConfigHook(const KConfigGroup& config_) {
  QString s = config_.readPathEntry("ExecPath", QString());
  if(!s.isEmpty()) {
    m_path = s;
  }
  QList<int> il;
  if(config_.hasKey("ArgumentKeys")) {
    il = config_.readEntry("ArgumentKeys", il);
  } else {
    il.append(Keyword);
  }
  QStringList sl = config_.readEntry("Arguments", QStringList());
  if(il.count() != sl.count()) {
    kWarning() << "ExecExternalFetcher::readConfig() - unequal number of arguments and keys";
  }
  int n = qMin(il.count(), sl.count());
  for(int i = 0; i < n; ++i) {
    m_args[static_cast<FetchKey>(il[i])] = sl[i];
  }
  if(config_.hasKey("UpdateArgs")) {
    m_canUpdate = true;
    m_updateArgs = config_.readEntry("UpdateArgs");
  } else {
    m_canUpdate = false;
  }
  m_collType = config_.readEntry("CollectionType", -1);
  m_formatType = config_.readEntry("FormatType", -1);
  m_deleteOnRemove = config_.readEntry("DeleteOnRemove", false);
  m_newStuffName = config_.readEntry("NewStuffName");
}

void ExecExternalFetcher::search(Tellico::Fetch::FetchKey key_, const QString& value_) {
  m_started = true;

  if(!m_args.contains(key_)) {
    stop();
    return;
  }

  // should KShell::quoteArg() be used?
  // %1 gets replaced by the search value, but since the arguments are going to be split
  // the search value needs to be enclosed in quotation marks
  // but first check to make sure the user didn't do that already
  // AND the "%1" wasn't used in the settings
  QString value = value_;
  if(key_ == ISBN) {
    value.remove(QLatin1Char('-')); // remove hyphens from isbn values
    // shouldn't hurt and might keep from confusing stupid search sources
  }
  QRegExp rx1(QLatin1String("['\"].*\\1"));
  if(!rx1.exactMatch(value)) {
    value = QLatin1Char('"') + value + QLatin1Char('"');
  }
  QString args = m_args[key_];
  QRegExp rx2(QLatin1String("['\"]%1\\1"));
  args.replace(rx2, QLatin1String("%1"));
  startSearch(parseArguments(args.arg(value))); // replace %1 with search value
}

void ExecExternalFetcher::startSearch(const QStringList& args_) {
  if(m_path.isEmpty()) {
    stop();
    return;
  }

#if 0
  myDebug() << m_path << endl;
  for(QStringList::ConstIterator it = args_.begin(); it != args_.end(); ++it) {
    myDebug() << "  " << *it << endl;
  }
#endif

  m_process = new KProcess();
  connect(m_process, SIGNAL(readyReadStandardOutput()), SLOT(slotData()));
  connect(m_process, SIGNAL(readyReadStandardError()), SLOT(slotError()));
  connect(m_process, SIGNAL(finished(int, QProcess::ExitStatus)), SLOT(slotProcessExited()));
  m_process->setOutputChannelMode(KProcess::SeparateChannels);
  m_process->setProgram(m_path, args_);
  if(m_process->execute() < 0) {
    myDebug() << "ExecExternalFetcher::startSearch() - process failed to start" << endl;
    stop();
  }
}

void ExecExternalFetcher::stop() {
  if(!m_started) {
    return;
  }
  if(m_process) {
    m_process->kill();
    delete m_process;
    m_process = 0;
  }
  m_data.clear();
  m_started = false;
  m_errors.clear();
  emit signalDone(this);
}

void ExecExternalFetcher::slotData() {
  m_data.append(m_process->readAllStandardOutput());
}

void ExecExternalFetcher::slotError() {
  GUI::CursorSaver cs(Qt::ArrowCursor);
  QString msg = QString::fromLocal8Bit(m_process->readAllStandardError());
  msg.prepend(source() + QLatin1String(": "));
  if(msg.endsWith(QChar::fromLatin1('\n'))) {
    msg.truncate(msg.length()-1);
  }
  myDebug() << "ExecExternalFetcher::slotError() - " << msg << endl;
  m_errors << msg;
}

void ExecExternalFetcher::slotProcessExited() {
//  myDebug() << "ExecExternalFetcher::slotProcessExited()" << endl;
  if(m_process->exitStatus() != QProcess::NormalExit || m_process->exitCode() != 0) {
    myDebug() << "ExecExternalFetcher::slotProcessExited() - "<< source() << ": process did not exit successfully" << endl;
    if(!m_errors.isEmpty()) {
      message(m_errors.join(QChar::fromLatin1('\n')), MessageHandler::Error);
    }
    stop();
    return;
  }
  if(!m_errors.isEmpty()) {
    message(m_errors.join(QChar::fromLatin1('\n')), MessageHandler::Warning);
  }

  if(m_data.isEmpty()) {
    myDebug() << "ExecExternalFetcher::slotProcessExited() - "<< source() << ": no data" << endl;
    stop();
    return;
  }

  Import::Format format = static_cast<Import::Format>(m_formatType > -1 ? m_formatType : Import::TellicoXML);
  Import::Importer* imp = ImportDialog::importer(format, KUrl::List());
  if(!imp) {
    stop();
    return;
  }

  imp->setText(QString::fromUtf8(m_data, m_data.size()));
  Data::CollPtr coll = imp->collection();
  if(!coll) {
    if(!imp->statusMessage().isEmpty()) {
      message(imp->statusMessage(), MessageHandler::Status);
    }
    myDebug() << "ExecExternalFetcher::slotProcessExited() - "<< source() << ": no collection pointer" << endl;
    delete imp;
    stop();
    return;
  }

  delete imp;
  if(coll->entryCount() == 0) {
//    myDebug() << "ExecExternalFetcher::slotProcessExited() - no results" << endl;
    stop();
    return;
  }

  Data::EntryList entries = coll->entries();
  foreach(Data::EntryPtr entry, entries) {
    QString desc;
    switch(coll->type()) {
      case Data::Collection::Book:
      case Data::Collection::Bibtex:
        desc = entry->field(QLatin1String("author"))
               + QLatin1Char('/')
               + entry->field(QLatin1String("publisher"));
        if(!entry->field(QLatin1String("cr_year")).isEmpty()) {
          desc += QLatin1Char('/') + entry->field(QLatin1String("cr_year"));
        } else if(!entry->field(QLatin1String("pub_year")).isEmpty()){
          desc += QLatin1Char('/') + entry->field(QLatin1String("pub_year"));
        }
        break;

      case Data::Collection::Video:
        desc = entry->field(QLatin1String("studio"))
               + QLatin1Char('/')
               + entry->field(QLatin1String("director"))
               + QLatin1Char('/')
               + entry->field(QLatin1String("year"))
               + QLatin1Char('/')
               + entry->field(QLatin1String("medium"));
        break;

      case Data::Collection::Album:
        desc = entry->field(QLatin1String("artist"))
               + QLatin1Char('/')
               + entry->field(QLatin1String("label"))
               + QLatin1Char('/')
               + entry->field(QLatin1String("year"));
        break;

      case Data::Collection::Game:
        desc = entry->field(QLatin1String("platform"));
        break;

      case Data::Collection::ComicBook:
        desc = entry->field(QLatin1String("publisher"))
               + QLatin1Char('/')
               + entry->field(QLatin1String("pub_year"));
        break;

     case Data::Collection::BoardGame:
       desc = entry->field(QLatin1String("designer"))
              + QLatin1Char('/')
              + entry->field(QLatin1String("publisher"))
              + QLatin1Char('/')
              + entry->field(QLatin1String("year"));
       break;

      default:
        break;
    }
    SearchResult* r = new SearchResult(Fetcher::Ptr(this), entry->title(), desc, entry->field(QLatin1String("isbn")));
    m_entries.insert(r->uid, entry);
    emit signalResultFound(r);
  }
  stop(); // be sure to call this
}

Tellico::Data::EntryPtr ExecExternalFetcher::fetchEntry(uint uid_) {
  return m_entries[uid_];
}

void ExecExternalFetcher::updateEntry(Tellico::Data::EntryPtr entry_) {
  if(!m_canUpdate) {
    emit signalDone(this); // must do this
  }

  m_started = true;

  QStringList args = parseArguments(m_updateArgs);
  for(QStringList::Iterator it = args.begin(); it != args.end(); ++it) {
    *it = Data::Entry::dependentValue(entry_.data(), *it, false);
  }
  startSearch(args);
}

Tellico::Fetch::ConfigWidget* ExecExternalFetcher::configWidget(QWidget* parent_) const {
  return new ExecExternalFetcher::ConfigWidget(parent_, this);
}

ExecExternalFetcher::ConfigWidget::ConfigWidget(QWidget* parent_, const ExecExternalFetcher* fetcher_/*=0*/)
    : Fetch::ConfigWidget(parent_), m_deleteOnRemove(false) {
  QGridLayout* l = new QGridLayout(optionsWidget());
  l->setSpacing(4);
  l->setColumnStretch(1, 10);

  int row = -1;

  QLabel* label = new QLabel(i18n("Collection &type:"), optionsWidget());
  l->addWidget(label, ++row, 0);
  m_collCombo = new GUI::CollectionTypeCombo(optionsWidget());
  connect(m_collCombo, SIGNAL(activated(int)), SLOT(slotSetModified()));
  l->addWidget(m_collCombo, row, 1);
  QString w = i18n("Set the collection type of the data returned from the external application.");
  label->setWhatsThis(w);
  m_collCombo->setWhatsThis(w);
  label->setBuddy(m_collCombo);

  label = new QLabel(i18n("&Result type: "), optionsWidget());
  l->addWidget(label, ++row, 0);
  m_formatCombo = new GUI::ComboBox(optionsWidget());
  Import::FormatMap formatMap = ImportDialog::formatMap();
  for(Import::FormatMap::Iterator it = formatMap.begin(); it != formatMap.end(); ++it) {
    if(ImportDialog::formatImportsText(it.key())) {
      m_formatCombo->addItem(it.value(), it.key());
    }
  }
  connect(m_formatCombo, SIGNAL(activated(int)), SLOT(slotSetModified()));
  l->addWidget(m_formatCombo, row, 1);
  w = i18n("Set the result type of the data returned from the external application.");
  label->setWhatsThis(w);
  m_formatCombo->setWhatsThis(w);
  label->setBuddy(m_formatCombo);

  label = new QLabel(i18n("Application &path: "), optionsWidget());
  l->addWidget(label, ++row, 0);
  m_pathEdit = new KUrlRequester(optionsWidget());
  connect(m_pathEdit, SIGNAL(textChanged(const QString&)), SLOT(slotSetModified()));
  l->addWidget(m_pathEdit, row, 1);
  w = i18n("Set the path of the application to run that should output a valid Tellico data file.");
  label->setWhatsThis(w);
  m_pathEdit->setWhatsThis(w);
  label->setBuddy(m_pathEdit);

  w = i18n("Select the search keys supported by the data source.");
  QString w2 = i18n("Add any arguments that may be needed. <b>%1</b> will be replaced by the search term.");
  QGroupBox* gbox = new QGroupBox(i18n("Arguments"), optionsWidget());
  ++row;
  l->addWidget(gbox, row, 0, 1, 2);
  QGridLayout* gridLayout = new QGridLayout(gbox);
  gridLayout->setSpacing(2);
  row = -1;
  const Fetch::KeyMap keyMap = Fetch::Manager::self()->keyMap();
  for(Fetch::KeyMap::ConstIterator it = keyMap.begin(); it != keyMap.end(); ++it) {
    FetchKey key = it.key();
    if(key == Raw) {
      continue;
    }
    QCheckBox* cb = new QCheckBox(it.value(), gbox);
    gridLayout->addWidget(cb, ++row, 0);
    m_cbDict.insert(key, cb);
    GUI::LineEdit* le = new GUI::LineEdit(gbox);
    le->setClickMessage(QLatin1String("%1")); // for example
    le->completionObject()->addItem(QLatin1String("%1"));
    gridLayout->addWidget(le, row, 1);
    m_leDict.insert(key, le);
    if(fetcher_ && fetcher_->m_args.contains(key)) {
      cb->setChecked(true);
      le->setEnabled(true);
      le->setText(fetcher_->m_args[key]);
    } else {
      cb->setChecked(false);
      le->setEnabled(false);
    }
    connect(cb, SIGNAL(toggled(bool)), le, SLOT(setEnabled(bool)));
    cb->setWhatsThis(w);
    le->setWhatsThis(w2);
  }
  m_cbUpdate = new QCheckBox(i18n("Update"), gbox);
  gridLayout->addWidget(m_cbUpdate, ++row, 0);
  m_leUpdate = new GUI::LineEdit(gbox);
  m_leUpdate->setClickMessage(QLatin1String("%{title}")); // for example
  m_leUpdate->completionObject()->addItem(QLatin1String("%{title}"));
  m_leUpdate->completionObject()->addItem(QLatin1String("%{isbn}"));
  gridLayout->addWidget(m_leUpdate, row, 1);
  /* TRANSLATORS: Do not translate %{author}. */
  w2 = i18n("<p>Enter the arguments which should be used to search for available updates to an entry.</p><p>"
           "The format is the same as for <i>Dependent</i> fields, where field values "
           "are contained inside braces, such as <i>%{author}</i>. See the documentation for details.</p>");
  m_cbUpdate->setWhatsThis(w);
  m_leUpdate->setWhatsThis(w2);
  if(fetcher_ && fetcher_->m_canUpdate) {
    m_cbUpdate->setChecked(true);
    m_leUpdate->setEnabled(true);
    m_leUpdate->setText(fetcher_->m_updateArgs);
  } else {
    m_cbUpdate->setChecked(false);
    m_leUpdate->setEnabled(false);
  }
  connect(m_cbUpdate, SIGNAL(toggled(bool)), m_leUpdate, SLOT(setEnabled(bool)));

  l->setRowStretch(++row, 1);

  if(fetcher_) {
    m_pathEdit->setUrl(fetcher_->m_path);
    m_newStuffName = fetcher_->m_newStuffName;
  }
  if(fetcher_ && fetcher_->m_collType > -1) {
    m_collCombo->setCurrentType(fetcher_->m_collType);
  } else {
    m_collCombo->setCurrentType(Data::Collection::Book);
  }
  if(fetcher_ && fetcher_->m_formatType > -1) {
    m_formatCombo->setCurrentItem(formatMap[static_cast<Import::Format>(fetcher_->m_formatType)]);
  } else {
    m_formatCombo->setCurrentItem(formatMap[Import::TellicoXML]);
  }
  m_deleteOnRemove = fetcher_ && fetcher_->m_deleteOnRemove;
  KAcceleratorManager::manage(optionsWidget());
}

ExecExternalFetcher::ConfigWidget::~ConfigWidget() {
}

void ExecExternalFetcher::ConfigWidget::readConfig(const KConfigGroup& config_) {
  m_pathEdit->setUrl(KUrl(config_.readPathEntry("ExecPath", QString())));
  QList<int> argKeys = config_.readEntry("ArgumentKeys", QList<int>());
  QStringList argValues = config_.readEntry("Arguments", QStringList());
  if(argKeys.count() != argValues.count()) {
    kWarning() << "ExecExternalFetcher::ConfigWidget::readConfig() - unequal number of arguments and keys";
  }
  int n = qMin(argKeys.count(), argValues.count());
  QMap<FetchKey, QString> args;
  for(int i = 0; i < n; ++i) {
    args[static_cast<FetchKey>(argKeys[i])] = argValues[i];
  }
  for(QList<int>::Iterator it = argKeys.begin(); it != argKeys.end(); ++it) {
    if(*it == Raw) {
      continue;
    }
    FetchKey key = static_cast<FetchKey>(*it);
    QCheckBox* cb = m_cbDict[key];
    KLineEdit* le = m_leDict[key];
    if(cb && le) {
      if(args.contains(key)) {
        cb->setChecked(true);
        le->setEnabled(true);
        le->setText(args[key]);
      } else {
        cb->setChecked(false);
        le->setEnabled(false);
        le->clear();
      }
    }
  }

  if(config_.hasKey("UpdateArgs")) {
    m_cbUpdate->setChecked(true);
    m_leUpdate->setEnabled(true);
    m_leUpdate->setText(config_.readEntry("UpdateArgs"));
  } else {
    m_cbUpdate->setChecked(false);
    m_leUpdate->setEnabled(false);
    m_leUpdate->clear();
  }

  int collType = config_.readEntry("CollectionType", -1);
  m_collCombo->setCurrentType(collType);

  Import::FormatMap formatMap = ImportDialog::formatMap();
  int formatType = config_.readEntry("FormatType", -1);
  m_formatCombo->setCurrentItem(formatMap[static_cast<Import::Format>(formatType)]);
  m_deleteOnRemove = config_.readEntry("DeleteOnRemove", false);
  m_name = config_.readEntry("Name");
  m_newStuffName = config_.readEntry("NewStuffName");
}

void ExecExternalFetcher::ConfigWidget::saveConfig(KConfigGroup& config_) {
        KUrl u = m_pathEdit->url();
  if(!u.isEmpty()) {
    config_.writePathEntry("ExecPath", u.path());
  }
  QList<int> keys;
  QStringList args;
  QHash<int, QCheckBox*>::const_iterator it = m_cbDict.constBegin();
  for( ; it != m_cbDict.constEnd(); ++it) {
    if(it.value()->isChecked()) {
      keys << it.key();
      args << m_leDict[it.key()]->text();
    }
  }
  config_.writeEntry("ArgumentKeys", keys);
  config_.writeEntry("Arguments", args);

  if(m_cbUpdate->isChecked()) {
    config_.writeEntry("UpdateArgs", m_leUpdate->text());
  } else {
    config_.deleteEntry("UpdateArgs");
  }

  config_.writeEntry("CollectionType", m_collCombo->currentType());
  config_.writeEntry("FormatType", m_formatCombo->currentData().toInt());
  config_.writeEntry("DeleteOnRemove", m_deleteOnRemove);
  if(!m_newStuffName.isEmpty()) {
    config_.writeEntry("NewStuffName", m_newStuffName);
  }
  slotSetModified(false);
}

void ExecExternalFetcher::ConfigWidget::removed() {
  if(!m_deleteOnRemove) {
    return;
  }
#if 0
if(!m_newStuffName.isEmpty()) {
    NewStuff::Manager man(this);
    man.removeScript(m_newStuffName);
  }
#endif
}

QString ExecExternalFetcher::ConfigWidget::preferredName() const {
  return m_name.isEmpty() ? ExecExternalFetcher::defaultName() : m_name;
}

#include "execexternalfetcher.moc"
