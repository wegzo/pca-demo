#pragma once

#include "wtl.h"
#include "pca.h"
#include <vector>
#include <optional>
#include <libpng16/png.h>
#include <EGL/egl.h>
#include <ANGLE/GLES2/gl2.h>

#define WM_CHANGE_TEXTURE_QUALITY WM_USER + 1

class ControlDlg final : public CDialogImpl<ControlDlg>
{
private:
    static const int drawTimerId = 1;

    std::optional<PCA> pca;

    DWORD backgroundColor;

    EGLDisplay eglDisplay;
    EGLContext eglContext;
    EGLSurface eglSurface;

    png_uint_32 pngWidth, pngHeight;
    int pngBitDepth, pngColorType;
    std::vector<unsigned char> pngImageData;

    GLuint programObject;
    GLuint ogTexture = 0, processedTexture = 0;

    void loadThePng();
    void initGlES2();
    void createShaderProgram();
    GLuint loadShader(GLenum type, const char* shaderSrc);
    void loadOriginalTexture();
    void loadProcessedTexture(const std::vector<unsigned char>& processedImageData);

    void drawOriginalTexture(GLsizei width, GLsizei height);
    void drawProcessedTexture(GLsizei width, GLsizei height);
public:
    enum { IDD = IDD_CONTROLDLG };

    BEGIN_MSG_MAP(ControlDlg)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MSG_WM_SIZE(OnSize)
        MSG_WM_PAINT(OnPaint)
        MSG_WM_SHOWWINDOW(OnShowWindow)
    END_MSG_MAP()

    void initialize();
    void draw(GLsizei width, GLsizei height);
    void changeTextureQuality(int granularity, int quality, int blockSize);

    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    void OnSize(UINT nType, CSize size);
    void OnPaint(CDCHandle dc);
    void OnShowWindow(BOOL bShow, UINT nStatus);
};

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

class ControlsDlg final : public CDialogImpl<ControlsDlg>
{
private:
    CTrackBarCtrl sliderCtrl, sliderCtrlQuality, sliderCtrlBlockSize;
    CStatic sliderStatic, sliderQualityStatic, sliderBlockSizeStatic;
    int oldSliderPos = 1, oldSliderQualityPos = 1, oldSliderBlockSizePos = 1;
public:
    enum { IDD = IDD_CONTROLS };

    BEGIN_MSG_MAP(ControlsDlg)
        MSG_WM_SIZE(OnSize)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_HSCROLL, OnTrackBarScroll)
    END_MSG_MAP()

    void OnSize(UINT nType, CSize size);
    LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
    LRESULT OnTrackBarScroll(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
};

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

static const wchar_t windowTitle[] = L"PCA Demo";

class MainWnd final :
    public CWindowImpl<MainWnd, CWindow, CWinTraits<
    WS_OVERLAPPEDWINDOW |
    WS_CLIPCHILDREN, WS_EX_APPWINDOW | WS_EX_WINDOWEDGE>>,
    public CMessageFilter
{
private:
    ControlDlg controlDlg;
    ControlsDlg controlsDlg;

    void setChildDlgLocations();
public:
    DECLARE_WND_CLASS(L"PCA Demo")

    BOOL PreTranslateMessage(MSG * pMsg);

    BEGIN_MSG_MAP(MainWnd)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_DESTROY(OnDestroy)
        MSG_WM_SIZE(OnSize)
        MESSAGE_HANDLER(WM_CHANGE_TEXTURE_QUALITY, OnChangeTextureQuality)
    END_MSG_MAP()

    int OnCreate(LPCREATESTRUCT);
    void OnDestroy();
    void OnSize(UINT nType, CSize size);
    LRESULT OnChangeTextureQuality(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
};