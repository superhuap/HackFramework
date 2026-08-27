//
// Created by superhuap on 2026/8/25.
//

#include "backend/DX12_Backend.h"
#include "backend/MinHookHelper.h"

#include <d3d12.h>
#include <dxgi1_4.h>

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include "menu/Menu.h"
#include "utils/DxgiFormat.h"
#include "utils/InputHook.h"
#include "utils/Logger.h"

// DX12 后端不支持 Windows 7 及更早系统
namespace
{

    struct DescriptorHeapAllocator
    {
        ID3D12DescriptorHeap* Heap = nullptr;
        D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
        D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu = {};
        D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu = {};
        UINT HeapHandleIncrement = 0;
        ImVector<int> FreeIndices;

        void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
        {
            IM_ASSERT(Heap == nullptr && FreeIndices.empty());
            Heap = heap;
            const D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
            HeapType = desc.Type;
            HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
            HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
            HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
            FreeIndices.reserve(static_cast<int>(desc.NumDescriptors));
            for (int n = static_cast<int>(desc.NumDescriptors); n > 0; n--)
                FreeIndices.push_back(n - 1);
        }

        void Destroy()
        {
            Heap = nullptr;
            FreeIndices.clear();
        }

        void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* outCpuDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuDescHandle)
        {
            IM_ASSERT(FreeIndices.Size > 0);
            const int idx = FreeIndices.back();
            FreeIndices.pop_back();
            outCpuDescHandle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
            outGpuDescHandle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
        }

        void Free(D3D12_CPU_DESCRIPTOR_HANDLE outCpuDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE outGpuDescHandle)
        {
            const int cpuIdx = static_cast<int>((outCpuDescHandle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
            const int gpuIdx = static_cast<int>((outGpuDescHandle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
            IM_ASSERT(cpuIdx == gpuIdx);
            FreeIndices.push_back(cpuIdx);
        }
    };

    constexpr int DUMMY_BUFFER_COUNT = 3;
    constexpr UINT MAX_BACK_BUFFERS = 8;

    IDXGIFactory4* g_factory = nullptr;
    ID3D12Device* g_device = nullptr;
    ID3D12DescriptorHeap* g_rtvDescHeap = nullptr;
    ID3D12DescriptorHeap* g_srvDescHeap = nullptr;
    ID3D12CommandQueue* g_commandQueue = nullptr;
    ID3D12GraphicsCommandList* g_commandList = nullptr;
    ID3D12Fence* g_fence = nullptr;
    HANDLE g_fenceEvent = nullptr;
    UINT64 g_fenceValues[MAX_BACK_BUFFERS] = {};
    IDXGISwapChain3* g_swapChain = nullptr;
    ID3D12CommandAllocator* g_commandAllocators[MAX_BACK_BUFFERS] = {};
    ID3D12Resource* g_renderTargetResource[MAX_BACK_BUFFERS] = {};
    D3D12_CPU_DESCRIPTOR_HANDLE g_renderTargetDescriptor[MAX_BACK_BUFFERS] = {};
    DescriptorHeapAllocator g_srvHeapAlloc;

    void CleanupRenderTarget()
    {
        for (UINT i = 0; i < MAX_BACK_BUFFERS; ++i)
        {
            if (g_renderTargetResource[i])
            {
                g_renderTargetResource[i]->Release();
                g_renderTargetResource[i] = nullptr;
            }
        }
    }

    void CleanupDeviceD3D12()
    {
        CleanupRenderTarget();

        if (g_swapChain)
        {
            g_swapChain->Release();
            g_swapChain = nullptr;
        }
        for (UINT i = 0; i < MAX_BACK_BUFFERS; ++i)
        {
            if (g_commandAllocators[i])
            {
                g_commandAllocators[i]->Release();
                g_commandAllocators[i] = nullptr;
            }
        }
        if (g_commandList)
        {
            g_commandList->Release();
            g_commandList = nullptr;
        }
        if (g_fence)
        {
            g_fence->Release();
            g_fence = nullptr;
        }
        if (g_fenceEvent)
        {
            CloseHandle(g_fenceEvent);
            g_fenceEvent = nullptr;
        }
        if (g_rtvDescHeap)
        {
            g_rtvDescHeap->Release();
            g_rtvDescHeap = nullptr;
        }
        if (g_srvDescHeap)
        {
            g_srvDescHeap->Release();
            g_srvDescHeap = nullptr;
        }
        if (g_device)
        {
            g_device->Release();
            g_device = nullptr;
        }
        if (g_factory)
        {
            g_factory->Release();
            g_factory = nullptr;
        }
    }

    bool CreateDeviceD3D12(HWND hWnd)
    {
        DXGI_SWAP_CHAIN_DESC1 sd = {};
        sd.BufferCount = DUMMY_BUFFER_COUNT;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SampleDesc.Count = 1;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
        if (D3D12CreateDevice(nullptr, featureLevel, IID_PPV_ARGS(&g_device)) != S_OK)
            return false;

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        if (g_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_commandQueue)) != S_OK)
        {
            CleanupDeviceD3D12();
            return false;
        }

        IDXGISwapChain1* swapChain1 = nullptr;
        if (CreateDXGIFactory1(IID_PPV_ARGS(&g_factory)) != S_OK)
        {
            CleanupDeviceD3D12();
            return false;
        }
        if (g_factory->CreateSwapChainForHwnd(g_commandQueue, hWnd, &sd, nullptr, nullptr, &swapChain1) != S_OK)
        {
            CleanupDeviceD3D12();
            return false;
        }
        if (swapChain1->QueryInterface(IID_PPV_ARGS(&g_swapChain)) != S_OK)
        {
            swapChain1->Release();
            CleanupDeviceD3D12();
            return false;
        }
        swapChain1->Release();

        return true;
    }

    bool EnsureRtvDescriptorHeap()
    {
        if (g_rtvDescHeap)
            return true;

        D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
        rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvDesc.NumDescriptors = MAX_BACK_BUFFERS;
        rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        rtvDesc.NodeMask = 1;
        if (g_device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&g_rtvDescHeap)) != S_OK)
            return false;

