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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef TITANIC_SOUND_MANAGER_H
#define TITANIC_SOUND_MANAGER_H

#include "titanic/core/list.h"
#include "titanic/support/simple_file.h"
#include "titanic/sound/proximity.h"
#include "titanic/sound/qmixer.h"
#include "titanic/sound/wave_file.h"
#include "titanic/true_talk/dialogue_file.h"

namespace Titanic {

/**
 * Abstract interface class for a sound manager
 */
class CSoundManager {
protected:
	double _musicPercent;
	double _speechPercent;
	double _masterPercent;
	double _parrotPercent;
	uint _handleCtr;
public:
	CSoundManager();
	virtual ~CSoundManager() {}

	/**
	 * Loads a sound
	 * @param name		Name of sound resource
	 * @returns			Loaded wave file
	 */
	virtual CWaveFile *loadSound(const CString &name) { return nullptr; }

	/**
	 * Loads a speech resource from a dialogue file
	 * @param name		Name of sound resource
	 * @returns			Loaded wave file
	 */
	virtual CWaveFile *loadSpeech(CDialogueFile *dialogueFile, int speechId) { return 0; }

	/**
	 * Loads a music file
	 * @param name		Name of music resource
	 * @returns			Loaded wave file
	 * @remarks The original created a streaming audio buffer for the wave file,
	 *		and passed this to the method. For ScummVM, this has been discarded
	 *		in favor of simply passing the filename.
	 */
	virtual CWaveFile *loadMusic(const CString &name) { return nullptr; }

	/**
	 * Start playing a previously loaded wave file
	 */
	virtual int playSound(CWaveFile &waveFile, CProximity &prox) = 0;

	/**
	 * Stop playing the specified sound
	 */
	virtual void stopSound(int handle) = 0;

	/**
	 * Stops a designated range of channels
	 */
	virtual void stopChannel(int channel) = 0;

	virtual void proc9(int handle) {}

	/**
	 * Stops sounds on all playing channels
	 */
	virtual void stopAllChannels() = 0;

	/**
	 * Sets the volume for a sound
	 * @param handle	Handle for sound
	 * @param volume	New volume
	 * @param seconds	Number of seconds to transition to the new volume
	 */
	virtual void setVolume(int handle, uint volume, uint seconds) = 0;

	/**
	 * Set the position for a sound
	 * @param handle	Handle for sound
	 * @param x			x position in metres
	 * @param y			y position in metres
	 * @param z			z position in metres
	 * @param panRate	Rate in milliseconds to transition
	 */
	virtual void setVectorPosition(int handle, double x, double y, double z, uint panRate) {}

	/**
	 * Set the position for a sound
	 * @param handle	Handle for sound
	 * @param range		Range value in metres
	 * @param azimuth	Azimuth value in degrees
	 * @param elevation	Elevation value in degrees
	 * @param panRate	Rate in milliseconds to transition
	 */
	virtual void setPolarPosition(int handle, double range, double azimuth, double elevation, uint panRate) {}

	/**
	 * Returns true if the given sound is currently active
	 */
	virtual bool isActive(int handle) const = 0;

	/**
	 * Returns true if the given sound is currently active
	 */
	virtual bool isActive(const CWaveFile *waveFile) const { return false; }

	/**
	 * Handles regularly updating the mixer
	 */
	virtual void waveMixPump() = 0;

	/**
	 * Returns the movie latency
	 */
	virtual uint getLatency() const { return 0; }

	/**
	 * Sets the music volume percent
	 */
	virtual void setMusicPercent(double percent) = 0;

	/**
	 * Sets the speech volume percent
	 */
	virtual void setSpeechPercent(double percent) = 0;

	/**
	 * Sets the master volume percent
	 */
	virtual void setMasterPercent(double percent) = 0;

	/**
	 * Sets the Parrot NPC volume percent
	 */
	virtual void setParrotPercent(double percent) = 0;

	/**
	 * Called when a game is about to be loaded
	 */
	virtual void preLoad() { stopAllChannels(); }

	/**
	 * Load the data for the class from file
	 */
	void load(SimpleFile *file) {}

	/**
	 * Called after loading of a game is completed
	 */
	virtual void postLoad() {}

	/**
	 * Called when a game is about to be saved
	 */
	virtual void preSave() {}

	/**
	 * Save the data for the class to file
	 */
	void save(SimpleFile *file) const {}

	/**
	 * Called after saving is complete
	 */
	virtual void postSave() {}

	/**
	 * Sets the position and orientation for the listener (player)
	 */
	virtual void setListenerPosition(double posX, double posY, double posZ,
		double directionX, double directionY, double directionZ, bool stopSounds) {}

	/**
	 * Returns the music volume percent
	 */
	double getMusicVolume() const { return _musicPercent; }

	/**
	 * Returns the speech volume percent
	 */
	double getSpeechVolume() const { return _speechPercent; }

	/**
	 * Returns the parrot volume percent
	 */
	double getParrotVolume() const { return _parrotPercent; }

	/**
	 * Gets the volume for a given mode? value
	 */
	uint getModeVolume(int mode);
};

class QSoundManagerSound : public ListItem {
public:
	CWaveFile *_waveFile;
	int _iChannel;
	CEndTalkerFn _endFn;
	TTtalker *_talker;
public:
	QSoundManagerSound() : ListItem(), _waveFile(nullptr),
		_iChannel(0), _endFn(nullptr), _talker(nullptr) {}
	QSoundManagerSound(CWaveFile *waveFile, int iChannel, CEndTalkerFn endFn, TTtalker *talker) :
		ListItem(), _waveFile(waveFile), _iChannel(iChannel), _endFn(endFn), _talker(talker) {}
};

class QSoundManagerSounds : public List<QSoundManagerSound> {
public:
	/**
	 * Adds a new sound entry to the list
	 */
	void add(CWaveFile *waveFile, int iChannel, CEndTalkerFn endFn, TTtalker *talker);

