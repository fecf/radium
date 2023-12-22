#include "live++.h"
#include "config.h"

#include <cassert>
#include <filesystem>
#include <windows.h>

#ifdef ENABLE_LIVEPP
#include LivePPSDKInclude
#endif

namespace livepp {

#ifdef ENABLE_LIVEPP
class Singleton {
 public:
  ~Singleton() {
    if (!initialized_) {
      return;
    }
    lpp::LppDestroySynchronizedAgent(&agent_);
  }

  static Singleton& GetInstance() {
    static Singleton instance;
    return instance;
  }

  void Sync() {
    if (!initialized_) {
      return;
    }
    if (agent_.WantsReload()) {
      agent_.CompileAndReloadChanges(lpp::LPP_RELOAD_BEHAVIOUR_WAIT_UNTIL_CHANGES_ARE_APPLIED);
    }
    if (agent_.WantsRestart()) {
      agent_.Restart(lpp::LPP_RESTART_BEHAVIOUR_INSTANT_TERMINATION, 0u);
    }
  }

 private:
  Singleton() {
    std::filesystem::path fs(LivePPSDKPath);
    if (std::filesystem::exists(fs)) {
      std::wstring path = std::filesystem::absolute(fs).wstring();
      agent_ = lpp::LppCreateSynchronizedAgent(NULL, path.c_str());
      agent_.EnableModule(lpp::LppGetCurrentModulePath(), lpp::LPP_MODULES_OPTION_NONE, NULL, NULL);
      initialized_ = true;
    }
  }
  lpp::LppSynchronizedAgent agent_;
  bool initialized_ = false;
};
#else
class Singleton {
 public:
  ~Singleton() {}
  static Singleton& GetInstance() {
    static Singleton instance;
    return instance;
  }
  void Sync() {}
 private:
  Singleton() {}
};
#endif

void sync() { Singleton::GetInstance().Sync(); }

}  // namespace livepp
