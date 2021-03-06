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


#include "backends/graphics/opengl/opengl-graphics.h"
#include "backends/graphics/opengl/texture.h"
#include "backends/graphics/opengl/pipelines/pipeline.h"
#include "backends/graphics/opengl/pipelines/fixed.h"
#include "backends/graphics/opengl/pipelines/shader.h"
#include "backends/graphics/opengl/shader.h"

#include "common/textconsole.h"
#include "common/translation.h"
#include "common/algorithm.h"
#include "common/file.h"
#ifdef USE_OSD
#include "common/tokenizer.h"
#include "common/rect.h"
#endif

#include "graphics/conversion.h"
#ifdef USE_OSD
#include "graphics/fontman.h"
#include "graphics/font.h"
#endif

namespace OpenGL {

OpenGLGraphicsManager::OpenGLGraphicsManager()
    : _currentState(), _oldState(), _transactionMode(kTransactionNone), _screenChangeID(1 << (sizeof(int) * 8 - 2)),
      _pipeline(nullptr),
      _outputScreenWidth(0), _outputScreenHeight(0), _displayX(0), _displayY(0),
      _displayWidth(0), _displayHeight(0), _defaultFormat(), _defaultFormatAlpha(),
      _gameScreen(nullptr), _gameScreenShakeOffset(0), _overlay(nullptr),
      _overlayVisible(false), _cursor(nullptr),
      _cursorX(0), _cursorY(0), _cursorDisplayX(0),_cursorDisplayY(0), _cursorHotspotX(0), _cursorHotspotY(0),
      _cursorHotspotXScaled(0), _cursorHotspotYScaled(0), _cursorWidthScaled(0), _cursorHeightScaled(0),
      _cursorKeyColor(0), _cursorVisible(false), _cursorDontScale(false), _cursorPaletteEnabled(false),
      _forceRedraw(false), _scissorOverride(3)
#ifdef USE_OSD
      , _osdMessageChangeRequest(false), _osdMessageAlpha(0), _osdMessageFadeStartTime(0), _osdMessageSurface(nullptr),
      _osdIconSurface(nullptr)
#endif
    {
	memset(_gamePalette, 0, sizeof(_gamePalette));
	g_context.reset();
}

OpenGLGraphicsManager::~OpenGLGraphicsManager() {
	delete _gameScreen;
	delete _overlay;
	delete _cursor;
#ifdef USE_OSD
	delete _osdMessageSurface;
	delete _osdIconSurface;
#endif
#if !USE_FORCED_GLES
	ShaderManager::destroy();
#endif
}

bool OpenGLGraphicsManager::hasFeature(OSystem::Feature f) {
	switch (f) {
	case OSystem::kFeatureAspectRatioCorrection:
	case OSystem::kFeatureCursorPalette:
		return true;

	case OSystem::kFeatureOverlaySupportsAlpha:
		return _defaultFormatAlpha.aBits() > 3;

	default:
		return false;
	}
}

void OpenGLGraphicsManager::setFeatureState(OSystem::Feature f, bool enable) {
	switch (f) {
	case OSystem::kFeatureAspectRatioCorrection:
		assert(_transactionMode != kTransactionNone);
		_currentState.aspectRatioCorrection = enable;
		break;

	case OSystem::kFeatureCursorPalette:
		_cursorPaletteEnabled = enable;
		updateCursorPalette();
		break;

	default:
		break;
	}
}

bool OpenGLGraphicsManager::getFeatureState(OSystem::Feature f) {
	switch (f) {
	case OSystem::kFeatureAspectRatioCorrection:
		return _currentState.aspectRatioCorrection;

	case OSystem::kFeatureCursorPalette:
		return _cursorPaletteEnabled;

	default:
		return false;
	}
}

namespace {

const OSystem::GraphicsMode glGraphicsModes[] = {
	{ "opengl_linear",  _s("OpenGL"),                GFX_LINEAR  },
	{ "opengl_nearest", _s("OpenGL (No filtering)"), GFX_NEAREST },
	{ nullptr, nullptr, 0 }
};

} // End of anonymous namespace

const OSystem::GraphicsMode *OpenGLGraphicsManager::getSupportedGraphicsModes() const {
	return glGraphicsModes;
}

int OpenGLGraphicsManager::getDefaultGraphicsMode() const {
	return GFX_LINEAR;
}

bool OpenGLGraphicsManager::setGraphicsMode(int mode) {
	assert(_transactionMode != kTransactionNone);

	switch (mode) {
	case GFX_LINEAR:
	case GFX_NEAREST:
		_currentState.graphicsMode = mode;

		if (_gameScreen) {
			_gameScreen->enableLinearFiltering(mode == GFX_LINEAR);
		}

		if (_cursor) {
			_cursor->enableLinearFiltering(mode == GFX_LINEAR);
		}

		return true;

	default:
		warning("OpenGLGraphicsManager::setGraphicsMode(%d): Unknown graphics mode", mode);
		return false;
	}
}

int OpenGLGraphicsManager::getGraphicsMode() const {
	return _currentState.graphicsMode;
}

#ifdef USE_RGB_COLOR
Graphics::PixelFormat OpenGLGraphicsManager::getScreenFormat() const {
	return _currentState.gameFormat;
}
#endif

void OpenGLGraphicsManager::beginGFXTransaction() {
	assert(_transactionMode == kTransactionNone);

	// Start a transaction.
	_oldState = _currentState;
	_transactionMode = kTransactionActive;
}

OSystem::TransactionError OpenGLGraphicsManager::endGFXTransaction() {
	assert(_transactionMode == kTransactionActive);

	uint transactionError = OSystem::kTransactionSuccess;

	bool setupNewGameScreen = false;
	if (   _oldState.gameWidth  != _currentState.gameWidth
	    || _oldState.gameHeight != _currentState.gameHeight) {
		setupNewGameScreen = true;
	}

#ifdef USE_RGB_COLOR
	if (_oldState.gameFormat != _currentState.gameFormat) {
		setupNewGameScreen = true;
	}

	// Check whether the requested format can actually be used.
	Common::List<Graphics::PixelFormat> supportedFormats = getSupportedFormats();
	// In case the requested format is not usable we will fall back to CLUT8.
	if (Common::find(supportedFormats.begin(), supportedFormats.end(), _currentState.gameFormat) == supportedFormats.end()) {
		_currentState.gameFormat = Graphics::PixelFormat::createFormatCLUT8();
		transactionError |= OSystem::kTransactionFormatNotSupported;
	}
#endif

	do {
		uint requestedWidth  = _currentState.gameWidth;
		uint requestedHeight = _currentState.gameHeight;
		const uint desiredAspect = getDesiredGameScreenAspect();
		requestedHeight = intToFrac(requestedWidth) / desiredAspect;

		if (!loadVideoMode(requestedWidth, requestedHeight,
#ifdef USE_RGB_COLOR
		                   _currentState.gameFormat
#else
		                   Graphics::PixelFormat::createFormatCLUT8()
#endif
		                  )
		   // HACK: This is really nasty but we don't have any guarantees of
		   // a context existing before, which means we don't know the maximum
		   // supported texture size before this. Thus, we check whether the
		   // requested game resolution is supported over here.
		   || (   _currentState.gameWidth  > (uint)g_context.maxTextureSize
		       || _currentState.gameHeight > (uint)g_context.maxTextureSize)) {
			if (_transactionMode == kTransactionActive) {
				// Try to setup the old state in case its valid and is
				// actually different from the new one.
				if (_oldState.valid && _oldState != _currentState) {
					// Give some hints on what failed to set up.
					if (   _oldState.gameWidth  != _currentState.gameWidth
					    || _oldState.gameHeight != _currentState.gameHeight) {
						transactionError |= OSystem::kTransactionSizeChangeFailed;
					}

#ifdef USE_RGB_COLOR
					if (_oldState.gameFormat != _currentState.gameFormat) {
						transactionError |= OSystem::kTransactionFormatNotSupported;
					}
#endif

					if (_oldState.aspectRatioCorrection != _currentState.aspectRatioCorrection) {
						transactionError |= OSystem::kTransactionAspectRatioFailed;
					}

					if (_oldState.graphicsMode != _currentState.graphicsMode) {
						transactionError |= OSystem::kTransactionModeSwitchFailed;
					}

					// Roll back to the old state.
					_currentState = _oldState;
					_transactionMode = kTransactionRollback;

					// Try to set up the old state.
					continue;
				}
			}

			// DON'T use error(), as this tries to bring up the debug
			// console, which WON'T WORK now that we might no have a
			// proper screen.
			warning("OpenGLGraphicsManager::endGFXTransaction: Could not load any graphics mode!");
			g_system->quit();
		}

		// In case we reach this we have a valid state, yay.
		_transactionMode = kTransactionNone;
		_currentState.valid = true;
	} while (_transactionMode == kTransactionRollback);

	if (setupNewGameScreen) {
		delete _gameScreen;
		_gameScreen = nullptr;

#ifdef USE_RGB_COLOR
		_gameScreen = createSurface(_currentState.gameFormat);
#else
		_gameScreen = createSurface(Graphics::PixelFormat::createFormatCLUT8());
#endif
		assert(_gameScreen);
		if (_gameScreen->hasPalette()) {
			_gameScreen->setPalette(0, 256, _gamePalette);
		}

		_gameScreen->allocate(_currentState.gameWidth, _currentState.gameHeight);
		_gameScreen->enableLinearFiltering(_currentState.graphicsMode == GFX_LINEAR);
		// We fill the screen to all black or index 0 for CLUT8.
#ifdef USE_RGB_COLOR
		if (_currentState.gameFormat.bytesPerPixel == 1) {
			_gameScreen->fill(0);
		} else {
			_gameScreen->fill(_gameScreen->getSurface()->format.RGBToColor(0, 0, 0));
		}
#else
		_gameScreen->fill(0);
#endif
	}

	// Update our display area and cursor scaling. This makes sure we pick up
	// aspect ratio correction and game screen changes correctly.
	recalculateDisplayArea();
	recalculateCursorScaling();

	// Something changed, so update the screen change ID.
	++_screenChangeID;

	// Since transactionError is a ORd list of TransactionErrors this is
	// clearly wrong. But our API is simply broken.
	return (OSystem::TransactionError)transactionError;
}

int OpenGLGraphicsManager::getScreenChangeID() const {
	return _screenChangeID;
}

void OpenGLGraphicsManager::initSize(uint width, uint height, const Graphics::PixelFormat *format) {
	Graphics::PixelFormat requestedFormat;
#ifdef USE_RGB_COLOR
	if (!format) {
		requestedFormat = Graphics::PixelFormat::createFormatCLUT8();
	} else {
		requestedFormat = *format;
	}
	_currentState.gameFormat = requestedFormat;
#endif

	_currentState.gameWidth = width;
	_currentState.gameHeight = height;
}

int16 OpenGLGraphicsManager::getWidth() {
	return _currentState.gameWidth;
}

int16 OpenGLGraphicsManager::getHeight() {
	return _currentState.gameHeight;
}

void OpenGLGraphicsManager::copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) {
	_gameScreen->copyRectToTexture(x, y, w, h, buf, pitch);
}

