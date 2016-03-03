/*
 Copyright (c) 2010-2015, Paul Houx - All rights reserved.
 This code is intended for use with the Cinder C++ library: http://libcinder.org

 This file is part of Cinder-Warping.

 Cinder-Warping is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Cinder-Warping is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Cinder-Warping.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"
#include "cinder/ImageIo.h"
#include "cinder/Rand.h"

#include "Warp.h"
// Settings
#include "VDSettings.h"
// Utils
#include "VDUtils.h"
// Audio
#include "VDAudio.h"

using namespace ci;
using namespace ci::app;
using namespace ph::warping;
using namespace std;
using namespace VideoDromm;

#define IM_ARRAYSIZE(_ARR)			((int)(sizeof(_ARR)/sizeof(*_ARR)))

/*
Tessellation Shader from Philip Rideout

"Triangle Tessellation with OpenGL 4.0"
http://prideout.net/blog/?p=48

Cinder experimented from Simon Geilfus
https://github.com/simongeilfus/Cinder-Experiments
*/

class BatchassSkyApp : public App {
public:
	static void prepare(Settings *settings);

	void setup() override;
	void cleanup() override;
	void update() override;
	void draw() override;

	void resize() override;

	void mouseMove(MouseEvent event) override;
	void mouseDown(MouseEvent event) override;
	void mouseDrag(MouseEvent event) override;
	void mouseUp(MouseEvent event) override;

	void keyDown(KeyEvent event) override;
	void keyUp(KeyEvent event) override;

	void updateWindowTitle();
private:
	// Settings
	VDSettingsRef	mVDSettings;
	// Utils
	VDUtilsRef		mVDUtils;
	// Audio
	VDAudioRef		mVDAudio;

	bool			mUseBeginEnd;

	fs::path		mSettings;

	gl::TextureRef	mImage;
	WarpList		mWarps;

	Area			mSrcArea;

	gl::BatchRef	mBatch;
	float			mInnerLevel, mOuterLevel;
	// fbo
	void			renderSceneToFbo();
	gl::FboRef		mRenderFbo;

};

void BatchassSkyApp::prepare(Settings *settings)
{
	settings->setWindowSize(1440, 900);
}

void BatchassSkyApp::setup()
{
	// Settings
	mVDSettings = VDSettings::create();
	mVDSettings->mLiveCode = false;
	mVDSettings->mRenderThumbs = false;
	// Utils
	mVDUtils = VDUtils::create(mVDSettings);
	mVDUtils->getWindowsResolution();
	// Audio
	mVDAudio = VDAudio::create(mVDSettings);
	fs::path waveFile = getAssetPath("") / "batchass-sky.wav";
	mVDAudio->loadWaveFile(waveFile.string());
	setWindowSize(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight);
	setWindowPos(ivec2(mVDSettings->mRenderX, mVDSettings->mRenderY));
	// create a batch with our tesselation shader
	auto format = gl::GlslProg::Format()
		.vertex(loadAsset("shader.vert"))
		.fragment(loadAsset("shader.frag"))
		.geometry(loadAsset("shader.geom"))
		.tessellationCtrl(loadAsset("shader.cont"))
		.tessellationEval(loadAsset("shader.eval"));
	auto shader = gl::GlslProg::create(format);
	mBatch = gl::Batch::create(geom::Icosahedron(), shader);

	mInnerLevel = 1.0f;
	mOuterLevel = 1.0f;

	gl::enableDepthWrite();
	gl::enableDepthRead();
	gl::disableBlending();

	// warping
	mUseBeginEnd = false;
	updateWindowTitle();
	disableFrameRate();

	// initialize warps
	mSettings = getAssetPath("") / "warps.xml";
	if (fs::exists(mSettings)) {
		// load warp settings from file if one exists
		mWarps = Warp::readSettings(loadFile(mSettings));
	}
	else {
		// otherwise create a warp from scratch
		mWarps.push_back(WarpPerspectiveBilinear::create());
	}

	// load test image
	try {
		mImage = gl::Texture::create(loadImage(loadAsset("help.png")),
			gl::Texture2d::Format().loadTopDown().mipmap(true).minFilter(GL_LINEAR_MIPMAP_LINEAR));

		mSrcArea = mImage->getBounds();

		// adjust the content size of the warps
		Warp::setSize(mWarps, mImage->getSize());
	}
	catch (const std::exception &e) {
		console() << e.what() << std::endl;
	}
	// render fbo
	gl::Fbo::Format fboFormat;
	//format.setSamples( 4 ); // uncomment this to enable 4x antialiasing
	mRenderFbo = gl::Fbo::create(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight, fboFormat.colorTexture());

}

