#include "window.h"
#include <iostream>
#include <format>
#include <stdio.h>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#undef max
#undef min

extern CAppModule module_;

using ShaderVariableLocation = std::pair<GLuint, const char*>;

constexpr ShaderVariableLocation attrVVertexPosition = {0, "vertexPosition"};
constexpr ShaderVariableLocation attrVTexCoordIn = {1, "texCoordIn"};
ShaderVariableLocation uniformVVecTransformationMatrix = {0, "vecTransformationMatrix"};

const char* vertexShaderSrc =
"uniform mat4 vecTransformationMatrix; \n"
"attribute vec4 vertexPosition; \n"
"attribute vec4 texCoordIn; \n"
"varying vec4 texCoord; \n"
"\n"
"void main() \n"
"{ \n"
"   gl_Position = vecTransformationMatrix * vertexPosition; \n"
"   texCoord = texCoordIn; \n"
"}";

ShaderVariableLocation uniformFImage = {0, "image"};

const char* fragmentShaderSrc =
"precision mediump float;"
"varying vec4 texCoord;"
"uniform sampler2D image;"
""
"void main() {"
"   gl_FragColor = texture2D(image, texCoord.xy);"
"}";

LRESULT ControlDlg::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    this->backgroundColor = ::GetSysColor(COLOR_3DFACE);
    return TRUE;
}

void ControlDlg::OnPaint(CDCHandle /*dc*/)
{
    RECT dlgRect;
    this->GetClientRect(&dlgRect);
    this->draw(dlgRect.right, dlgRect.bottom);

    std::cout << "paint" << std::endl;

    this->SetMsgHandled(FALSE);
}

void ControlDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
    if(bShow)
    {
        RECT dlgRect;
        this->GetClientRect(&dlgRect);
        this->draw(dlgRect.right, dlgRect.bottom);
    }
}

void ControlDlg::OnSize(UINT nType, CSize size)
{
    if(nType == SIZE_MAXIMIZED || nType == SIZE_RESTORED)
        this->draw(size.cx, size.cy);
}

void ControlDlg::changeTextureQuality(int granularity, int quality, int blockSize)
{
    if(blockSize > 0)
        this->pca->setBlockSize((blockSize * this->pngWidth * this->pngHeight) / 10000);
    this->pca->setGranularity(granularity);
    this->pca->process(quality);
    this->loadProcessedTexture(this->pca->getProcessedImageData());

    this->PostMessageW(WM_PAINT);
}

void ControlDlg::initialize()
{
    this->loadThePng();
    this->initGlES2();
    this->createShaderProgram();
    this->loadOriginalTexture();
    this->loadProcessedTexture(this->pngImageData);

    this->pca = std::make_optional<PCA>(this->pngWidth, this->pngHeight, this->pngImageData);

    // uncompressed
    this->changeTextureQuality(4, 4, 0);
    /*this->changeTextureQuality(32, 1, 1);*/
}

void ControlDlg::loadThePng()
{
    std::FILE* demoFile = fopen("demoImage.png", "rb");
    if(!demoFile)
        throw std::runtime_error("demoImage.png not found");

    unsigned char header[8] = {0};
    fread(header, 1, sizeof(header), demoFile);

    if(png_sig_cmp(header, 0, sizeof(header)) != 0)
        throw std::runtime_error("file is not png");

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if(!png)
        throw std::runtime_error("error occured in loadThePng()");

    png_infop pngInfo = png_create_info_struct(png);
    if(!pngInfo)
        throw std::runtime_error("error occured in loadThePng()");

    png_init_io(png, demoFile);
    png_set_sig_bytes(png, sizeof(header));
    // read png info
    png_read_info(png, pngInfo);

    png_get_IHDR(png, pngInfo, &this->pngWidth, &this->pngHeight,
        &this->pngBitDepth, &this->pngColorType, nullptr, nullptr, nullptr);

    if(this->pngColorType != PNG_COLOR_TYPE_RGB_ALPHA)
        throw std::runtime_error("loadThePng(): wrong color type");

    const bool hasBackground = !!png_get_valid(png, pngInfo, PNG_INFO_bKGD); hasBackground;

    const double defaultDisplayExponent = 1.0 * 2.2;
    double gamma = 0.0;
    const bool hasGamma = png_get_gAMA(png, pngInfo, &gamma);

    if(!hasGamma)
        throw std::runtime_error("loadThePng(): no gamma");

    png_set_gamma(png, defaultDisplayExponent, gamma);

    png_read_update_info(png, pngInfo);

    const png_uint_32 rowBytesCount = png_get_rowbytes(png, pngInfo);
    const png_byte colorChannelsCount = png_get_channels(png, pngInfo);

    if(colorChannelsCount != 4)
        throw std::runtime_error("loadThePng()");

    png_bytep* rowPointers = new png_bytep[this->pngHeight];
    this->pngImageData = std::vector<unsigned char>(rowBytesCount * this->pngHeight);

    for(png_uint_32 i = 0; i < this->pngHeight; i++)
        rowPointers[i] = this->pngImageData.data() + i * rowBytesCount;

    png_read_image(png, rowPointers);
    png_read_end(png, nullptr); // closes the input file handle

    delete[] rowPointers;
    png_destroy_read_struct(&png, &pngInfo, nullptr);
}