        const SIZE_T rtvDescriptorSize =
            g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < MAX_BACK_BUFFERS; ++i)
        {
            g_renderTargetDescriptor[i] = rtvHandle;
            rtvHandle.ptr += rtvDescriptorSize;
        }

        return true;
    }

    void CreateRenderTarget(IDXGISwapChain3* pSwapChain)
    {
        if (!EnsureRtvDescriptorHeap())
            return;

        DXGI_SWAP_CHAIN_DESC sd;
        pSwapChain->GetDesc(&sd);
        const UINT bufferCount = sd.BufferCount < MAX_BACK_BUFFERS ? sd.BufferCount : MAX_BACK_BUFFERS;

        for (UINT i = 0; i < bufferCount; ++i)
        {
            if (g_renderTargetResource[i])
            {
                g_renderTargetResource[i]->Release();
                g_renderTargetResource[i] = nullptr;
            }

            ID3D12Resource* pBackBuffer = nullptr;
            if (SUCCEEDED(pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer))))
            {
                D3D12_RENDER_TARGET_VIEW_DESC desc = {};
                desc.Format = static_cast<DXGI_FORMAT>(Utils::GetCorrectDXGIFormat(sd.BufferDesc.Format));
                desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

                g_device->CreateRenderTargetView(pBackBuffer, &desc, g_renderTargetDescriptor[i]);
                g_renderTargetResource[i] = pBackBuffer;
            }
        }
    }

    void WaitForFrameCompletion(UINT frameIndex)
    {
        if (!g_fence || !g_commandQueue)
            return;
        if (g_fence->GetCompletedValue() < g_fenceValues[frameIndex])
        {
            g_fence->SetEventOnCompletion(g_fenceValues[frameIndex], g_fenceEvent);
            WaitForSingleObject(g_fenceEvent, INFINITE);
        }
    }

    void RenderImGui_DX12(IDXGISwapChain3* pSwapChain)
    {
        if (!g_commandQueue)
            return;

        if (!ImGui::GetIO().BackendRendererUserData)
        {
            if (SUCCEEDED(pSwapChain->GetDevice(IID_PPV_ARGS(&g_device))))
            {
                if (!EnsureRtvDescriptorHeap())
                    return;

                D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
                srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                srvDesc.NumDescriptors = 64;
                srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                if (g_device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&g_srvDescHeap)) != S_OK)
                    return;
                g_srvHeapAlloc.Create(g_device, g_srvDescHeap);

                for (UINT i = 0; i < MAX_BACK_BUFFERS; ++i)
                {
                    if (g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                         IID_PPV_ARGS(&g_commandAllocators[i])) != S_OK)
                        return;
                }

                if (g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandAllocators[0], nullptr,
                                                IID_PPV_ARGS(&g_commandList)) != S_OK ||
                    g_commandList->Close() != S_OK)
                    return;

                if (g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)) != S_OK)
                    return;
                g_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
                if (!g_fenceEvent)
                    return;

                ImGui_ImplDX12_InitInfo init_info = {};
                init_info.Device = g_device;
                init_info.CommandQueue = g_commandQueue;
                init_info.NumFramesInFlight = DUMMY_BUFFER_COUNT;
                init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                init_info.SrvDescriptorHeap = g_srvDescHeap;
                init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu,
                                                    D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu) {
                    g_srvHeapAlloc.Alloc(out_cpu, out_gpu);
                };
                init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu,
                                                   D3D12_GPU_DESCRIPTOR_HANDLE gpu) { g_srvHeapAlloc.Free(cpu, gpu); };

                if (!ImGui_ImplDX12_Init(&init_info))
                    return;
            }
        }

        if (Utils::Input::shutting_down.load())
            return;

        if (!g_renderTargetResource[0])
            CreateRenderTarget(pSwapChain);

        if (ImGui::GetCurrentContext() && g_commandQueue && g_renderTargetResource[0])
        {
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            Menu::Render();

            ImGui::Render();

            const UINT backBufferIdx = pSwapChain->GetCurrentBackBufferIndex();
            WaitForFrameCompletion(backBufferIdx);

            ID3D12CommandAllocator* commandAllocator = g_commandAllocators[backBufferIdx];
            commandAllocator->Reset();

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = g_renderTargetResource[backBufferIdx];
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            g_commandList->Reset(commandAllocator, nullptr);
            g_commandList->ResourceBarrier(1, &barrier);

            g_commandList->OMSetRenderTargets(1, &g_renderTargetDescriptor[backBufferIdx], FALSE, nullptr);
            g_commandList->SetDescriptorHeaps(1, &g_srvDescHeap);
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_commandList);
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            g_commandList->ResourceBarrier(1, &barrier);
            g_commandList->Close();

            g_commandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&g_commandList));

            ++g_fenceValues[backBufferIdx];
            g_commandQueue->Signal(g_fence, g_fenceValues[backBufferIdx]);
        }
    }

    HRESULT(WINAPI * oPresent)(IDXGISwapChain3*, UINT, UINT) = nullptr;
    HRESULT WINAPI Hook_Present(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags)
    {
        RenderImGui_DX12(pSwapChain);
        return oPresent(pSwapChain, SyncInterval, Flags);
    }

    HRESULT(WINAPI * oPresent1)(IDXGISwapChain3*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*) = nullptr;
    HRESULT WINAPI Hook_Present1(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT PresentFlags,
                                 const DXGI_PRESENT_PARAMETERS* pPresentParameters)
    {
        RenderImGui_DX12(pSwapChain);
        return oPresent1(pSwapChain, SyncInterval, PresentFlags, pPresentParameters);
    }

    HRESULT(WINAPI * oResizeBuffers)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT) = nullptr;
    HRESULT WINAPI Hook_ResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height,
                                      DXGI_FORMAT NewFormat, UINT SwapChainFlags)
    {
        CleanupRenderTarget();
        return oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    }

    HRESULT(WINAPI * oResizeBuffers1)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT*,
                                      IUnknown* const*) = nullptr;
    HRESULT WINAPI Hook_ResizeBuffers1(IDXGISwapChain3* pSwapChain, UINT BufferCount, UINT Width, UINT Height,
                                       DXGI_FORMAT NewFormat, UINT SwapChainFlags, const UINT* pCreationNodeMask,
                                       IUnknown* const* ppPresentQueue)
    {
        CleanupRenderTarget();
        return oResizeBuffers1(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags,
                               pCreationNodeMask, ppPresentQueue);
    }

    void(WINAPI * oExecuteCommandLists)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*) = nullptr;
    void WINAPI Hook_ExecuteCommandLists(ID3D12CommandQueue* pCommandQueue, UINT NumCommandLists,
                                         ID3D12CommandList* const* ppCommandLists)
    {
        if (!g_commandQueue)
            g_commandQueue = pCommandQueue;

        oExecuteCommandLists(pCommandQueue, NumCommandLists, ppCommandLists);
    }

    HRESULT(WINAPI * oCreateSwapChain)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**) = nullptr;
    HRESULT WINAPI Hook_CreateSwapChain(IDXGIFactory* pFactory, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc,
                                        IDXGISwapChain** ppSwapChain)
    {
        CleanupRenderTarget();
        return oCreateSwapChain(pFactory, pDevice, pDesc, ppSwapChain);
    }

    HRESULT(WINAPI * oCreateSwapChainForHwnd)(IDXGIFactory*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*,
                                              const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*,
                                              IDXGISwapChain1**) = nullptr;
    HRESULT WINAPI Hook_CreateSwapChainForHwnd(IDXGIFactory* pFactory, IUnknown* pDevice, HWND hWnd,
                                               const DXGI_SWAP_CHAIN_DESC1* pDesc,
                                               const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
                                               IDXGIOutput* pRestrictOutput, IDXGISwapChain1** ppSwapChain)
    {
        CleanupRenderTarget();
        return oCreateSwapChainForHwnd(pFactory, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictOutput, ppSwapChain);
    }

    HRESULT(WINAPI * oCreateSwapChainForCoreWindow)(IDXGIFactory*, IUnknown*, IUnknown*,
                                                    const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*,
                                                    IDXGISwapChain1**) = nullptr;
    HRESULT WINAPI Hook_CreateSwapChainForCoreWindow(IDXGIFactory* pFactory, IUnknown* pDevice, IUnknown* pWindow,
                                                     const DXGI_SWAP_CHAIN_DESC1* pDesc, IDXGIOutput* pRestrictOutput,
                                                     IDXGISwapChain1** ppSwapChain)
    {
        CleanupRenderTarget();
        return oCreateSwapChainForCoreWindow(pFactory, pDevice, pWindow, pDesc, pRestrictOutput, ppSwapChain);
    }

    HRESULT(WINAPI * oCreateSwapChainForComposition)(IDXGIFactory*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*,
                                                     IDXGIOutput*, IDXGISwapChain1**) = nullptr;
    HRESULT WINAPI Hook_CreateSwapChainForComposition(IDXGIFactory* pFactory, IUnknown* pDevice,
                                                      const DXGI_SWAP_CHAIN_DESC1* pDesc, IDXGIOutput* pRestrictOutput,
                                                      IDXGISwapChain1** ppSwapChain)
    {
        CleanupRenderTarget();
        return oCreateSwapChainForComposition(pFactory, pDevice, pDesc, pRestrictOutput, ppSwapChain);
    }

} // namespace

