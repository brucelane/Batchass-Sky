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
	settings->setWindowSize(40, 10);
}

void BatchassSkyApp::setup()
{
	mWaveDelay = mFadeInDelay = mFadeOutDelay = true;
	// Settings
	mVDSettings = VDSettings::create();
	mVDSession = VDSession::create(mVDSettings);
	mVDSession->getWindowsResolution();
	//load mix shader
	try
	{
		fs::path mixFragFile = getAssetPath("") / mVDSettings->mAssetsPath / "mix.frag";
		if (fs::exists(mixFragFile))
		{
			aShader = gl::GlslProg::create(loadAsset("passthru.vert"), loadFile(mixFragFile));
		}
		else
		{
			CI_LOG_V("mix.frag does not exist, should quit");

		}
	}
	catch (gl::GlslProgCompileExc &exc)
	{
		CI_LOG_V("unable to load/compile shader:" + string(exc.what()));
	}
	catch (const std::exception &e)
	{
		CI_LOG_V("unable to load shader:" + string(e.what()));
	}
	// create a batch with our tesselation shader
	auto format = gl::GlslProg::Format()
		.vertex(loadAsset("sky/shader.vert"))
		.fragment(loadAsset("sky/shader.frag"))
		.geometry(loadAsset("sky/shader.geom"))
		.tessellationCtrl(loadAsset("sky/shader.cont"))
		.tessellationEval(loadAsset("sky/shader.eval"));
	auto shader = gl::GlslProg::create(format);
	mBatch = gl::Batch::create(geom::TorusKnot(), shader);

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
	mSettings = getAssetPath("") / mVDSettings->mAssetsPath / "warps.xml";
	if (fs::exists(mSettings)) {
		// load warp settings from file if one exists
		mWarps = Warp::readSettings(loadFile(mSettings));
	}
	else {
		// otherwise create a warp from scratch
		mWarps.push_back(WarpPerspectiveBilinear::create());
		mWarps.push_back(WarpPerspectiveBilinear::create());
	}
	//mWarps.push_back(WarpPerspectiveBilinear::create());

// render fbo
	gl::Fbo::Format fboFormat;
	//format.setSamples( 4 ); // uncomment this to enable 4x antialiasing
	mRenderFbo = gl::Fbo::create(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight, fboFormat.colorTexture());
	mFbo = gl::Fbo::create(mVDSettings->mFboWidth, mVDSettings->mFboHeight, fboFormat.colorTexture());
	// mouse cursor
	if (mVDSettings->mCursorVisible)
	{
		hideCursor();
	}
	else
	{
		showCursor();
	}
	// Sky bpm = 180.0f;
	setFrameRate(mVDSession->getTargetFps());

}

void BatchassSkyApp::cleanup()
{
	// save warp settings
	Warp::writeSettings(mWarps, writeFile(mSettings));
	mVDSettings->save();
	mVDSession->save();
}

void BatchassSkyApp::update()
{
	mVDSession->setControlValue(mVDSettings->IFPS, getAverageFps());
	mVDSession->update();

	updateWindowTitle();
}

