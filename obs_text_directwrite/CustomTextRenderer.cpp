#include "CustomTextRenderer.h"

#include "obs_text_directwrite.h"

CustomTextRenderer::CustomTextRenderer(
    ID2D1Factory7* pD2DFactory_, IDWriteFactory7* pDWriteFactory_,
    ID2D1RenderTarget* pRT_, ID2D1Brush* pOutlineBrush_,
    ID2D1Brush* pFillBrush_, const float& Outline_size_, const bool& vertical_)
    : cRefCount_(0),
      pD2DFactory(pD2DFactory_),
      pDWriteFactory(pDWriteFactory_),
      pRT(pRT_),
      pFillBrush(pFillBrush_),
      Outline_size(Outline_size_),
      vertical(vertical_) {
  pD2DFactory->AddRef();
  pDWriteFactory->AddRef();
  pRT->AddRef();
  pFillBrush->AddRef();

  if (pOutlineBrush_) {
    pOutlineBrush = pOutlineBrush_;
    pOutlineBrush->AddRef();
  }

  IDWriteTextAnalyzer* analyzer;
  pDWriteFactory->CreateTextAnalyzer(&analyzer);
  analyzer->QueryInterface<IDWriteTextAnalyzer2>(&pAnalyzer);
  SafeRelease(&analyzer);
}

CustomTextRenderer::~CustomTextRenderer() {
  SafeRelease(&pD2DFactory);
  SafeRelease(&pDWriteFactory);
  SafeRelease(&pAnalyzer);
  SafeRelease(&pRT);
  SafeRelease(&pOutlineBrush);
  SafeRelease(&pFillBrush);
}

HRESULT CustomTextRenderer::DrawGlyphRun(const DWRITE_GLYPH_RUN* glyphRun,
                                         const D2D1::Matrix3x2F* matrix,
                                         ID2D1Brush* brush) {
  ID2D1PathGeometry* pPathGeometry = nullptr;
  HRESULT hr = pD2DFactory->CreatePathGeometry(&pPathGeometry);

  ID2D1GeometrySink* pSink = nullptr;
  if (SUCCEEDED(hr)) {
    hr = pPathGeometry->Open(&pSink);
  }

  if (SUCCEEDED(hr)) {
    hr = glyphRun->fontFace->GetGlyphRunOutline(
        glyphRun->fontEmSize, glyphRun->glyphIndices, glyphRun->glyphAdvances,
        glyphRun->glyphOffsets, glyphRun->glyphCount, glyphRun->isSideways,
        glyphRun->bidiLevel & 1, pSink);
  }

  if (SUCCEEDED(hr)) {
    hr = pSink->Close();
  }

  ID2D1TransformedGeometry* pTransformedGeometry = nullptr;
  if (SUCCEEDED(hr)) {
    hr = pD2DFactory->CreateTransformedGeometry(pPathGeometry, matrix,
                                                &pTransformedGeometry);
  }

  if (SUCCEEDED(hr)) {
    if (pOutlineBrush) {
      pRT->DrawGeometry(pTransformedGeometry, pOutlineBrush, Outline_size);
    }

    pRT->FillGeometry(pTransformedGeometry, brush);
  }

  SafeRelease(&pPathGeometry);
  SafeRelease(&pSink);
  SafeRelease(&pTransformedGeometry);
  return hr;
}

IFACEMETHODIMP CustomTextRenderer::DrawGlyphRun(
    __maybenull void* clientDrawingContext, FLOAT baselineOriginX,
    FLOAT baselineOriginY, DWRITE_MEASURING_MODE measuringMode,
    __in DWRITE_GLYPH_RUN const* glyphRun,
    __in DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription,
    __maybenull IUnknown* clientDrawingEffect) {
  return DrawGlyphRun(clientDrawingContext, baselineOriginX, baselineOriginY,
                      DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES, measuringMode,
                      glyphRun, glyphRunDescription, clientDrawingEffect);
}