void OpenGLGraphicsManager::fillScreen(uint32 col) {
	// FIXME: This does not conform to the OSystem specs because fillScreen
	// is always taking CLUT8 color values and use color indexed mode. This is,
	// however, plain odd and probably was a forgotten when we introduced
	// RGB support. Thus, we simply do the "sane" thing here and hope OSystem
	// gets fixed one day.
	_gameScreen->fill(col);
}

void OpenGLGraphicsManager::setShakePos(int shakeOffset) {
	if (_gameScreenShakeOffset != shakeOffset) {
		_gameScreenShakeOffset = shakeOffset;
		_forceRedraw = true;
	}
}

void OpenGLGraphicsManager::updateScreen() {
	if (!_gameScreen) {
		return;
	}

#ifdef USE_OSD
	{
		Common::StackLock lock(_osdMutex);
		if (_osdMessageChangeRequest) {
			osdMessageUpdateSurface();
		}
	}

	if (_osdIconSurface) {
		_osdIconSurface->updateGLTexture();
	}
#endif

	// We only update the screen when there actually have been any changes.
	if (   !_forceRedraw
	    && !_gameScreen->isDirty()
	    && !(_overlayVisible && _overlay->isDirty())
	    && !(_cursorVisible && _cursor && _cursor->isDirty())
#ifdef USE_OSD
	    && !_osdMessageSurface && !_osdIconSurface
#endif
	    ) {
		return;
	}
	_forceRedraw = false;

	// Update changes to textures.
	_gameScreen->updateGLTexture();
	if (_cursor) {
		_cursor->updateGLTexture();
	}
	_overlay->updateGLTexture();

	// Clear the screen buffer.
	if (_scissorOverride && !_overlayVisible) {
		// In certain cases we need to assure that the whole screen area is
		// cleared. For example, when switching from overlay visible to
		// invisible, we need to assure that all contents are cleared to
		// properly remove all overlay contents.
		_backBuffer.enableScissorTest(false);
		GL_CALL(glClear(GL_COLOR_BUFFER_BIT));
		_backBuffer.enableScissorTest(true);

		--_scissorOverride;
	} else {
		GL_CALL(glClear(GL_COLOR_BUFFER_BIT));
	}

	const GLfloat shakeOffset = _gameScreenShakeOffset * (GLfloat)_displayHeight / _gameScreen->getHeight();

	// First step: Draw the (virtual) game screen.
	g_context.getActivePipeline()->drawTexture(_gameScreen->getGLTexture(), _displayX, _displayY + shakeOffset, _displayWidth, _displayHeight);

	// Second step: Draw the overlay if visible.
	if (_overlayVisible) {
		g_context.getActivePipeline()->drawTexture(_overlay->getGLTexture(), 0, 0, _outputScreenWidth, _outputScreenHeight);
	}

	// Third step: Draw the cursor if visible.
	if (_cursorVisible && _cursor) {
		// Adjust game screen shake position, but only when the overlay is not
		// visible.
		const GLfloat cursorOffset = _overlayVisible ? 0 : shakeOffset;

		g_context.getActivePipeline()->drawTexture(_cursor->getGLTexture(),
		                         _cursorDisplayX - _cursorHotspotXScaled,
		                         _cursorDisplayY - _cursorHotspotYScaled + cursorOffset,
		                         _cursorWidthScaled, _cursorHeightScaled);
	}

#ifdef USE_OSD
	// Fourth step: Draw the OSD.
	if (_osdMessageSurface) {
		// Update alpha value.
		const int diff = g_system->getMillis(false) - _osdMessageFadeStartTime;
		if (diff > 0) {
			if (diff >= kOSDMessageFadeOutDuration) {
				// Back to full transparency.
				_osdMessageAlpha = 0;
			} else {
				// Do a fade out.
				_osdMessageAlpha = kOSDMessageInitialAlpha - diff * kOSDMessageInitialAlpha / kOSDMessageFadeOutDuration;
			}
		}

		// Set the OSD transparency.
		g_context.getActivePipeline()->setColor(1.0f, 1.0f, 1.0f, _osdMessageAlpha / 100.0f);

		int dstX = (_outputScreenWidth - _osdMessageSurface->getWidth()) / 2;
		int dstY = (_outputScreenHeight - _osdMessageSurface->getHeight()) / 2;

		// Draw the OSD texture.
		g_context.getActivePipeline()->drawTexture(_osdMessageSurface->getGLTexture(),
		                                           dstX, dstY, _osdMessageSurface->getWidth(), _osdMessageSurface->getHeight());

		// Reset color.
		g_context.getActivePipeline()->setColor(1.0f, 1.0f, 1.0f, 1.0f);

		if (_osdMessageAlpha <= 0) {
			delete _osdMessageSurface;
			_osdMessageSurface = nullptr;
		}
	}

	if (_osdIconSurface) {
		int dstX = _outputScreenWidth - _osdIconSurface->getWidth() - kOSDIconRightMargin;
		int dstY = kOSDIconTopMargin;

		// Draw the OSD icon texture.
		g_context.getActivePipeline()->drawTexture(_osdIconSurface->getGLTexture(),
		                                           dstX, dstY, _osdIconSurface->getWidth(), _osdIconSurface->getHeight());
	}
#endif

	refreshScreen();
}