void BatchassSkyApp::cleanup()
{
	// save warp settings
	Warp::writeSettings(mWarps, writeFile(mSettings));
	mVDSettings->save();
}

void BatchassSkyApp::update()
{
	mVDAudio->update();
	updateWindowTitle();
}
// Render the scene into the FBO
void BatchassSkyApp::renderSceneToFbo()
{
	// this will restore the old framebuffer binding when we leave this function
	// on non-OpenGL ES platforms, you can just call mFbo->unbindFramebuffer() at the end of the function
	// but this will restore the "screen" FBO on OpenGL ES, and does the right thing on both platforms
	gl::ScopedFramebuffer fbScp(mRenderFbo);
	gl::clear(Color::gray(0.3f), true);//mBlack
	// setup the viewport to match the dimensions of the FBO
	gl::ScopedViewport scpVp(ivec2(0), mRenderFbo->getSize());
	gl::color(Color::white());

	// setup basic camera
	auto cam = CameraPersp(getWindowWidth(), getWindowHeight(), 60, 1, 1000).calcFraming(Sphere(vec3(0.0f), 1.25f));
	gl::setMatrices(cam);
	gl::rotate(getElapsedSeconds() * 0.1f, vec3(0.123, 0.456, 0.789));
	gl::viewport(getWindowSize());

	// update uniforms
	mBatch->getGlslProg()->uniform("uTessLevelInner", mInnerLevel);
	mBatch->getGlslProg()->uniform("uTessLevelOuter", mOuterLevel);

	// bypass gl::Batch::draw method so we can use GL_PATCHES
	gl::ScopedVao scopedVao(mBatch->getVao().get());
	gl::ScopedGlslProg scopedShader(mBatch->getGlslProg());

	gl::context()->setDefaultShaderVars();

	if (mBatch->getVboMesh()->getNumIndices())
		glDrawElements(GL_PATCHES, mBatch->getVboMesh()->getNumIndices(), mBatch->getVboMesh()->getIndexDataType(), (GLvoid*)(0));
	else
		glDrawArrays(GL_PATCHES, 0, mBatch->getVboMesh()->getNumIndices());

}
void BatchassSkyApp::draw()
{
	// clear the window and set the drawing color to white
	gl::clear(Color::black());
	//renderSceneToFbo();

	// setup basic camera
	auto cam = CameraPersp(getWindowWidth(), getWindowHeight(), 60, 1, 1000).calcFraming(Sphere(vec3(0.0f), 1.25f));
	gl::setMatrices(cam);
	//gl::rotate(getElapsedSeconds() * 0.1f, vec3(0.123, 0.456, 0.789));
	gl::rotate(getElapsedSeconds() * 0.1f + mVDSettings->maxVolume/100, vec3(0.123, 0.456, 0.789));
	gl::viewport(getWindowSize());

	// update uniforms
	mBatch->getGlslProg()->uniform("uTessLevelInner", mInnerLevel + mVDSettings->maxVolume/10);
	mBatch->getGlslProg()->uniform("uTessLevelOuter", mOuterLevel);

	// bypass gl::Batch::draw method so we can use GL_PATCHES
	gl::ScopedVao scopedVao(mBatch->getVao().get());
	gl::ScopedGlslProg scopedShader(mBatch->getGlslProg());

	gl::context()->setDefaultShaderVars();

	if (mBatch->getVboMesh()->getNumIndices())
		glDrawElements(GL_PATCHES, mBatch->getVboMesh()->getNumIndices(), mBatch->getVboMesh()->getIndexDataType(), (GLvoid*)(0));
	else
		glDrawArrays(GL_PATCHES, 0, mBatch->getVboMesh()->getNumIndices());

		/*for (auto &warp : mWarps) {
			if (mUseBeginEnd) {
				// a) issue your draw commands between begin() and end() statements
				warp->begin();

				// in this demo, we want to draw a specific area of our image,
				// but if you want to draw the whole image, you can simply use: gl::draw( mImage );
				gl::draw(mImage, mSrcArea, warp->getBounds());

				warp->end();
			}
			else {
				// b) simply draw a texture on them (ideal for video)

				// in this demo, we want to draw a specific area of our image,
				// but if you want to draw the whole image, you can simply use: warp->draw( mImage );
				warp->draw(mImage, mSrcArea);
			}
			warp->draw(mRenderFbo->getColorTexture(), mRenderFbo->getBounds());

		}*/

}