// Render the scene into the FBO
void BatchassSkyApp::renderSceneToFbo()
{
	// this will restore the old framebuffer binding when we leave this function
	// on non-OpenGL ES platforms, you can just call mFbo->unbindFramebuffer() at the end of the function
	// but this will restore the "screen" FBO on OpenGL ES, and does the right thing on both platforms
	gl::ScopedFramebuffer fbScp(mRenderFbo);
	gl::clear(Color::gray(0.03f), true);//mBlack
	// setup the viewport to match the dimensions of the FBO
	gl::ScopedViewport scpVp(ivec2(0), mRenderFbo->getSize());
	gl::color(Color::white());
	// setup basic camera
	auto cam = CameraPersp(mVDSettings->mFboWidth + ((int)mVDSession->getMaxVolume() * 5), mVDSettings->mFboHeight, 60, 1, 1000).calcFraming(Sphere(vec3(0.0f), 1.25f));
	gl::setMatrices(cam);
	//gl::rotate(getElapsedSeconds() * 0.1f, vec3(0.123, 0.456, 0.789));
	gl::rotate(getElapsedSeconds() * 0.1f + mVDSession->getMaxVolume() / 100, vec3(0.123, 0.456, 0.789));
	gl::viewport(getWindowSize());

	// update uniforms
	mBatch->getGlslProg()->uniform("uTessLevelInner", mInnerLevel + mVDSession->getMaxVolume() / 10);
	mBatch->getGlslProg()->uniform("uTessLevelOuter", mOuterLevel);
	mBatch->getGlslProg()->uniform("iFR", mVDSession->getControlValueByName("iFR"));
	mBatch->getGlslProg()->uniform("iFG", mVDSession->getControlValueByName("iFG"));
	mBatch->getGlslProg()->uniform("iAlpha", mVDSession->getControlValueByName("iAlpha"));

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

	if (mWaveDelay) {
		if (getElapsedFrames() > mVDSession->getWavePlaybackDelay()) {
			mWaveDelay = false;
			setWindowSize(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight);
			setWindowPos(ivec2(mVDSettings->mRenderX, mVDSettings->mRenderY));
			fs::path waveFile = getAssetPath("") / mVDSettings->mAssetsPath / mVDSession->getWaveFileName();
			mVDSession->loadAudioFile(waveFile.string());
		}
	}
	if (mFadeOutDelay) {
		/*if (getElapsedFrames() > mVDSession->getEndFrame()) {
			mFadeOutDelay = false;
			timeline().apply(&mVDSettings->iAlpha, 1.0f, 0.0f, 2.0f, EaseInCubic());
		}*/
	}
	if (mFadeInDelay) {
		if (getElapsedFrames() > mVDSession->getFadeInDelay()) {
			mFadeInDelay = false;
			setWindowSize(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight);
			setWindowPos(ivec2(mVDSettings->mRenderX, mVDSettings->mRenderY));
			timeline().apply(&mVDSettings->iAlpha, 0.0f, 1.0f, 1.5f, EaseInCubic());
		}
	}
	else {
		renderSceneToFbo();

		/***********************************************
		* mix 2 FBOs begin
		* first render the 2 frags to fbos (done before)
		* then use them as textures for the mix shader
		*/

		// draw using the mix shader
		mFbo->bindFramebuffer();
		//gl::setViewport(mVDFbos[mVDSettings->mMixFboIndex].fbo.getBounds());

		// clear the FBO
		gl::clear();
		gl::setMatricesWindow(mVDSettings->mFboWidth, mVDSettings->mFboHeight);

		aShader->bind();
		aShader->uniform("iGlobalTime", mVDSession->getControlValue(0));
		//20140703 aShader->uniform("iResolution", vec3(mVDSettings->mRenderResoXY.x, mVDSettings->mRenderResoXY.y, 1.0));
		aShader->uniform("iResolution", vec3(mVDSettings->mFboWidth, mVDSettings->mFboHeight, 1.0));
		//aShader->uniform("iChannelResolution", mVDSettings->iChannelResolution, 4);
		aShader->uniform("iMouse", mVDSession->getVec4UniformValueByName("iMouse"));
		aShader->uniform("iChannel0", 0);
		aShader->uniform("iChannel1", 1);
		aShader->uniform("iAudio0", 0);
		aShader->uniform("iAlpha", mVDSession->getControlValueByName("iAlpha") * mVDSettings->iAlpha);
		aShader->uniform("iColor", vec3(mVDSession->getControlValueByName("iFR"), mVDSession->getControlValueByName("iFG"), mVDSession->getControlValueByName("iFB")));
		aShader->uniform("iBackgroundColor", vec3(mVDSession->getControlValueByName("iBR"), mVDSession->getControlValueByName("iBG"), mVDSession->getControlValueByName("iBB")));
		aShader->uniform("iSteps", (int)mVDSession->getControlValueByName("iSteps"));
		aShader->uniform("iRatio", mVDSession->getControlValueByName("iRatio"));
		aShader->uniform("iBlendmode", mVDSession->getIntUniformValueByName("iBlendmode"));
		aShader->uniform("iChromatic", mVDSession->getControlValueByName("iChromatic"));
		aShader->uniform("iRotationSpeed", mVDSession->getControlValueByName("iRotationSpeed"));
		aShader->uniform("iCrossfade", mVDSession->getControlValueByName("iCrossfade"));
		aShader->uniform("iPixelate", mVDSession->getControlValueByName("iPixelate"));
		aShader->uniform("iExposure", mVDSession->getControlValueByName("iExposure"));
		aShader->uniform("iToggle", (int)mVDSession->getBoolUniformValueByName("iToggle"));
		aShader->uniform("iVignette", (int)mVDSession->getBoolUniformValueByName("iVignette"));
		aShader->uniform("iInvert", (int)mVDSession->getBoolUniformValueByName("iInvert"));
		aShader->uniform("iGlitch", (int)mVDSession->getBoolUniformValueByName("iGlitch"));
		aShader->uniform("iTrixels", mVDSession->getControlValueByName("iTrixels"));
		aShader->uniform("iGridSize", mVDSession->getControlValueByName("iGridSize"));
		aShader->uniform("iRedMultiplier", mVDSession->getControlValueByName("iRedMultiplier"));
		aShader->uniform("iGreenMultiplier", mVDSession->getControlValueByName("iGreenMultiplier"));
		aShader->uniform("iBlueMultiplier", mVDSession->getControlValueByName("iBlueMultiplier"));

		aShader->uniform("iFps", mVDSession->getControlValueByName("iFps"));
		aShader->uniform("iBadTv", mVDSession->getControlValueByName("iBadTv"));
		aShader->uniform("iTempoTime", mVDSession->getControlValueByName("iTempoTime"));
		/*aShader->uniform("iFreq0", mVDAnimation->iFreqs[0]);
		aShader->uniform("iFreq1", mVDAnimation->iFreqs[1]);
		aShader->uniform("iFreq2", mVDAnimation->iFreqs[2]);
		aShader->uniform("iFreq3", mVDAnimation->iFreqs[3]);
		aShader->uniform("iChannelTime", mVDSettings->iChannelTime, 4);
		aShader->uniform("iDeltaTime", mVDAnimation->iDeltaTime);
		*/
		aShader->uniform("iZoom", mVDSession->getControlValueByName("iZoom"));
		aShader->uniform("iRenderXY", mVDSettings->mRenderXY);
		aShader->uniform("iFade", (int)mVDSettings->iFade);
		aShader->uniform("iLight", (int)mVDSettings->iLight);
		aShader->uniform("iLightAuto", (int)mVDSettings->iLightAuto);
		aShader->uniform("iGreyScale", (int)mVDSettings->iGreyScale);
		aShader->uniform("iTransition", mVDSettings->iTransition);
		aShader->uniform("iAnim", mVDSettings->iAnim.value());
		aShader->uniform("iRepeat", (int)mVDSettings->iRepeat);
		aShader->uniform("iDebug", (int)mVDSettings->iDebug);
		aShader->uniform("iShowFps", (int)mVDSettings->iShowFps);
		aShader->uniform("iBeat", mVDSettings->iBeat);
		aShader->uniform("iSeed", mVDSettings->iSeed);
		aShader->uniform("iFlipH", 0);
		aShader->uniform("iFlipV", 0);
		aShader->uniform("iParam1", mVDSettings->iParam1);
		aShader->uniform("iParam2", mVDSettings->iParam2);
		aShader->uniform("iXorY", mVDSettings->iXorY);

		mRenderFbo->getColorTexture()->bind(0);
		mRenderFbo->getColorTexture()->bind(1);
		gl::drawSolidRect(Rectf(0, 0, mVDSettings->mFboWidth, mVDSettings->mFboHeight));
		// stop drawing into the FBO
		mFbo->unbindFramebuffer();
		mRenderFbo->getColorTexture()->unbind();
		mRenderFbo->getColorTexture()->unbind();

		//aShader->unbind();
		//sTextures[5] = mVDFbos[mVDSettings->mMixFboIndex]->getTexture();

		//}

		/***********************************************
		* mix 2 FBOs end
		*/
		gl::clear(Color::black());
		gl::setMatricesWindow(toPixels(getWindowSize()));
		//gl::draw(mRenderFbo->getColorTexture());
		int i = 0;
		for (auto &warp : mWarps) {
			/*if (mUseBeginEnd) {
			int w = mRenderFbo->getColorTexture()->getWidth();
			int h = mRenderFbo->getColorTexture()->getHeight();
			warp->draw(mRenderFbo->getColorTexture(), Area(0, 0, w, h), Rectf(200, 100, 200 + w / 2, 100 + h / 2));
			}
			else {
			if (i%2 == 0) {
			warp->draw(mFbo->getColorTexture(), mVDUtils->getSrcAreaLeftOrTop());
			}
			else {
			warp->draw(mFbo->getColorTexture(), mVDUtils->getSrcAreaRightOrBottom());
			}
			}*/
			warp->draw(mFbo->getColorTexture(), mFbo->getBounds());
			//if (i == 0) warp->draw(mRenderFbo->getColorTexture(), mRenderFbo->getBounds());
			i++;
		}
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
		//mVDAnimation->controlValues[10] = event.getX() / mVDSettings->mRenderWidth;
		//mVDUtils->moveX1LeftOrTop(event.getX());
		//mVDUtils->moveY1LeftOrTop(event.getY());
	}
}

