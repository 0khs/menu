#ifndef A_NATIVE_WINDOW_CREATOR_H
#define A_NATIVE_WINDOW_CREATOR_H

#include <android/native_window.h>
#include <android/log.h>
#include <dlfcn.h>
#include <sys/system_properties.h>
#include <jni.h>
#include <cstdlib>
#include <cstddef>
#include <unordered_map>
#include <string>
#include <vector>

#define ResolveMethod(ClassName, MethodName, Handle, MethodSignature) \
    ClassName##__##MethodName = reinterpret_cast<decltype(ClassName##__##MethodName)>(symbolMethod.Find(Handle, MethodSignature));

namespace android {
    namespace detail {
        namespace ui {
            struct LayerStack {
                uint32_t id = UINT32_MAX;
            };

            enum class Rotation {
                Rotation0 = 0,
                Rotation90 = 1,
                Rotation180 = 2,
                Rotation270 = 3
            };

            struct Size {
                int32_t width = -1;
                int32_t height = -1;
            };

            struct DisplayState {
                LayerStack layerStack;
                Rotation orientation = Rotation::Rotation0;
                Size layerStackSpaceRect;
                uint8_t padding[128];
            };

            typedef int64_t nsecs_t;
            
            struct DisplayInfo {
                uint32_t w{0};
                uint32_t h{0};
                float xdpi{0};
                float ydpi{0};
                float fps{0};
                float density{0};
                uint8_t orientation{0};
                bool secure{false};
                nsecs_t appVsyncOffset{0};
                nsecs_t presentationDeadline{0};
                uint32_t viewportW{0};
                uint32_t viewportH{0};
            };

            enum class DisplayType {
                DisplayIdMain = 0,
                DisplayIdHdmi = 1
            };

            struct PhysicalDisplayId {
                uint64_t value;
            };
        }

        struct String8;
        struct LayerMetadata;
        struct Surface;
        struct SurfaceControl;
        struct SurfaceComposerClientTransaction;
        struct SurfaceComposerClient;

        template <typename any_t>
        struct StrongPointer {
            union {
                any_t* pointer;
                char padding[sizeof(std::max_align_t)];
            };

            inline any_t* operator->() const { return pointer; }
            inline any_t* get() const { return pointer; }
            inline explicit operator bool() const { return nullptr != pointer; }
        };

        struct Functionals {
            struct SymbolMethod {
                void*(*Open)(const char* filename, int flag) = nullptr;
                void*(*Find)(void* handle, const char* symbol) = nullptr;
                int (*Close)(void* handle) = nullptr;
            };

            size_t systemVersion = 13;

            void (*RefBase__IncStrong)(void* thiz, void* id) = nullptr;
            void (*RefBase__DecStrong)(void* thiz, void* id) = nullptr;
            void (*String8__Constructor)(void* thiz, const char* const data) = nullptr;
            void (*String8__Destructor)(void* thiz) = nullptr;
            void (*LayerMetadata__Constructor)(void* thiz) = nullptr;
            void (*LayerMetadata__setInt32)(void* thiz, uint32_t type, int32_t value) = nullptr;
            void (*SurfaceComposerClient__Constructor)(void* thiz) = nullptr;
            void (*SurfaceComposerClient__Destructor)(void* thiz) = nullptr;

            StrongPointer<void> (*SurfaceComposerClient__CreateSurface)(void* thiz, void* name, uint32_t w, uint32_t h, int32_t format,
                                                    uint32_t flags, void* parentHandle, void* layerMetadata,
                                                    uint32_t* outTransformHint) = nullptr;

