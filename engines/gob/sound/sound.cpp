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
 * $URL$
 * $Id$
 *
 */

#include "gob/gob.h"
#include "gob/sound/sound.h"
#include "gob/global.h"
#include "gob/util.h"
#include "gob/dataio.h"
#include "gob/game.h"
#include "gob/inter.h"

namespace Gob {

Sound::Sound(GobEngine *vm) : _vm(vm) {
	_pcspeaker = new PCSpeaker(*_vm->_mixer);
	_blaster = new SoundBlaster(*_vm->_mixer);

	_adlib = 0;
	_infogrames = 0;
	_cdrom = 0;
	_bgatmos = 0;

	if (!_vm->_noMusic && _vm->hasAdlib())
		_adlib = new AdLib(*_vm->_mixer);
	if (!_vm->_noMusic && (_vm->getPlatform() == Common::kPlatformAmiga))
		_infogrames = new Infogrames(*_vm->_mixer);
	if (_vm->isCD())
		_cdrom = new CDROM;
	if (_vm->getGameType() == kGameTypeWoodruff)
		_bgatmos = new BackgroundAtmosphere(*_vm->_mixer);
}

Sound::~Sound() {
	delete _pcspeaker;
	delete _blaster;
	delete _adlib;
	delete _infogrames;
	delete _cdrom;
	delete _bgatmos;

	for (int i = 0; i < kSoundsCount; i++)
		_sounds[i].free();
}

void Sound::convToSigned(byte *buffer, int length) {
	while (length-- > 0)
		*buffer++ ^= 0x80;
}

SoundDesc *Sound::sampleGetBySlot(int slot) {
	if ((slot < 0) || (slot >= kSoundsCount))
		return 0;

	return &_sounds[slot];
}

const SoundDesc *Sound::sampleGetBySlot(int slot) const {
	if ((slot < 0) || (slot >= kSoundsCount))
		return 0;

	return &_sounds[slot];
}

int Sound::sampleGetNextFreeSlot() const {
	for (int i = 0; i < kSoundsCount; i++)
		if (_sounds[i].empty())
			return i;

	return -1;
}

bool Sound::sampleLoad(SoundDesc *sndDesc, const char *fileName, bool tryExist) {
	if (!sndDesc)
		return false;

	debugC(2, kDebugSound, "Loading sample \"%s\"", fileName);

	int16 handle = _vm->_dataIO->openData(fileName);
	if (handle < 0) {
		warning("Can't open sample file \"%s\"", fileName);
		return false;
	}

	_vm->_dataIO->closeData(handle);

	byte *data;
	uint32 size;

	data = (byte *) _vm->_dataIO->getData(fileName);
	if (!data)
		return false;

	size = _vm->_dataIO->getDataSize(fileName);
	sndDesc->load(SOUND_SND, SOUND_FILE, data, size);

	return true;
}

void Sound::sampleFree(SoundDesc *sndDesc, bool noteAdlib, int index) {
	if (!sndDesc || sndDesc->empty())
		return;

	if (sndDesc->getType() == SOUND_ADL) {

		if (_adlib && noteAdlib)
			if ((index == -1) || (_adlib->getIndex() == index))
				_adlib->stopPlay();

	} else {

		if (_blaster)
			_blaster->stopSound(0, sndDesc);

	}

	sndDesc->free();
}

void Sound::speakerOn(int16 frequency, int32 length) {
	if (!_pcspeaker)
		return;

	debugC(1, kDebugSound, "PCSpeaker: Playing tone (%d, %d)", frequency, length);

	_pcspeaker->speakerOn(frequency, length);
}

void Sound::speakerOff() {
	if (!_pcspeaker)
		return;

	debugC(1, kDebugSound, "PCSpeaker: Stopping tone");

	_pcspeaker->speakerOff();
}

void Sound::speakerOnUpdate(uint32 millis) {
	if (!_pcspeaker)
		return;

	_pcspeaker->onUpdate(millis);
}

bool Sound::infogramesLoadInstruments(const char *fileName) {
	if (!_infogrames)
		return false;

	debugC(1, kDebugSound, "Infogrames: Loading instruments \"%s\"", fileName);

	return _infogrames->loadInstruments(fileName);
}

bool Sound::infogramesLoadSong(const char *fileName) {
	if (!_infogrames)
		return false;

	debugC(1, kDebugSound, "Infogrames: Loading song \"%s\"", fileName);

	return _infogrames->loadSong(fileName);
}

void Sound::infogramesPlay() {
	if (!_infogrames)
		return;

	debugC(1, kDebugSound, "Infogrames: Starting playback");

	_infogrames->play();
}

void Sound::infogramesStop() {
	if (!_infogrames)
		return;

	debugC(1, kDebugSound, "Infogrames: Stopping playback");

	_infogrames->stop();
}

bool Sound::adlibLoad(const char *fileName) {
	if (!_adlib)
		return false;

	debugC(1, kDebugSound, "Adlib: Loading data (\"%s\")", fileName);

	return _adlib->load(fileName);
}

bool Sound::adlibLoad(byte *data, uint32 size, int index) {
	if (!_adlib)
		return false;

	debugC(1, kDebugSound, "Adlib: Loading data (%d)", index);

	return _adlib->load(data, size, index);
}

void Sound::adlibUnload() {
	if (!_adlib)
		return;

	debugC(1, kDebugSound, "Adlib: Unloading data");

	_adlib->unload();
}

void Sound::adlibPlayTrack(const char *trackname) {
	if (!_adlib || _adlib->isPlaying())
		return;

	debugC(1, kDebugSound, "Adlib: Playing track \"%s\"", trackname);

	_adlib->unload();
	_adlib->load(trackname);
	_adlib->startPlay();
}

void Sound::adlibPlayBgMusic() {
	if (!_adlib)
		return;

	static const char *tracks[] = {
//		"musmac1.adl", // TODO: This track isn't played correctly at all yet
		"musmac2.adl",
		"musmac3.adl",
		"musmac4.adl",
		"musmac5.adl",
		"musmac6.adl"
	};

	int track = _vm->_util->getRandom(ARRAYSIZE(tracks));
	adlibPlayTrack(tracks[track]);
}

void Sound::adlibPlay() {
	if (!_adlib)
		return;

	debugC(1, kDebugSound, "Adlib: Starting playback");

	_adlib->startPlay();
}

void Sound::adlibStop() {
	if (!_adlib)
		return;

	debugC(1, kDebugSound, "Adlib: Stopping playback");

	_adlib->stopPlay();
}

bool Sound::adlibIsPlaying() const {
	if (!_adlib)
		return false;

	return _adlib->isPlaying();
}

int Sound::adlibGetIndex() const {
	if (!_adlib)
		return -1;

	return _adlib->getIndex();
}

bool Sound::adlibGetRepeating() const {
	if (!_adlib)
		return false;

	return _adlib->getRepeating();
}

void Sound::adlibSetRepeating(int32 repCount) {
	if (!_adlib)
		return;

	_adlib->setRepeating(repCount);
}

void Sound::blasterPlay(SoundDesc *sndDesc, int16 repCount,
		int16 frequency, int16 fadeLength) {
	if (!_blaster || !sndDesc)
		return;

	debugC(1, kDebugSound, "SoundBlaster: Playing sample (%d, %d, %d)",
			repCount, frequency, fadeLength);

	_blaster->playSample(*sndDesc, repCount, frequency, fadeLength);
}

void Sound::blasterStop(int16 fadeLength, SoundDesc *sndDesc) {
	if (!_blaster)
		return;

	debugC(1, kDebugSound, "SoundBlaster: Stopping playback");

	_blaster->stopSound(fadeLength, sndDesc);
}

void Sound::blasterPlayComposition(int16 *composition, int16 freqVal,
		SoundDesc *sndDescs, int8 sndCount) {
	if (!_blaster)
		return;

	debugC(1, kDebugSound, "SoundBlaster: Playing composition (%d, %d)",
			freqVal, sndCount);

	blasterWaitEndPlay();
	_blaster->stopComposition();

	if (!sndDescs)
		sndDescs = _sounds;

	_blaster->playComposition(composition, freqVal, sndDescs, sndCount);
}

void Sound::blasterStopComposition() {
	if (!_blaster)
		return;

	debugC(1, kDebugSound, "SoundBlaster: Stopping composition");

	_blaster->stopComposition();
}

char Sound::blasterPlayingSound() const {
	if (!_blaster)
		return 0;

	return _blaster->getPlayingSound();
}

void Sound::blasterSetRepeating(int32 repCount) {
	if (!_blaster)
		return;

	_blaster->setRepeating(repCount);
}

void Sound::blasterWaitEndPlay(bool interruptible, bool stopComp) {
	if (!_blaster)
		return;

	debugC(1, kDebugSound, "SoundBlaster: Waiting for playback to end");

	if (stopComp)
		_blaster->endComposition();

	while (_blaster->isPlaying() && !_vm->quit()) {
		if (interruptible && (_vm->_util->checkKey() == 0x11B)) {
			WRITE_VAR(57, (uint32) -1);
			return;
		}
		_vm->_util->longDelay(200);
	}

	_blaster->stopSound(0);
}

void Sound::cdLoadLIC(const char *fname) {
	if (!_cdrom)
		return;

	debugC(1, kDebugSound, "CDROM: Loading LIC \"%s\"", fname);

	int handle = _vm->_dataIO->openData(fname);

	if (handle == -1)
		return;

	_vm->_dataIO->closeData(handle);

	_vm->_dataIO->getUnpackedData(fname);

	handle = _vm->_dataIO->openData(fname);
	DataStream *stream = _vm->_dataIO->openAsStream(handle, true);

	_cdrom->readLIC(*stream);

	delete stream;
}

void Sound::cdUnloadLIC() {
	if (!_cdrom)
		return;

	debugC(1, kDebugSound, "CDROM: Unloading LIC");

	_cdrom->freeLICBuffer();
}

void Sound::cdPlayBgMusic() {
	if (!_cdrom)
		return;

	static const char *tracks[][2] = {
		{"avt00.tot",  "mine"},
		{"avt001.tot", "nuit"},
		{"avt002.tot", "campagne"},
		{"avt003.tot", "extsor1"},
		{"avt004.tot", "interieure"},
		{"avt005.tot", "zombie"},
		{"avt006.tot", "zombie"},
		{"avt007.tot", "campagne"},
		{"avt008.tot", "campagne"},
		{"avt009.tot", "extsor1"},
		{"avt010.tot", "extsor1"},
		{"avt011.tot", "interieure"},
		{"avt012.tot", "zombie"},
		{"avt014.tot", "nuit"},
		{"avt015.tot", "interieure"},
		{"avt016.tot", "statue"},
		{"avt017.tot", "zombie"},
		{"avt018.tot", "statue"},
		{"avt019.tot", "mine"},
		{"avt020.tot", "statue"},
		{"avt021.tot", "mine"},
		{"avt022.tot", "zombie"}
	};

	for (int i = 0; i < ARRAYSIZE(tracks); i++)
		if (!scumm_stricmp(_vm->_game->_curTotFile, tracks[i][0])) {
			debugC(1, kDebugSound, "CDROM: Playing background music \"%s\" (\"%s\")",
					tracks[i][1], _vm->_game->_curTotFile);

			_cdrom->startTrack(tracks[i][1]);
			break;
		}
}

void Sound::cdPlayMultMusic() {
	if (!_cdrom)
		return;

	static const char *tracks[][6] = {
		{"avt005.tot", "fra1", "all1", "ang1", "esp1", "ita1"},
		{"avt006.tot", "fra2", "all2", "ang2", "esp2", "ita2"},
		{"avt012.tot", "fra3", "all3", "ang3", "esp3", "ita3"},
		{"avt016.tot", "fra4", "all4", "ang4", "esp4", "ita4"},
		{"avt019.tot", "fra5", "all5", "ang5", "esp5", "ita5"},
		{"avt022.tot", "fra6", "all6", "ang6", "esp6", "ita6"}
	};

	// Default to "ang?" for other languages (including EN_USA)
	int language = _vm->_global->_language <= 4 ? _vm->_global->_language : 2;
	for (int i = 0; i < ARRAYSIZE(tracks); i++)
		if (!scumm_stricmp(_vm->_game->_curTotFile, tracks[i][0])) {
			debugC(1, kDebugSound, "CDROM: Playing mult music \"%s\" (\"%s\")",
					tracks[i][language + 1], _vm->_game->_curTotFile);

			_cdrom->startTrack(tracks[i][language + 1]);
			break;
		}
}

void Sound::cdPlay(const char *trackName) {
	if (!_cdrom)
		return;

	debugC(1, kDebugSound, "CDROM: Playing track \"%s\"", trackName);
	_cdrom->startTrack(trackName);
}

void Sound::cdStop() {
	if (!_cdrom)
		return;

	debugC(1, kDebugSound, "CDROM: Stopping playback");
	_cdrom->stopPlaying();
}

bool Sound::cdIsPlaying() const {
	if (!_cdrom)
		return false;

	return _cdrom->isPlaying();
}

int32 Sound::cdGetTrackPos(const char *keyTrack) const {
	if (!_cdrom)
		return -1;

	return _cdrom->getTrackPos(keyTrack);
}

const char *Sound::cdGetCurrentTrack() const {
	if (!_cdrom)
		return "";

	return _cdrom->getCurTrack();
}

void Sound::cdTest(int trySubst, const char *label) {
	if (!_cdrom)
		return;

	_cdrom->testCD(trySubst, label);
}

void Sound::bgPlay(const char *base, int count) {
	if (!_bgatmos)
		return;

	debugC(1, kDebugSound, "BackgroundAtmosphere: Playing \"%s\" (%d)", base, count);

	_bgatmos->stop();
	_bgatmos->queueClear();

	int length = strlen(base) + 7;
	char *fileName = new char[length];
	SoundDesc *sndDesc;

	for (int i = 1; i <= count; i++) {
		snprintf(fileName, length, "%s%02d.SND", base, i);

		sndDesc = new SoundDesc;
		if (sampleLoad(sndDesc, fileName))
			_bgatmos->queueSample(*sndDesc);
	}

	_bgatmos->play();
}

void Sound::bgStop() {
	if (!_bgatmos)
		return;

	debugC(1, kDebugSound, "BackgroundAtmosphere: Stopping playback");

	_bgatmos->stop();
	_bgatmos->queueClear();
}

void Sound::bgSetPlayMode(BackgroundAtmosphere::PlayMode mode) {
	if (!_bgatmos)
		return;

	_bgatmos->setPlayMode(mode);
}

void Sound::bgShade() {
	if (!_bgatmos)
		return;

	debugC(1, kDebugSound, "BackgroundAtmosphere: Shading playback");

	_bgatmos->shade();
}

void Sound::bgUnshade() {
	if (!_bgatmos)
		return;

	debugC(1, kDebugSound, "BackgroundAtmosphere: Unshading playback");

	_bgatmos->unshade();
}

} // End of namespace Gob