	/**
	 * Flushes a wave file attached to the specified channel
	 */
	void flushChannel(int iChannel);

	/**
	 * Flushes a wave file attached to the specified channel
	 */
	void flushChannel(CWaveFile *waveFile, int iChannel);

	/**
	 * Returns true if the list contains the specified wave file
	 */
	bool contains(const CWaveFile *waveFile) const;
};

/**
 * Concrete sound manager class that handles interfacing with
 * the QMixer sound mixer class
 */
class QSoundManager : public CSoundManager, public QMixer {
	struct Slot {
		CWaveFile *_waveFile;
		bool _isTimed;
		uint _ticks;
		int _channel;
		int _handle;
		PositioningMode _positioningMode;

		Slot() : _waveFile(0), _isTimed(0), _ticks(0), _channel(-1),
			_handle(0), _positioningMode(POSMODE_NONE) {}
		void clear();
	};
private:
	QSoundManagerSounds _sounds;
	Common::Array<Slot> _slots;
	uint _channelsVolume[16];
	int _channelsMode[16];
private:
	/**
	 * Updates the volume for a channel
	 * @param channel	Channel to be update
	 * @param panRate	Time in milliseconds for change to occur
	 */
	void updateVolume(int channel, uint panRate);

	/**
	 * Updates all the volumes
	 */
	void updateVolumes();

	/**
	 * Called by the QMixer when a sound finishes playing
	 */
	static void soundFinished(int iChannel, CWaveFile *waveFile, void *soundManager);

	/**
	 * Finds the first free slot
	 */
	int findFreeSlot();

	/**
	 * Sets a channel volume
	 */
	void setChannelVolume(int iChannel, uint volume, uint mode);

	/**
	 * Resets the specified channel and returns a new free one
	 */
	int resetChannel(int iChannel);
public:
	int _field18;
	int _field1C;

public:
	QSoundManager(Audio::Mixer *mixer);
	virtual ~QSoundManager();

	/**
	 * Loads a sound
	 * @param name		Name of sound resource
	 * @returns			Loaded wave file
	 */
	virtual CWaveFile *loadSound(const CString &name);

	/**
	 * Loads a speech resource from a dialogue file
	 * @param name		Name of sound resource
	 * @returns			Loaded wave file
	 */
	virtual CWaveFile *loadSpeech(CDialogueFile *dialogueFile, int speechId);

	/**
	 * Loads a music file
	 * @param name		Name of music resource
	 * @returns			Loaded wave file
	 * @remarks The original created a streaming audio buffer for the wave file,
	 *		and passed this to the method. For ScummVM, this has been discarded
	 *		in favor of simply passing the filename.
	 */
	virtual CWaveFile *loadMusic(const CString &name);

	/**
	 * Start playing a previously loaded sound resource
	 */
	virtual int playSound(CWaveFile &waveFile, CProximity &prox);

	/**
	 * Stop playing the specified sound
	 */
	virtual void stopSound(int handle);

	/**
	 * Stops a designated range of channels
	 */
	virtual void stopChannel(int channel);

	/**
	 * Flags that a sound can be freed if a timeout is set
	 */
	virtual void setCanFree(int handle);

	/**
	 * Stops sounds on all playing channels
	 */
	virtual void stopAllChannels();

	/**
	 * Sets the volume for a sound
	 * @param handle	Handle for sound
	 * @param volume	New volume
	 * @param seconds	Number of seconds to transition to the new volume
	 */
	virtual void setVolume(int handle, uint volume, uint seconds);

	/**
	 * Set the position for a sound
	 * @param handle	Handle for sound
	 * @param x			x position in metres
	 * @param y			y position in metres
	 * @param z			z position in metres
	 * @param panRate	Rate in milliseconds to transition
	 */
	virtual void setVectorPosition(int handle, double x, double y, double z, uint panRate);

	/**
	 * Set the position for a sound
	 * @param handle	Handle for sound
	 * @param range		Range value in metres
	 * @param azimuth	Azimuth value in degrees
	 * @param elevation	Elevation value in degrees
	 * @param panRate	Rate in milliseconds to transition
	 */
	virtual void setPolarPosition(int handle, double range, double azimuth, double elevation, uint panRate);

	/**
	 * Returns true if the given sound is currently active
	 */
	virtual bool isActive(int handle) const;

	/**
	 * Returns true if the given sound is currently active
	 */
	virtual bool isActive(const CWaveFile *waveFile) const;

	/**
	 * Handles regularly updating the mixer
	 */
	virtual void waveMixPump();

	/**
	 * Returns the movie latency
	 */
	virtual uint getLatency() const;

	/**
	 * Sets the music volume percent
	 */
	virtual void setMusicPercent(double percent);

	/**
	 * Sets the speech volume percent
	 */
	virtual void setSpeechPercent(double percent);

	/**
	 * Sets the master volume percent
	 */
	virtual void setMasterPercent(double percent);

	/**
	 * Sets the Parrot NPC volume percent
	 */
	virtual void setParrotPercent(double percent);

	/**
	 * Sets the position and orientation for the listener (player)
	 */
	virtual void setListenerPosition(double posX, double posY, double posZ,
		double directionX, double directionY, double directionZ, bool stopSounds);

	/**
	 * Starts a wave file playing
	 */
	virtual int playWave(CWaveFile *waveFile, int iChannel, uint flags, CProximity &prox);

	/**
	 * Called when a wave file is freed
	 */
	void soundFreed(Audio::SoundHandle &handle);
};

} // End of namespace Titanic

#endif /* TITANIC_QSOUND_MANAGER_H */
