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
	firstDraw = true;
	iBadTvRunning = false;
	// Settings
	mVDSettings = VDSettings::create();
	mVDSettings->mLiveCode = false;
	mVDSettings->mRenderThumbs = false;
	mVDSession = VDSession::create(mVDSettings);
	// Utils
	mVDUtils = VDUtils::create(mVDSettings);
	mVDUtils->getWindowsResolution();
	// Audio
	mVDAudio = VDAudio::create(mVDSettings);
	// Animation
	mVDAnimation = VDAnimation::create(mVDSettings, mVDSession);
	// Shaders
	mVDShaders = VDShaders::create(mVDSettings);
	// mix fbo at index 0
	mVDFbos.push_back(VDFbo::create(mVDSettings, "mix", mVDSettings->mFboWidth, mVDSettings->mFboHeight));

	// create a batch with our tesselation shader
	auto format = gl::GlslProg::Format()
		.vertex(loadAsset("shader.vert"))
		.fragment(loadAsset("shader.frag"))
		.geometry(loadAsset("shader.geom"))
		.tessellationCtrl(loadAsset("shader.cont"))
		.tessellationEval(loadAsset("shader.eval"));
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
	mVDAudio->update();
	mVDAnimation->update();
	if (mVDAnimation->getBadTV(getElapsedFrames()) == 1) {
		iBadTvRunning = true;
		// duration = 0.2
		timeline().apply(&mVDSettings->iBadTv, 60.0f, 0.0f, 0.2f, EaseInCubic()).finishFn(resetBadTv);
	}
	/*if (!iBadTvRunning && mVDSettings->iBadTv > 0.0) {
		timeline().apply(&mVDSettings->iBadTv, 60.0f, 0.0f, 1.0f, EaseInCubic()).finishFn(resetBadTv);
	}*/
	updateWindowTitle();
}
void resetBadTv()
{
	iBadTvRunning = false;
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
	auto cam = CameraPersp(mVDSettings->mFboWidth + ((int)mVDSettings->maxVolume * 5), mVDSettings->mFboHeight, 60, 1, 1000).calcFraming(Sphere(vec3(0.0f), 1.25f));
	gl::setMatrices(cam);
	//gl::rotate(getElapsedSeconds() * 0.1f, vec3(0.123, 0.456, 0.789));
	gl::rotate(getElapsedSeconds() * 0.1f + mVDSettings->maxVolume / 100, vec3(0.123, 0.456, 0.789));
	gl::viewport(getWindowSize());

	// update uniforms
	mBatch->getGlslProg()->uniform("uTessLevelInner", mInnerLevel + mVDSettings->maxVolume / 10);
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
	renderSceneToFbo();

	/***********************************************
	* mix 2 FBOs begin
	* first render the 2 frags to fbos (done before)
	* then use them as textures for the mix shader
	*/

	// draw using the mix shader
	mVDFbos[mVDSettings->mMixFboIndex]->getFboRef()->bindFramebuffer();
	//gl::setViewport(mVDFbos[mVDSettings->mMixFboIndex].fbo.getBounds());

	// clear the FBO
	gl::clear();
	gl::setMatricesWindow(mVDSettings->mFboWidth, mVDSettings->mFboHeight);

	aShader = mVDShaders->getMixShader();
	aShader->bind();
	aShader->uniform("iGlobalTime", mVDSettings->iGlobalTime);
	//20140703 aShader->uniform("iResolution", vec3(mVDSettings->mRenderResoXY.x, mVDSettings->mRenderResoXY.y, 1.0));
	aShader->uniform("iResolution", vec3(mVDSettings->mFboWidth, mVDSettings->mFboHeight, 1.0));
	aShader->uniform("iChannelResolution", mVDSettings->iChannelResolution, 4);
	aShader->uniform("iMouse", vec4(mVDSettings->mRenderPosXY.x, mVDSettings->mRenderPosXY.y, mVDSettings->iMouse.z, mVDSettings->iMouse.z));//iMouse =  Vec3i( event.getX(), mRenderHeight - event.getY(), 1 );
	aShader->uniform("iChannel0", 0);
	aShader->uniform("iChannel1", 1);
	aShader->uniform("iAudio0", 0);
	aShader->uniform("iFreq0", mVDSettings->iFreqs[0]);
	aShader->uniform("iFreq1", mVDSettings->iFreqs[1]);
	aShader->uniform("iFreq2", mVDSettings->iFreqs[2]);
	aShader->uniform("iFreq3", mVDSettings->iFreqs[3]);
	aShader->uniform("iChannelTime", mVDSettings->iChannelTime, 4);
	aShader->uniform("iColor", vec3(mVDSettings->controlValues[1], mVDSettings->controlValues[2], mVDSettings->controlValues[3]));// mVDSettings->iColor);
	aShader->uniform("iBackgroundColor", vec3(mVDSettings->controlValues[5], mVDSettings->controlValues[6], mVDSettings->controlValues[7]));// mVDSettings->iBackgroundColor);
	aShader->uniform("iSteps", (int)mVDSettings->controlValues[20]);
	aShader->uniform("iRatio", mVDSettings->controlValues[11]);//check if needed: +1;//mVDSettings->iRatio); 
	aShader->uniform("width", 1);
	aShader->uniform("height", 1);
	aShader->uniform("iRenderXY", mVDSettings->mRenderXY);
	aShader->uniform("iZoom", mVDSettings->controlValues[22]);
	aShader->uniform("iAlpha", mVDSettings->controlValues[4]);
	aShader->uniform("iBlendmode", mVDSettings->iBlendMode);
	aShader->uniform("iChromatic", mVDSettings->controlValues[10]);
	aShader->uniform("iRotationSpeed", mVDSettings->controlValues[19]);
	aShader->uniform("iCrossfade", mVDSettings->controlValues[18]);
	aShader->uniform("iPixelate", mVDSettings->controlValues[15]);
	aShader->uniform("iExposure", mVDSettings->controlValues[14]);
	aShader->uniform("iDeltaTime", mVDAnimation->iDeltaTime);
	aShader->uniform("iFade", (int)mVDSettings->iFade);
	aShader->uniform("iToggle", (int)mVDSettings->controlValues[46]);
	aShader->uniform("iLight", (int)mVDSettings->iLight);
	aShader->uniform("iLightAuto", (int)mVDSettings->iLightAuto);
	aShader->uniform("iGreyScale", (int)mVDSettings->iGreyScale);
	aShader->uniform("iTransition", mVDSettings->iTransition);
	aShader->uniform("iAnim", mVDSettings->iAnim.value());
	aShader->uniform("iRepeat", (int)mVDSettings->iRepeat);
	aShader->uniform("iVignette", (int)mVDSettings->controlValues[47]);
	aShader->uniform("iInvert", (int)mVDSettings->controlValues[48]);
	aShader->uniform("iDebug", (int)mVDSettings->iDebug);
	aShader->uniform("iShowFps", (int)mVDSettings->iShowFps);
	aShader->uniform("iFps", mVDSettings->iFps);
	aShader->uniform("iTempoTime", mVDAnimation->iTempoTime);
	aShader->uniform("iGlitch", (int)mVDSettings->controlValues[45]);
	aShader->uniform("iTrixels", mVDSettings->controlValues[16]);
	aShader->uniform("iGridSize", mVDSettings->controlValues[17]);
	aShader->uniform("iBeat", mVDSettings->iBeat);
	aShader->uniform("iSeed", mVDSettings->iSeed);
	aShader->uniform("iRedMultiplier", mVDSettings->iRedMultiplier);
	aShader->uniform("iGreenMultiplier", mVDSettings->iGreenMultiplier);
	aShader->uniform("iBlueMultiplier", mVDSettings->iBlueMultiplier);
	aShader->uniform("iFlipH", mVDFbos[mVDSettings->mMixFboIndex]->isFlipH());
	aShader->uniform("iFlipV", mVDFbos[mVDSettings->mMixFboIndex]->isFlipV());
	aShader->uniform("iParam1", mVDSettings->iParam1);
	aShader->uniform("iParam2", mVDSettings->iParam2);
	aShader->uniform("iXorY", mVDSettings->iXorY);
	aShader->uniform("iBadTv", mVDSettings->iBadTv);

	mRenderFbo->getColorTexture()->bind(0);
	mRenderFbo->getColorTexture()->bind(1);
	gl::drawSolidRect(Rectf(0, 0, mVDSettings->mFboWidth, mVDSettings->mFboHeight));
	// stop drawing into the FBO
	mVDFbos[mVDSettings->mMixFboIndex]->getFboRef()->unbindFramebuffer();
	mRenderFbo->getColorTexture()->unbind();
	mRenderFbo->getColorTexture()->unbind();

	//aShader->unbind();
	//sTextures[5] = mVDFbos[mVDSettings->mMixFboIndex]->getTexture();

	//}
	/***********************************************
	* mix 2 FBOs end
	*/
	if (firstDraw) {
		if (getElapsedFrames() > 10) {

			firstDraw = false;
			setWindowSize(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight);
			setWindowPos(ivec2(mVDSettings->mRenderX, mVDSettings->mRenderY));
			fs::path waveFile = getAssetPath("") / mVDSettings->mAssetsPath / "batchass-sky.wav";
			mVDAudio->loadWaveFile(waveFile.string());
		}
	}
	gl::clear(Color::black());
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
			if (i%2 == 0) {
				warp->draw(mVDFbos[mVDSettings->mMixFboIndex]->getTexture(), mVDUtils->getSrcAreaLeftOrTop());
			}
			else {
				warp->draw(mVDFbos[mVDSettings->mMixFboIndex]->getTexture(), mVDUtils->getSrcAreaRightOrBottom());
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
		mVDSettings->controlValues[10] = event.getX() / mVDSettings->mRenderWidth;
	}
}

