//
// Created by superhuap on 2026/11/18.
// Adapted from Universal-Dear-ImGui-Hook (MIT), src/windows/d3d12.cpp and
// dxgi.cpp (the capture paths are not ported).
//

#include "backend/DX12_Backend.h"

#include <d3d12.h>
#include <dxgi1_4.h>

#include <atomic>
#include <mutex>
#include <vector>

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include "backend/DxgiUtil.h"
#include "backend/ImGuiColors.h"
#include "menu/Menu.h"
#include "utils/HookHelper.h"
#include "utils/InputHook.h"
#include "utils/InputLock.h"
#include "utils/Logger.h"

#include "core/Guarded.h"
#include "core/ScopedCall.h"
#include "core/Teardown.h"
#include "core/WinUtil.h"

namespace
{

    using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
    using Present1Fn = HRESULT(__stdcall*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
    using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    using ResizeBuffers1Fn = HRESULT(__stdcall*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT,
                                                 const UINT*, IUnknown* const*);
    using ExecuteCommandListsFn = void(__stdcall*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
    using D3D12CreateDeviceFn = HRESULT(__stdcall*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
    using CreateDXGIFactory1Fn = HRESULT(__stdcall*)(REFIID, void**);
    using CreateSwapChainFn = HRESULT(__stdcall*)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
    using CreateSwapChainForHwndFn = HRESULT(__stdcall*)(IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*,
                                                         const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*,
                                                         IDXGISwapChain1**);
    using CreateSwapChainForCoreWindowFn = HRESULT(__stdcall*)(IDXGIFactory2*, IUnknown*, IUnknown*,
                                                               const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*,
                                                               IDXGISwapChain1**);
    using CreateSwapChainForCompositionFn = HRESULT(__stdcall*)(IDXGIFactory2*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*,
                                                                IDXGIOutput*, IDXGISwapChain1**);

    constexpr size_t kExecuteCommandListsIndex = 10;
    constexpr size_t kCreateSwapChainIndex = 10;
    constexpr size_t kCreateSwapChainForHwndIndex = 15;
    constexpr size_t kCreateSwapChainForCoreWindowIndex = 16;
    constexpr size_t kCreateSwapChainForCompositionIndex = 24;
    // Swap chains and direct queues remembered; the oldest entry goes first.
    constexpr size_t kMaxSwapChains = 16;
    constexpr size_t kMaxDirectQueues = 16;
    // Presents of a swap chain to wait before following an application that
    // moved to another direct queue.
    constexpr int kQueueSwitchFrames = 60;
    // Descriptors the SRV heap reserves for the ImGui textures while it grows.
    constexpr UINT kSrvDescriptors = 264;

    HMODULE g_module = nullptr;
    HMODULE g_dxgiModule = nullptr;
    ExecuteCommandListsFn g_executeCommandLists = nullptr;
    CreateSwapChainFn g_createSwapChain = nullptr;
    CreateSwapChainForHwndFn g_createSwapChainForHwnd = nullptr;
    CreateSwapChainForCoreWindowFn g_createSwapChainForCoreWindow = nullptr;
    CreateSwapChainForCompositionFn g_createSwapChainForComposition = nullptr;

    PresentFn oPresent = nullptr;
    Present1Fn oPresent1 = nullptr;
    ResizeBuffersFn oResizeBuffers = nullptr;
    ResizeBuffers1Fn oResizeBuffers1 = nullptr;

    struct Hook
    {
        const char* name;
        void* target;
    };
    std::vector<Hook> g_installedHooks;

    // The queue a swap chain presents on is only known when the swap chain is
    // created. When the hook comes later, a direct queue of the swap chain's
    // device stands in for it: the one the presenting thread submitted to last,
    // as engines usually present from the thread that submits the frame, or
    // else the most recently used one.
    struct SwapChainQueue
    {
        IUnknown* swapChain;
        ID3D12CommandQueue* queue;
    };

    // Only the queue is stored: it keeps its device alive, while the old
    // `device` field was filled after its reference had been released.
    struct DirectQueue
    {
        ID3D12CommandQueue* queue;
    };

    std::mutex g_queueMutex;
    std::vector<SwapChainQueue> g_swapChainQueues;
    std::vector<DirectQueue> g_directQueues;
    // The per-thread cache outlives Shutdown(): it is only trusted for the
    // session of the hook that filled it.
    std::atomic<unsigned> g_session{0};
    thread_local ID3D12CommandQueue* t_lastQueue = nullptr;
    thread_local unsigned t_lastSession = 0;

    ID3D12CommandQueue* LastQueueOfThread()
    {
        return t_lastSession == g_session.load(std::memory_order_relaxed) ? t_lastQueue : nullptr;
    }

    struct FrameContext
    {
        ID3D12CommandAllocator* allocator = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE target{};
        UINT64 fenceValue = 0;
    };

    ID3D12Device* g_device = nullptr;
    ID3D12CommandQueue* g_queue = nullptr;
    IDXGISwapChain* g_swapChain = nullptr;
    ID3D12DescriptorHeap* g_targetHeap = nullptr;
    ID3D12DescriptorHeap* g_srvHeap = nullptr;
    ID3D12GraphicsCommandList* g_commandList = nullptr;
    ID3D12Fence* g_fence = nullptr;
    HANDLE g_fenceEvent = nullptr;
    UINT64 g_fenceValue = 0;
    std::vector<FrameContext> g_frames;
    // Unused slots of the SRV heap, the lowest last.
    std::vector<UINT> g_freeSrvs;
    UINT g_bufferCount = 0;
    DXGI_FORMAT g_format = DXGI_FORMAT_UNKNOWN;
    int g_queueMisses = 0;
    bool g_framesDirty = false;
    bool g_imguiReady = false;
    HWND g_boundWindow = nullptr;

    // One overlay thread at a time, on the swap chain that claimed the process.
    std::mutex g_frameMutex;

    void Teardown();

    IUnknown* Identity(IUnknown* object)
    {
        IUnknown* identity = nullptr;
        if (object && SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(&identity))))
            identity->Release();
        return identity;
    }

    void RecordSwapChain(IUnknown* device, IUnknown* swapChain)
    {
        ID3D12CommandQueue* queue = nullptr;
        if (!device || !swapChain || FAILED(device->QueryInterface(IID_PPV_ARGS(&queue))))
            return;
        IUnknown* identity = Identity(swapChain);
        std::lock_guard<std::mutex> lock(g_queueMutex);
        for (SwapChainQueue& entry : g_swapChainQueues)
        {
            if (entry.swapChain == identity)
            {
                entry.queue->Release();
                entry.queue = queue;
                return;
            }
        }
        if (g_swapChainQueues.size() == kMaxSwapChains)
        {
            g_swapChainQueues.front().queue->Release();
            g_swapChainQueues.erase(g_swapChainQueues.begin());
        }
        g_swapChainQueues.push_back({identity, queue});
    }

    void RecordDirectQueue(ID3D12CommandQueue* queue)
    {
        if (queue == LastQueueOfThread())
            return;
        t_lastQueue = queue;
        t_lastSession = g_session.load(std::memory_order_relaxed);
        std::lock_guard<std::mutex> lock(g_queueMutex);
        for (size_t i = 0; i < g_directQueues.size(); ++i)
        {
            if (g_directQueues[i].queue == queue)
            {
                DirectQueue entry = g_directQueues[i];
                g_directQueues.erase(g_directQueues.begin() + static_cast<std::ptrdiff_t>(i));
                g_directQueues.push_back(entry);
                return;
            }
        }
        ID3D12Device* device = nullptr;
        if (FAILED(queue->GetDevice(IID_PPV_ARGS(&device))))
            return;
        HRESULT removed = device->GetDeviceRemovedReason();
        device->Release();
        if (FAILED(removed))
            return;
        if (g_directQueues.size() == kMaxDirectQueues)
        {
            g_directQueues.front().queue->Release();
            g_directQueues.erase(g_directQueues.begin());
        }
        queue->AddRef();
        g_directQueues.push_back({queue});
    }

    bool OnRemovedDevice(ID3D12CommandQueue* queue)
    {
        ID3D12Device* device = nullptr;
        if (FAILED(queue->GetDevice(IID_PPV_ARGS(&device))))
            return true;
        HRESULT removed = device->GetDeviceRemovedReason();
        device->Release();
        return FAILED(removed);
    }

    template <typename Entry>
    bool ReleaseRemoved(std::vector<Entry>& entries)
    {
        bool released = false;
        for (size_t i = 0; i < entries.size();)
        {
            if (OnRemovedDevice(entries[i].queue))
            {
                entries[i].queue->Release();
                entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(i));
                released = true;
                continue;
            }
            ++i;
        }
        return released;
    }