Graphics::Surface *OpenGLGraphicsManager::lockScreen() {
	return _gameScreen->getSurface();
}

void OpenGLGraphicsManager::unlockScreen() {
	_gameScreen->flagDirty();
}

void OpenGLGraphicsManager::setFocusRectangle(const Common::Rect& rect) {
}

void OpenGLGraphicsManager::clearFocusRectangle() {
}

int16 OpenGLGraphicsManager::getOverlayWidth() {
	if (_overlay) {
		return _overlay->getWidth();
	} else {
		return 0;
	}
}

int16 OpenGLGraphicsManager::getOverlayHeight() {
	if (_overlay) {
		return _overlay->getHeight();
	} else {
		return 0;
	}
}

void OpenGLGraphicsManager::showOverlay() {
	_overlayVisible = true;
	_forceRedraw = true;

	// Allow drawing inside full screen area.
	_backBuffer.enableScissorTest(false);

	// Update cursor position.
	setMousePosition(_cursorX, _cursorY);
}

void OpenGLGraphicsManager::hideOverlay() {
	_overlayVisible = false;
	_forceRedraw = true;

	// Limit drawing to screen area.
	_backBuffer.enableScissorTest(true);
	_scissorOverride = 3;

	// Update cursor position.
	setMousePosition(_cursorX, _cursorY);
}

Graphics::PixelFormat OpenGLGraphicsManager::getOverlayFormat() const {
	return _overlay->getFormat();
}

void OpenGLGraphicsManager::copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h) {
	_overlay->copyRectToTexture(x, y, w, h, buf, pitch);
}

void OpenGLGraphicsManager::clearOverlay() {
	_overlay->fill(0);
}

