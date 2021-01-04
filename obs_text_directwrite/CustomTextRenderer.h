#pragma once

#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1_2.h>
#include <d2d1_3.h>
#include <dwrite.h>
#include <dwrite_1.h>
#include <dwrite_2.h>
#include <dwrite_3.h>

#include <array>
#include <exception>

template <class T>
void SafeRelease(T** ppT) {
  if (*ppT) {
    (*ppT)->Release();
    *ppT = NULL;
  }
}

class CustomTextRenderer : public IDWriteTextRenderer1 {
 public:
  CustomTextRenderer(ID2D1Factory7* pD2DFactory,
                     IDWriteFactory7* pDWriteFactory, ID2D1RenderTarget* pRT,
                     ID2D1Brush* pOutlineBrush, ID2D1Brush* pFillBrush,
                     const float& Outline_size_, const bool& vertical_);

  ~CustomTextRenderer();

  IFACEMETHOD(IsPixelSnappingDisabled)
  (__maybenull void* clientDrawingContext, __out BOOL* isDisabled);

  IFACEMETHOD(GetCurrentTransform)
  (__maybenull void* clientDrawingContext, __out DWRITE_MATRIX* transform);

  IFACEMETHOD(GetPixelsPerDip)
  (__maybenull void* clientDrawingContext, __out FLOAT* pixelsPerDip);

  IFACEMETHOD(DrawGlyphRun)
  (__maybenull void* clientDrawingContext, FLOAT baselineOriginX,
   FLOAT baselineOriginY, DWRITE_MEASURING_MODE measuringMode,
   __in DWRITE_GLYPH_RUN const* glyphRun,
   __in DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription,
   __maybenull IUnknown* clientDrawingEffect);

  IFACEMETHOD(DrawGlyphRun)
  (__maybenull void* clientDrawingContext, FLOAT baselineOriginX,
   FLOAT baselineOriginY, DWRITE_GLYPH_ORIENTATION_ANGLE orientationAngle,
   DWRITE_MEASURING_MODE measuringMode, _In_ DWRITE_GLYPH_RUN const* glyphRun,
   _In_ DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription,
   __maybenull IUnknown* clientDrawingEffect);

  IFACEMETHOD(DrawUnderline)
  (__maybenull void* clientDrawingContext, FLOAT baselineOriginX,
   FLOAT baselineOriginY, __in DWRITE_UNDERLINE const* underline,
   __maybenull IUnknown* clientDrawingEffect);

  IFACEMETHOD(DrawUnderline)
  (__maybenull void* clientDrawingContext, FLOAT baselineOriginX,
   FLOAT baselineOriginY, DWRITE_GLYPH_ORIENTATION_ANGLE orientationAngle,
   __in DWRITE_UNDERLINE const* underline,
   __maybenull IUnknown* clientDrawingEffect);

  IFACEMETHOD(DrawStrikethrough)
  (__maybenull void* clientDrawingContext, FLOAT baselineOriginX,
   FLOAT baselineOriginY, __in DWRITE_STRIKETHROUGH const* strikethrough,
   __maybenull IUnknown* clientDrawingEffect);

  IFACEMETHOD(DrawStrikethrough)
  (__maybenull void* clientDrawingContext, FLOAT baselineOriginX,
   FLOAT baselineOriginY, DWRITE_GLYPH_ORIENTATION_ANGLE orientationAngle,
   __in DWRITE_STRIKETHROUGH const* strikethrough,
   __maybenull IUnknown* clientDrawingEffect);

  IFACEMETHOD(DrawInlineObject)
  (__maybenull void* clientDrawingContext, FLOAT originX, FLOAT originY,
   IDWriteInlineObject* inlineObject, BOOL isSideways, BOOL isRightToLeft,
   __maybenull IUnknown* clientDrawingEffect);

  IFACEMETHOD(DrawInlineObject)
  (__maybenull void* clientDrawingContext, FLOAT originX, FLOAT originY,
   DWRITE_GLYPH_ORIENTATION_ANGLE orientationAngle,
   __in IDWriteInlineObject* inlineObject, BOOL isSideways, BOOL isRightToLeft,
   __maybenull IUnknown* clientDrawingEffect);

 public:
  IFACEMETHOD_(unsigned long, AddRef)();
  IFACEMETHOD_(unsigned long, Release)();
  IFACEMETHOD(QueryInterface)(IID const& riid, void** ppvObject);

 private:
  unsigned long cRefCount_;
  float Outline_size;
  bool vertical;
  ID2D1Factory7* pD2DFactory;
  IDWriteFactory7* pDWriteFactory;
  IDWriteTextAnalyzer2* pAnalyzer;
  ID2D1RenderTarget* pRT;
  ID2D1Brush* pOutlineBrush = nullptr;
  ID2D1Brush* pFillBrush;

  std::array<D2D1::Matrix3x2F, 4> rotations = {
      D2D1::Matrix3x2F::Rotation(0.f), D2D1::Matrix3x2F::Rotation(90.f),
      D2D1::Matrix3x2F::Rotation(180.f), D2D1::Matrix3x2F::Rotation(270.f)};

  HRESULT DrawGlyphRun(const DWRITE_GLYPH_RUN* glyphRun,
                       const D2D1::Matrix3x2F* matrix, ID2D1Brush* brush);
};