IFACEMETHODIMP CustomTextRenderer::DrawGlyphRun(
    __maybenull void* clientDrawingContext, FLOAT baselineOriginX,
    FLOAT baselineOriginY, DWRITE_GLYPH_ORIENTATION_ANGLE orientationAngle,
    DWRITE_MEASURING_MODE measuringMode, __in DWRITE_GLYPH_RUN const* glyphRun,
    __in DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription,
    __maybenull IUnknown* clientDrawingEffect) {
  HRESULT hr = S_OK;

  bool isColor = false;
  IDWriteColorGlyphRunEnumerator1* colorLayer;

  if (SUCCEEDED(pDWriteFactory->TranslateColorGlyphRun(
          {0, 0}, glyphRun, glyphRunDescription,
          DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE | DWRITE_GLYPH_IMAGE_FORMATS_CFF |
              DWRITE_GLYPH_IMAGE_FORMATS_COLR,
          measuringMode, nullptr, 0, &colorLayer))) {
    isColor = true;
  }

  if (isColor) {
    BOOL hasRun;
    const DWRITE_COLOR_GLYPH_RUN1* colorRun;
    ID2D1SolidColorBrush* temp_brush;
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &temp_brush);
    while (SUCCEEDED(colorLayer->MoveNext(&hasRun)) && hasRun) {
      hr = colorLayer->GetCurrentRun(&colorRun);
      if (FAILED(hr)) {
        break;
      }
      ID2D1Brush* brush = pFillBrush;
      if (colorRun->paletteIndex != 0xFFFF) {
        temp_brush->SetColor(colorRun->runColor);
        brush = temp_brush;
      }
      D2D1::Matrix3x2F const origin =
          D2D1::Matrix3x2F(1.f, 0.f, 0.f, 1.f, colorRun->baselineOriginX,
                           colorRun->baselineOriginY);
      DWRITE_MATRIX matrix{};
      pAnalyzer->GetGlyphOrientationTransform(
          orientationAngle, colorRun->glyphRun.isSideways, &matrix);
      matrix.dx = baselineOriginX;
      matrix.dy = baselineOriginY;

      const auto& result = origin * (*(D2D1::Matrix3x2F*)&matrix);

      hr = DrawGlyphRun(&colorRun->glyphRun, &result, brush);
    }
    SafeRelease(&temp_brush);
  } else {
    DWRITE_MATRIX matrix{};
    pAnalyzer->GetGlyphOrientationTransform(orientationAngle,
                                            glyphRun->isSideways, &matrix);
    matrix.dx = baselineOriginX;
    matrix.dy = baselineOriginY;
    hr = DrawGlyphRun(glyphRun, (D2D1::Matrix3x2F*)&matrix, pFillBrush);
  }

  return hr;
}

IFACEMETHODIMP CustomTextRenderer::DrawUnderline(
    __maybenull void* clientDrawingContext, FLOAT baselineOriginX,
    FLOAT baselineOriginY, __in DWRITE_UNDERLINE const* underline,
    __maybenull IUnknown* clientDrawingEffect) {
  return DrawUnderline(clientDrawingContext, baselineOriginX, baselineOriginY,
                       DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES, underline,
                       clientDrawingEffect);
}

IFACEMETHODIMP CustomTextRenderer::DrawUnderline(
    __maybenull void* clientDrawingContext, FLOAT baselineOriginX,
    FLOAT baselineOriginY, DWRITE_GLYPH_ORIENTATION_ANGLE orientationAngle,
    __in DWRITE_UNDERLINE const* underline,
    __maybenull IUnknown* clientDrawingEffect) {
  D2D1_RECT_F rect = D2D1::RectF(0.f, underline->offset, underline->width,
                                 underline->offset + underline->thickness);

  ID2D1RectangleGeometry* pRectangleGeometry = nullptr;

  HRESULT hr = pD2DFactory->CreateRectangleGeometry(&rect, &pRectangleGeometry);

  D2D1::Matrix3x2F const matrix =
      D2D1::Matrix3x2F(1.f, 0.f, 0.f, 1.f, baselineOriginX, baselineOriginY);
  const auto& result = matrix * rotations[orientationAngle];

  ID2D1TransformedGeometry* pTransformedGeometry = nullptr;
  if (SUCCEEDED(hr)) {
    hr = pD2DFactory->CreateTransformedGeometry(pRectangleGeometry, &result,
                                                &pTransformedGeometry);
  }

  if (SUCCEEDED(hr)) {
    if (pOutlineBrush)
      pRT->DrawGeometry(pTransformedGeometry, pOutlineBrush, Outline_size);

    pRT->FillGeometry(pTransformedGeometry, pFillBrush);
  }

  SafeRelease(&pRectangleGeometry);
  SafeRelease(&pTransformedGeometry);

  return S_OK;
}