void OpenGLGraphicsManager::grabOverlay(void *buf, int pitch) {
	const Graphics::Surface *overlayData = _overlay->getSurface();

	const byte *src = (const byte *)overlayData->getPixels();
	byte *dst = (byte *)buf;

	for (uint h = overlayData->h; h > 0; --h) {
		memcpy(dst, src, overlayData->w * overlayData->format.bytesPerPixel);
		dst += pitch;
		src += overlayData->pitch;
	}
}

bool OpenGLGraphicsManager::showMouse(bool visible) {
	// In case the mouse cursor visibility changed we need to redraw the whole
	// screen even when nothing else changed.
	if (_cursorVisible != visible) {
		_forceRedraw = true;
	}

	bool last = _cursorVisible;
	_cursorVisible = visible;
	return last;
}

void OpenGLGraphicsManager::warpMouse(int x, int y) {
	int16 currentX = _cursorX;
	int16 currentY = _cursorY;
	adjustMousePosition(currentX, currentY);

	// Check whether the (virtual) coordinate actually changed. If not, then
	// simply do nothing. This avoids ugly "jittering" due to the actual
	// output screen having a bigger resolution than the virtual coordinates.
	if (currentX == x && currentY == y) {
		return;
	}

	// Scale the virtual coordinates into actual physical coordinates.
	if (_overlayVisible) {
		if (!_overlay) {
			return;
		}

		// It might be confusing that we actually have to handle something
		// here when the overlay is visible. This is because for very small
		// resolutions we have a minimal overlay size and have to adjust
		// for that.
		x = (x * _outputScreenWidth)  / _overlay->getWidth();
		y = (y * _outputScreenHeight) / _overlay->getHeight();
	} else {
		if (!_gameScreen) {
			return;
		}

		x = (x * _outputScreenWidth)  / _gameScreen->getWidth();
		y = (y * _outputScreenHeight) / _gameScreen->getHeight();
	}

	setMousePosition(x, y);
	setInternalMousePosition(x, y);
}

namespace {
template<typename DstPixel, typename SrcPixel>
void applyColorKey(DstPixel *dst, const SrcPixel *src, uint w, uint h, uint dstPitch, uint srcPitch, SrcPixel keyColor, DstPixel alphaMask) {
	const uint srcAdd = srcPitch - w * sizeof(SrcPixel);
	const uint dstAdd = dstPitch - w * sizeof(DstPixel);

	while (h-- > 0) {
		for (uint x = w; x > 0; --x, ++dst, ++src) {
			if (*src == keyColor) {
				*dst &= ~alphaMask;
			}
		}

		dst = (DstPixel *)((byte *)dst + dstAdd);
		src = (const SrcPixel *)((const byte *)src + srcAdd);
	}
}
} // End of anonymous namespace

void OpenGLGraphicsManager::setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY, uint32 keycolor, bool dontScale, const Graphics::PixelFormat *format) {
	Graphics::PixelFormat inputFormat;
#ifdef USE_RGB_COLOR
	if (format) {
		inputFormat = *format;
	} else {
		inputFormat = Graphics::PixelFormat::createFormatCLUT8();
	}
#else
	inputFormat = Graphics::PixelFormat::createFormatCLUT8();
#endif

	// In case the color format has changed we will need to create the texture.
	if (!_cursor || _cursor->getFormat() != inputFormat) {
		delete _cursor;
		_cursor = nullptr;

		GLenum glIntFormat, glFormat, glType;

		Graphics::PixelFormat textureFormat;
		if (inputFormat.bytesPerPixel == 1 || (inputFormat.aBits() && getGLPixelFormat(inputFormat, glIntFormat, glFormat, glType))) {
			// There is two cases when we can use the cursor format directly.
			// The first is when it's CLUT8, here color key handling can
			// always be applied because we use the alpha channel of
			// _defaultFormatAlpha for that.
			// The other is when the input format has alpha bits and
			// furthermore is directly supported.
			textureFormat = inputFormat;
		} else {
			textureFormat = _defaultFormatAlpha;
		}
		_cursor = createSurface(textureFormat, true);
		assert(_cursor);
		_cursor->enableLinearFiltering(_currentState.graphicsMode == GFX_LINEAR);
	}

	_cursorKeyColor = keycolor;
	_cursorHotspotX = hotspotX;
	_cursorHotspotY = hotspotY;
	_cursorDontScale = dontScale;

	_cursor->allocate(w, h);
	if (inputFormat.bytesPerPixel == 1) {
		// For CLUT8 cursors we can simply copy the input data into the
		// texture.
		_cursor->copyRectToTexture(0, 0, w, h, buf, w * inputFormat.bytesPerPixel);
	} else {
		// Otherwise it is a bit more ugly because we have to handle a key
		// color properly.

		Graphics::Surface *dst = _cursor->getSurface();
		const uint srcPitch = w * inputFormat.bytesPerPixel;

		// Copy the cursor data to the actual texture surface. This will make
		// sure that the data is also converted to the expected format.
		Graphics::crossBlit((byte *)dst->getPixels(), (const byte *)buf, dst->pitch, srcPitch,
		                    w, h, dst->format, inputFormat);

		// We apply the color key by setting the alpha bits of the pixels to
		// fully transparent.
		const uint32 aMask = (0xFF >> dst->format.aLoss) << dst->format.aShift;
		if (dst->format.bytesPerPixel == 2) {
			if (inputFormat.bytesPerPixel == 2) {
				applyColorKey<uint16, uint16>((uint16 *)dst->getPixels(), (const uint16 *)buf, w, h,
				                              dst->pitch, srcPitch, keycolor, aMask);
			} else if (inputFormat.bytesPerPixel == 4) {
				applyColorKey<uint16, uint32>((uint16 *)dst->getPixels(), (const uint32 *)buf, w, h,
				                              dst->pitch, srcPitch, keycolor, aMask);
			}
		} else {
			if (inputFormat.bytesPerPixel == 2) {
				applyColorKey<uint32, uint16>((uint32 *)dst->getPixels(), (const uint16 *)buf, w, h,
				                              dst->pitch, srcPitch, keycolor, aMask);
			} else if (inputFormat.bytesPerPixel == 4) {
				applyColorKey<uint32, uint32>((uint32 *)dst->getPixels(), (const uint32 *)buf, w, h,
				                              dst->pitch, srcPitch, keycolor, aMask);
			}
		}

		// Flag the texture as dirty.
		_cursor->flagDirty();
	}

	// In case we actually use a palette set that up properly.
	if (inputFormat.bytesPerPixel == 1) {
		updateCursorPalette();
	}

	// Update the scaling.
	recalculateCursorScaling();
}

