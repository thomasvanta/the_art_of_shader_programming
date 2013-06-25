//--redFrik

//keys:
// i - info on/off
// esc - fullscreen on/off
// f - load fragment shader
// m - switch drawing mode

//note: there need to be a folder called 'data' in the same folder as this application
//it should include the file _default_frag.glsl

#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"
#include "cinder/Utilities.h"
#include "cinder/Filesystem.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/audio/FftProcessor.h"  //osx only
#include "cinder/audio/Input.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class shader01audioInputApp : public AppNative {
public:
	void setup();
	void update();
	void draw();
    void keyDown(KeyEvent event);
    void mouseDown(MouseEvent event);
    void mouseDrag(MouseEvent event);
    void loadShader();
    void drawWaveform(bool fill);
    void drawSpectrum(bool fill);
    
    gl::Texture             mTextureSnd;
    gl::Texture             mTextureFft;
    gl::GlslProgRef         mShader;
    std::time_t             mTimeFrag;
    fs::path                mPathFrag;
    bool                    mHide;          //show/hide error and fps
    std::string             mError;
    int                     mMode;          //which shape
    
    audio::Input            mInput;
	std::shared_ptr<float>  mFftLeft;
	audio::PcmBuffer32fRef  mPcmBuffer;
    audio::Buffer32fRef     mBufferLeft;
    uint32_t                mBufferSize;
    float                   mAmplitude;     //amptracker
    Vec2f                   mMouse;
};

void shader01audioInputApp::setup() {
    
    //--defaults
    mHide= false;       //also keydown 'i'
    mError= "";
    mMode= 0;
    mAmplitude= 0.0f;
    mMouse= Vec2f(0.0f, 0.0f);
    
    //--audio
    mInput= audio::Input();     //use default input device
    mInput.start();             //start capturing
    
    //--shader
    mPathFrag= getPathDirectory(app::getAppPath().string())+"data/_default_frag.glsl";
    loadShader();
}
void shader01audioInputApp::keyDown(ci::app::KeyEvent event) {
	if(event.getChar()=='i') {
        mHide= !mHide;
    } else if(event.getCode()==KeyEvent::KEY_ESCAPE) {
        setFullScreen(!isFullScreen());
    } else if(event.getChar()=='f') {
        fs::path path= getOpenFilePath(mPathFrag);
        if(!path.empty()) {
			mPathFrag= path;
            loadShader();
		}
    } else if(event.getChar()=='m') {
        mMode= (mMode+1)%9;
    }
}
void shader01audioInputApp::mouseDown(MouseEvent event) {
    mMouse= event.getPos();
}
void shader01audioInputApp::mouseDrag(MouseEvent event) {
    mMouse= event.getPos();
}

