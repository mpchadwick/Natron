/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of Natron <https://natrongithub.github.io/>,
 * (C) 2018-2023 The Natron developers
 * (C) 2013-2018 INRIA and Alexandre Gauthier-Foichat
 *
 * Natron is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Natron is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Natron.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
 * ***** END LICENSE BLOCK ***** */

// ***** BEGIN PYTHON BLOCK *****
// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>
// ***** END PYTHON BLOCK *****

#include "RectD.h"

#include <cmath>
#include <cassert>
#include <stdexcept>

#include "Engine/RectI.h"

NATRON_NAMESPACE_ENTER

RectI
RectD::toPixelEnclosing(const RenderScale& scale,
                        double par) const
{
    return RectI(std::floor(x1 * scale.x / par),
                 std::floor(y1 * scale.y),
                 std::ceil(x2 * scale.x / par),
                 std::ceil(y2 * scale.y));
}

RectI
RectD::toPixelEnclosing(unsigned int mipMapLevel,
                        double par) const
{
    const double scale = 1. / (1 << mipMapLevel);
    return RectI(std::floor(x1 * scale / par),
                 std::floor(y1 * scale),
                 std::ceil(x2 * scale / par),
                 std::ceil(y2 * scale));
}

NATRON_NAMESPACE_EXIT