void OpenGLGraphicsManager::setCursorPalette(const byte *colors, uint start, uint num) {
	// FIXME: For some reason client code assumes that usage of this function
	// automatically enables the cursor palette.
	_cursorPaletteEnabled = true;

	memcpy(_cursorPalette + start * 3, colors, num * 3);
	updateCursorPalette();
}

void OpenGLGraphicsManager::displayMessageOnOSD(const char *msg) {
#ifdef USE_OSD
	// HACK: Actually no client code should use graphics functions from
	// another thread. But the MT-32 emulator and network synchronization still do,
	// thus we need to make sure this doesn't happen while a updateScreen call is done.
	Common::StackLock lock(_osdMutex);

	_osdMessageChangeRequest = true;

	_osdMessageNextData = msg;
#endif
}

#ifdef USE_OSD
void OpenGLGraphicsManager::osdMessageUpdateSurface() {
	// Split up the lines.
	Common::Array<Common::String> osdLines;
	Common::StringTokenizer tokenizer(_osdMessageNextData, "\n");
	while (!tokenizer.empty()) {
		osdLines.push_back(tokenizer.nextToken());
	}

	// Do the actual drawing like the SDL backend.
	const Graphics::Font *font = getFontOSD();

	// Determine a rect which would contain the message string (clipped to the
	// screen dimensions).
	const int vOffset = 6;
	const int lineSpacing = 1;
	const int lineHeight = font->getFontHeight() + 2 * lineSpacing;
	uint width = 0;
	uint height = lineHeight * osdLines.size() + 2 * vOffset;
	for (uint i = 0; i < osdLines.size(); i++) {
		width = MAX<uint>(width, font->getStringWidth(osdLines[i]) + 14);
	}

	// Clip the rect
	width  = MIN<uint>(width,  _displayWidth);
	height = MIN<uint>(height, _displayHeight);

	delete _osdMessageSurface;
	_osdMessageSurface = nullptr;

	_osdMessageSurface = createSurface(_defaultFormatAlpha);
	assert(_osdMessageSurface);
	// We always filter the osd with GL_LINEAR. This assures it's
	// readable in case it needs to be scaled and does not affect it
	// otherwise.
	_osdMessageSurface->enableLinearFiltering(true);

	_osdMessageSurface->allocate(width, height);

	Graphics::Surface *dst = _osdMessageSurface->getSurface();

	// Draw a dark gray rect.
	const uint32 color = dst->format.RGBToColor(40, 40, 40);
	dst->fillRect(Common::Rect(0, 0, width, height), color);

	// Render the message in white
	const uint32 white = dst->format.RGBToColor(255, 255, 255);
	for (uint i = 0; i < osdLines.size(); ++i) {
		font->drawString(dst, osdLines[i],
		                 0, i * lineHeight + vOffset + lineSpacing, width,
		                 white, Graphics::kTextAlignCenter);
	}

	_osdMessageSurface->updateGLTexture();

	// Init the OSD display parameters.
	_osdMessageAlpha = kOSDMessageInitialAlpha;
	_osdMessageFadeStartTime = g_system->getMillis() + kOSDMessageFadeOutDelay;

	// Clear the text update request
	_osdMessageNextData.clear();
	_osdMessageChangeRequest = false;
}
#endif

void OpenGLGraphicsManager::displayActivityIconOnOSD(const Graphics::Surface *icon) {
#ifdef USE_OSD
	if (_osdIconSurface) {
		delete _osdIconSurface;
		_osdIconSurface = nullptr;

		// Make sure the icon is cleared on the next update
		_forceRedraw = true;
	}

	if (icon) {
		Graphics::Surface *converted = icon->convertTo(_defaultFormatAlpha);

		_osdIconSurface = createSurface(_defaultFormatAlpha);
		assert(_osdIconSurface);
		// We always filter the osd with GL_LINEAR. This assures it's
		// readable in case it needs to be scaled and does not affect it
		// otherwise.
		_osdIconSurface->enableLinearFiltering(true);

		_osdIconSurface->allocate(converted->w, converted->h);

		Graphics::Surface *dst = _osdIconSurface->getSurface();

		// Copy the icon to the texture
		dst->copyRectToSurface(*converted, 0, 0, Common::Rect(0, 0, converted->w, converted->h));

		converted->free();
		delete converted;
	}
#endif
}

void OpenGLGraphicsManager::setPalette(const byte *colors, uint start, uint num) {
	assert(_gameScreen->hasPalette());

	memcpy(_gamePalette + start * 3, colors, num * 3);
	_gameScreen->setPalette(start, num, colors);

	// We might need to update the cursor palette here.
	updateCursorPalette();
}

void OpenGLGraphicsManager::grabPalette(byte *colors, uint start, uint num) {
	assert(_gameScreen->hasPalette());

	memcpy(colors, _gamePalette + start * 3, num * 3);
}