void BatchassSkyApp::mouseDown(MouseEvent event)
{
	// pass this mouse event to the warp editor first
	if (!Warp::handleMouseDown(mWarps, event)) {
		// let your application perform its mouseDown handling here
		//mVDAnimation->controlValues[45] = 1.0f;
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
		//mVDAnimation-> controlValues[45] = 0.0f;
	}
}

void BatchassSkyApp::keyDown(KeyEvent event)
{
	string fileName;
	// pass this key event to the warp editor first
	if (!Warp::handleKeyDown(mWarps, event)) {
		// warp editor did not handle the key, so handle it here
		if (!mVDSession->handleKeyDown(event)) {
			// Animation did not handle the key, so handle it here
			switch (event.getCode()) {

			case KeyEvent::KEY_LEFT: mInnerLevel -= 0.1f; break;
			case KeyEvent::KEY_RIGHT: mInnerLevel += 0.1f; break;
			case KeyEvent::KEY_DOWN: mOuterLevel -= 0.1f; break;
			case KeyEvent::KEY_UP: mOuterLevel += 0.1f; break;
			case KeyEvent::KEY_1: mBatch->replaceVboMesh(gl::VboMesh::create(geom::Cube())); break;
			case KeyEvent::KEY_2: mBatch->replaceVboMesh(gl::VboMesh::create(geom::Icosahedron())); break;
			case KeyEvent::KEY_3: mBatch->replaceVboMesh(gl::VboMesh::create(geom::Sphere())); break;
			case KeyEvent::KEY_4: mBatch->replaceVboMesh(gl::VboMesh::create(geom::Icosphere())); break;
			case KeyEvent::KEY_5: mBatch->replaceVboMesh(gl::VboMesh::create(geom::Cylinder())); break;
			case KeyEvent::KEY_6: mBatch->replaceVboMesh(gl::VboMesh::create(geom::Torus())); break;
			case KeyEvent::KEY_7: mBatch->replaceVboMesh(gl::VboMesh::create(geom::TorusKnot())); break;


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
				//gl::enableVerticalSync(!gl::isVerticalSyncEnabled());
				//mVDSettings->mSplitWarpV = !mVDSettings->mSplitWarpV;
				//mVDUtils->splitWarp(mRenderFbo->getWidth(), mRenderFbo->getHeight());
				break;
			case KeyEvent::KEY_h:
				//mVDSettings->mSplitWarpH = !mVDSettings->mSplitWarpH;
				//mVDUtils->splitWarp(mRenderFbo->getWidth(), mRenderFbo->getHeight());
				break;
				/*case KeyEvent::KEY_r:
					// reset split the image
					mVDSettings->mSplitWarpV = false;
					mVDSettings->mSplitWarpH = false;
					mVDUtils->splitWarp(mRenderFbo->getWidth(), mRenderFbo->getHeight());
					break;*/
			case KeyEvent::KEY_c:
				mWarps.push_back(WarpPerspectiveBilinear::create());
				mWarps[mWarps.size() - 1]->setWidth(100);
				mWarps[mWarps.size() - 1]->setHeight(100);

			case KeyEvent::KEY_a:
				fileName = "warps" + toString(getElapsedFrames()) + ".xml";
				mSettings = getAssetPath("") / mVDSettings->mAssetsPath / fileName;
				Warp::writeSettings(mWarps, writeFile(mSettings));
				mSettings = getAssetPath("") / mVDSettings->mAssetsPath / "warps.xml";
				break;
			case KeyEvent::KEY_w:
				// toggle warp edit mode
				Warp::enableEditMode(!Warp::isEditModeEnabled());
				break;

			}
			mInnerLevel = math<float>::max(mInnerLevel, 1.0f);
			mOuterLevel = math<float>::max(mOuterLevel, 1.0f);
		}
	}
}

void BatchassSkyApp::keyUp(KeyEvent event)
{
	// pass this key event to the warp editor first
	if (!Warp::handleKeyUp(mWarps, event)) {
		if (!mVDSession->handleKeyUp(event)) {
			// Animation did not handle the key, so handle it here
		}
	}
}

void BatchassSkyApp::updateWindowTitle()
{
	getWindow()->setTitle(to_string(getElapsedFrames()) + " " + to_string((int)getAverageFps()) + " fps Batchass Sky");
}

CINDER_APP(BatchassSkyApp, RendererGl(RendererGl::Options().msaa(8)), &BatchassSkyApp::prepare)
