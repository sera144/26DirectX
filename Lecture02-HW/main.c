#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cstring>

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

// For movement
struct GameContext {
    float posX;
    float posY;
} gGame = { 0.0f, 0.0f };

struct CUSTOMVertex {
    float x, y, z;
    float r, g, b, a;
};

const char* shaderSource = R"(
struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f);
    output.col = input.col;
    return output;
}

float4 PS(PS_INPUT input) : SV_Target {
    return input.col;
}
)";

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

void Update()
{
    float speed = 0.01f;

    if (GetAsyncKeyState(VK_LEFT) & 0x8000)  gGame.posX -= speed;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) gGame.posX += speed;
    if (GetAsyncKeyState(VK_UP) & 0x8000)    gGame.posY += speed;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000)  gGame.posY -= speed;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // Window setup
    WNDCLASSEXW wc = { sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DX11Class";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    HWND hWnd = CreateWindowW(L"DX11Class", L"Moving Hexagram",
        WS_OVERLAPPEDWINDOW, 100, 100, 800, 600,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hWnd, nCmdShow);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 800;
    sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(
        NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
        NULL, 0, D3D11_SDK_VERSION,
        &sd, &g_pSwapChain, &g_pd3dDevice, NULL, &g_pImmediateContext
    );

    ID3D11Texture2D* backBuffer;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    g_pd3dDevice->CreateRenderTargetView(backBuffer, NULL, &g_pRenderTargetView);
    backBuffer->Release();

    ID3DBlob* vsBlob;
    ID3DBlob* psBlob;

    D3DCompile(shaderSource, strlen(shaderSource), NULL, NULL, NULL,
        "VS", "vs_4_0", 0, 0, &vsBlob, NULL);

    D3DCompile(shaderSource, strlen(shaderSource), NULL, NULL, NULL,
        "PS", "ps_4_0", 0, 0, &psBlob, NULL);

    ID3D11VertexShader* vShader;
    ID3D11PixelShader* pShader;

    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), NULL, &vShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), NULL, &pShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
        { "COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0 }
    };

    ID3D11InputLayout* pInputLayout;
    g_pd3dDevice->CreateInputLayout(layout, 2,
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        &pInputLayout);

    vsBlob->Release();
    psBlob->Release();

    CUSTOMVertex baseVertices[] = {

        // Point for Triangle 1
        { 0.0f,  0.7f, 0.0f, 1,0,0,1 },
        { 0.5f, -0.5f, 0.0f, 0,1,0,1 },
        {-0.5f, -0.5f, 0.0f, 0,0,1,1 },

        // Point for Triangle 2
        { 0.0f, -0.7f, 0.0f, 1,1,0,1 },
        {-0.5f,  0.5f, 0.0f, 1,0,1,1 }, 
        { 0.5f,  0.5f, 0.0f, 0,1,1,1 }
    };

    ID3D11Buffer* pVBuffer;

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(baseVertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = baseVertices;

    g_pd3dDevice->CreateBuffer(&bd, &initData, &pVBuffer);

    MSG msg = {};

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            // --- Update ---
            Update();

            // Apply movement
            CUSTOMVertex moved[6];
            for (int i = 0; i < 6; i++) {
                moved[i] = baseVertices[i];
                moved[i].x += gGame.posX;
                moved[i].y += gGame.posY;
            }

            g_pImmediateContext->UpdateSubresource(pVBuffer, 0, NULL, moved, 0, 0);

            // --- Render ---
            float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, NULL);

            D3D11_VIEWPORT vp = { 0,0,800,600,0,1 };
            g_pImmediateContext->RSSetViewports(1, &vp);

            g_pImmediateContext->IASetInputLayout(pInputLayout);

            UINT stride = sizeof(CUSTOMVertex);
            UINT offset = 0;
            g_pImmediateContext->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset);

            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            g_pImmediateContext->VSSetShader(vShader, NULL, 0);
            g_pImmediateContext->PSSetShader(pShader, NULL, 0);

            g_pImmediateContext->Draw(6, 0);

            g_pSwapChain->Present(1, 0);
        }
    }

    // Cleanup
    pVBuffer->Release();
    pInputLayout->Release();
    vShader->Release();
    pShader->Release();
    g_pRenderTargetView->Release();
    g_pSwapChain->Release();
    g_pImmediateContext->Release();
    g_pd3dDevice->Release();

    return (int)msg.wParam;
}