void OpenGLGraphicsManager::setActualScreenSize(uint width, uint height) {
	_outputScreenWidth = width;
	_outputScreenHeight = height;

	// Setup backbuffer size.
	_backBuffer.setDimensions(width, height);

	uint overlayWidth = width;
	uint overlayHeight = height;

	// WORKAROUND: We can only support surfaces up to the maximum supported
	// texture size. Thus, in case we encounter a physical size bigger than
	// this maximum texture size we will simply use an overlay as big as
	// possible and then scale it to the physical display size. This sounds
	// bad but actually all recent chips should support full HD resolution
	// anyway. Thus, it should not be a real issue for modern hardware.
	if (   overlayWidth  > (uint)g_context.maxTextureSize
	    || overlayHeight > (uint)g_context.maxTextureSize) {
		const frac_t outputAspect = intToFrac(_outputScreenWidth) / _outputScreenHeight;

		if (outputAspect > (frac_t)FRAC_ONE) {
			overlayWidth  = g_context.maxTextureSize;
			overlayHeight = intToFrac(overlayWidth) / outputAspect;
		} else {
			overlayHeight = g_context.maxTextureSize;
			overlayWidth  = fracToInt(overlayHeight * outputAspect);
		}
	}

	// HACK: We limit the minimal overlay size to 256x200, which is the
	// minimum of the dimensions of the two resolutions 256x240 (NES) and
	// 320x200 (many DOS games use this). This hopefully assure that our
	// GUI has working layouts.
	overlayWidth = MAX<uint>(overlayWidth, 256);
	overlayHeight = MAX<uint>(overlayHeight, 200);

	if (!_overlay || _overlay->getFormat() != _defaultFormatAlpha) {
		delete _overlay;
		_overlay = nullptr;

		_overlay = createSurface(_defaultFormatAlpha);
		assert(_overlay);
		// We always filter the overlay with GL_LINEAR. This assures it's
		// readable in case it needs to be scaled and does not affect it
		// otherwise.
		_overlay->enableLinearFiltering(true);
	}
	_overlay->allocate(overlayWidth, overlayHeight);
	_overlay->fill(0);

	// Re-setup the scaling for the screen and cursor
	recalculateDisplayArea();
	recalculateCursorScaling();

	// Something changed, so update the screen change ID.
	++_screenChangeID;
}

void OpenGLGraphicsManager::notifyContextCreate(const Graphics::PixelFormat &defaultFormat, const Graphics::PixelFormat &defaultFormatAlpha) {
	// Initialize context for use.
	initializeGLContext();

	// Initialize pipeline.
	delete _pipeline;
	_pipeline = nullptr;

#if !USE_FORCED_GLES
	if (g_context.shadersSupported) {
		ShaderMan.notifyCreate();
		_pipeline = new ShaderPipeline(ShaderMan.query(ShaderManager::kDefault));
	}
#endif

#if !USE_FORCED_GLES2
	if (_pipeline == nullptr) {
		_pipeline = new FixedPipeline();
	}
#endif

	g_context.setPipeline(_pipeline);

	// Disable 3D properties.
	GL_CALL(glDisable(GL_CULL_FACE));
	GL_CALL(glDisable(GL_DEPTH_TEST));
	GL_CALL(glDisable(GL_DITHER));

	g_context.getActivePipeline()->setColor(1.0f, 1.0f, 1.0f, 1.0f);

	GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

	// Setup backbuffer state.

	// Default to black as clear color.
	_backBuffer.setClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	// Setup alpha blend (for overlay and cursor).
	_backBuffer.enableBlend(true);
	// Setup scissor state accordingly.
	_backBuffer.enableScissorTest(!_overlayVisible);

	g_context.getActivePipeline()->setFramebuffer(&_backBuffer);

	// Clear the whole screen for the first three frames to assure any
	// leftovers are cleared.
	_scissorOverride = 3;

	// We use a "pack" alignment (when reading from textures) to 4 here,
	// since the only place where we really use it is the BMP screenshot
	// code and that requires the same alignment too.
	GL_CALL(glPixelStorei(GL_PACK_ALIGNMENT, 4));

	// Refresh the output screen dimensions if some are set up.
	if (_outputScreenWidth != 0 && _outputScreenHeight != 0) {
		setActualScreenSize(_outputScreenWidth, _outputScreenHeight);
	}

	// TODO: Should we try to convert textures into one of those formats if
	// possible? For example, when _gameScreen is CLUT8 we might want to use
	// defaultFormat now.
	_defaultFormat = defaultFormat;
	_defaultFormatAlpha = defaultFormatAlpha;

	if (_gameScreen) {
		_gameScreen->recreate();
	}

	if (_overlay) {
		_overlay->recreate();
	}

	if (_cursor) {
		_cursor->recreate();
	}

#ifdef USE_OSD
	if (_osdMessageSurface) {
		_osdMessageSurface->recreate();
	}

	if (_osdIconSurface) {
		_osdIconSurface->recreate();
	}
#endif
}

void OpenGLGraphicsManager::notifyContextDestroy() {
	if (_gameScreen) {
		_gameScreen->destroy();
	}

	if (_overlay) {
		_overlay->destroy();
	}

	if (_cursor) {
		_cursor->destroy();
	}

#ifdef USE_OSD
	if (_osdMessageSurface) {
		_osdMessageSurface->destroy();
	}

	if (_osdIconSurface) {
		_osdIconSurface->destroy();
	}
#endif

#if !USE_FORCED_GLES
	if (g_context.shadersSupported) {
		ShaderMan.notifyDestroy();
	}
#endif

	// Destroy rendering pipeline.
	g_context.setPipeline(nullptr);
	delete _pipeline;
	_pipeline = nullptr;

	// Rest our context description since the context is gone soon.
	g_context.reset();
}

void OpenGLGraphicsManager::adjustMousePosition(int16 &x, int16 &y) {
	if (_overlayVisible) {
		// It might be confusing that we actually have to handle something
		// here when the overlay is visible. This is because for very small
		// resolutions we have a minimal overlay size and have to adjust
		// for that.
		// This can also happen when the overlay is smaller than the actual
		// display size because of texture size limitations.
		if (_overlay) {
			x = (x * _overlay->getWidth())  / _outputScreenWidth;
			y = (y * _overlay->getHeight()) / _outputScreenHeight;
		}
	} else if (_gameScreen) {
		const int16 width  = _gameScreen->getWidth();
		const int16 height = _gameScreen->getHeight();

		x = (x * width)  / (int)_outputScreenWidth;
		y = (y * height) / (int)_outputScreenHeight;
	}
}

void OpenGLGraphicsManager::setMousePosition(int x, int y) {
	// Whenever the mouse position changed we force a screen redraw to reflect
	// changes properly.
	if (_cursorX != x || _cursorY != y) {
		_forceRedraw = true;
	}

	_cursorX = x;
	_cursorY = y;

	if (_overlayVisible) {
		_cursorDisplayX = x;
		_cursorDisplayY = y;
	} else {
		_cursorDisplayX = _displayX + (x * _displayWidth)  / _outputScreenWidth;
		_cursorDisplayY = _displayY + (y * _displayHeight) / _outputScreenHeight;
	}
}