            StrongPointer<void> (*SurfaceComposerClient__GetInternalDisplayToken)() = nullptr;
            StrongPointer<void> (*SurfaceComposerClient__GetBuiltInDisplay)(ui::DisplayType type) = nullptr;
            int32_t (*SurfaceComposerClient__GetDisplayState)(StrongPointer<void>& display, ui::DisplayState* displayState) = nullptr;
            int32_t (*SurfaceComposerClient__GetDisplayInfo)(StrongPointer<void>& display, ui::DisplayInfo* displayInfo) = nullptr;
            std::vector<ui::PhysicalDisplayId> (*SurfaceComposerClient__GetPhysicalDisplayIds)() = nullptr;
            StrongPointer<void> (*SurfaceComposerClient__GetPhysicalDisplayToken)(ui::PhysicalDisplayId displayId) = nullptr;
            void (*SurfaceComposerClient__Transaction__Constructor)(void* thiz) = nullptr;
            void* (*SurfaceComposerClient__Transaction__SetLayer)(void* thiz, StrongPointer<void>& surfaceControl, int32_t z) = nullptr;
            void* (*SurfaceComposerClient__Transaction__SetTrustedOverlay)(void* thiz, StrongPointer<void>& surfaceControl, bool isTrustedOverlay) = nullptr;
            int32_t (*SurfaceComposerClient__Transaction__Apply)(void* thiz, bool synchronous, bool oneWay) = nullptr;
            void* (*SurfaceComposerClient__Transaction__SetLayerStack)(void* thiz, StrongPointer<void>& surfaceControl, uint32_t layerStackId) = nullptr;
            int32_t (*SurfaceControl__Validate)(void* thiz) = nullptr;
            StrongPointer<Surface> (*SurfaceControl__GetSurface)(void* thiz) = nullptr;
            void (*SurfaceControl__DisConnect)(void* thiz) = nullptr;

