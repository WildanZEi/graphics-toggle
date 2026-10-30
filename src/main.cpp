#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>
#include <pl/memory/Hook.hpp>
#include <pl/Logger.hpp>

#include <EGL/egl.h>
#include <dlfcn.h>
#include <atomic>
#include <filesystem>
#include <vector>

// Flag global yang dibaca oleh hook
static std::atomic_bool g_graphicsEnabled{true};

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

    // Daftarkan modul Mod Menu dengan satu toggle
    return pl::modmenu::ModuleBuilder("graphics_toggle.main",
                                      "Graphics Toggle")
        .modId(self.getId())
        .description("Membekukan presentasi grafis tanpa menghentikan logika game.")
        .defaultEnabled(true)
        .config("graphics_enabled", "Graphics Enabled",
                pl::modmenu::ConfigType::Toggle, "true")
        .onConfigChanged([this](std::string_view key, std::string_view value) {
          if (key == "graphics_enabled") {
            bool enabled = (value == "true" || value == "1");
            g_graphicsEnabled.store(enabled, std::memory_order_relaxed);
            getSelf().getLogger().info("Graphics enabled = {}", enabled);
          }
        })
        .registerModule();
  }

  bool disable() {
    // Lepas hook
    g_hooks.clear(); // RAII, otomatis unhook
    // Unregister modul
    pl::modmenu::unregisterModule("graphics_toggle.main");
    return true;
  }

  bool unload() {
    g_hooks.clear();
    return true;
  }

private:
  ll::mod::NativeMod &mSelf;

  static EGLBoolean detour_eglSwapBuffers(EGLDisplay dpy, EGLSurface surf) {
    if (!g_graphicsEnabled.load(std::memory_order_relaxed)) {
      // Lewati presentasi frame: layar beku, game tetap jalan
      return EGL_TRUE;
    }
    return real_eglSwapBuffers(dpy, surf);
  }

  static EGLBoolean detour_eglSwapBuffersWithDamageKHR(
      EGLDisplay dpy, EGLSurface surf, const EGLint *rects, EGLint n_rects) {
    if (!g_graphicsEnabled.load(std::memory_order_relaxed)) {
      return EGL_TRUE;
    }
    return real_eglSwapBuffersWithDamageKHR(dpy, surf, rects, n_rects);
  }
};

PL_REGISTER_MOD(GraphicsToggleMod, GraphicsToggleMod::instance())