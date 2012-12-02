/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef XEEN_ARCHIVE_TOC_H
#define XEEN_ARCHIVE_TOC_H

#include "common/scummsys.h"
#include "common/stream.h"

#include "xeen/utility.h"

namespace XEEN
{
    struct CCFileEntry
    {
        uint16 id;
        uint32 offset;
        uint16 size;
        uint8 padding;
    };
    
    class Toc : public Validateable
    {
        public:
            Toc();
            virtual ~Toc();

            const CCFileEntry* getEntry(CCFileId id) const;

        protected:
            void readToc(Common::SeekableReadStream& data);

        protected:
            uint16 _entryCount;
            CCFileEntry* _entries;            
    };
}

#endif // XEEN_ARCHIVE_TOC_H