            Functionals(const SymbolMethod& symbolMethod) {
                char sdk_ver[32] = {0};
                __system_property_get("ro.build.version.sdk", sdk_ver);
                if (sdk_ver[0] != '\0') {
                    int sdk = std::atoi(sdk_ver);
                    if (sdk >= 35) systemVersion = 15;
                    else if (sdk == 34) systemVersion = 14;
                    else if (sdk == 33) systemVersion = 13;
                    else if (sdk >= 31) systemVersion = 12;
                    else if (sdk == 30) systemVersion = 11;
                    else if (sdk == 29) systemVersion = 10;
                    else if (sdk == 28) systemVersion = 9;
                    else if (sdk == 27) systemVersion = 8;
                } else {
                    char rel_ver[32] = {0};
                    __system_property_get("ro.build.version.release", rel_ver);
                    if (rel_ver[0] != '\0') {
                        systemVersion = std::atoi(rel_ver);
                    }
                }

                if (systemVersion < 8) {
                    __android_log_print(ANDROID_LOG_ERROR, "ImGui", "[-] Unsupported system version: %zu", systemVersion);
                    exit(0);
                    return;
                }

                static std::unordered_map<size_t, std::unordered_map<void**, const char*>> patchesTable = {
                    {
                        15,
                        {
                            { reinterpret_cast<void**>(&LayerMetadata__Constructor), "_ZN7android3gui13LayerMetadataC2Ev" },
                            { reinterpret_cast<void**>(&SurfaceComposerClient__CreateSurface), "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjiiRKNS_2spINS_7IBinderEEENS_3gui13LayerMetadataEPj" },
                            { reinterpret_cast<void**>(&LayerMetadata__setInt32), "_ZN7android3gui13LayerMetadata8setInt32Eji" },
                        },
                    },
                    {
                        14,
                        {
                            { reinterpret_cast<void**>(&LayerMetadata__Constructor), "_ZN7android3gui13LayerMetadataC2Ev" },
                            { reinterpret_cast<void**>(&SurfaceComposerClient__CreateSurface), "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjiiRKNS_2spINS_7IBinderEEENS_3gui13LayerMetadataEPj" },
                            { reinterpret_cast<void**>(&LayerMetadata__setInt32), "_ZN7android3gui13LayerMetadata8setInt32Eji" },
                        },
                    },
                    {
                        12,
                        {
                            { reinterpret_cast<void**>(&SurfaceComposerClient__Transaction__Apply), "_ZN7android21SurfaceComposerClient11Transaction5applyEb" },
                        },
                    },
                    {
                        11,
                        {
                            { reinterpret_cast<void**>(&SurfaceComposerClient__CreateSurface), "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjijPNS_14SurfaceControlENS_13LayerMetadataEPj" },
                            { reinterpret_cast<void**>(&SurfaceControl__GetSurface), "_ZNK7android14SurfaceControl10getSurfaceEv" },
                        },
                    },
                    {
                        10,
                        {
                            { reinterpret_cast<void**>(&SurfaceComposerClient__CreateSurface), "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjijPNS_14SurfaceControlENS_13LayerMetadataE" },
                            { reinterpret_cast<void**>(&SurfaceControl__GetSurface), "_ZNK7android14SurfaceControl10getSurfaceEv" },
                        },
                    },
                    {
                        9,
                        {
                            { reinterpret_cast<void**>(&SurfaceComposerClient__CreateSurface), "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjijPNS_14SurfaceControlEii" },
                            { reinterpret_cast<void**>(&SurfaceComposerClient__GetBuiltInDisplay), "_ZN7android21SurfaceComposerClient17getBuiltInDisplayEi" },
                            { reinterpret_cast<void**>(&SurfaceControl__GetSurface), "_ZNK7android14SurfaceControl10getSurfaceEv" },
                        },
                    },
                    {
                        8,
                        {
                            { reinterpret_cast<void**>(&SurfaceComposerClient__CreateSurface), "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjijPNS_14SurfaceControlEjj" },
                            { reinterpret_cast<void**>(&SurfaceComposerClient__GetBuiltInDisplay), "_ZN7android21SurfaceComposerClient17getBuiltInDisplayEi" },
                            { reinterpret_cast<void**>(&SurfaceControl__GetSurface), "_ZNK7android14SurfaceControl10getSurfaceEv" },
                        },
                    },
                };

#ifdef __LP64__
                auto libgui = symbolMethod.Open("/system/lib64/libgui.so", RTLD_LAZY);
                auto libutils = symbolMethod.Open("/system/lib64/libutils.so", RTLD_LAZY);
#else
                auto libgui = symbolMethod.Open("/system/lib/libgui.so", RTLD_LAZY);
                auto libutils = symbolMethod.Open("/system/lib/libutils.so", RTLD_LAZY);
#endif

                ResolveMethod(RefBase, IncStrong, libutils, "_ZNK7android7RefBase9incStrongEPKv");
                ResolveMethod(RefBase, DecStrong, libutils, "_ZNK7android7RefBase9decStrongEPKv");

                ResolveMethod(String8, Constructor, libutils, "_ZN7android7String8C2EPKc");
                ResolveMethod(String8, Destructor, libutils, "_ZN7android7String8D2Ev");

                ResolveMethod(LayerMetadata, Constructor, libgui, "_ZN7android13LayerMetadataC2Ev");
                ResolveMethod(LayerMetadata, setInt32, libgui, "_ZN7android13LayerMetadata8setInt32Eji");

                ResolveMethod(SurfaceComposerClient, Constructor, libgui, "_ZN7android21SurfaceComposerClientC2Ev");
                ResolveMethod(SurfaceComposerClient, CreateSurface, libgui, "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjijRKNS_2spINS_7IBinderEEENS_13LayerMetadataEPj");
                ResolveMethod(SurfaceComposerClient, GetInternalDisplayToken, libgui, "_ZN7android21SurfaceComposerClient23getInternalDisplayTokenEv");
                ResolveMethod(SurfaceComposerClient, GetDisplayState, libgui, "_ZN7android21SurfaceComposerClient15getDisplayStateERKNS_2spINS_7IBinderEEEPNS_2ui12DisplayStateE");
                ResolveMethod(SurfaceComposerClient, GetDisplayInfo, libgui, "_ZN7android21SurfaceComposerClient14getDisplayInfoERKNS_2spINS_7IBinderEEEPNS_11DisplayInfoE");
                ResolveMethod(SurfaceComposerClient, GetPhysicalDisplayIds, libgui, "_ZN7android21SurfaceComposerClient21getPhysicalDisplayIdsEv");
                ResolveMethod(SurfaceComposerClient, GetPhysicalDisplayToken, libgui, "_ZN7android21SurfaceComposerClient23getPhysicalDisplayTokenENS_17PhysicalDisplayIdE");

                ResolveMethod(SurfaceComposerClient__Transaction, Constructor, libgui, "_ZN7android21SurfaceComposerClient11TransactionC2Ev");
                ResolveMethod(SurfaceComposerClient__Transaction, SetLayer, libgui, "_ZN7android21SurfaceComposerClient11Transaction8setLayerERKNS_2spINS_14SurfaceControlEEEi");
                ResolveMethod(SurfaceComposerClient__Transaction, SetTrustedOverlay, libgui, "_ZN7android21SurfaceComposerClient11Transaction17setTrustedOverlayERKNS_2spINS_14SurfaceControlEEEb");
                ResolveMethod(SurfaceComposerClient__Transaction, Apply, libgui, "_ZN7android21SurfaceComposerClient11Transaction5applyEbb");
                ResolveMethod(SurfaceComposerClient__Transaction, SetLayerStack, libgui, "_ZN7android21SurfaceComposerClient11Transaction13setLayerStackERKNS_2spINS_14SurfaceControlEEENS_2ui10LayerStackE");

                ResolveMethod(SurfaceControl, Validate, libgui, "_ZNK7android14SurfaceControl8validateEv");
                ResolveMethod(SurfaceControl, GetSurface, libgui, "_ZN7android14SurfaceControl10getSurfaceEv");
                ResolveMethod(SurfaceControl, DisConnect, libgui, "_ZN7android14SurfaceControl10disconnectEv");

                if (patchesTable.contains(systemVersion)) {
                    for (const auto& [patchTo, signature] : patchesTable.at(systemVersion)) {
                        *patchTo = symbolMethod.Find(libgui, signature);
                    }
                }

                symbolMethod.Close(libutils);
                symbolMethod.Close(libgui);
            }

