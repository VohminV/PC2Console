// minplay.cpp — PC2Console Phase 5: minimal PLAYABLE Xbox 360 sample.
//
// Original code written for this project (not copied from the XDK samples).
// It only *uses* the documented XDK API surface verified in the installed
// XDK 21256 headers and the build pattern verified in the XDK sample project:
//   - entry point   : VOID __cdecl main()              (xtl.h convention)
//   - graphics      : Direct3DCreate9 + CreateDevice    (d3d9.h)
//   - shaders       : D3DXCompileShader vs_2_0/ps_2_0   (d3dx9shader.h)
//   - vertex decl   : D3DVERTEXELEMENT9                 (d3d9types.h)
//   - matrices      : XMMATRIX orthographic             (xboxmath.h)
//   - gamepad poll  : XInputGetState / XInputSetState   (xbox.h:230-259)
//   - pad layout    : XINPUT_STATE / XINPUT_VIBRATION   (xinputdefs.h:158-179)
//   - buttons/dz    : XINPUT_GAMEPAD_*                  (xinputdefs.h:66-88)
//
// NOTE: Xbox 360 d3d9types.h has NO D3DFVF_XYZRHW (verified by failed
// compile) — pretransformed vertices are unsupported, so the sample uses
// untransformed XYZ + a tiny HLSL shader pair + orthographic projection.
//
// Game: move the square with the LEFT STICK (deadzone-aware). Press A to
// score: the square cycles color and the pad rumbles. START/BACK exits.
// No mouse, no keyboard, no Win32 messages — gamepad only, as required
// on Xbox 360 (see docs/PORTING_LIMITATIONS.md).

#include <xtl.h>
#include <xboxmath.h>