Surface *OpenGLGraphicsManager::createSurface(const Graphics::PixelFormat &format, bool wantAlpha) {
	GLenum glIntFormat, glFormat, glType;
	if (format.bytesPerPixel == 1) {
#if !USE_FORCED_GLES
		if (TextureCLUT8GPU::isSupportedByContext()) {
			return new TextureCLUT8GPU();
		}
#endif

		const Graphics::PixelFormat &virtFormat = wantAlpha ? _defaultFormatAlpha : _defaultFormat;
		const bool supported = getGLPixelFormat(virtFormat, glIntFormat, glFormat, glType);
		if (!supported) {
			return nullptr;
		} else {
			return new TextureCLUT8(glIntFormat, glFormat, glType, virtFormat);
		}
#if !USE_FORCED_GL
	} else if (isGLESContext() && format == Graphics::PixelFormat(2, 5, 5, 5, 0, 10, 5, 0, 0)) {
		// OpenGL ES does not support a texture format usable for RGB555.
		// Since SCUMM uses this pixel format for some games (and there is no
		// hope for this to change anytime soon) we use pixel format
		// conversion to a supported texture format. However, this is a one
		// time exception.
		return new TextureRGB555();
#endif // !USE_FORCED_GL
	} else {
		const bool supported = getGLPixelFormat(format, glIntFormat, glFormat, glType);
		if (!supported) {
			return nullptr;
		} else {
			return new Texture(glIntFormat, glFormat, glType, format);
		}
	}
}

bool OpenGLGraphicsManager::getGLPixelFormat(const Graphics::PixelFormat &pixelFormat, GLenum &glIntFormat, GLenum &glFormat, GLenum &glType) const {
#ifdef SCUMM_LITTLE_ENDIAN
	if (pixelFormat == Graphics::PixelFormat(4, 8, 8, 8, 8, 0, 8, 16, 24)) { // ABGR8888
#else
	if (pixelFormat == Graphics::PixelFormat(4, 8, 8, 8, 8, 24, 16, 8, 0)) { // RGBA8888
#endif
		glIntFormat = GL_RGBA;
		glFormat = GL_RGBA;
		glType = GL_UNSIGNED_BYTE;
		return true;
	} else if (pixelFormat == Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0)) { // RGB565
		glIntFormat = GL_RGB;
		glFormat = GL_RGB;
		glType = GL_UNSIGNED_SHORT_5_6_5;
		return true;
	} else if (pixelFormat == Graphics::PixelFormat(2, 5, 5, 5, 1, 11, 6, 1, 0)) { // RGBA5551
		glIntFormat = GL_RGBA;
		glFormat = GL_RGBA;
		glType = GL_UNSIGNED_SHORT_5_5_5_1;
		return true;
	} else if (pixelFormat == Graphics::PixelFormat(2, 4, 4, 4, 4, 12, 8, 4, 0)) { // RGBA4444
		glIntFormat = GL_RGBA;
		glFormat = GL_RGBA;
		glType = GL_UNSIGNED_SHORT_4_4_4_4;
		return true;
#if !USE_FORCED_GLES && !USE_FORCED_GLES2
	// The formats below are not supported by every GLES implementation.
	// Thus, we do not mark them as supported when a GLES context is setup.
	} else if (isGLESContext()) {
		return false;
#ifdef SCUMM_LITTLE_ENDIAN
	} else if (pixelFormat == Graphics::PixelFormat(4, 8, 8, 8, 8, 24, 16, 8, 0)) { // RGBA8888
		glIntFormat = GL_RGBA;
		glFormat = GL_RGBA;
		glType = GL_UNSIGNED_INT_8_8_8_8;
		return true;
#endif
	} else if (pixelFormat == Graphics::PixelFormat(2, 5, 5, 5, 0, 10, 5, 0, 0)) { // RGB555
		glIntFormat = GL_RGB;
		glFormat = GL_BGRA;
		glType = GL_UNSIGNED_SHORT_1_5_5_5_REV;
		return true;
	} else if (pixelFormat == Graphics::PixelFormat(2, 4, 4, 4, 4, 8, 4, 0, 12)) { // ARGB4444
		glIntFormat = GL_RGBA;
		glFormat = GL_BGRA;
		glType = GL_UNSIGNED_SHORT_4_4_4_4_REV;
		return true;
#ifdef SCUMM_BIG_ENDIAN
	} else if (pixelFormat == Graphics::PixelFormat(4, 8, 8, 8, 8, 0, 8, 16, 24)) { // ABGR8888
		glIntFormat = GL_RGBA;
		glFormat = GL_RGBA;
		glType = GL_UNSIGNED_INT_8_8_8_8_REV;
		return true;
#endif
	} else if (pixelFormat == Graphics::PixelFormat(4, 8, 8, 8, 8, 8, 16, 24, 0)) { // BGRA8888
		glIntFormat = GL_RGBA;
		glFormat = GL_BGRA;
		glType = GL_UNSIGNED_INT_8_8_8_8;
		return true;
	} else if (pixelFormat == Graphics::PixelFormat(2, 5, 6, 5, 0, 0, 5, 11, 0)) { // BGR565
		glIntFormat = GL_RGB;
		glFormat = GL_RGB;
		glType = GL_UNSIGNED_SHORT_5_6_5_REV;
		return true;
	} else if (pixelFormat == Graphics::PixelFormat(2, 5, 5, 5, 1, 1, 6, 11, 0)) { // BGRA5551
		glIntFormat = GL_RGBA;
		glFormat = GL_BGRA;
		glType = GL_UNSIGNED_SHORT_5_5_5_1;
		return true;
	} else if (pixelFormat == Graphics::PixelFormat(2, 4, 4, 4, 4, 0, 4, 8, 12)) { // ABGR4444
		glIntFormat = GL_RGBA;
		glFormat = GL_RGBA;
		glType = GL_UNSIGNED_SHORT_4_4_4_4_REV;
		return true;
	} else if (pixelFormat == Graphics::PixelFormat(2, 4, 4, 4, 4, 4, 8, 12, 0)) { // BGRA4444
		glIntFormat = GL_RGBA;
		glFormat = GL_BGRA;
		glType = GL_UNSIGNED_SHORT_4_4_4_4;
		return true;
#endif // !USE_FORCED_GLES && !USE_FORCED_GLES2
	} else {
		return false;
	}
}