void shader01audioInputApp::loadShader() {
    mError= "";
    try {
        mTimeFrag= fs::last_write_time(mPathFrag);
        mShader= gl::GlslProg::create(NULL, loadFile(mPathFrag), NULL);   //only fragment
    }
    catch(gl::GlslProgCompileExc &exc) {
        mError= exc.what();
    }
    catch(...) {
        mError= "Unable to load shader";
    }
}
void shader01audioInputApp::update() {
    
    //--audio input
    mPcmBuffer= mInput.getPcmBuffer();
    if(mPcmBuffer) {
        mBufferSize= mPcmBuffer->getSampleCount();
        //std::cout<<"mBufferSize: "<<mBufferSize<<std::endl;
        mBufferLeft= mPcmBuffer->getChannelData(audio::CHANNEL_FRONT_LEFT);
        mFftLeft= audio::calculateFft(mPcmBuffer->getChannelData(audio::CHANNEL_FRONT_LEFT), mBufferSize/2);
        mAmplitude= 0.0f;
        for(uint32_t i= 0; i<mBufferSize; i++) {
            mAmplitude += abs(mBufferLeft->mData[i]);
        }
        mAmplitude /= float(mBufferSize);   //average amplitude
        
        Surface32f mSurfaceSnd(mBufferSize, 1, true);
        Surface32f::Iter sndIter(mSurfaceSnd.getIter());
        uint32_t i= 0;
        while(sndIter.line()) {
            while(sndIter.pixel()) {
                sndIter.r()= mBufferLeft->mData[i];
                i++;
            }
        }
        mTextureSnd= gl::Texture(mSurfaceSnd);
        
        Surface32f mSurfaceFft(mBufferSize/2, 1, true);
        Surface32f::Iter fftIter(mSurfaceFft.getIter());
        uint32_t j= 0;
        float *fftBuffer= mFftLeft.get();
        while(fftIter.line()) {
            while(fftIter.pixel()) {
                fftIter.r()= fftBuffer[j];
                j++;
            }
        }
        mTextureFft= gl::Texture(mSurfaceFft);
    }
    
    //--shaders
    if(fs::last_write_time(mPathFrag)>mTimeFrag) {
        loadShader();   //hot-loading shader
    }
}
void shader01audioInputApp::drawWaveform(bool fill) {
    if(!mPcmBuffer) {
        return;
    }
    glPushMatrix();
    gl::translate(getWindowCenter());
    float w= getWindowWidth();
    float a= getWindowHeight()*0.25f;    //wave amplitude
    PolyLine<Vec2f>	line;
    for(uint32_t i= 0; i<mBufferSize; i++) {
        float x= (i/(float)(mBufferSize-1))*w-(w*0.5f);
        float y= mBufferLeft->mData[i]*a;
        line.push_back(Vec2f(x, y));
        if(fill) {
            line.push_back(Vec2f(x, 0.0f-y));
        }
    }
    gl::draw(line);
    glPopMatrix();
}
void shader01audioInputApp::drawSpectrum(bool fill) {
    if(!mFftLeft) {
        return;
    }
    float *fftBuffer= mFftLeft.get();
    glPushMatrix();
    gl::translate(getWindowCenter());
    float w= getWindowWidth();
    float a= getWindowHeight()*0.01f;    //spectrum scale
    uint32_t fftSize= mBufferSize/2;
    PolyLine<Vec2f>	line;
    for(uint32_t i= 0; i<fftSize; i++) {
        float x= (i/(float)(fftSize-1))*w-(w*0.5f);
        float y= fftBuffer[i]*a;
        line.push_back(Vec2f(x, 0.0f-y));
        if(fill) {
            line.push_back(Vec2f(x, y));
        }
    }
    gl::draw(line);
    glPopMatrix();
}

void shader01audioInputApp::draw() {
    
	gl::clear(Color(0, 0, 0));
    if(mPcmBuffer) {
        mTextureSnd.bind(0);
        mTextureFft.bind(1);
    }
    mShader->bind();
    mShader->uniform("iResolution", (Vec2f)getWindowSize());
    mShader->uniform("iGlobalTime", (float)getElapsedSeconds());
    mShader->uniform("iAmplitude", mAmplitude);
    if(mPcmBuffer) {
        mShader->uniform("iChannel0", 0);   //sound
        mShader->uniform("iChannel1", 1);   //fft
    }
    mShader->uniform("iMouse", mMouse);
    
    gl::color(1.0f, 1.0f, 1.0f);
    switch(mMode) {
        case 0:
            gl::drawSolidRect(Rectf(getWindowBounds()));
            break;
        case 1:
            gl::drawSolidRect(Rectf(getWindowBounds()).scaledCentered(0.8f));
            break;
        case 2:
            gl::drawSolidTriangle(
                                  getWindowCenter()*Vec2f(1.0f, 0.25f),
                                  getWindowCenter()*Vec2f(0.25f, 1.75f),
                                  getWindowCenter()*Vec2f(1.75f, 1.75f));
            break;
        case 3:
            gl::drawSolidCircle(getWindowCenter(), getWindowHeight()*0.4f, 100);
            break;
        case 4:
            gl::drawSphere((Vec3f)getWindowCenter(), getWindowHeight()*0.4f, 12);   //sphere detail
            break;
        case 5:
            drawWaveform(false);
            break;
        case 6:
            drawWaveform(true);
            break;
        case 7:
            drawSpectrum(false);
            break;
        case 8:
            drawSpectrum(true);
            break;
    }
    mShader->unbind();
    if(mPcmBuffer) {
        mTextureSnd.unbind();
        mTextureFft.unbind();
    }
    
    if(!mHide) {
        Color col= Color(1, 1, 1);
        Font fnt= Font("Verdana", 12);
        gl::drawString("mode (m): "+toString(mMode), Vec2f(30.0f, getWindowHeight()-80.0f), col, fnt);
        gl::drawString("frag (f): "+toString(mPathFrag.filename()), Vec2f(30.0f, getWindowHeight()-60.0f), col, fnt);
        gl::drawString("error: "+mError, Vec2f(30.0f, getWindowHeight()-40.0f), col, fnt);
        gl::drawString("fps: "+toString(getAverageFps()), Vec2f(30.0f, getWindowHeight()-20.0f), col, fnt);
    }
}

CINDER_APP_NATIVE(shader01audioInputApp, RendererGl)
