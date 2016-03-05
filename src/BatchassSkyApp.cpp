#include "BatchassSkyApp.h"
/*
Tessellation Shader from Philip Rideout

"Triangle Tessellation with OpenGL 4.0"
http://prideout.net/blog/?p=48

Cinder experimented from Simon Geilfus
https://github.com/simongeilfus/Cinder-Experiments
*/



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
	setWindowSize(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight);
	setWindowPos(ivec2(mVDSettings->mRenderX, mVDSettings->mRenderY));
	// Audio
	mVDAudio = VDAudio::create(mVDSettings);
	fs::path waveFile = getAssetPath("") / "batchass-sky.wav";
	mVDAudio->loadWaveFile(waveFile.string());
	// Animation
	mVDAnimation = VDAnimation::create(mVDSettings);

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
		mWarps.push_back(WarpPerspectiveBilinear::create());
	}

	// render fbo
	gl::Fbo::Format fboFormat;
	//format.setSamples( 4 ); // uncomment this to enable 4x antialiasing
	mRenderFbo = gl::Fbo::create(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight, fboFormat.colorTexture());
	mSrcArea1 = mRenderFbo->getBounds();
	mSrcArea2 = mRenderFbo->getBounds();
	iChromatic = 1.0f;
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
	//gl::rotate(getElapsedSeconds() * 0.1f, vec3(0.123, 0.456, 0.789));
	gl::rotate(getElapsedSeconds() * 0.1f + mVDSettings->maxVolume / 100, vec3(0.123, 0.456, 0.789));
	gl::viewport(getWindowSize());

	// update uniforms
	mBatch->getGlslProg()->uniform("uTessLevelInner", mInnerLevel + mVDSettings->maxVolume / 10);
	mBatch->getGlslProg()->uniform("uTessLevelOuter", mOuterLevel);
	mBatch->getGlslProg()->uniform("iChromatic", iChromatic);

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
	renderSceneToFbo();

	gl::setMatricesWindow(toPixels(getWindowSize()));

	//gl::draw(mRenderFbo->getColorTexture());
	int i = 0;
	for (auto &warp : mWarps) {
		if (mUseBeginEnd) {

			int w = mRenderFbo->getColorTexture()->getWidth();
			int h = mRenderFbo->getColorTexture()->getHeight();
			warp->draw(mRenderFbo->getColorTexture(), Area(0, 0, w, h), Rectf(200, 100, 200 + w / 2, 100 + h / 2));
		}
		else {
			if (i == 0) {
				warp->draw(mRenderFbo->getColorTexture(), mSrcArea1);
			}
			else {
				warp->draw(mRenderFbo->getColorTexture(), mSrcArea2);
			}
		}
		//warp->draw(mRenderFbo->getColorTexture(), mRenderFbo->getBounds());
		i++;
	}

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
		iChromatic = event.getX() / 100.0;

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
			// toggle split the image
			if (mSrcArea1.getWidth() != mRenderFbo->getWidth() || mSrcArea1.getHeight() != mRenderFbo->getHeight()) {
				mSrcArea1 = mRenderFbo->getBounds();
				mSrcArea2 = mRenderFbo->getBounds();
			}
			else {
				int x1 = 0;
				int y1 = 0;
				int x2 = mRenderFbo->getWidth() / 2;
				int y2 = mRenderFbo->getHeight();
				mSrcArea1 = Area(x1, y1, x2, y2);
				x1 = mRenderFbo->getWidth() / 2;
				x2 = mRenderFbo->getWidth();
				mSrcArea2 = Area(x1, y1, x2, y2);
			}
			break;
		case KeyEvent::KEY_s:
			// save animation
			mVDAnimation->save();
			break;
		case KeyEvent::KEY_c:
			mUseBeginEnd = !mUseBeginEnd;
			break;
		case KeyEvent::KEY_k:
			// save keyframe
			mVDAnimation->saveKeyframe(to_string(getElapsedSeconds()));
			break;
		case KeyEvent::KEY_SPACE:

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
