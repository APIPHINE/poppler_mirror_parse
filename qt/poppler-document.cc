/* poppler-document.cc: qt interface to poppler
 * Copyright (C) 2005, Net Integration Technologies, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <poppler-qt.h>
#include <qfile.h>
#include <qimage.h>
#include <GlobalParams.h>
#include <PDFDoc.h>
#include <Catalog.h>
#include <ErrorCodes.h>
#include <SplashOutputDev.h>
#include <splash/SplashBitmap.h>
#include "poppler-private.h"

namespace Poppler {

Document *Document::load(const QString &filePath)
{
  if (!globalParams) {
    globalParams = new GlobalParams("/etc/xpdfrc");
    globalParams->setupBaseFontsFc(NULL);
  }

  DocumentData *doc = new DocumentData(new GooString(QFile::encodeName(filePath)), NULL);
  Document *pdoc;
  if (doc->doc.isOk() || doc->doc.getErrorCode() == errEncrypted) {
    pdoc = new Document(doc);
    if (doc->doc.getErrorCode() == errEncrypted)
      pdoc->data->locked = true;
    else
      pdoc->data->locked = false;
    return pdoc;
  }
  else
    return NULL;
}

Document::Document(DocumentData *dataA)
{
  data = dataA;
}

Document::~Document()
{
  delete data;
}

bool Document::isLocked()
{
  return data->locked;
}

bool Document::unlock(QCString &password)
{
  if (data->locked) {
    /* racier then it needs to be */
    GooString *pwd = new GooString(password.data());
    DocumentData *doc2 = new DocumentData(data->doc.getFileName(), pwd);
    delete pwd;
    if (!doc2->doc.isOk()) {
      delete doc2;
    } else {
      delete data;
      data = doc2;
      data->locked = false;
    }
  }
  return data->locked;
}

Document::PageMode Document::getPageMode(void)
{
  switch (data->doc.getCatalog()->getPageMode()) {
    case Catalog::pageModeNone:
      return UseNone;
    case Catalog::pageModeOutlines:
      return UseOutlines;
    case Catalog::pageModeThumbs:
      return UseThumbs;
    case Catalog::pageModeFullScreen:
      return FullScreen;
    case Catalog::pageModeOC:
      return UseOC;
    default:
      return UseNone;
  }
}

int Document::getNumPages()
{
  return data->doc.getNumPages();
}

/* borrowed from kpdf */
static QString unicodeToQString(Unicode* u, int len) {
  QString ret;
  ret.setLength(len);
  QChar* qch = (QChar*) ret.unicode();
  for (;len;--len)
    *qch++ = (QChar) *u++;
  return ret;
}

/* borrowed from kpdf */
QString Document::getInfo( const QString & type ) const
{
  // [Albert] Code adapted from pdfinfo.cc on xpdf
  Object info;
  if ( data->locked )
    return NULL;

  data->doc.getDocInfo( &info );
  if ( !info.isDict() )
    return NULL;

  QString result;
  Object obj;
  GooString *s1;
  GBool isUnicode;
  Unicode u;
  int i;
  Dict *infoDict = info.getDict();

  if ( infoDict->lookup( (char*)type.latin1(), &obj )->isString() )
  {
    s1 = obj.getString();
    if ( ( s1->getChar(0) & 0xff ) == 0xfe && ( s1->getChar(1) & 0xff ) == 0xff )
    {
      isUnicode = gTrue;
      i = 2;
    }
    else
    {
      isUnicode = gFalse;
      i = 0;
    }
    while ( i < obj.getString()->getLength() )
    {
      if ( isUnicode )
      {
	u = ( ( s1->getChar(i) & 0xff ) << 8 ) | ( s1->getChar(i+1) & 0xff );
	i += 2;
      }
      else
      {
	u = s1->getChar(i) & 0xff;
	++i;
      }
      result += unicodeToQString( &u, 1 );
    }
    obj.free();
    info.free();
    return result;
  }
  obj.free();
  info.free();
  return NULL;
}

/* borrowed from kpdf */
QDateTime Document::getDate( const QString & type ) const
{
  // [Albert] Code adapted from pdfinfo.cc on xpdf
  if ( data->locked )
    return QDateTime();

  Object info;
  data->doc.getDocInfo( &info );
  if ( !info.isDict() ) {
    info.free();
    return QDateTime();
  }

  Object obj;
  char *s;
  int year, mon, day, hour, min, sec;
  Dict *infoDict = info.getDict();
  QString result;

  if ( infoDict->lookup( (char*)type.latin1(), &obj )->isString() )
  {
    s = obj.getString()->getCString();
    if ( s[0] == 'D' && s[1] == ':' )
      s += 2;
    /* FIXME process time zone on systems that support it */  
    if ( sscanf( s, "%4d%2d%2d%2d%2d%2d", &year, &mon, &day, &hour, &min, &sec ) == 6 )
    {
      /* Workaround for y2k bug in Distiller 3 stolen from gpdf, hoping that it won't
       *   * be used after y2.2k */
      if ( year < 1930 && strlen (s) > 14) {
	int century, years_since_1900;
	if ( sscanf( s, "%2d%3d%2d%2d%2d%2d%2d",
	    &century, &years_since_1900,
	    &mon, &day, &hour, &min, &sec) == 7 )
	  year = century * 100 + years_since_1900;
	else {
	  obj.free();
	  info.free();
	  return QDateTime();
	}
      }

      QDate d( year, mon, day );  //CHECK: it was mon-1, Jan->0 (??)
      QTime t( hour, min, sec );
      if ( d.isValid() && t.isValid() ) {
	obj.free();
	info.free();
	return QDateTime( d, t );
      }
    }
  }
  obj.free();
  info.free();
  return QDateTime();
}

}