    // Queue `swapChain` presents on, with a reference. `known` is false for a
    // guess. Null when its device was removed: nothing is drawn on it.
    ID3D12CommandQueue* QueueFor(IDXGISwapChain* swapChain, bool& known)
    {
        IUnknown* identity = Identity(swapChain);
        ID3D12Device* device = nullptr;
        if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(&device))))
            return nullptr;
        const HRESULT removed = device->GetDeviceRemovedReason();
        if (FAILED(removed))
        {
            device->Release();
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(g_queueMutex);
        ID3D12CommandQueue* queue = nullptr;
        known = false;
        for (const SwapChainQueue& entry : g_swapChainQueues)
        {
            if (entry.swapChain == identity)
            {
                queue = entry.queue;
                known = true;
            }
        }
        for (const DirectQueue& entry : g_directQueues)
        {
            if (known)
                break;
            // Query the device through the queue, which keeps it alive, instead
            // of comparing against a device pointer stored after its Release().
            ID3D12Device* queueDevice = nullptr;
            if (FAILED(entry.queue->GetDevice(IID_PPV_ARGS(&queueDevice))))
                continue;
            const bool sameDevice = queueDevice == device;
            queueDevice->Release();
            if (!sameDevice)
                continue;
            queue = entry.queue;
            if (queue == LastQueueOfThread())
                break;
        }
        device->Release();
        if (queue)
            queue->AddRef();
        return queue;
    }

    void WaitForGpu()
    {
        if (!g_fence || !g_queue || g_fence->GetCompletedValue() >= g_fenceValue)
            return;
        if (SUCCEEDED(g_fence->SetEventOnCompletion(g_fenceValue, g_fenceEvent)))
            WaitForSingleObject(g_fenceEvent, 2000);
    }

    void WaitForFrame(const FrameContext& frame)
    {
        if (g_fence->GetCompletedValue() >= frame.fenceValue)
            return;
        if (SUCCEEDED(g_fence->SetEventOnCompletion(frame.fenceValue, g_fenceEvent)))
            WaitForSingleObject(g_fenceEvent, 2000);
    }

    void ReleaseFrames()
    {
        for (FrameContext& frame : g_frames)
            Core::Release(frame.allocator);
        g_frames.clear();
        Core::Release(g_targetHeap);
    }

    void AllocateSrv(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
    {
        UINT index = g_freeSrvs.empty() ? 0 : g_freeSrvs.back();
        if (!g_freeSrvs.empty())
            g_freeSrvs.pop_back();
        UINT increment = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        *cpu = g_srvHeap->GetCPUDescriptorHandleForHeapStart();
        *gpu = g_srvHeap->GetGPUDescriptorHandleForHeapStart();
        cpu->ptr += static_cast<SIZE_T>(index) * increment;
        gpu->ptr += static_cast<UINT64>(index) * increment;
    }

    void FreeSrv(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE)
    {
        UINT increment = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        SIZE_T index = (cpu.ptr - g_srvHeap->GetCPUDescriptorHandleForHeapStart().ptr) / increment;
        g_freeSrvs.push_back(static_cast<UINT>(index));
    }

    // Releases the renderer, the ImGui context and all device resources.
    // Runs on the rendering thread, from ServiceTeardown() or after a device
    // removal; idempotent.
    void Teardown()
    {
        if (!g_device)
            return;
        Logger::Debug("[DX12] Teardown: releasing the overlay");
        WaitForGpu();
        if (g_imguiReady)
            ImGui_ImplDX12_Shutdown();
        g_imguiReady = false;
        Menu::Shutdown();
        ReleaseFrames();
        Core::Release(g_srvHeap);
        Core::Release(g_commandList);
        Core::Release(g_fence);
        if (g_fenceEvent)
        {
            CloseHandle(g_fenceEvent);
            g_fenceEvent = nullptr;
        }
        Core::Release(g_queue);
        Core::Release(g_device);
        g_swapChain = nullptr;
        g_boundWindow = nullptr;
        g_format = DXGI_FORMAT_UNKNOWN;
        g_bufferCount = 0;
        g_queueMisses = 0;
    }

    bool CreateFrames(IDXGISwapChain* swapChain, const DXGI_SWAP_CHAIN_DESC& desc)
    {
        // The overlay may move from another swap chain whose frames are still in flight.
        if (!g_frames.empty())
            WaitForGpu();
        ReleaseFrames();

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDesc.NumDescriptors = desc.BufferCount;
        if (FAILED(g_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&g_targetHeap))))
            return false;

        UINT increment = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE target = g_targetHeap->GetCPUDescriptorHandleForHeapStart();
        g_frames.resize(desc.BufferCount);
        for (FrameContext& frame : g_frames)
        {
            if (FAILED(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                        IID_PPV_ARGS(&frame.allocator))))
                return false;
            frame.target = target;
            target.ptr += increment;
        }

        if (!g_commandList)
        {
            if (FAILED(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_frames[0].allocator, nullptr,
                                                   IID_PPV_ARGS(&g_commandList))))
                return false;
            g_commandList->Close();
        }
        g_swapChain = swapChain;
        g_bufferCount = desc.BufferCount;
        g_framesDirty = false;
        return true;
    }

    bool Init(IDXGISwapChain* swapChain, ID3D12CommandQueue* queue)
    {
        DXGI_SWAP_CHAIN_DESC desc{};
        ID3D12Device* device = nullptr;
        if (FAILED(swapChain->GetDesc(&desc)) || FAILED(swapChain->GetDevice(IID_PPV_ARGS(&device))))
            return false;

        DXGI_FORMAT format = Dxgi::RenderTargetFormat(desc.BufferDesc.Format);
        if (g_device && (device != g_device || queue != g_queue || format != g_format || desc.BufferCount != g_bufferCount))
            Teardown();

        HWND window = Dxgi::SwapChainWindow(swapChain, desc);
        if (!Menu::Initialize(window))
        {
            device->Release();
            return false;
        }

        if (!g_device)
        {
            g_device = device;
            g_queue = queue;
            g_queue->AddRef();
            g_format = format;
            g_boundWindow = window;

            D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.NumDescriptors = kSrvDescriptors;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            g_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (FAILED(g_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&g_srvHeap))) ||
                FAILED(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence))) || !g_fenceEvent ||
                !CreateFrames(swapChain, desc))
            {
                Logger::Error("[DX12] Init: resource creation failed");
                Teardown();
                return false;
            }
            g_freeSrvs.clear();
            for (UINT index = kSrvDescriptors; index-- > 0;)
                g_freeSrvs.push_back(index);
            g_fenceValue = 0;

            ImGui_ImplDX12_InitInfo info{};
            info.Device = g_device;
            info.CommandQueue = g_queue;
            info.NumFramesInFlight = static_cast<int>(desc.BufferCount);
            info.RTVFormat = g_format;
            info.DSVFormat = DXGI_FORMAT_UNKNOWN;
            info.SrvDescriptorHeap = g_srvHeap;
            info.SrvDescriptorAllocFn = AllocateSrv;
            info.SrvDescriptorFreeFn = FreeSrv;
            g_imguiReady = ImGui_ImplDX12_Init(&info);
            if (!g_imguiReady)
                Teardown();
            return g_imguiReady;
        }

        device->Release();
        if (!CreateFrames(swapChain, desc))
        {
            Logger::Error("[DX12] Init: frame resource creation failed");
            Teardown();
            return false;
        }
        return true;
    }

    // ImGui frame: renderer new frame, menu between NewFrame() and Render()
    // under the input lock, then the draw data to render.
    ImDrawData* Frame()
    {
        InputLock::Scoped lock;
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        Menu::Render();

        ImGui::Render();
        return ImGui::GetDrawData();
    }

    void Render(IDXGISwapChain* swapChain)
    {
        // The swap chain may present while the hooks are still being installed.
        if (!g_executeCommandLists)
            return;

        bool known = false;
        ID3D12CommandQueue* queue = QueueFor(swapChain, known);
        if (!queue)
            return;
        if (!known && swapChain == g_swapChain && queue != g_queue && ++g_queueMisses < kQueueSwitchFrames)
        {
            queue->Release();
            queue = g_queue;
            queue->AddRef();
        }
        else
        {
            g_queueMisses = 0;
        }
        bool ok = true;
        if (swapChain != g_swapChain || queue != g_queue || g_framesDirty)
            ok = Init(swapChain, queue);
        queue->Release();
        if (!ok)
            return;

        IDXGISwapChain3* swapChain3 = nullptr;
        if (FAILED(swapChain->QueryInterface(IID_PPV_ARGS(&swapChain3))))
            return;
        UINT index = swapChain3->GetCurrentBackBufferIndex();
        swapChain3->Release();
        if (index >= g_frames.size())
            return;

        // Before any reference to the back buffer, which an exception in the frame would leak.
        ImGui_ImplDX12_NewFrame();
        ImDrawData* drawData = Frame();
        if (Dxgi::StoresLinear(g_format))
            ImGuiColors::LinearizeColors(drawData);

        ID3D12Resource* buffer = nullptr;
        if (FAILED(swapChain->GetBuffer(index, IID_PPV_ARGS(&buffer))))
            return;

        FrameContext& frame = g_frames[index];
        WaitForFrame(frame);
        if (FAILED(frame.allocator->Reset()) || FAILED(g_commandList->Reset(frame.allocator, nullptr)))
        {
            buffer->Release();
            return;
        }

        D3D12_RENDER_TARGET_VIEW_DESC viewDesc{};
        viewDesc.Format = g_format;
        viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        g_device->CreateRenderTargetView(buffer, &viewDesc, frame.target);

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = buffer;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        g_commandList->ResourceBarrier(1, &barrier);
        g_commandList->OMSetRenderTargets(1, &frame.target, FALSE, nullptr);
        g_commandList->SetDescriptorHeaps(1, &g_srvHeap);
        // A fault in the renderer must not leave the back buffer referenced,
        // and its commands are not run.
        if (!Core::Guarded([&] { ImGui_ImplDX12_RenderDrawData(drawData, g_commandList); }))
        {
            g_commandList->Close();
            buffer->Release();
            return;
        }

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        g_commandList->ResourceBarrier(1, &barrier);
        g_commandList->Close();

        ID3D12CommandList* lists[] = {g_commandList};
        g_executeCommandLists(g_queue, 1, lists);
        g_queue->Signal(g_fence, ++g_fenceValue);
        frame.fenceValue = g_fenceValue;
        buffer->Release();
    }

    void OnPresent(IDXGISwapChain* swapChain, UINT flags)
    {
        if (flags & DXGI_PRESENT_TEST)
            return;
        if (Core::Faulted())
            return;
        // The window of the bound swap chain is gone: wait for the new one.
        if (g_boundWindow && !IsWindow(g_boundWindow))
            return;

        // Several swap chains may present, from one thread or more: the first
        // one that presents here draws the overlay.
        if (!g_frameMutex.try_lock())
            return;
        Core::Guarded([&] { Render(swapChain); });
        Core::ServiceTeardown(&Teardown);
        g_frameMutex.unlock();
    }

    // The application answers a removed device with a new one: release our
    // references before they see the result of the present.
    HRESULT AfterPresent(HRESULT result)
    {
        if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET)
        {
            static std::atomic<bool> g_logged{false};
            if (!g_logged.exchange(true))
                Logger::Error("[DX12] Present: device removed (0x{:08X}), releasing the overlay",
                              static_cast<unsigned long>(result));
            Core::Guarded(&Teardown);
            Core::Guarded([] {
                std::lock_guard<std::mutex> lock(g_queueMutex);
                bool swapChains = ReleaseRemoved(g_swapChainQueues);
                // A new queue may reuse the address of a released one in the
                // per-thread cache.
                if (ReleaseRemoved(g_directQueues) || swapChains)
                    ++g_session;
            });
        }
        else
        {
            return result;
        }
        return result;
    }

    HRESULT STDMETHODCALLTYPE HookPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
    {
        Core::ScopedCall call;
        if (!InputHook::shutting_down.load())
            Core::Guarded([&] { OnPresent(swapChain, flags); });
        else
            Core::ServiceTeardown(&Teardown);
        return AfterPresent(oPresent(swapChain, syncInterval, flags));
    }

    HRESULT STDMETHODCALLTYPE HookPresent1(IDXGISwapChain1* swapChain, UINT syncInterval, UINT flags,
                                           const DXGI_PRESENT_PARAMETERS* params)
    {
        Core::ScopedCall call;
        if (!InputHook::shutting_down.load())
            Core::Guarded([&] { OnPresent(swapChain, flags); });
        else
            Core::ServiceTeardown(&Teardown);
        return AfterPresent(oPresent1(swapChain, syncInterval, flags, params));
    }

    void PreResize(IDXGISwapChain* swapChain)
    {
        if (swapChain != g_swapChain)
            return;
        WaitForGpu();
        g_framesDirty = true;
    }

    HRESULT STDMETHODCALLTYPE HookResizeBuffers(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height,
                                                DXGI_FORMAT newFormat, UINT flags)
    {
        Core::ScopedCall call;
        if (!InputHook::shutting_down.load())
            Core::Guarded([&] { PreResize(swapChain); });
        return oResizeBuffers(swapChain, bufferCount, width, height, newFormat, flags);
    }

    HRESULT STDMETHODCALLTYPE HookResizeBuffers1(IDXGISwapChain3* swapChain, UINT bufferCount, UINT width, UINT height,
                                                 DXGI_FORMAT newFormat, UINT flags, const UINT* nodeMasks,
                                                 IUnknown* const* queues)
    {
        Core::ScopedCall call;
        if (!InputHook::shutting_down.load())
            Core::Guarded([&] { PreResize(swapChain); });
        return oResizeBuffers1(swapChain, bufferCount, width, height, newFormat, flags, nodeMasks, queues);
    }

    void STDMETHODCALLTYPE HookExecuteCommandLists(ID3D12CommandQueue* queue, UINT count,
                                                   ID3D12CommandList* const* lists)
    {
        Core::ScopedCall call;
        if (!InputHook::shutting_down.load() && count && lists && lists[0] &&
            lists[0]->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT)
            Core::Guarded([&] { RecordDirectQueue(queue); });
        g_executeCommandLists(queue, count, lists);
    }

    HRESULT STDMETHODCALLTYPE HookCreateSwapChain(IDXGIFactory* factory, IUnknown* device, DXGI_SWAP_CHAIN_DESC* desc,
                                                  IDXGISwapChain** swapChain)
    {
        Core::ScopedCall call;
        HRESULT hr = g_createSwapChain(factory, device, desc, swapChain);
        if (SUCCEEDED(hr) && swapChain && !InputHook::shutting_down.load())
            Core::Guarded([&] { RecordSwapChain(device, *swapChain); });
        return hr;
    }

    HRESULT STDMETHODCALLTYPE HookCreateSwapChainForHwnd(IDXGIFactory2* factory, IUnknown* device, HWND window,
                                                         const DXGI_SWAP_CHAIN_DESC1* desc,
                                                         const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen,
                                                         IDXGIOutput* output, IDXGISwapChain1** swapChain)
    {
        Core::ScopedCall call;
        HRESULT hr = g_createSwapChainForHwnd(factory, device, window, desc, fullscreen, output, swapChain);
        if (SUCCEEDED(hr) && swapChain && !InputHook::shutting_down.load())
            Core::Guarded([&] { RecordSwapChain(device, *swapChain); });
        return hr;
    }

    HRESULT STDMETHODCALLTYPE HookCreateSwapChainForCoreWindow(IDXGIFactory2* factory, IUnknown* device, IUnknown* window,
                                                               const DXGI_SWAP_CHAIN_DESC1* desc, IDXGIOutput* output,
                                                               IDXGISwapChain1** swapChain)
    {
        Core::ScopedCall call;
        HRESULT hr = g_createSwapChainForCoreWindow(factory, device, window, desc, output, swapChain);
        if (SUCCEEDED(hr) && swapChain && !InputHook::shutting_down.load())
            Core::Guarded([&] { RecordSwapChain(device, *swapChain); });
        return hr;
    }

    HRESULT STDMETHODCALLTYPE HookCreateSwapChainForComposition(IDXGIFactory2* factory, IUnknown* device,
                                                                const DXGI_SWAP_CHAIN_DESC1* desc, IDXGIOutput* output,
                                                                IDXGISwapChain1** swapChain)
    {
        Core::ScopedCall call;
        HRESULT hr = g_createSwapChainForComposition(factory, device, desc, output, swapChain);
        if (SUCCEEDED(hr) && swapChain && !InputHook::shutting_down.load())
            Core::Guarded([&] { RecordSwapChain(device, *swapChain); });
        return hr;
    }

    bool InstallHook(const char* name, void* target, void* detour, void** original)
    {
        if (!HookHelper::InstallHook(name, target, detour, original))
            return false;
        g_installedHooks.push_back({name, target});
        return true;
    }

    void HookFactory()
    {
        auto createFactory = Core::GetProc<CreateDXGIFactory1Fn>(g_dxgiModule, "CreateDXGIFactory1");
        IDXGIFactory2* factory = nullptr;
        if (!createFactory || FAILED(createFactory(IID_PPV_ARGS(&factory))))
        {
            Logger::Error("[DX12] Install: DXGI factory creation failed, swap chain queues will be guessed");
            return;
        }
        InstallHook("IDXGIFactory::CreateSwapChain", Core::VirtualMethod(factory, kCreateSwapChainIndex),
                    reinterpret_cast<void*>(&HookCreateSwapChain), reinterpret_cast<void**>(&g_createSwapChain));
        InstallHook("IDXGIFactory2::CreateSwapChainForHwnd",
                    Core::VirtualMethod(factory, kCreateSwapChainForHwndIndex),
                    reinterpret_cast<void*>(&HookCreateSwapChainForHwnd),
                    reinterpret_cast<void**>(&g_createSwapChainForHwnd));
        InstallHook("IDXGIFactory2::CreateSwapChainForCoreWindow",
                    Core::VirtualMethod(factory, kCreateSwapChainForCoreWindowIndex),
                    reinterpret_cast<void*>(&HookCreateSwapChainForCoreWindow),
                    reinterpret_cast<void**>(&g_createSwapChainForCoreWindow));
        InstallHook("IDXGIFactory2::CreateSwapChainForComposition",
                    Core::VirtualMethod(factory, kCreateSwapChainForCompositionIndex),
                    reinterpret_cast<void*>(&HookCreateSwapChainForComposition),
                    reinterpret_cast<void**>(&g_createSwapChainForComposition));
        factory->Release();
    }

    ID3D12Device* CreateDummyDevice()
    {
        auto createDevice = Core::GetProc<D3D12CreateDeviceFn>(g_module, "D3D12CreateDevice");
        if (!createDevice)
            return nullptr;

        ID3D12Device* device = nullptr;
        if (SUCCEEDED(createDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
            return device;

        auto createFactory = Core::GetProc<CreateDXGIFactory1Fn>(g_dxgiModule, "CreateDXGIFactory1");
        IDXGIFactory4* factory = nullptr;
        if (!createFactory || FAILED(createFactory(IID_PPV_ARGS(&factory))))
            return nullptr;
        IDXGIAdapter* warp = nullptr;
        if (SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))))
        {
            createDevice(warp, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
            warp->Release();
        }
        factory->Release();
        return device;
    }

    // The Present/ResizeBuffers hooks come from the DXGI swap chain vtable of
    // a dummy D3D12 swap chain; the queue tracking comes from the command
    // queue and the swap chain factories.
    bool InstallPresentHooks(ID3D12Device* device)
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        ID3D12CommandQueue* queue = nullptr;
        if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue))))
        {
            Logger::Error("[DX12] Install: dummy command queue creation failed");
            return false;
        }
        auto createFactory = Core::GetProc<CreateDXGIFactory1Fn>(g_dxgiModule, "CreateDXGIFactory1");
        IDXGIFactory2* factory = nullptr;
        if (!createFactory || FAILED(createFactory(IID_PPV_ARGS(&factory))))
        {
            Logger::Error("[DX12] Install: DXGI factory creation failed");
            queue->Release();
            return false;
        }

        HWND window = Core::CreateHiddenWindow();
        DXGI_SWAP_CHAIN_DESC desc{};
        // D3D12 only supports the flip model: bitblt (DISCARD) is rejected
        // with DXGI_ERROR_INVALID_CALL.
        desc.BufferCount = 2;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = window;
        desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        IDXGISwapChain* swapChain = nullptr;
        HRESULT hr = factory->CreateSwapChain(queue, &desc, &swapChain);

        bool hooked = false;
        if (SUCCEEDED(hr))
        {
            hooked |= InstallHook("IDXGISwapChain::Present", Core::VirtualMethod(swapChain, Dxgi::kPresentIndex),
                                  reinterpret_cast<void*>(&HookPresent), reinterpret_cast<void**>(&oPresent));
            hooked |= InstallHook("IDXGISwapChain::ResizeBuffers",
                                  Core::VirtualMethod(swapChain, Dxgi::kResizeBuffersIndex),
                                  reinterpret_cast<void*>(&HookResizeBuffers),
                                  reinterpret_cast<void**>(&oResizeBuffers));

            IDXGISwapChain1* swapChain1 = nullptr;
            if (SUCCEEDED(swapChain->QueryInterface(IID_PPV_ARGS(&swapChain1))))
            {
                hooked |= InstallHook("IDXGISwapChain1::Present1",
                                      Core::VirtualMethod(swapChain1, Dxgi::kPresent1Index),
                                      reinterpret_cast<void*>(&HookPresent1), reinterpret_cast<void**>(&oPresent1));
                swapChain1->Release();
            }
            IDXGISwapChain3* swapChain3 = nullptr;
            if (SUCCEEDED(swapChain->QueryInterface(IID_PPV_ARGS(&swapChain3))))
            {
                hooked |= InstallHook("IDXGISwapChain3::ResizeBuffers1",
                                      Core::VirtualMethod(swapChain3, Dxgi::kResizeBuffers1Index),
                                      reinterpret_cast<void*>(&HookResizeBuffers1),
                                      reinterpret_cast<void**>(&oResizeBuffers1));
                swapChain3->Release();
            }
            Core::Release(swapChain);
        }
        else
        {
            Logger::Error("[DX12] Install: dummy swap chain creation failed (0x{:08X})",
                          static_cast<unsigned long>(hr));
        }

        if (window)
            DestroyWindow(window);
        factory->Release();
        queue->Release();
        return hooked;
    }

} // namespace