bool DX12_Backend::Initialize(HWND hWnd)
{
    if (!Menu::Initialize(hWnd))
        return false;

    if (!CreateDeviceD3D12(GetConsoleWindow()))
    {
        LOG_WARN("CreateDeviceD3D12() failed.");
        return false;
    }

    LOG_INFO("DirectX12: g_pd3dDevice: {}", reinterpret_cast<void*>(g_device));
    LOG_INFO("DirectX12: g_dxgiFactory: {}", reinterpret_cast<void*>(g_factory));
    LOG_INFO("DirectX12: g_pd3dCommandQueue: {}", reinterpret_cast<void*>(g_commandQueue));
    LOG_INFO("DirectX12: g_pSwapChain: {}", reinterpret_cast<void*>(g_swapChain));

    void** vtable = *reinterpret_cast<void***>(g_swapChain);
    void** commandQueueVtable = *reinterpret_cast<void***>(g_commandQueue);
    void** factoryVtable = *reinterpret_cast<void***>(g_factory);

    void* fnCreateSwapChain = factoryVtable[10];
    void* fnCreateSwapChainForHwnd = factoryVtable[15];
    void* fnCreateSwapChainForCoreWindow = factoryVtable[16];
    void* fnCreateSwapChainForComposition = factoryVtable[24];

    void* fnPresent = vtable[8];
    void* fnPresent1 = vtable[22];
    void* fnResizeBuffers = vtable[13];
    void* fnResizeBuffers1 = vtable[39];
    void* fnExecuteCommandLists = commandQueueVtable[10];

    if (g_commandQueue)
    {
        g_commandQueue->Release();
        g_commandQueue = nullptr;
    }
    CleanupDeviceD3D12();

    bool ok = true;
    ok &= backend::CreateHookOnce(fnCreateSwapChain, &Hook_CreateSwapChain,
                                  reinterpret_cast<void**>(&oCreateSwapChain), "CreateSwapChain") != nullptr;
    ok &= backend::CreateHookOnce(fnCreateSwapChainForHwnd, &Hook_CreateSwapChainForHwnd,
                                  reinterpret_cast<void**>(&oCreateSwapChainForHwnd), "CreateSwapChainForHwnd") != nullptr;
    ok &= backend::CreateHookOnce(fnCreateSwapChainForCoreWindow, &Hook_CreateSwapChainForCoreWindow,
                                  reinterpret_cast<void**>(&oCreateSwapChainForCoreWindow),
                                  "CreateSwapChainForCoreWindow") != nullptr;
    ok &= backend::CreateHookOnce(fnCreateSwapChainForComposition, &Hook_CreateSwapChainForComposition,
                                  reinterpret_cast<void**>(&oCreateSwapChainForComposition),
                                  "CreateSwapChainForComposition") != nullptr;

    ok &= backend::CreateHookOnce(fnPresent, &Hook_Present, reinterpret_cast<void**>(&oPresent), "Present") != nullptr;
    ok &= backend::CreateHookOnce(fnPresent1, &Hook_Present1, reinterpret_cast<void**>(&oPresent1), "Present1") != nullptr;

    ok &= backend::CreateHookOnce(fnResizeBuffers, &Hook_ResizeBuffers,
                                  reinterpret_cast<void**>(&oResizeBuffers), "ResizeBuffers") != nullptr;
    ok &= backend::CreateHookOnce(fnResizeBuffers1, &Hook_ResizeBuffers1,
                                  reinterpret_cast<void**>(&oResizeBuffers1), "ResizeBuffers1") != nullptr;

    ok &= backend::CreateHookOnce(fnExecuteCommandLists, &Hook_ExecuteCommandLists,
                                  reinterpret_cast<void**>(&oExecuteCommandLists), "ExecuteCommandLists") != nullptr;

    ok &= backend::EnableHook(fnCreateSwapChain, "CreateSwapChain");
    ok &= backend::EnableHook(fnCreateSwapChainForHwnd, "CreateSwapChainForHwnd");
    ok &= backend::EnableHook(fnCreateSwapChainForCoreWindow, "CreateSwapChainForCoreWindow");
    ok &= backend::EnableHook(fnCreateSwapChainForComposition, "CreateSwapChainForComposition");

    ok &= backend::EnableHook(fnPresent, "Present");
    ok &= backend::EnableHook(fnPresent1, "Present1");

    ok &= backend::EnableHook(fnResizeBuffers, "ResizeBuffers");
    ok &= backend::EnableHook(fnResizeBuffers1, "ResizeBuffers1");

    ok &= backend::EnableHook(fnExecuteCommandLists, "ExecuteCommandLists");

    if (!ok)
        return false;

    return true;
}

void DX12_Backend::Shutdown()
{
    if (ImGui::GetCurrentContext())
    {
        if (ImGui::GetIO().BackendRendererUserData)
            ImGui_ImplDX12_Shutdown();
    }

    Menu::Shutdown();

    g_srvHeapAlloc.Destroy();
    CleanupDeviceD3D12();
}