void ControlDlg::initGlES2()
{
    EGLBoolean ret = EGL_FALSE;
    EGLint eglRet;

    const EGLint eglConfigAttrs[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    const EGLint eglContextAttrs[] =
    {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    EGLDisplay eglDisplay = eglGetDisplay(this->GetDC());
    if((eglRet = eglGetError()) != EGL_SUCCESS || eglDisplay == EGL_NO_DISPLAY)
    {
        std::cout << "egl display: " << std::hex << "0x" << eglDisplay << std::dec << std::endl;
        std::cout << "egl error: " << std::hex << "0x" << eglRet << std::dec << std::endl;
        throw std::runtime_error("error occured in eglGetDisplay");
    }

    ret = eglInitialize(eglDisplay, nullptr, nullptr);
    if((eglRet = eglGetError()) != EGL_SUCCESS || ret != EGL_TRUE)
    {
        std::cout << "egl error: " << std::hex << "0x" << eglRet << std::dec << std::endl;
        throw std::runtime_error("error occured in eglInitialize");
    }

    ret = eglBindAPI(EGL_OPENGL_ES_API);
    if(!ret)
    {
        std::cout << "egl error: " << std::hex << "0x" << eglGetError() << std::dec << std::endl;
        throw std::runtime_error("error occured in eglBindAPI");
    }

    EGLConfig config;
    EGLint numConfigsReturned;
    ret = eglChooseConfig(eglDisplay, eglConfigAttrs, &config, 1, &numConfigsReturned);
    if(!ret)
    {
        std::cout << "egl error: " << std::hex << "0x" << eglGetError() << std::dec << std::endl;
        throw std::runtime_error("error occured in eglChooseConfig");
    }

    EGLSurface eglSurface = eglCreateWindowSurface(eglDisplay, config, *this, nullptr);
    if(!eglSurface)
    {
        std::cout << "egl error: " << std::hex << "0x" << eglGetError() << std::dec << std::endl;
        throw std::runtime_error("error occured in eglCreateWindowSurface");
    }

    EGLContext eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, eglContextAttrs);
    if(!eglContext)
    {
        std::cout << "egl error: " << std::hex << "0x" << eglGetError() << std::dec << std::endl;
        throw std::runtime_error("error occured in eglCreateContext");
    }

    ret = eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
    if(!ret)
    {
        std::cout << "egl error: " << std::hex << "0x" << eglGetError() << std::dec << std::endl;
        throw std::runtime_error("error occured in eglMakeCurrent");
    }

    /*glDisable(GL_BLEND);*/

    this->eglDisplay = eglDisplay;
    this->eglContext = eglContext;
    this->eglSurface = eglSurface;
}

GLuint ControlDlg::loadShader(GLenum type, const char* shaderSrc)
{
    GLint compiled;
    const GLuint shader = glCreateShader(type);

    if(shader == 0)
    {
        std::cout << "gl error: " << std::hex << "0x" << glGetError() << std::dec << std::endl;
        throw std::runtime_error("error occured in ControlDlg::loadShader()");
    }

    glShaderSource(shader, 1, &shaderSrc, nullptr);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if(!compiled)
    {
        GLint infoLen = 0;

        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

        if(infoLen > 1)
        {
            char* infoLog = new char[infoLen];

            glGetShaderInfoLog(shader, infoLen, nullptr, infoLog);
            std::cout << "error compiling shader: " << infoLog << std::endl;

            delete[] infoLog;
        }

        throw std::runtime_error("error occured in ControlDlg::loadShader()");
    }

    return shader;
}

void ControlDlg::loadOriginalTexture()
{
    GLenum glRet = GL_NO_ERROR;

    glGenTextures(1, &this->ogTexture);
    glBindTexture(GL_TEXTURE_2D, this->ogTexture);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA,
        static_cast<GLsizei>(this->pngWidth), static_cast<GLsizei>(this->pngHeight),
        0, GL_RGBA, GL_UNSIGNED_BYTE, this->pngImageData.data());

    if((glRet = glGetError()) != GL_NO_ERROR)
    {
        std::cout << "gl error: " << std::hex << "0x" << glRet << std::dec << std::endl;
        throw std::runtime_error("error occured in ControlDlg::loadOriginalTexture()");
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void ControlDlg::loadProcessedTexture(const std::vector<unsigned char>& processedImageData)
{
    GLenum glRet = GL_NO_ERROR;

    // glDeleteTextures silently ignores 0's and names that do not correspond to existing textures.
    glDeleteTextures(1, &this->processedTexture);

    glGenTextures(1, &this->processedTexture);
    glBindTexture(GL_TEXTURE_2D, this->processedTexture);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA,
        static_cast<GLsizei>(this->pngWidth), static_cast<GLsizei>(this->pngHeight),
        0, GL_RGBA, GL_UNSIGNED_BYTE, processedImageData.data());
    if((glRet = glGetError()) != GL_NO_ERROR)
    {
        std::cout << "gl error: " << std::hex << "0x" << glRet << std::dec << std::endl;
        throw std::runtime_error("error occured in ControlDlg::loadProcessedTexture()");
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void ControlDlg::createShaderProgram()
{
    GLenum glRet = GL_NO_ERROR;
    const GLuint vShader = this->loadShader(GL_VERTEX_SHADER, vertexShaderSrc);
    const GLuint fShader = this->loadShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
    const GLuint programObject = glCreateProgram();

    if(programObject == 0)
        throw std::runtime_error("error occured in ControlDlg::createShaderProgram()");

    glAttachShader(programObject, vShader);
    glAttachShader(programObject, fShader);

    // must be called before linking the program
    glBindAttribLocation(programObject, attrVVertexPosition.first, attrVVertexPosition.second);
    glBindAttribLocation(programObject, attrVTexCoordIn.first, attrVTexCoordIn.second);

    GLint linked;
    glLinkProgram(programObject);
    glGetProgramiv(programObject, GL_LINK_STATUS, &linked);

    if(!linked)
    {
        GLint infoLen = 0;

        glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);

        if(infoLen > 1)
        {
            char* infoLog = new char[infoLen];

            glGetProgramInfoLog(programObject, infoLen, nullptr, infoLog);
            std::cout << "error linking program: " << infoLog << std::endl;

            delete[] infoLog;
        }

        throw std::runtime_error("error occured in ControlDlg::createShaderProgram()");
    }

    uniformVVecTransformationMatrix.first = glGetUniformLocation(programObject, uniformVVecTransformationMatrix.second);
    uniformFImage.first = glGetUniformLocation(programObject, uniformFImage.second);

    if((glRet = glGetError()) != GL_NO_ERROR)
    {
        std::cout << "gl error: " << std::hex << "0x" << glRet << std::dec << std::endl;
        throw std::runtime_error("error occured in ControlDlg::createShaderProgram()");
    }

    this->programObject = programObject;
}

void ControlDlg::drawOriginalTexture(GLsizei width, GLsizei height)
{
    GLenum glRet = GL_NO_ERROR;

    const GLfloat vertices[] = {
        -1.0f, 1.0f,   // top left
        1.0f, 1.0f,    // top right
        1.0f, -1.0f,    // bottom right
        -1.0f, -1.0f    // bottom left
    };
    const GLfloat texCoords[] = {
        0.0f, 0.0f, // Top-left
        1.0f, 0.0f, // Top-right
        1.0f, 1.0f, // Bottom-right
        0.0f, 1.0f  // Bottom-left
    };

    glm::mat4 vecTransformationMatrix = glm::translate(glm::mat4(1.f), glm::vec3(-1.f, 1.f, 0.f));
    vecTransformationMatrix = glm::scale(vecTransformationMatrix, glm::vec3(
        (float)this->pngWidth / width,
        (float)this->pngHeight / height, 1.f));
    vecTransformationMatrix = glm::translate(vecTransformationMatrix, glm::vec3(1.f, -1.f, 0.f));

    glViewport(0, 0, width, height);
    glUseProgram(this->programObject);

    // load the vertex coords
    glVertexAttribPointer(attrVVertexPosition.first, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(attrVVertexPosition.first);

    // load the texture coords
    glVertexAttribPointer(attrVTexCoordIn.first, 2, GL_FLOAT, GL_FALSE, 0, texCoords);
    glEnableVertexAttribArray(attrVTexCoordIn.first);

    // set the sampled texture parameters
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->ogTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // set the shader parameters
    glUniformMatrix4fv(uniformVVecTransformationMatrix.first, 1, GL_FALSE, glm::value_ptr(vecTransformationMatrix));
    glUniform1i(uniformFImage.first, 0);

    // draw the quad
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    if((glRet = glGetError()) != GL_NO_ERROR)
    {
        std::cout << "gl error: " << std::hex << "0x" << glRet << std::dec << std::endl;
        throw std::runtime_error("error occured in ControlDlg::draw()");
    }
}

void ControlDlg::drawProcessedTexture(GLsizei width, GLsizei height)
{
    GLenum glRet = GL_NO_ERROR;

    const GLfloat vertices[] = {
        -1.0f, 1.0f,   // top left
        1.0f, 1.0f,    // top right
        1.0f, -1.0f,    // bottom right
        -1.0f, -1.0f    // bottom left
    };
    const GLfloat texCoords[] = {
        0.0f, 0.0f, // Top-left
        1.0f, 0.0f, // Top-right
        1.0f, 1.0f, // Bottom-right
        0.0f, 1.0f  // Bottom-left
    };

    glm::mat4 vecTransformationMatrix = glm::translate(glm::mat4(1.f), glm::vec3(1.f, 1.f, 0.f));
    vecTransformationMatrix = glm::scale(vecTransformationMatrix, glm::vec3(
        (float)this->pngWidth / width,
        (float)this->pngHeight / height, 1.f));
    vecTransformationMatrix = glm::translate(vecTransformationMatrix, glm::vec3(-1.f, -1.f, 0.f));

    glViewport(0, 0, width, height);
    glUseProgram(this->programObject);

    // load the vertex coords
    glVertexAttribPointer(attrVVertexPosition.first, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(attrVVertexPosition.first);

    // load the texture coords
    glVertexAttribPointer(attrVTexCoordIn.first, 2, GL_FLOAT, GL_FALSE, 0, texCoords);
    glEnableVertexAttribArray(attrVTexCoordIn.first);

    // set the sampled texture parameters
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->processedTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // set the shader parameters
    glUniformMatrix4fv(uniformVVecTransformationMatrix.first, 1, GL_FALSE, glm::value_ptr(vecTransformationMatrix));
    glUniform1i(uniformFImage.first, 0);

    // draw the quad
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    if((glRet = glGetError()) != GL_NO_ERROR)
    {
        std::cout << "gl error: " << std::hex << "0x" << glRet << std::dec << std::endl;
        throw std::runtime_error("error occured in ControlDlg::draw()");
    }
}

void ControlDlg::draw(GLsizei width, GLsizei height)
{
    EGLint eglRet = EGL_SUCCESS;

    glViewport(0, 0, width, height);
    glClearColor(
        GetRValue(this->backgroundColor) / 255.f,
        GetGValue(this->backgroundColor) / 255.f,
        GetBValue(this->backgroundColor) / 255.f, 
        1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    this->drawOriginalTexture(width, height);
    this->drawProcessedTexture(width, height);
    

    eglRet = eglSwapBuffers(this->eglDisplay, this->eglSurface);
    if(!eglRet)
    {
        std::cout << "egl error: " << std::hex << "0x" << eglGetError() << std::dec << std::endl;
        throw std::runtime_error("error occured in eglChooseConfig");
    }
}




/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

LRESULT ControlsDlg::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    this->sliderCtrl.Attach(this->GetDlgItem(IDC_QUALITYSLIDER));
    this->sliderCtrl.SetRange(1, 6);
    this->sliderCtrl.SetPos(3);

    this->sliderCtrlQuality.Attach(this->GetDlgItem(IDC_QUALITYSLIDER2));
    this->sliderCtrlQuality.SetRange(1, 4);
    this->sliderCtrlQuality.SetPos(4);

    this->sliderCtrlBlockSize.Attach(this->GetDlgItem(IDC_QUALITYSLIDER3));
    this->sliderCtrlBlockSize.SetRange(1, 10000);
    this->sliderCtrlBlockSize.SetPos(10000);

    this->oldSliderPos = this->sliderCtrl.GetPos();
    this->oldSliderQualityPos = this->sliderCtrlQuality.GetPos();
    this->oldSliderBlockSizePos = this->sliderCtrlBlockSize.GetPos();

    this->sliderStatic.Attach(this->GetDlgItem(IDC_STATIC1));
    this->sliderQualityStatic.Attach(this->GetDlgItem(IDC_STATIC2));
    this->sliderBlockSizeStatic.Attach(this->GetDlgItem(IDC_STATIC3));

    this->sliderStatic.SetWindowTextW(
        std::format(L"Compression complexity: {}", this->sliderCtrl.GetPos()).c_str());
    this->sliderQualityStatic.SetWindowTextW(
        std::format(L"Compression level: {}", this->sliderCtrlQuality.GetPos()).c_str());
    this->sliderBlockSizeStatic.SetWindowTextW(
        std::format(L"Block size: {}%", static_cast<int>((this->sliderCtrlBlockSize.GetPos() / 10000.f) * 100)).c_str());

    return TRUE;
}


void ControlsDlg::OnSize(UINT nType, CSize /*size*/)
{
    if(nType != SIZE_MINIMIZED)
    {
        RECT ctrlRect, ctrlRect2, ctrlRect3, dlgRect;
        this->sliderCtrl.GetWindowRect(&ctrlRect);
        this->sliderCtrlQuality.GetWindowRect(&ctrlRect2);
        this->sliderCtrlBlockSize.GetWindowRect(&ctrlRect3);
        this->GetClientRect(&dlgRect);

        const LONG dlgHeight = dlgRect.bottom - dlgRect.top;
        const LONG ctrlHeight = ctrlRect.bottom - ctrlRect.top;
        const LONG ctrlHeight2 = ctrlRect2.bottom - ctrlRect2.top;
        const LONG ctrlHeight3 = ctrlRect3.bottom - ctrlRect3.top;

        this->sliderCtrl.SetWindowPos(nullptr, 0, 0, dlgRect.right, ctrlHeight, SWP_NOMOVE);
        this->sliderCtrlQuality.SetWindowPos(nullptr, 0, 0, dlgRect.right, ctrlHeight2, SWP_NOMOVE);
        this->sliderCtrlBlockSize.SetWindowPos(nullptr, 0, 0, dlgRect.right, ctrlHeight3, SWP_NOMOVE);
    }
}

LRESULT ControlsDlg::OnTrackBarScroll(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    if(LOWORD(wParam) != SB_ENDSCROLL && LOWORD(wParam) != SB_THUMBPOSITION)
        return 0;

    if(this->oldSliderPos == this->sliderCtrl.GetPos() &&
        this->oldSliderQualityPos == this->sliderCtrlQuality.GetPos() &&
        this->oldSliderBlockSizePos == this->sliderCtrlBlockSize.GetPos())
    {
        return 0;
    }

    const HWND ctrlHwnd = (HWND)lParam;
    const int granularity = std::pow(2, this->sliderCtrl.GetPos() - 1);
    const int oldGranularity = std::pow(2, this->oldSliderPos - 1);

    if(ctrlHwnd == this->sliderCtrl)
    {
        const int granularityQualityRatio = oldGranularity / this->sliderCtrlQuality.GetPos();
        this->sliderCtrlQuality.SetRange(1, granularity);
        this->sliderCtrlQuality.SetPos(granularity / granularityQualityRatio);
    }

    if(ctrlHwnd == this->sliderCtrl || 
        ctrlHwnd == this->sliderCtrlQuality || 
        ctrlHwnd == this->sliderCtrlBlockSize)
    {
        this->sliderStatic.SetWindowTextW(
            std::format(L"Compression complexity: {}", this->sliderCtrl.GetPos()).c_str());
        this->sliderQualityStatic.SetWindowTextW(
            std::format(L"Compression level: {}", this->sliderCtrlQuality.GetPos()).c_str());
        this->sliderBlockSizeStatic.SetWindowTextW(
            std::format(L"Block size: {}%", static_cast<int>((this->sliderCtrlBlockSize.GetPos() / 10000.f) * 100)).c_str());

        ::PostMessage(this->GetParent(), WM_CHANGE_TEXTURE_QUALITY, 
            MAKEWPARAM(granularity, this->sliderCtrlQuality.GetPos()), 
            this->sliderCtrlBlockSize.GetPos());
    }

    this->oldSliderPos = this->sliderCtrl.GetPos();
    this->oldSliderQualityPos = this->sliderCtrlQuality.GetPos();
    this->oldSliderBlockSizePos = this->sliderCtrlBlockSize.GetPos();

    return 0;
}


/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////


BOOL MainWnd::PreTranslateMessage(MSG* pMsg)
{
    return this->IsDialogMessageW(pMsg);
}

int MainWnd::OnCreate(LPCREATESTRUCT /*createstruct*/)
{
    CMessageLoop* loop = module_.GetMessageLoop();
    loop->AddMessageFilter(this);

    /*SetClassLongPtr(*this, GCLP_HBRBACKGROUND, (LONG_PTR)GetSysColorBrush(COLOR_3DFACE));*/

    this->controlDlg.Create(*this);
    this->controlsDlg.Create(*this);

    this->controlDlg.initialize();

    // adjust the locations
    this->setChildDlgLocations();

    this->controlDlg.ShowWindow(SW_SHOW);
    this->controlsDlg.ShowWindow(SW_SHOW);

    return 0;
}

void MainWnd::OnDestroy()
{
    CMessageLoop* loop = module_.GetMessageLoop();
    loop->RemoveMessageFilter(this);

    PostQuitMessage(0);
}

void MainWnd::OnSize(UINT nType, CSize size)
{
    if(this->controlDlg && this->controlsDlg && nType != SIZE_MINIMIZED)
        this->setChildDlgLocations();
}

void MainWnd::setChildDlgLocations()
{
    RECT clientRect;
    this->GetClientRect(&clientRect);

    RECT controlsDlgRect;
    this->controlsDlg.GetClientRect(&controlsDlgRect);

    RECT drawDlgLocation;
    drawDlgLocation.left = 0;
    drawDlgLocation.top = 0;
    drawDlgLocation.right = clientRect.right;
    drawDlgLocation.bottom = clientRect.bottom - controlsDlgRect.bottom;
    this->controlDlg.MoveWindow(&drawDlgLocation);

    RECT controlsDlgLocation;
    controlsDlgLocation.left = 0;
    controlsDlgLocation.top = drawDlgLocation.bottom;
    controlsDlgLocation.right = clientRect.right;
    controlsDlgLocation.bottom = clientRect.bottom;
    this->controlsDlg.MoveWindow(&controlsDlgLocation);
}

LRESULT MainWnd::OnChangeTextureQuality(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
    this->controlDlg.changeTextureQuality(LOWORD(wParam), HIWORD(wParam), lParam);
    return 0;
}