void BatchassSkyApp::resize()
{
	// tell the warps our window has been resized, so they properly scale up or down
	Warp::handleResize(mWarps);
}

void BatchassSkyApp::mouseMove(MouseEvent event)
{
	// pass this mouse event to the warp editor first
	if (!Warp::handleMouseMove(mWarps, event)) {
		// let your application perform its mouseMove handling here
	}
}

void BatchassSkyApp::mouseDown(MouseEvent event)
{
	// pass this mouse event to the warp editor first
	if (!Warp::handleMouseDown(mWarps, event)) {
		// let your application perform its mouseDown handling here
	}
}

void BatchassSkyApp::mouseDrag(MouseEvent event)
{
	// pass this mouse event to the warp editor first
	if (!Warp::handleMouseDrag(mWarps, event)) {
		// let your application perform its mouseDrag handling here
	}
}

void BatchassSkyApp::mouseUp(MouseEvent event)
{
	// pass this mouse event to the warp editor first
	if (!Warp::handleMouseUp(mWarps, event)) {
		// let your application perform its mouseUp handling here
	}
}

void BatchassSkyApp::keyDown(KeyEvent event)
{
	// pass this key event to the warp editor first
	if (!Warp::handleKeyDown(mWarps, event)) {
		// warp editor did not handle the key, so handle it here
		switch (event.getCode()) {

		case KeyEvent::KEY_LEFT: mInnerLevel--; break;
		case KeyEvent::KEY_RIGHT: mInnerLevel++; break;
		case KeyEvent::KEY_DOWN: mOuterLevel--; break;
		case KeyEvent::KEY_UP: mOuterLevel++; break;
		case KeyEvent::KEY_1: mBatch->replaceVboMesh(gl::VboMesh::create(geom::Cube())); break;
		case KeyEvent::KEY_2: mBatch->replaceVboMesh(gl::VboMesh::create(geom::Icosahedron())); break;
		case KeyEvent::KEY_3: mBatch->replaceVboMesh(gl::VboMesh::create(geom::Sphere())); break;
		case KeyEvent::KEY_4: mBatch->replaceVboMesh(gl::VboMesh::create(geom::Icosphere())); break;


		case KeyEvent::KEY_ESCAPE:
			// quit the application
			quit();
			break;
		case KeyEvent::KEY_f:
			// toggle full screen
			setFullScreen(!isFullScreen());
			break;
		case KeyEvent::KEY_v:
			// toggle vertical sync
			gl::enableVerticalSync(!gl::isVerticalSyncEnabled());
			break;
		case KeyEvent::KEY_w:
			// toggle warp edit mode
			Warp::enableEditMode(!Warp::isEditModeEnabled());
			break;
		case KeyEvent::KEY_a:
			// toggle drawing a random region of the image
			if (mSrcArea.getWidth() != mImage->getWidth() || mSrcArea.getHeight() != mImage->getHeight())
				mSrcArea = mImage->getBounds();
			else {
				int x1 = Rand::randInt(0, mImage->getWidth() - 150);
				int y1 = Rand::randInt(0, mImage->getHeight() - 150);
				int x2 = Rand::randInt(x1 + 150, mImage->getWidth());
				int y2 = Rand::randInt(y1 + 150, mImage->getHeight());
				mSrcArea = Area(x1, y1, x2, y2);
			}
			break;
		case KeyEvent::KEY_SPACE:
			// toggle drawing mode
			mUseBeginEnd = !mUseBeginEnd;
			updateWindowTitle();
			break;
		}
		mInnerLevel = math<float>::max(mInnerLevel, 1.0f);
		mOuterLevel = math<float>::max(mOuterLevel, 1.0f);
	}

}

void BatchassSkyApp::keyUp(KeyEvent event)
{
	// pass this key event to the warp editor first
	if (!Warp::handleKeyUp(mWarps, event)) {
		// let your application perform its keyUp handling here
	}
}

void BatchassSkyApp::updateWindowTitle()
{
	getWindow()->setTitle(to_string((int)getAverageFps()) + " fps Batchass Sky");
}

CINDER_APP(BatchassSkyApp, RendererGl(RendererGl::Options().msaa(8)), &BatchassSkyApp::prepare)