namespace {

IDirect3D9* g_d3d = 0;
IDirect3DDevice9* g_dev = 0;
IDirect3DVertexDeclaration9* g_decl = 0;
IDirect3DVertexShader9* g_vs = 0;
IDirect3DPixelShader9* g_ps = 0;

// Screen-space passthrough shaders (own minimal HLSL, orthographic WVP).
const CHAR* kVS =
    " float4x4 matWVP : register(c0); "
    " struct VS_IN { float4 ObjPos : POSITION; float4 Color : COLOR; }; "
    " struct VS_OUT { float4 ProjPos : POSITION; float4 Color : COLOR; }; "
    " VS_OUT main( VS_IN In ) "
    " { VS_OUT Out; Out.ProjPos = mul( matWVP, In.ObjPos ); "
    "   Out.Color = In.Color; return Out; } ";
const CHAR* kPS =
    " struct PS_IN { float4 Color : COLOR; }; "
    " float4 main( PS_IN In ) : COLOR { return In.Color; } ";

struct ColorVertex {
  FLOAT pos[3];
  DWORD color;
};

bool CompileShaders() {
  ID3DXBuffer* code = 0;
  ID3DXBuffer* errors = 0;
  HRESULT hr = D3DXCompileShader(kVS, (UINT)strlen(kVS), NULL, NULL, "main", "vs_2_0", 0,
                                 &code, &errors, NULL);
  if (FAILED(hr)) {
    OutputDebugStringA(errors ? (CHAR*)errors->GetBufferPointer() : "vs compile failed");
    return false;
  }
  if (FAILED(g_dev->CreateVertexShader((DWORD*)code->GetBufferPointer(), &g_vs))) {
    code->Release();
    return false;
  }
  code->Release();
  code = 0;

  hr = D3DXCompileShader(kPS, (UINT)strlen(kPS), NULL, NULL, "main", "ps_2_0", 0,
                         &code, &errors, NULL);
  if (FAILED(hr)) {
    OutputDebugStringA(errors ? (CHAR*)errors->GetBufferPointer() : "ps compile failed");
    return false;
  }
  if (FAILED(g_dev->CreatePixelShader((DWORD*)code->GetBufferPointer(), &g_ps))) {
    code->Release();
    return false;
  }
  code->Release();

  static const D3DVERTEXELEMENT9 kElements[] = {
      {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
      {0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
      D3DDECL_END()};
  if (FAILED(g_dev->CreateVertexDeclaration(kElements, &g_decl))) return false;
  return true;
}

bool InitD3D() {
  g_d3d = Direct3DCreate9(D3D_SDK_VERSION);
  if (!g_d3d) return false;

  D3DPRESENT_PARAMETERS d3dpp;
  ZeroMemory(&d3dpp, sizeof(d3dpp));
  d3dpp.BackBufferWidth = 1280;
  d3dpp.BackBufferHeight = 720;
  d3dpp.BackBufferFormat = (D3DFORMAT)MAKESRGBFMT(D3DFMT_A8R8G8B8);
  d3dpp.FrontBufferFormat = (D3DFORMAT)MAKESRGBFMT(D3DFMT_LE_X8R8G8B8);
  d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
  d3dpp.MultiSampleQuality = 0;
  d3dpp.BackBufferCount = 1;
  d3dpp.EnableAutoDepthStencil = FALSE;
  d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
  d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

  if (FAILED(g_d3d->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                 &d3dpp, &g_dev)))
    return false;
  if (!CompileShaders()) return false;

  // Screen-space orthographic: x right, y DOWN, matching stick math below.
  XMMATRIX wvp = XMMatrixOrthographicOffCenterLH(0.0f, 1280.0f, 720.0f, 0.0f, 0.0f, 1.0f);
  g_dev->SetVertexShaderConstantF(0, (FLOAT*)&wvp, 4);
  return true;
}

// Maps raw stick axis to [-1,1] with deadzone; 0 inside the deadzone.
float StickAxis(SHORT raw) {
  const SHORT dz = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
  if (raw > -dz && raw < dz) return 0.0f;
  if (raw >= 0) return (float)(raw - dz) / (float)(32767 - dz);
  return (float)(raw + dz) / (float)(32767 - dz);
}

void DrawSquare(float cx, float cy, float half, DWORD color) {
  ColorVertex v[4];
  v[0].pos[0] = cx - half;
  v[0].pos[1] = cy - half;
  v[1].pos[0] = cx + half;
  v[1].pos[1] = cy - half;
  v[2].pos[0] = cx - half;
  v[2].pos[1] = cy + half;
  v[3].pos[0] = cx + half;
  v[3].pos[1] = cy + half;
  for (int i = 0; i < 4; ++i) {
    v[i].pos[2] = 0.0f;
    v[i].color = color;
  }
  g_dev->SetVertexShader(g_vs);
  g_dev->SetPixelShader(g_ps);
  g_dev->SetVertexDeclaration(g_decl);
  g_dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(ColorVertex));
}

float ClampF(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

}  // namespace

VOID __cdecl main() {
  if (!InitD3D()) return;

  static const DWORD kColors[] = {0xFFFF8000, 0xFF00C000, 0xFF0080FF,
                                  0xFFFF00C0, 0xFFFFFF00};
  float px = 640.0f;
  float py = 360.0f;
  int colorIdx = 0;
  int score = 0;
  int rumbleFrames = 0;
  bool wasA = false;

  for (;;) {
    XINPUT_STATE st;
    ZeroMemory(&st, sizeof(st));
    if (XInputGetState(0, &st) == ERROR_SUCCESS) {
      px += StickAxis(st.Gamepad.sThumbLX) * 9.0f;
      py -= StickAxis(st.Gamepad.sThumbLY) * 9.0f;
      px = ClampF(px, 40.0f, 1240.0f);
      py = ClampF(py, 40.0f, 680.0f);

      bool a = (st.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
      if (a && !wasA) {
        ++score;
        colorIdx = score % 5;
        rumbleFrames = 12;
      }
      wasA = a;

      if (st.Gamepad.wButtons & (XINPUT_GAMEPAD_START | XINPUT_GAMEPAD_BACK)) break;

      XINPUT_VIBRATION vib;
      vib.wLeftMotorSpeed = 0;
      vib.wRightMotorSpeed = 0;
      if (rumbleFrames > 0) {
        vib.wLeftMotorSpeed = 28000;
        vib.wRightMotorSpeed = 28000;
        --rumbleFrames;
      }
      XInputSetState(0, &vib);
    }

    // Dark background lightens every 25 points (progress feedback).
    BYTE bg = (BYTE)(8 + (score / 25) * 6);
    if (bg > 64) bg = 64;
    g_dev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(bg, bg, bg + 16), 1.0f, 0);

    if (SUCCEEDED(g_dev->BeginScene())) {
      float half = 32.0f + (float)(score % 10) * 2.0f;
      DrawSquare(px, py, half, kColors[colorIdx]);
      g_dev->EndScene();
    }
    g_dev->Present(NULL, NULL, NULL, NULL);
  }
}
