#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>
#include <pl/memory/Hook.hpp>
#include <pl/Logger.hpp>
#include <pl/Config.hpp>

#include <EGL/egl.h>
#include <dlfcn.h>
#include <filesystem>
#include <vector>
#include <optional>
#include <string>

// Struktur konfigurasi harus sama dengan config/config.json
struct ToggleConfig {
  int version = 1;
  bool graphics_enabled = true;
};

// Pointer ke fungsi asli
static EGLBoolean (*real_eglSwapBuffers)(EGLDisplay, EGLSurface) = nullptr;
static EGLBoolean (*real_eglSwapBuffersWithDamageKHR)(EGLDisplay, EGLSurface, const EGLint*, EGLint) = nullptr;

// Handle hook (RAII)
static std::vector<pl::memory::HookHandle> g_hooks;

class GraphicsToggleMod {
public:
  static GraphicsToggleMod &instance() {
    static GraphicsToggleMod mod;
    return mod;
  }

  GraphicsToggleMod() : mSelf(*ll::mod::NativeMod::current()) {}

  [[nodiscard]] ll::mod::NativeMod &getSelf() const { return mSelf; }

  bool load() {
    auto &self = getSelf();
    self.getLogger().info("Graphics Toggle loading...");

    // Load config
    mConfig.emplace();
    if (!mConfig->load()) {
      self.getLogger().error("Gagal load config");
      return false;
    }

    // Resolve simbol EGL
    real_eglSwapBuffers = reinterpret_cast<EGLBoolean (*)(EGLDisplay, EGLSurface)>(
        dlsym(RTLD_DEFAULT, "eglSwapBuffers"));
    if (!real_eglSwapBuffers) {
      self.getLogger().error("Gagal resolve eglSwapBuffers");
      return false;
    }

    // Hook eglSwapBuffers
    pl::memory::FuncPtr original = nullptr;
    auto handle = pl::memory::HookHandle(
        reinterpret_cast<pl::memory::FuncPtr>(real_eglSwapBuffers),
        reinterpret_cast<pl::memory::FuncPtr>(&detour_eglSwapBuffers),
        &original);
    if (!handle.installed()) {
      self.getLogger().error("Gagal memasang hook eglSwapBuffers");
      return false;
    }
    real_eglSwapBuffers = reinterpret_cast<EGLBoolean (*)(EGLDisplay, EGLSurface)>(original);
    g_hooks.push_back(std::move(handle));

    // Hook eglSwapBuffersWithDamageKHR jika ada
    real_eglSwapBuffersWithDamageKHR =
        reinterpret_cast<EGLBoolean (*)(EGLDisplay, EGLSurface, const EGLint*, EGLint)>(
            dlsym(RTLD_DEFAULT, "eglSwapBuffersWithDamageKHR"));
    if (real_eglSwapBuffersWithDamageKHR) {
      pl::memory::FuncPtr original2 = nullptr;
      auto handle2 = pl::memory::HookHandle(
          reinterpret_cast<pl::memory::FuncPtr>(real_eglSwapBuffersWithDamageKHR),
          reinterpret_cast<pl::memory::FuncPtr>(&detour_eglSwapBuffersWithDamageKHR),
          &original2);
      if (handle2.installed()) {
        real_eglSwapBuffersWithDamageKHR =
            reinterpret_cast<EGLBoolean (*)(EGLDisplay, EGLSurface, const EGLint*, EGLint)>(original2);
        g_hooks.push_back(std::move(handle2));
      }
    }

    self.getLogger().info("Hook EGL terpasang");
    return true;
  }

  bool enable() {
    auto &self = getSelf();
    
    // PENTING: Simpan ID di string lokal agar umurnya tetap hidup selama registerModule
    std::string modId(self.getId());

    // Daftarkan modul Mod Menu (Tanpa onConfigChanged untuk mencegah crash)
    return pl::modmenu::ModuleBuilder("graphics_toggle.main",
                                      "Graphics Toggle")
        .modId(modId)
        .description("Membekukan presentasi grafis tanpa menghentikan logika game.")
        .defaultEnabled(mConfig->value().graphics_enabled)
        .config("graphics_enabled", "Graphics Enabled",
                pl::modmenu::ConfigType::Toggle, "true", "false", "true")
        .registerModule();
  }

  bool disable() {
    g_hooks.clear(); // RAII, otomatis unhook
    pl::modmenu::unregisterModule("graphics_toggle.main");
    return true;
  }

  bool unload() {
    g_hooks.clear();
    return true;
  }

  // Fungsi untuk membaca config secara langsung di dalam hook
  bool isGraphicsEnabled() const {
    return mConfig && mConfig->value().graphics_enabled;
  }

private:
  ll::mod::NativeMod &mSelf;
  std::optional<pl::config::ConfigFile<ToggleConfig>> mConfig;

  static EGLBoolean detour_eglSwapBuffers(EGLDisplay dpy, EGLSurface surf) {
    if (!instance().isGraphicsEnabled()) {
      // Lewati presentasi frame: layar beku, game tetap jalan
      return EGL_TRUE;
    }
    return real_eglSwapBuffers(dpy, surf);
  }

  static EGLBoolean detour_eglSwapBuffersWithDamageKHR(
      EGLDisplay dpy, EGLSurface surf, const EGLint *rects, EGLint n_rects) {
    if (!instance().isGraphicsEnabled()) {
      return EGL_TRUE;
    }
    return real_eglSwapBuffersWithDamageKHR(dpy, surf, rects, n_rects);
  }
};

PL_REGISTER_MOD(GraphicsToggleMod, GraphicsToggleMod::instance())