IFACEMETHODIMP CustomTextRenderer::DrawStrikethrough(
    __maybenull void* clientDrawingContext, FLOAT baselineOriginX,
    FLOAT baselineOriginY, __in DWRITE_STRIKETHROUGH const* strikethrough,
    __maybenull IUnknown* clientDrawingEffect) {
  return DrawStrikethrough(clientDrawingContext, baselineOriginX,
                           baselineOriginY, strikethrough, clientDrawingEffect);
}

IFACEMETHODIMP CustomTextRenderer::DrawStrikethrough(
    __maybenull void* clientDrawingContext, FLOAT baselineOriginX,
    FLOAT baselineOriginY, DWRITE_GLYPH_ORIENTATION_ANGLE orientationAngle,
    __in DWRITE_STRIKETHROUGH const* strikethrough,
    __maybenull IUnknown* clientDrawingEffect) {
  D2D1_RECT_F rect =
      D2D1::RectF(0.f, strikethrough->offset, strikethrough->width,
                  strikethrough->offset + strikethrough->thickness);

  ID2D1RectangleGeometry* pRectangleGeometry = nullptr;
  HRESULT hr = pD2DFactory->CreateRectangleGeometry(&rect, &pRectangleGeometry);

  D2D1::Matrix3x2F const matrix =
      D2D1::Matrix3x2F(1.f, 0.f, 0.f, 1.f, baselineOriginX, baselineOriginY);
  const auto& result = matrix * rotations[orientationAngle];

  ID2D1TransformedGeometry* pTransformedGeometry = nullptr;
  if (SUCCEEDED(hr)) {
    hr = pD2DFactory->CreateTransformedGeometry(pRectangleGeometry, &result,
                                                &pTransformedGeometry);
  }
  if (SUCCEEDED(hr)) {
    if (pOutlineBrush)
      pRT->DrawGeometry(pTransformedGeometry, pOutlineBrush, Outline_size);

    pRT->FillGeometry(pTransformedGeometry, pFillBrush);
  }

  SafeRelease(&pRectangleGeometry);
  SafeRelease(&pTransformedGeometry);

  return S_OK;
}

IFACEMETHODIMP CustomTextRenderer::DrawInlineObject(
    __maybenull void* clientDrawingContext, FLOAT originX, FLOAT originY,
    IDWriteInlineObject* inlineObject, BOOL isSideways, BOOL isRightToLeft,
    __maybenull IUnknown* clientDrawingEffect) {
  return E_NOTIMPL;
}

IFACEMETHODIMP CustomTextRenderer::DrawInlineObject(
    __maybenull void* clientDrawingContext, FLOAT originX, FLOAT originY,
    DWRITE_GLYPH_ORIENTATION_ANGLE orientationAngle,
    __in IDWriteInlineObject* inlineObject, BOOL isSideways, BOOL isRightToLeft,
    __maybenull IUnknown* clientDrawingEffect) {
  return E_NOTIMPL;
}

IFACEMETHODIMP CustomTextRenderer::IsPixelSnappingDisabled(
    __maybenull void* clientDrawingContext, __out BOOL* isDisabled) {
  *isDisabled = FALSE;
  return S_OK;
}

IFACEMETHODIMP CustomTextRenderer::GetCurrentTransform(
    __maybenull void* clientDrawingContext, __out DWRITE_MATRIX* transform) {
  // forward the render target's transform
  pRT->GetTransform(reinterpret_cast<D2D1_MATRIX_3X2_F*>(transform));
  return S_OK;
}

IFACEMETHODIMP CustomTextRenderer::GetPixelsPerDip(
    __maybenull void* clientDrawingContext, __out FLOAT* pixelsPerDip) {
  *pixelsPerDip = 1.0f;
  return S_OK;
}

IFACEMETHODIMP_(unsigned long) CustomTextRenderer::AddRef() {
  return InterlockedIncrement(&cRefCount_);
}

IFACEMETHODIMP_(unsigned long) CustomTextRenderer::Release() {
  unsigned long newCount = InterlockedDecrement(&cRefCount_);
  if (newCount == 0) {
    delete this;
    return 0;
  }

  return newCount;
}

IFACEMETHODIMP CustomTextRenderer::QueryInterface(IID const& riid,
                                                  void** ppvObject) {
  if (__uuidof(IDWriteTextRenderer1) == riid ||
      __uuidof(IDWriteTextRenderer) == riid ||
      __uuidof(IDWritePixelSnapping) == riid || __uuidof(IUnknown) == riid) {
    *ppvObject = this;
    this->AddRef();
    return S_OK;
  } else {
    *ppvObject = nullptr;
    return E_FAIL;
  }
}