frac_t OpenGLGraphicsManager::getDesiredGameScreenAspect() const {
	const uint width  = _currentState.gameWidth;
	const uint height = _currentState.gameHeight;

	if (_currentState.aspectRatioCorrection) {
		// In case we enable aspect ratio correction we force a 4/3 ratio.
		// But just for 320x200 and 640x400 games, since other games do not need
		// this.
		if ((width == 320 && height == 200) || (width == 640 && height == 400)) {
			return intToFrac(4) / 3;
		}
	}

	return intToFrac(width) / height;
}

void OpenGLGraphicsManager::recalculateDisplayArea() {
	if (!_gameScreen || _outputScreenHeight == 0) {
		return;
	}

	const frac_t outputAspect = intToFrac(_outputScreenWidth) / _outputScreenHeight;
	const frac_t desiredAspect = getDesiredGameScreenAspect();

	_displayWidth = _outputScreenWidth;
	_displayHeight = _outputScreenHeight;

	// Adjust one dimension for mantaining the aspect ratio.
	if (outputAspect < desiredAspect) {
		_displayHeight = intToFrac(_displayWidth) / desiredAspect;
	} else if (outputAspect > desiredAspect) {
		_displayWidth = fracToInt(_displayHeight * desiredAspect);
	}

	// We center the screen in the middle for now.
	_displayX = (_outputScreenWidth  - _displayWidth ) / 2;
	_displayY = (_outputScreenHeight - _displayHeight) / 2;

	// Setup drawing limitation for game graphics.
	// This invovles some trickery because OpenGL's viewport coordinate system
	// is upside down compared to ours.
	_backBuffer.setScissorBox(_displayX,
	                          _outputScreenHeight - _displayHeight - _displayY,
	                          _displayWidth,
	                          _displayHeight);
	// Clear the whole screen for the first three frames to remove leftovers.
	_scissorOverride = 3;

	// Update the cursor position to adjust for new display area.
	setMousePosition(_cursorX, _cursorY);

	// Force a redraw to assure screen is properly redrawn.
	_forceRedraw = true;
}

void OpenGLGraphicsManager::updateCursorPalette() {
	if (!_cursor || !_cursor->hasPalette()) {
		return;
	}

	if (_cursorPaletteEnabled) {
		_cursor->setPalette(0, 256, _cursorPalette);
	} else {
		_cursor->setPalette(0, 256, _gamePalette);
	}

	_cursor->setColorKey(_cursorKeyColor);
}

void OpenGLGraphicsManager::recalculateCursorScaling() {
	if (!_cursor || !_gameScreen) {
		return;
	}

	// By default we use the unscaled versions.
	_cursorHotspotXScaled = _cursorHotspotX;
	_cursorHotspotYScaled = _cursorHotspotY;
	_cursorWidthScaled = _cursor->getWidth();
	_cursorHeightScaled = _cursor->getHeight();

	// In case scaling is actually enabled we will scale the cursor according
	// to the game screen.
	if (!_cursorDontScale) {
		const frac_t screenScaleFactorX = intToFrac(_displayWidth)  / _gameScreen->getWidth();
		const frac_t screenScaleFactorY = intToFrac(_displayHeight) / _gameScreen->getHeight();

		_cursorHotspotXScaled = fracToInt(_cursorHotspotXScaled * screenScaleFactorX);
		_cursorWidthScaled    = fracToInt(_cursorWidthScaled    * screenScaleFactorX);

		_cursorHotspotYScaled = fracToInt(_cursorHotspotYScaled * screenScaleFactorY);
		_cursorHeightScaled   = fracToInt(_cursorHeightScaled   * screenScaleFactorY);
	}
}

#ifdef USE_OSD
const Graphics::Font *OpenGLGraphicsManager::getFontOSD() {
	return FontMan.getFontByUsage(Graphics::FontManager::kLocalizedFont);
}
#endif

void OpenGLGraphicsManager::saveScreenshot(const Common::String &filename) const {
	const uint width  = _outputScreenWidth;
	const uint height = _outputScreenHeight;

	// A line of a BMP image must have a size divisible by 4.
	// We calculate the padding bytes needed here.
	// Since we use a 3 byte per pixel mode, we can use width % 4 here, since
	// it is equal to 4 - (width * 3) % 4. (4 - (width * Bpp) % 4, is the
	// usual way of computing the padding bytes required).
	const uint linePaddingSize = width % 4;
	const uint lineSize        = width * 3 + linePaddingSize;

	// Allocate memory for screenshot
	uint8 *pixels = new uint8[lineSize * height];

	// Get pixel data from OpenGL buffer
	GL_CALL(glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels));

	// BMP stores as BGR. Since we can't assume that GL_BGR is supported we
	// will swap the components from the RGB we read to BGR on our own.
	for (uint y = height; y-- > 0;) {
		uint8 *line = pixels + y * lineSize;

		for (uint x = width; x > 0; --x, line += 3) {
			SWAP(line[0], line[2]);
		}
	}

	// Open file
	Common::DumpFile out;
	out.open(filename);

	// Write BMP header
	out.writeByte('B');
	out.writeByte('M');
	out.writeUint32LE(height * lineSize + 54);
	out.writeUint32LE(0);
	out.writeUint32LE(54);
	out.writeUint32LE(40);
	out.writeUint32LE(width);
	out.writeUint32LE(height);
	out.writeUint16LE(1);
	out.writeUint16LE(24);
	out.writeUint32LE(0);
	out.writeUint32LE(0);
	out.writeUint32LE(0);
	out.writeUint32LE(0);
	out.writeUint32LE(0);
	out.writeUint32LE(0);

	// Write pixel data to BMP
	out.write(pixels, lineSize * height);

	// Free allocated memory
	delete[] pixels;
}

} // End of namespace OpenGL