            static const Functionals& GetInstance(const SymbolMethod& symbolMethod = {.Open = dlopen, .Find = dlsym, .Close = dlclose}) {
                static Functionals functionals(symbolMethod);
                return functionals;
            }
        };

        struct String8 {
            char data[1024];
            String8(const char* const string) {
                Functionals::GetInstance().String8__Constructor(data, string);
            }
            ~String8() {
                Functionals::GetInstance().String8__Destructor(data);
            }
            operator void*() {
                return reinterpret_cast<void*>(data);
            }
        };

        struct LayerMetadata {
            char data[1024];
            LayerMetadata() {
                if (9 < Functionals::GetInstance().systemVersion)
                    Functionals::GetInstance().LayerMetadata__Constructor(data);
            }
            void setInt32(uint32_t type, int32_t value) {
                if (9 < Functionals::GetInstance().systemVersion)
                    Functionals::GetInstance().LayerMetadata__setInt32(data, type, value);
            }
            operator void*() {
                if (9 < Functionals::GetInstance().systemVersion)
                    return reinterpret_cast<void*>(data);
                else
                    return nullptr;
            }
        };

        struct Surface {};

        struct SurfaceControl {
            void* data;
            SurfaceControl() : data(nullptr) {}
            SurfaceControl(void* data) : data(data) {}

            int32_t Validate() {
                if (nullptr == data) return 0;
                return Functionals::GetInstance().SurfaceControl__Validate(data);
            }

            Surface* GetSurface() {
                if (nullptr == data) return nullptr;
                auto result = Functionals::GetInstance().SurfaceControl__GetSurface(data);
                return reinterpret_cast<Surface*>(reinterpret_cast<size_t>(result.pointer) + sizeof(std::max_align_t) / 2);
            }

            void DisConnect() {
                if (nullptr == data) return;
                Functionals::GetInstance().SurfaceControl__DisConnect(data);
            }

            void DestroySurface(Surface* surface) {
                if (nullptr == data || nullptr == surface) return;
                Functionals::GetInstance().RefBase__DecStrong(reinterpret_cast<Surface*>(reinterpret_cast<size_t>(surface) - sizeof(std::max_align_t) / 2), this);
                DisConnect();
                Functionals::GetInstance().RefBase__DecStrong(data, this);
            }
        };

        struct SurfaceComposerClientTransaction {
            char data[1024];

            SurfaceComposerClientTransaction() {
                Functionals::GetInstance().SurfaceComposerClient__Transaction__Constructor(data);
            }

            void* SetLayer(StrongPointer<void>& surfaceControl, int32_t z) {
                return Functionals::GetInstance().SurfaceComposerClient__Transaction__SetLayer(data, surfaceControl, z);
            }

            void* SetTrustedOverlay(StrongPointer<void>& surfaceControl, bool isTrustedOverlay) {
                return Functionals::GetInstance().SurfaceComposerClient__Transaction__SetTrustedOverlay(data, surfaceControl, isTrustedOverlay);
            }

            int32_t Apply(bool synchronous, bool oneWay) {
                if (12 >= Functionals::GetInstance().systemVersion)
                    return reinterpret_cast<int32_t (*)(void*, bool)>(Functionals::GetInstance().SurfaceComposerClient__Transaction__Apply)(data, synchronous);
                else
                    return Functionals::GetInstance().SurfaceComposerClient__Transaction__Apply(data, synchronous, oneWay);
            }

