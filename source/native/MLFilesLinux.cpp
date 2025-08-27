// mlvg: GUI library for madronalib apps and plugins
// Copyright (C) 2019-2022 Madrona Labs LLC
// This software is provided 'as-is', without any express or implied warranty.
// See LICENSE.txt for details.

#include "MLFiles.h"
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <cstdlib>

namespace ml
{
namespace FileUtils
{

Path getUserDataPath()
{
  Path resultPath;
  
  // Try to get home directory from environment variable first
  const char* homeDir = getenv("HOME");
  
  // If HOME is not set, try to get it from passwd
  if (!homeDir)
  {
    struct passwd *pw = getpwuid(getuid());
    if (pw)
    {
      homeDir = pw->pw_dir;
    }
  }
  
  if (homeDir)
  {
    TextFragment frag(homeDir);
    resultPath = textToPath(frag, '/');
  }
  
  return resultPath;
}

Path getApplicationDataPath(TextFragment maker, TextFragment app, Symbol type)
{
  Path result;
  
  // On Linux, use ~/.local/share for application data (XDG Base Directory Specification)
  Path homeDataPath;
  
  // Check for XDG_DATA_HOME first
  const char* xdgDataHome = getenv("XDG_DATA_HOME");
  if (xdgDataHome)
  {
    TextFragment frag(xdgDataHome);
    homeDataPath = textToPath(frag, '/');
  }
  else
  {
    // Fall back to ~/.local/share
    Path homePath = getUserDataPath();
    if (homePath)
    {
      homeDataPath = Path(homePath, ".local", "share");
    }
  }
  
  if (homeDataPath)
  {
    Path makerPath(homeDataPath, maker);
    Path appPath(homeDataPath, maker, app);
    
    switch(hash(type))
    {
      default:
      case(hash("root")):
      {
        result = Path(appPath);
        break;
      }
      case(hash("patches")):
      {
        result = Path(appPath);
        break;
      }
      case(hash("scales")):
      case(hash("mappings")):
      {
        result = Path(makerPath, "Scales");
        break;
      }
      case(hash("licenses")):
      {
        result = Path(makerPath, "Licenses");
        break;
      }
      case(hash("samples")):
      {
        result = Path(appPath, "Samples");
        break;
      }
      case(hash("partials")):
      {
        result = Path(appPath, "Partials");
        break;
      }
    }
  }
  
  return result;
}

}
}
