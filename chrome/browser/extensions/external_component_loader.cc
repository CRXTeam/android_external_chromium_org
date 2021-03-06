// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_component_loader.h"

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/enhanced_bookmarks_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"

namespace extensions {

ExternalComponentLoader::ExternalComponentLoader() {}

ExternalComponentLoader::~ExternalComponentLoader() {}

void ExternalComponentLoader::StartLoading() {
  prefs_.reset(new base::DictionaryValue());
  std::string appId = extension_misc::kInAppPaymentsSupportAppId;
  prefs_->SetString(appId + ".external_update_url",
                    extension_urls::GetWebstoreUpdateUrl().spec());

  if (CommandLine::ForCurrentProcess()->
          GetSwitchValueASCII(switches::kEnableEnhancedBookmarks) != "0") {
    std::string ext_id = GetEnhancedBookmarksExtensionId();
    if (!ext_id.empty()) {
      prefs_->SetString(ext_id + ".external_update_url",
                        extension_urls::GetWebstoreUpdateUrl().spec());
    }
  }
  LoadFinished();
}

}  // namespace extensions