            void* SetLayerStack(StrongPointer<void>& surfaceControl, uint32_t layerStackId) {
                return Functionals::GetInstance().SurfaceComposerClient__Transaction__SetLayerStack(data, surfaceControl, layerStackId);
            }
        };

        struct SurfaceComposerClient {
            char data[1024];

            SurfaceComposerClient() {
                Functionals::GetInstance().SurfaceComposerClient__Constructor(data);
                Functionals::GetInstance().RefBase__IncStrong(data, this);
            }

            SurfaceControl CreateSurface(const char* name, int32_t width, int32_t height, bool hide, bool secure) {
                void* parentHandle = nullptr;
                String8 windowName(name);
                LayerMetadata layerMetadata;
                uint32_t flags = secure ? 0x80 : 0;

                if (12 <= Functionals::GetInstance().systemVersion) {
                    static void* fakeParentHandleForBinder = nullptr;
                    parentHandle = &fakeParentHandleForBinder;
                }

                StrongPointer<void> result{};
                if (Functionals::GetInstance().systemVersion <= 9) {
                    int value = -1;
                    if (!secure && hide) value = 441731;

                    result = reinterpret_cast<StrongPointer<void> (*)(void*, void*, uint32_t, uint32_t, int32_t, uint32_t, void*, int32_t, int32_t)>(
                        Functionals::GetInstance().SurfaceComposerClient__CreateSurface)(data, windowName, width, height, 1, flags, nullptr, value, -1);
                } else {
                    if (!secure && hide) {
                        if (Functionals::GetInstance().systemVersion > 9 && Functionals::GetInstance().systemVersion < 12) {
                            layerMetadata.setInt32(2, 441731);
                        } else {
                            flags = 0x40;
                        }
                    }
                    result = Functionals::GetInstance().SurfaceComposerClient__CreateSurface(data, windowName, width, height, 1, flags, parentHandle, layerMetadata, nullptr);
                }

                if (12 <= Functionals::GetInstance().systemVersion) {
                    static SurfaceComposerClientTransaction transaction;
                    transaction.SetTrustedOverlay(result, true);
                    transaction.SetLayer(result, INT32_MAX);
                    transaction.SetLayerStack(result, 0);
                    transaction.Apply(false, true);
                }

                return {result.get()};
            }

            bool GetDisplayInfo(ui::DisplayState* displayInfo) {
                StrongPointer<void> defaultDisplay;

                if (9 >= Functionals::GetInstance().systemVersion) {
                    defaultDisplay = Functionals::GetInstance().SurfaceComposerClient__GetBuiltInDisplay(ui::DisplayType::DisplayIdMain);
                } else {
                    if (14 > Functionals::GetInstance().systemVersion) {
                        if (Functionals::GetInstance().SurfaceComposerClient__GetInternalDisplayToken) {
                            defaultDisplay = Functionals::GetInstance().SurfaceComposerClient__GetInternalDisplayToken();
                        }
                    } else {
                        if (Functionals::GetInstance().SurfaceComposerClient__GetPhysicalDisplayIds) {
                            auto displayIds = Functionals::GetInstance().SurfaceComposerClient__GetPhysicalDisplayIds();
                            if (!displayIds.empty() && Functionals::GetInstance().SurfaceComposerClient__GetPhysicalDisplayToken) {
                                defaultDisplay = Functionals::GetInstance().SurfaceComposerClient__GetPhysicalDisplayToken(displayIds[0]);
                            }
                        }
                    }
                }

                if (nullptr == defaultDisplay.get())
                    return false;

                if (11 <= Functionals::GetInstance().systemVersion) {
                    if (Functionals::GetInstance().SurfaceComposerClient__GetDisplayState) {
                        return 0 == Functionals::GetInstance().SurfaceComposerClient__GetDisplayState(defaultDisplay, displayInfo);
                    }
                    return false;
                } else {
                    ui::DisplayInfo realDisplayInfo{};
                    if (Functionals::GetInstance().SurfaceComposerClient__GetDisplayInfo) {
                        if (0 != Functionals::GetInstance().SurfaceComposerClient__GetDisplayInfo(defaultDisplay, &realDisplayInfo)) return false;
                        displayInfo->layerStackSpaceRect.width = realDisplayInfo.w;
                        displayInfo->layerStackSpaceRect.height = realDisplayInfo.h;
                        displayInfo->orientation = static_cast<ui::Rotation>(realDisplayInfo.orientation);
                        return true;
                    }
                    return false;
                }
            }
        };
    }

