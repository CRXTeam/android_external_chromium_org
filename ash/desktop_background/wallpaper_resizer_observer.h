// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_WALLPAPER_RESIZER_OBSERVER_H_
#define ASH_DESKTOP_BACKGROUND_WALLPAPER_RESIZER_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT WallpaperResizerObserver {
 public:
  // Invoked when the wallpaper is resized.
  virtual void OnWallpaperResized() = 0;

 protected:
  virtual ~WallpaperResizerObserver() {}
};

}  // namespace ash

#endif  // ASH_DESKTOP_BACKGROUND_WALLPAPER_RESIZER_OBSERVER_H_
