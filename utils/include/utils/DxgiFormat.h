//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_DXGIFORMAT_H
#define HACKFRAMEWORK_DXGIFORMAT_H

#include <dxgi.h>

namespace Utils
{

    inline int GetCorrectDXGIFormat(int currentFormat)
    {
        switch (currentFormat)
        {
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
                return DXGI_FORMAT_R8G8B8A8_UNORM;
        }
        return currentFormat;
    }

} // namespace Utils

#endif // HACKFRAMEWORK_DXGIFORMAT_H