void BatchassSkyApp::mouseDown(MouseEvent event)
{
	// pass this mouse event to the warp editor first
	if (!Warp::handleMouseDown(mWarps, event)) {
		// let your application perform its mouseDown handling here
		mVDSettings->controlValues[45] = 1.0f;
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
		mVDSettings->controlValues[45] = 0.0f;
	}
}

void BatchassSkyApp::keyDown(KeyEvent event)
{
	// pass this key event to the warp editor first
	if (!Warp::handleKeyDown(mWarps, event)) {
		// warp editor did not handle the key, so handle it here
		if (!mVDAnimation->handleKeyDown(event)) {
			// Animation did not handle the key, so handle it here
			switch (event.getCode()) {

			case KeyEvent::KEY_LEFT: mInnerLevel--; break;
			case KeyEvent::KEY_RIGHT: mInnerLevel++; break;
			case KeyEvent::KEY_DOWN: mOuterLevel--; break;
			case KeyEvent::KEY_UP: mOuterLevel++; break;
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
				mVDSettings->mSplitWarpV = !mVDSettings->mSplitWarpV;
				mVDUtils->splitWarp(mRenderFbo->getWidth(), mRenderFbo->getHeight());
				break;
			case KeyEvent::KEY_h:
				mVDSettings->mSplitWarpH = !mVDSettings->mSplitWarpH;
				mVDUtils->splitWarp(mRenderFbo->getWidth(), mRenderFbo->getHeight());
				break;
			case KeyEvent::KEY_r:
				// reset split the image
				mVDSettings->mSplitWarpV = false;
				mVDSettings->mSplitWarpH = false;
				mVDUtils->splitWarp(mRenderFbo->getWidth(), mRenderFbo->getHeight());
				break;
			case KeyEvent::KEY_w:
				// toggle warp edit mode
				Warp::enableEditMode(!Warp::isEditModeEnabled());
				break;
			case KeyEvent::KEY_c:
				mUseBeginEnd = !mUseBeginEnd;
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
		if (!mVDAnimation->handleKeyUp(event)) {
			// Animation did not handle the key, so handle it here
		}
	}
}

void BatchassSkyApp::updateWindowTitle()
{
	getWindow()->setTitle(to_string(getElapsedFrames()) + " " + to_string((int)getAverageFps()) + " fps Batchass Sky");
}

CINDER_APP(BatchassSkyApp, RendererGl(RendererGl::Options().msaa(8)), &BatchassSkyApp::prepare)