bool DX12_Backend::Initialize()
{
    Logger::Debug("[DX12] Install: starting");

    Core::ResetFault();
    InputHook::shutting_down.store(false);

    g_module = LoadLibraryW(L"d3d12.dll");
    g_dxgiModule = LoadLibraryW(L"dxgi.dll");
    if (!g_module || !g_dxgiModule)
    {
        Logger::Error("[DX12] Install: d3d12.dll or dxgi.dll not loadable");
        return false;
    }

    HookFactory();
    ID3D12Device* device = CreateDummyDevice();
    if (!device)
    {
        Logger::Error("[DX12] Install: dummy device creation failed");
        return false;
    }

    bool hooked = InstallPresentHooks(device);

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    ID3D12CommandQueue* queue = nullptr;
    if (SUCCEEDED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue))))
    {
        hooked |= InstallHook("ID3D12CommandQueue::ExecuteCommandLists",
                              Core::VirtualMethod(queue, kExecuteCommandListsIndex),
                              reinterpret_cast<void*>(&HookExecuteCommandLists),
                              reinterpret_cast<void**>(&g_executeCommandLists));
        queue->Release();
    }
    device->Release();

    if (!hooked)
    {
        Logger::Critical("[DX12] Install: no hook installed");
        // A hook that did install must not outlive this module.
        HookHelper::UninstallAll(g_installedHooks);
        for (HMODULE* module : {&g_module, &g_dxgiModule})
        {
            if (*module)
            {
                FreeLibrary(*module);
                *module = nullptr;
            }
        }
        return false;
    }

    Logger::Debug("[DX12] Install: {} hook(s) active", g_installedHooks.size());
    return true;
}

