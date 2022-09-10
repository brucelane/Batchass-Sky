/*
 Copyright (c) 2013-2022, Bruce Lane - All rights reserved.
 This code is intended for use with the Cinder C++ library: http://libcinder.org

 Using Cinder-Warping from Paul Houx.

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

// Animation
#include "VDAnimation.h"
// Session Facade
#include "VDSessionFacade.h"
// Spout
#include "CiSpoutOut.h"
// Uniforms
#include "VDUniforms.h"
// Params
#include "VDParams.h"
// Mix
#include "VDMix.h"

// UI
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS 1
#include "VDUI.h"
#define IM_ARRAYSIZE(_ARR)			((int)(sizeof(_ARR)/sizeof(*_ARR)))
using namespace ci;
using namespace ci::app;
using namespace videodromm;

class BatchassSkyApp : public App {
public:
	BatchassSkyApp();
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
	void fileDrop(FileDropEvent event) override;
private:
	// Settings
	VDSettingsRef					mVDSettings;
	// Animation
	VDAnimationRef					mVDAnimation;
	// Session
	VDSessionFacadeRef				mVDSessionFacade;
	// Mix
	VDMixRef						mVDMix;
	// Uniforms
	VDUniformsRef					mVDUniforms;
	// Params
	VDParamsRef						mVDParams;
	// UI
	VDUIRef							mVDUI;

	bool							mFadeInDelay = true;
	void							toggleCursorVisibility(bool visible);
	SpoutOut 						mSpoutOut;
	// SKY
		// shaders
	gl::GlslProgRef				aShader;

	bool						mUseBeginEnd;

	fs::path					mSettings;

	WarpList					mWarps;
	bool						mWaveDelay;
	bool						mFadeOutDelay;

	gl::BatchRef				mBatch;
	float						mInnerLevel, mOuterLevel;
	// fbo
	void						renderSceneToFbo();
	gl::FboRef					mRenderFbo;
};


BatchassSkyApp::BatchassSkyApp() : mSpoutOut("Sky", app::getWindowSize())
{

	// Settings
	mVDSettings = VDSettings::create("Sky");
	// Uniform
	mVDUniforms = VDUniforms::create();
	// Params
	mVDParams = VDParams::create();
	// Animation
	mVDAnimation = VDAnimation::create(mVDSettings, mVDUniforms);
	// Mix
	mVDMix = VDMix::create(mVDSettings, mVDAnimation, mVDUniforms);
	// Session
	mVDSessionFacade = VDSessionFacade::createVDSession(mVDSettings, mVDAnimation, mVDUniforms, mVDMix)
		->setUniformValue(mVDUniforms->IDISPLAYMODE, VDDisplayMode::POST)
		->setupSession()
		->setupWSClient()
		->wsConnect()
		//->setupOSCReceiver()
		//->addOSCObserver(mVDSettings->mOSCDestinationHost, mVDSettings->mOSCDestinationPort)
		->addUIObserver(mVDSettings, mVDUniforms)
		->toggleUI()
		->setUniformValue(mVDUniforms->IBPM, 160.0f)
		->setUniformValue(mVDUniforms->IMOUSEX, 0.27710f)
		->setUniformValue(mVDUniforms->IMOUSEY, 0.5648f);

	mFadeInDelay = true;

	// SKY
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
	// render fbo
	gl::Fbo::Format fboFormat;
	//format.setSamples( 4 ); // uncomment this to enable 4x antialiasing
	mRenderFbo = gl::Fbo::create(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight, fboFormat.colorTexture());

	// UI
	mVDUI = VDUI::create(mVDSettings, mVDSessionFacade, mVDUniforms);
}

void BatchassSkyApp::toggleCursorVisibility(bool visible)
{
	if (visible)
	{
		showCursor();
	}
	else
	{
		hideCursor();
	}
}

void BatchassSkyApp::fileDrop(FileDropEvent event)
{
	mVDSessionFacade->fileDrop(event);
}

void BatchassSkyApp::mouseMove(MouseEvent event)
{
	if (!mVDSessionFacade->handleMouseMove(event)) {

	}
}

void BatchassSkyApp::mouseDown(MouseEvent event)
{

	if (!mVDSessionFacade->handleMouseDown(event)) {

	}
}

void BatchassSkyApp::mouseDrag(MouseEvent event)
{

	if (!mVDSessionFacade->handleMouseDrag(event)) {

	}
}

void BatchassSkyApp::mouseUp(MouseEvent event)
{

	if (!mVDSessionFacade->handleMouseUp(event)) {

	}
}

void BatchassSkyApp::keyDown(KeyEvent event)
{

	// warp editor did not handle the key, so handle it here
	//if (!mVDSessionFacade->handleKeyDown(event)) {
		switch (event.getCode()) {
		case KeyEvent::KEY_F12:
			// quit the application
			quit();
			break;
		case KeyEvent::KEY_f:
			// toggle full screen
			setFullScreen(!isFullScreen());
			break;

		case KeyEvent::KEY_l:
			mVDSessionFacade->createWarp();
			break;
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
		}
		mInnerLevel = math<float>::max(mInnerLevel, 1.0f);
		mOuterLevel = math<float>::max(mOuterLevel, 1.0f);
	//}
}

void BatchassSkyApp::keyUp(KeyEvent event)
{

	// let your application perform its keyUp handling here
	if (!mVDSessionFacade->handleKeyUp(event)) {
		
	}
}
void BatchassSkyApp::cleanup()
{
	CI_LOG_V("cleanup and save");
	ui::Shutdown();
	mVDSessionFacade->saveWarps();
	mVDSettings->save();
	CI_LOG_V("quit");
}

void BatchassSkyApp::update()
{
	mVDSessionFacade->setUniformValue(mVDUniforms->IFPS, getAverageFps());
	mVDSessionFacade->update();
}


void BatchassSkyApp::resize()
{
	mVDUI->resize();
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
	//auto cam = CameraPersp(mVDSettings->mFboWidth + ((int)mVDSettings->maxVolume * 5), mVDSettings->mFboHeight, 60, 1, 1000).calcFraming(Sphere(vec3(0.0f), 1.25f));
	auto cam = CameraPersp(mVDParams->getFboWidth(), mVDParams->getFboHeight(), 60, 1, 1000).calcFraming(Sphere(vec3(0.0f), 1.25f));
	gl::setMatrices(cam);
	//gl::rotate(getElapsedSeconds() * 0.1f, vec3(0.123, 0.456, 0.789));
	//gl::rotate(getElapsedSeconds() * 0.1f + mVDSettings->maxVolume / 100, vec3(0.123, 0.456, 0.789));
	gl::rotate(getElapsedSeconds() * 0.1f, vec3(0.123, 0.456, 0.789));
	gl::viewport(getWindowSize());

	// update uniforms
	//mBatch->getGlslProg()->uniform("uTessLevelInner", mInnerLevel + mVDSettings->maxVolume / 10);
	mBatch->getGlslProg()->uniform("uTessLevelInner", mInnerLevel);
	mBatch->getGlslProg()->uniform("uTessLevelOuter", mOuterLevel);
	mBatch->getGlslProg()->uniform("iFR", mVDSessionFacade->getUniformValue(mVDUniforms->ICOLORX));
	mBatch->getGlslProg()->uniform("iFG", mVDSessionFacade->getUniformValue(mVDUniforms->ICOLORY));
	mBatch->getGlslProg()->uniform("iAlpha", mVDSessionFacade->getUniformValue(mVDUniforms->IALPHA));
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

	// clear the window and set the drawing color to white
	gl::clear();
	gl::color(Color::white());
	/*if (mFadeInDelay) {
		mVDSettings->iAlpha = 0.0f;
		if (getElapsedFrames() > 10.0) {// mVDSessionFacade->getFadeInDelay()) {
			mFadeInDelay = false;
			timeline().apply(&mVDSettings->iAlpha, 0.0f, 1.0f, 1.5f, EaseInCubic());
		}
	}
	else {
		gl::setMatricesWindow(mVDParams->getFboWidth(), mVDParams->getFboHeight());
		
		int m = mVDSessionFacade->getUniformValue(mVDUniforms->IDISPLAYMODE);
		if (m == VDDisplayMode::MIXETTE) {
			gl::draw(mVDSessionFacade->buildRenderedMixetteTexture(0));
			mSpoutOut.sendTexture(mVDSessionFacade->buildRenderedMixetteTexture(0));
		}
		else if (m == VDDisplayMode::POST) {
			gl::draw(mVDSessionFacade->buildPostFboTexture());
			mSpoutOut.sendTexture(mVDSessionFacade->buildPostFboTexture());
		}
		else if (m == VDDisplayMode::FX) {
			gl::draw(mVDSessionFacade->buildFxFboTexture());
			mSpoutOut.sendTexture(mVDSessionFacade->buildFxFboTexture());
		}
		else {
			if (m < mVDSessionFacade->getFboShaderListSize()) {
				gl::draw(mVDSessionFacade->getFboShaderTexture(m));
				mSpoutOut.sendTexture(mVDSessionFacade->getFboShaderTexture(m));
			}
			else {
				gl::draw(mVDSessionFacade->buildRenderedMixetteTexture(0), Area(50, 50, mVDParams->getFboWidth() / 2, mVDParams->getFboHeight() / 2));
				gl::draw(mVDSessionFacade->buildPostFboTexture(), Area(mVDParams->getFboWidth() / 2, mVDParams->getFboHeight() / 2, mVDParams->getFboWidth(), mVDParams->getFboHeight()));
			}
			//gl::draw(mVDSession->getRenderedMixetteTexture(0), Area(0, 0, mVDSettings->mFboWidth, mVDSettings->mFboHeight));
			// ok gl::draw(mVDSession->getWarpFboTexture(), Area(0, 0, mVDSettings->mFboWidth, mVDSettings->mFboHeight));//getWindowBounds()
		}
	}	*/


	/*aShader = mVDShaders->getMixShader();
	aShader->bind();
	aShader->uniform("iGlobalTime", mVDSessionFacade->getUniformValue(mVDUniforms->ITIME));
	//20140703 aShader->uniform("iResolution", vec3(mVDSettings->mRenderResoXY.x, mVDSettings->mRenderResoXY.y, 1.0));
	aShader->uniform("iResolution", vec3(mVDParams->getFboWidth(), mVDParams->getFboHeight(), 1.0));
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
	aShader->uniform("iAlpha", mVDSettings->controlValues[4] * mVDSettings->iAlpha);
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
	aShader->uniform("iBadTv", mVDSettings->iBadTv);*/

	mRenderFbo->getColorTexture()->bind(0);
	mRenderFbo->getColorTexture()->bind(1);
	gl::drawSolidRect(Rectf(0, 0, mVDParams->getFboWidth(), mVDParams->getFboHeight()));
	// stop drawing into the FBO
	//mVDFbos[mVDSettings->mMixFboIndex]->getFboRef()->unbindFramebuffer();
	mRenderFbo->getColorTexture()->unbind();
	mRenderFbo->getColorTexture()->unbind();


	gl::clear(Color::black());
	gl::setMatricesWindow(toPixels(getWindowSize()));
	gl::draw(mRenderFbo->getColorTexture());
	/*int i = 0;
	for (auto &warp : mWarps) {
		if (mUseBeginEnd) {

			int w = mRenderFbo->getColorTexture()->getWidth();
			int h = mRenderFbo->getColorTexture()->getHeight();
			warp->draw(mRenderFbo->getColorTexture(), Area(0, 0, w, h), Rectf(200, 100, 200 + w / 2, 100 + h / 2));
		}
		else {
			if (i % 2 == 0) {
				warp->draw(mVDFbos[mVDSettings->mMixFboIndex]->getTexture(), mVDUtils->getSrcAreaLeftOrTop());
			}
			else {
				warp->draw(mVDFbos[mVDSettings->mMixFboIndex]->getTexture(), mVDUtils->getSrcAreaRightOrBottom());
			}
		}
		//warp->draw(mRenderFbo->getColorTexture(), mRenderFbo->getBounds());
		i++;
	}*/



	// imgui
	if (mVDSessionFacade->showUI()) {
		mVDUI->Run("UI", (int)getAverageFps());
		if (mVDUI->isReady()) {
		}
	}
	getWindow()->setTitle(toString((int)getAverageFps()) + " fps");
}
void prepareSettings(App::Settings *settings)
{
	settings->setWindowSize(1280, 720);
}
CINDER_APP(BatchassSkyApp, RendererGl(RendererGl::Options().msaa(8)),  prepareSettings)
