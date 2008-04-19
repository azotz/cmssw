#ifndef PixelLowVoltageMap_h
#define PixelLowVoltageMap_h
//
// This class specifies which detector
// components are used in the configuration
// (and eventually should specify which 
// xdaq process controlls which components).
// 
//
//
//

#include <vector>
#include <set>
#include <map>
#include <utility>
#include <string>
#include "CalibFormats/SiPixelObjects/interface/PixelConfigBase.h"
#include "CalibFormats/SiPixelObjects/interface/PixelModuleName.h"
#include "CalibFormats/SiPixelObjects/interface/PixelHdwAddress.h"
#include "CalibFormats/SiPixelObjects/interface/PixelNameTranslation.h"
#include "CalibFormats/SiPixelObjects/interface/PixelROCStatus.h"

namespace pos{
  class PixelLowVoltageMap: public PixelConfigBase {

  public:

    PixelLowVoltageMap(std::vector< std::vector < std::string> > &tableMat);
    PixelLowVoltageMap(std::string filename);

    void writeASCII(std::string dir="") const;

    std::string dpNameIana(const PixelModuleName& module) const;
    std::string dpNameIdigi(const PixelModuleName& module) const;

    std::set <unsigned int> getFEDs(PixelNameTranslation* translation) const;
    std::map <unsigned int, std::set<unsigned int> > getFEDsAndChannels(PixelNameTranslation* translation) const;

  private:
    //ugly... FIXME
    std::map<PixelModuleName, std::pair<std::string, std::pair<std::string, std::string> > > dpNameMap_;
    //                                    base                    Iana          Idigi 
  };
}
#endif