    class ANativeWindowCreator {
    public:
        struct DisplayInfo {
            uint32_t orientation;
            int32_t width;
            int32_t height;
        };

        static inline JavaVM* g_VM = nullptr;

        static void SetVM(JavaVM* vm) {
            g_VM = vm;
        }

    public:
        static detail::SurfaceComposerClient& GetComposerInstance() {
            static detail::SurfaceComposerClient surfaceComposerClient;
            return surfaceComposerClient;
        }

        static DisplayInfo GetDisplayInfo() {
            auto& surfaceComposerClient = GetComposerInstance();
            detail::ui::DisplayState displayInfo{};

            if (surfaceComposerClient.GetDisplayInfo(&displayInfo)) {
                return DisplayInfo{
                    .orientation = static_cast<uint32_t>(displayInfo.orientation),
                    .width = displayInfo.layerStackSpaceRect.width,
                    .height = displayInfo.layerStackSpaceRect.height,
                };
            }

            if (g_VM) {
                JNIEnv* env = nullptr;
                bool attached = false;
                if (g_VM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
                    if (g_VM->AttachCurrentThread(&env, nullptr) == JNI_OK) {
                        attached = true;
                    }
                }
                if (env) {
                    int w = 0, h = 0;
                    jclass resourcesClass = env->FindClass("android/content/res/Resources");
                    if (resourcesClass) {
                        jmethodID getSystemMethod = env->GetStaticMethodID(resourcesClass, "getSystem", "()Landroid/content/res/Resources;");
                        if (getSystemMethod) {
                            jobject resourcesObj = env->CallStaticObjectMethod(resourcesClass, getSystemMethod);
                            if (resourcesObj) {
                                jmethodID getDisplayMetricsMethod = env->GetMethodID(resourcesClass, "getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
                                if (getDisplayMetricsMethod) {
                                    jobject displayMetricsObj = env->CallObjectMethod(resourcesObj, getDisplayMetricsMethod);
                                    if (displayMetricsObj) {
                                        jclass displayMetricsClass = env->GetObjectClass(displayMetricsObj);
                                        w = env->GetIntField(displayMetricsObj, env->GetFieldID(displayMetricsClass, "widthPixels", "I"));
                                        h = env->GetIntField(displayMetricsObj, env->GetFieldID(displayMetricsClass, "heightPixels", "I"));
                                        env->DeleteLocalRef(displayMetricsObj);
                                    }
                                }
                                env->DeleteLocalRef(resourcesObj);
                            }
                        }
                        env->DeleteLocalRef(resourcesClass);
                    }
                    if (attached) {
                        g_VM->DetachCurrentThread();
                    }
                    if (w > 0 && h > 0) {
                        return DisplayInfo{ 0, w, h };
                    }
                }
            }

            return {};
        }

        static ANativeWindow* Create(const char* name, int32_t width = -1, int32_t height = -1, bool hide = true, bool secure = false) {
            auto& surfaceComposerClient = GetComposerInstance();

            while (-1 == width || -1 == height) {
                DisplayInfo dInfo = GetDisplayInfo();
                if (dInfo.width <= 0 || dInfo.height <= 0) break;
                width = dInfo.width;
                height = dInfo.height;
                break;
            }

            auto surfaceControl = surfaceComposerClient.CreateSurface(name, width, height, hide, secure);
            auto nativeWindow = reinterpret_cast<ANativeWindow*>(surfaceControl.GetSurface());
            m_cachedSurfaceControl.emplace(nativeWindow, surfaceControl);
            return nativeWindow;
        }

        static void Destroy(ANativeWindow* nativeWindow) {
            if (!m_cachedSurfaceControl.contains(nativeWindow)) return;
            m_cachedSurfaceControl[nativeWindow].DestroySurface(reinterpret_cast<detail::Surface*>(nativeWindow));
            m_cachedSurfaceControl.erase(nativeWindow);
        }

    private:
        inline static std::unordered_map<ANativeWindow*, detail::SurfaceControl> m_cachedSurfaceControl;
    };
}

#undef ResolveMethod

#endif 
