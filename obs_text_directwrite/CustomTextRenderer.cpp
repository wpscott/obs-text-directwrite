#include "CustomTextRenderer.h"

#include "obs_text_directwrite.h"

CustomTextRenderer::CustomTextRenderer(ID2D1Factory* pD2DFactory_,
                                       ID2D1RenderTarget* pRT_,
                                       ID2D1Brush* pOutlineBrush_,
                                       ID2D1Brush* pFillBrush_,
                                       float Outline_size_)
    : cRefCount_(0),
      pD2DFactory(pD2DFactory_),
      pRT(pRT_),
      pFillBrush(pFillBrush_),
      Outline_size(Outline_size_) {
  pD2DFactory->AddRef();
  pRT->AddRef();
  pFillBrush->AddRef();

  if (pOutlineBrush_) {
    pOutlineBrush = pOutlineBrush_;
    pOutlineBrush->AddRef();
  }
}

CustomTextRenderer::~CustomTextRenderer() {
  SafeRelease(&pD2DFactory);
  SafeRelease(&pRT);
  SafeRelease(&pOutlineBrush);
  SafeRelease(&pFillBrush);
}

HRESULT CustomTextRenderer::DrawGlyphRun(const DWRITE_GLYPH_RUN* glyphRun,
                                         const FLOAT& baselineOriginX,
                                         const FLOAT& baselineOriginY,
                                         ID2D1Brush* brush) {
  ID2D1PathGeometry* pPathGeometry = NULL;
  HRESULT hr = pD2DFactory->CreatePathGeometry(&pPathGeometry);

  ID2D1GeometrySink* pSink = NULL;
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

  D2D1::Matrix3x2F const matrix =
      D2D1::Matrix3x2F(1.f, 0.f, 0.f, 1.f, baselineOriginX, baselineOriginY);

  ID2D1TransformedGeometry* pTransformedGeometry = NULL;
  if (SUCCEEDED(hr)) {
    hr = pD2DFactory->CreateTransformedGeometry(pPathGeometry, &matrix,
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
    IUnknown* clientDrawingEffect) {
  HRESULT hr = S_OK;

  const auto source = reinterpret_cast<TextSource*>(clientDrawingContext);

  bool isColor = false;
  IDWriteColorGlyphRunEnumerator1* colorLayer;

  if (source &&
      SUCCEEDED(source->pDWriteFactory->TranslateColorGlyphRun(
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
      if (FAILED(colorLayer->GetCurrentRun(&colorRun))) {
        break;
      }
      ID2D1Brush* brush = pFillBrush;
      if (colorRun->paletteIndex != 0xFFFF) {
        temp_brush->SetColor(colorRun->runColor);
        brush = temp_brush;
      }
      hr = DrawGlyphRun(&colorRun->glyphRun,
                        baselineOriginX + colorRun->baselineOriginX,
                        baselineOriginY + colorRun->baselineOriginY, brush);
    }
    SafeRelease(&temp_brush);
  } else {
    hr = DrawGlyphRun(glyphRun, baselineOriginX, baselineOriginY, pFillBrush);
  }

  return hr;
}

IFACEMETHODIMP CustomTextRenderer::DrawUnderline(
    __maybenull void* clientDrawingContext, FLOAT baselineOriginX,
    FLOAT baselineOriginY, __in DWRITE_UNDERLINE const* underline,
    IUnknown* clientDrawingEffect) {
  HRESULT hr;

  D2D1_RECT_F rect = D2D1::RectF(0.f, underline->offset, underline->width,
                                 underline->offset + underline->thickness);

  ID2D1RectangleGeometry* pRectangleGeometry = NULL;

  hr = pD2DFactory->CreateRectangleGeometry(&rect, &pRectangleGeometry);

  D2D1::Matrix3x2F const matrix =
      D2D1::Matrix3x2F(1.f, 0.f, 0.f, 1.f, baselineOriginX, baselineOriginY);

  ID2D1TransformedGeometry* pTransformedGeometry = NULL;
  if (SUCCEEDED(hr)) {
    hr = pD2DFactory->CreateTransformedGeometry(pRectangleGeometry, &matrix,
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
    IUnknown* clientDrawingEffect) {
  HRESULT hr;

  D2D1_RECT_F rect =
      D2D1::RectF(0.f, strikethrough->offset, strikethrough->width,
                  strikethrough->offset + strikethrough->thickness);

  ID2D1RectangleGeometry* pRectangleGeometry = NULL;
  hr = pD2DFactory->CreateRectangleGeometry(&rect, &pRectangleGeometry);

  D2D1::Matrix3x2F const matrix =
      D2D1::Matrix3x2F(1.f, 0.f, 0.f, 1.f, baselineOriginX, baselineOriginY);

  ID2D1TransformedGeometry* pTransformedGeometry = NULL;
  if (SUCCEEDED(hr)) {
    hr = pD2DFactory->CreateTransformedGeometry(pRectangleGeometry, &matrix,
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
    IUnknown* clientDrawingEffect) {
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
  if (__uuidof(IDWriteTextRenderer) == riid) {
    *ppvObject = this;
  } else if (__uuidof(IDWritePixelSnapping) == riid) {
    *ppvObject = this;
  } else if (__uuidof(IUnknown) == riid) {
    *ppvObject = this;
  } else {
    *ppvObject = NULL;
    return E_FAIL;
  }

  this->AddRef();

  return S_OK;
}
