/*
 *  Copyright (C) 2016-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "storage/MediaManager.h"
#include "ContextMenus.h"
#include "Application.h"
#include "Autorun.h"
//#include "Util.h"
#include "settings/AdvancedSettings.h"
#include "video/windows/GUIWindowVideoBase.h"
#include "ServiceBroker.h"
#include "music/windows/GUIWindowMusicNav.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"

namespace CONTEXTMENU
{

  bool CEjectDisk::IsVisible(const CFileItem& item) const
  {
#ifdef HAS_DVD_DRIVE
    return item.IsRemovable() && (item.IsDVD() || item.IsCDDA());
#else
    return false;
#endif
  }

  bool CEjectDisk::Execute(const CFileItemPtr& item) const
  {
#ifdef HAS_DVD_DRIVE
    g_mediaManager.ToggleTray(g_mediaManager.TranslateDevicePath(item->GetPath())[0]);
#endif
    return true;
  }

  bool CEjectDrive::IsVisible(const CFileItem& item) const
  {
    // Must be HDD
    return item.IsRemovable() && !item.IsDVD() && !item.IsCDDA();
  }

  bool CEjectDrive::Execute(const CFileItemPtr& item) const
  {
    return g_mediaManager.Eject(item->GetPath());
  }

  static void SetPathAndPlay(CFileItem& item)
  {
    if (item.IsVideoDb())
    {
      item.SetProperty("original_listitem_url", item.GetPath());
      item.SetPath(item.GetVideoInfoTag()->m_strFileNameAndPath);
    }
    item.SetProperty("check_resume", false);

    if (item.IsLiveTV()) // pvr tv or pvr radio?
      g_application.PlayMedia(item, "", PLAYLIST_NONE);
    else
      CServiceBroker::GetPlaylistPlayer().Play(std::make_shared<CFileItem>(item), "");
  }

  std::string CPlay::GetLabel(const CFileItem& itemIn) const
  {
    CFileItem item(itemIn.GetItemToPlay());
    if (item.IsLiveTV())
      return g_localizeStrings.Get(19000); // Switch to channel
    if (CGUIWindowVideoBase::HasResumeItemOffset(&item))
      return g_localizeStrings.Get(12021); // Play from beginning
    return g_localizeStrings.Get(208); // Play
  }

  bool CPlay::IsVisible(const CFileItem& itemIn) const
  {
    CFileItem item(itemIn.GetItemToPlay());
    if (item.IsDeleted()) // e.g. trashed pvr recording
      return false;

    if (itemIn.HasMusicInfoTag())
      return !itemIn.IsParentFolder() && itemIn.CanQueue() && !itemIn.IsAddonsPath() && !itemIn.IsScript() &&
      (itemIn.m_bIsFolder || (itemIn.IsPlayList() && !g_advancedSettings.m_playlistAsFolders));

    if (item.m_bIsFolder)
      return false; //! @todo implement play of video folder items here rather than in CGUIWindowVideoBase

    return item.IsVideo() || item.IsLiveTV() || item.IsDVD() || item.IsCDDA();
  }

  bool CPlay::Execute(const CFileItemPtr& itemIn) const
  {
    if (itemIn->HasMusicInfoTag())
    {
      auto window = CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIWindowMusicBase>(WINDOW_MUSIC_NAV);
      if (window)
        window->Play(itemIn);
      return true;
    }
    CFileItem item(itemIn->GetItemToPlay());
#ifdef HAS_DVD_DRIVE
    if (item.IsDVD() || item.IsCDDA())
      return MEDIA_DETECT::CAutorun::PlayDisc(item.GetPath(), true, true);
#endif
    SetPathAndPlay(item);
    return true;
  }

} // namespace CONTEXTMENU
