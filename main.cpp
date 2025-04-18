#include "window.h"
#include <iostream>
#include <libpng16/png.h>
#include <EGL/egl.h>
#include <ANGLE/GLES2/gl2.h>

inline void CHECK_HR(const HRESULT hr)
{
    if(FAILED(hr))
    {
        std::cout << std::hex << "0x" << hr << std::endl;
        system("pause");
        abort();
    }
}

CAppModule module_;

int main()
{
    HRESULT hr = S_OK;

    // initialize COM
    CHECK_HR(hr = CoInitialize(nullptr));
    AtlInitCommonControls(ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES);

    // create window
    CHECK_HR(hr = module_.Init(nullptr, nullptr));
    {
        CMessageLoop msgloop;
        module_.AddMessageLoop(&msgloop);

        RECT windowSize = {
            CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT + 438, CW_USEDEFAULT + 644
        };

        MainWnd mainWnd;
        if(mainWnd.Create(nullptr, windowSize, windowTitle) == nullptr)
            CHECK_HR(hr = E_UNEXPECTED);
        mainWnd.ShowWindow(SW_SHOW);
        const int ret = msgloop.Run(); ret;
        module_.RemoveMessageLoop();
    }

    CoUninitialize();

    return 0;
}
