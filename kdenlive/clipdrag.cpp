/***************************************************************************
                          clipdrag.cpp  -  description
                             -------------------
    begin                : Mon Apr 15 2002
    copyright            : (C) 2002 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "clipdrag.h"
#include "docclipavfile.h"

#include <iostream>

ClipDrag::ClipDrag(DocClipBase *clip, QWidget *dragSource, const char *name) :
			KURLDrag(createURLList(clip), dragSource, name)
{
	m_xml = clip->toXML().toString();
}

ClipDrag::~ClipDrag()
{
}

/** Reimplemented for internal reasons; the API is not affected.  */
const char * ClipDrag::format(int i) const
{
	switch (i)
	{
		case 0		:	return "application/x-kdenlive-clip";
		default 	:	return KURLDrag::format(i-1);
	}
}

/** Reimplemented for internal reasons; the API is not affected.  */
QByteArray ClipDrag::encodedData(const char *mime) const
{
	QCString mimetype(mime);

	if(mimetype == "application/x-kdenlive-clip") {
		QByteArray encoded;
		encoded.resize(m_xml.length()+1);
		memcpy(encoded.data(), m_xml.utf8().data(), m_xml.length()+1);
		return encoded;
	} else {
		return KURLDrag::encodedData(mime);
	}
}

/** Set the clip which is contained within this ClipDrag object. */
void ClipDrag::setClip(DocClipBase *clip)
{
}

/** Returns true if the mime type is decodable, false otherwise. */
bool ClipDrag::canDecode(const QMimeSource *mime)
{
  if(mime->provides("application/x-kdenlive-clip")) return true;

  return KURLDrag::canDecode(mime);
}

/** Attempts to decode the mimetype e as a clip. Returns a clip, or returns null */
QPtrList<DocClipBase> ClipDrag::decode(const QMimeSource *e)
{
	QPtrList<DocClipBase> cliplist;
 	cliplist.setAutoDelete(false);
	
	if(e->provides("application/x-kdenlive-clip")) {
		QByteArray data = e->encodedData("application/x-kdenlive-clip");
		QString xml = data;
		QDomDocument doc;
		doc.setContent(data);

		QDomNode node = doc.firstChild();

		while(!node.isNull()) {
			QDomElement element = node.toElement();

			if(!element.isNull()) {
				if(element.tagName() == "clip") {
					DocClipBase *temp = DocClipBase::createClip(element);
					cliplist.append(temp);
				}
			}
   		node = node.nextSibling();
		}
	} else {
   	KURL::List list;
		KURL::List::Iterator it;
		KURLDrag::decode(e, list);

		for(it = list.begin(); it != list.end(); ++it) {
			DocClipAVFile *file = new DocClipAVFile((*it).fileName(), *it);
			cliplist.append(file);
		}
	}

	return cliplist;
}

/** Returns a QValueList containing the URL of the clip.

This is necessary, because the KURLDrag class which ClipDrag inherits
requires a list of URL's rather than a single URL. */
KURL::List ClipDrag::createURLList(DocClipBase *clip)
{
	KURL::List list;
	
 	list.append(clip->fileURL());
	return list;
}