void DX12_Backend::Shutdown()
{
    static std::atomic<bool> g_shutdown{false};
    if (g_shutdown.exchange(true))
        return;
    Logger::Debug("[DX12] Shutdown: stopping");

    if (!g_installedHooks.empty())
    {
        // The GPU resources go first, on the rendering thread, at its next
        // present; then the hooks, once none of them is running.
        Core::TeardownOnRenderThread(&Teardown, 1000);
        for (const Hook& hook : g_installedHooks)
            HookHelper::DisableHook(hook.name, hook.target);
        if (!Core::WaitForIdleHooks(2000))
        {
            // A detour is still running: removing its trampoline would free
            // code a thread is executing. Leave the hooks disabled; the unload
            // path keeps the library loaded until they drain.
            Logger::Error("[DX12] Shutdown: hooks still running, left disabled");
            return;
        }
        for (const Hook& hook : g_installedHooks)
            HookHelper::UninstallHook(hook.name, hook.target);
        g_installedHooks.clear();
    }

    // The rendering thread did not answer (the game stopped presenting, say):
    // release inline now that no hook can run.
    if (Core::TeardownPending() || g_device)
        Core::Guarded(&Teardown);
    Core::ResetTeardown();

    {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        for (SwapChainQueue& entry : g_swapChainQueues)
            entry.queue->Release();
        for (DirectQueue& entry : g_directQueues)
            entry.queue->Release();
        g_swapChainQueues.clear();
        g_directQueues.clear();
        ++g_session;
    }
    g_executeCommandLists = nullptr;
    g_createSwapChain = nullptr;
    g_createSwapChainForHwnd = nullptr;
    g_createSwapChainForCoreWindow = nullptr;
    g_createSwapChainForComposition = nullptr;
    for (HMODULE* module : {&g_module, &g_dxgiModule})
    {
        if (*module)
        {
            FreeLibrary(*module);
            *module = nullptr;
        }
    }

    Logger::Debug("[DX12] Shutdown: done");
}
