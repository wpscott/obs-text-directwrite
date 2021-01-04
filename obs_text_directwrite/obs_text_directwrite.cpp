#include "obs_text_directwrite.h"

void TextSource::CalculateGradientAxis(float width, float height) {
  if (width <= 0.f || height <= 0.f) return;

  float angle = DEG(atan(height / width));

  if (gradient_dir <= angle || gradient_dir > 360.f - angle) {
    float y = width / 2.f * tan(RAD(gradient_dir));
    gradient_x = width;
    gradient_y = height / 2.f - y;
    gradient2_x = 0.f;
    gradient2_y = height / 2.f + y;

  } else if (gradient_dir <= 180.f - angle && gradient_dir > angle) {
    float x = height / 2.f * tan(RAD(90.f - gradient_dir));
    gradient_x = width / 2.f + x;
    gradient_y = 0.f;
    gradient2_x = width / 2.f - x;
    gradient2_y = height;
  } else if (gradient_dir <= 180.f + angle && gradient_dir > 180.f - angle) {
    float y = width / 2.f * tan(RAD(gradient_dir));
    gradient_x = 0.f;
    gradient_y = height / 2.f + y;
    gradient2_x = width;
    gradient2_y = height / 2.f - y;
  } else {
    float x = height / 2.f * tan(RAD(270.f - gradient_dir));
    gradient_x = width / 2.f - x;
    gradient_y = height;
    gradient2_x = width / 2.f + x;
    gradient2_y = 0.f;
  }
}

void TextSource::InitializeDirectWrite() {
  HRESULT hr =
      D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);

  if (SUCCEEDED(hr)) {
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                             __uuidof(IDWriteFactory7),
                             reinterpret_cast<IUnknown **>(&pDWriteFactory));
  }

  props = D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_DEFAULT,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                        D2D1_ALPHA_MODE_PREMULTIPLIED),
      0, 0, D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE,
      D2D1_FEATURE_LEVEL_DEFAULT);
}

void TextSource::ReleaseResource() {
  SafeRelease(&pTextFormat);
  SafeRelease(&pDWriteFactory);
  SafeRelease(&pD2DFactory);
}

void TextSource::UpdateBrush(ID2D1RenderTarget *pRT,
                             ID2D1Brush **ppOutlineBrush,
                             ID2D1Brush **ppFillBrush, float width,
                             float height) {
  HRESULT hr;

  if (gradient_count != 0) {
    CalculateGradientAxis(width, height);

    ID2D1GradientStopCollection *pGradientStops = NULL;

    float level = 1.f / (gradient_count - 1);

    gradientStops[0].color = D2D1::ColorF(color, opacity / 100.f);
    gradientStops[0].position = 0.f;
    gradientStops[1].color = D2D1::ColorF(color2, opacity2 / 100.f);
    gradientStops[1].position = gradientStops[0].position + level;
    gradientStops[2].color = D2D1::ColorF(color3, opacity2 / 100.f);
    gradientStops[2].position = gradientStops[1].position + level;
    gradientStops[3].color = D2D1::ColorF(color4, opacity2 / 100.f);
    gradientStops[3].position = 1.f;

    hr = pRT->CreateGradientStopCollection(
        gradientStops, gradient_count, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_MIRROR,
        &pGradientStops);

    if (SUCCEEDED(hr)) {
      hr = pRT->CreateLinearGradientBrush(
          D2D1::LinearGradientBrushProperties(
              D2D1::Point2F(gradient_x, gradient_y),
              D2D1::Point2F(gradient2_x, gradient2_y)),
          pGradientStops, (ID2D1LinearGradientBrush **)ppFillBrush);
    }
    SafeRelease(&pGradientStops);

  } else {
    hr = pRT->CreateSolidColorBrush(D2D1::ColorF(color, opacity / 100.f),
                                    (ID2D1SolidColorBrush **)ppFillBrush);
  }

  if (use_outline) {
    hr = pRT->CreateSolidColorBrush(
        D2D1::ColorF(outline_color, outline_opacity / 100.f),
        (ID2D1SolidColorBrush **)ppOutlineBrush);
  }
}

void TextSource::RenderText() {
  UINT32 TextLength = (UINT32)wcslen(text.c_str());

  float layout_cx = (use_extents) ? extents_cx : 1920.f;
  float layout_cy = (use_extents) ? extents_cy : 1080.f;
  float text_cx = 0.f;
  float text_cy = 0.f;

  SIZE size;
  UINT32 lines = 1;

  IDWriteTextLayout *layout = nullptr;
  ID2D1Brush *pFillBrush = nullptr;
  ID2D1Brush *pOutlineBrush = nullptr;
  ID2D1DCRenderTarget *pRT = nullptr;
  IDWriteTextLayout4 *pTextLayout = nullptr;

  HRESULT hr = pDWriteFactory->CreateTextLayout(
      text.c_str(), TextLength, pTextFormat, layout_cx, layout_cy, &layout);
  if (SUCCEEDED(hr)) {
    hr = layout->QueryInterface<IDWriteTextLayout4>(&pTextLayout);
    SafeRelease(&layout);
  }

  if (SUCCEEDED(hr)) {
    DWRITE_TEXT_METRICS textMetrics;
    hr = pTextLayout->GetMetrics(&textMetrics);

    text_cx = ceil(textMetrics.widthIncludingTrailingWhitespace);
    text_cy = ceil(textMetrics.height);
    lines = textMetrics.lineCount;

    if (!use_extents) {
      layout_cx = text_cx;
      layout_cy = text_cy;
    }

    clamp(layout_cx, MIN_SIZE_CX, MAX_SIZE_CX);
    clamp(layout_cy, MIN_SIZE_CY, MAX_SIZE_CY);

    size.cx = layout_cx;
    size.cy = layout_cy;
  }

  if (SUCCEEDED(hr)) {
    DWRITE_TEXT_RANGE text_range = {0, TextLength};
    pTextLayout->SetUnderline(underline, text_range);
    pTextLayout->SetStrikethrough(strikeout, text_range);
    pTextLayout->SetMaxWidth(layout_cx);
    pTextLayout->SetMaxHeight(layout_cy);
    if (vertical) {
      pTextLayout->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
      pTextLayout->SetFlowDirection(DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT);
    }

    hr = pD2DFactory->CreateDCRenderTarget(&props, &pRT);
  }

  if (SUCCEEDED(hr)) {
    obs_enter_graphics();
    RECT rc;
    SetRect(&rc, 0, 0, size.cx, size.cy);
    if (!tex || (LONG)cx != size.cx || (LONG)cy != size.cy) {
      if (tex) {
        gs_texture_destroy(tex);
      }
      tex = gs_texture_create_gdi(size.cx, size.cy);
    }
    HDC hdc = (HDC)gs_texture_get_dc(tex);
    if (hdc) {
      pRT->BindDC(hdc, &rc);

      UpdateBrush(pRT, &pOutlineBrush, &pFillBrush, text_cx, text_cy / lines);

      const auto &pTextRenderer = new CustomTextRenderer(
          pD2DFactory, pDWriteFactory, (ID2D1RenderTarget *)pRT,
          (ID2D1Brush *)pOutlineBrush, (ID2D1Brush *)pFillBrush, outline_size,
          vertical);
      if (pTextRenderer) {
        pRT->BeginDraw();

        pRT->SetTransform(D2D1::IdentityMatrix());

        pRT->Clear(D2D1::ColorF(bk_color, bk_opacity / 100.f));

        hr = pTextLayout->Draw(nullptr, pTextRenderer, 0.f, 0.f);

        hr = pRT->EndDraw();
      }
      gs_texture_release_dc(tex);
    }
    obs_leave_graphics();

    cx = (uint32_t)size.cx;
    cy = (uint32_t)size.cy;
  }

  SafeRelease(&pFillBrush);
  SafeRelease(&pOutlineBrush);
  SafeRelease(&pTextLayout);
  SafeRelease(&pRT);
}

const char *TextSource::GetMainString(const char *str) {
  if (!str) return "";
  if (!chatlog_mode || !chatlog_lines) return str;

  int lines = chatlog_lines;
  size_t len = strlen(str);
  if (!len) return str;

  const char *temp = str + len;

  while (temp != str) {
    temp--;

    if (temp[0] == '\n' && temp[1] != 0) {
      if (!--lines) break;
    }
  }

  return *temp == '\n' ? temp + 1 : temp;
}

void TextSource::LoadFileText() {
  BPtr<char> file_text = os_quick_read_utf8_file(file.c_str());
  text = to_wide(GetMainString(file_text));
}

void TextSource::UpdateFont() {
  SafeRelease(&pTextFormat);

  if (pDWriteFactory) {
    IDWriteTextFormat *format;
    HRESULT hr = pDWriteFactory->CreateTextFormat(
        face.c_str(), NULL,
        bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_REGULAR,
        italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, (float)face_size, L"en-us", &format);
    if (SUCCEEDED(hr)) {
      hr = format->QueryInterface<IDWriteTextFormat3>(&pTextFormat);
      SafeRelease(&format);
    }

    if (SUCCEEDED(hr)) {
      pTextFormat->SetTextAlignment(align);
      pTextFormat->SetParagraphAlignment(valign);
      pTextFormat->SetWordWrapping(wrap ? DWRITE_WORD_WRAPPING_WRAP
                                        : DWRITE_WORD_WRAPPING_NO_WRAP);

      if (vertical) {
        pTextFormat->SetReadingDirection(
            DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
        pTextFormat->SetFlowDirection(DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT);
      }
    }
  }
}

#define obs_data_get_uint32 (uint32_t) obs_data_get_int

inline void TextSource::Update(obs_data_t *s) {
  const char *new_text = obs_data_get_string(s, S_TEXT);
  obs_data_t *font_obj = obs_data_get_obj(s, S_FONT);
  const char *align_str = obs_data_get_string(s, S_ALIGN);
  const char *valign_str = obs_data_get_string(s, S_VALIGN);
  uint32_t new_color = obs_data_get_uint32(s, S_COLOR);
  uint32_t new_opacity = obs_data_get_uint32(s, S_OPACITY);
  const char *gradient_str = obs_data_get_string(s, S_GRADIENT);
  uint32_t new_color2 = obs_data_get_uint32(s, S_GRADIENT_COLOR);
  uint32_t new_color3 = obs_data_get_uint32(s, S_GRADIENT_COLOR2);
  uint32_t new_color4 = obs_data_get_uint32(s, S_GRADIENT_COLOR3);
  uint32_t new_opacity2 = obs_data_get_uint32(s, S_GRADIENT_OPACITY);
  float new_grad_dir = (float)obs_data_get_double(s, S_GRADIENT_DIR);
  bool new_vertical = obs_data_get_bool(s, S_VERTICAL);
  bool new_outline = obs_data_get_bool(s, S_OUTLINE);
  uint32_t new_o_color = obs_data_get_uint32(s, S_OUTLINE_COLOR);
  uint32_t new_o_opacity = obs_data_get_uint32(s, S_OUTLINE_OPACITY);
  uint32_t new_o_size = obs_data_get_uint32(s, S_OUTLINE_SIZE);
  bool new_use_file = obs_data_get_bool(s, S_USE_FILE);
  const char *new_file = obs_data_get_string(s, S_FILE);
  bool new_chat_mode = obs_data_get_bool(s, S_CHATLOG_MODE);
  int new_chat_lines = (int)obs_data_get_int(s, S_CHATLOG_LINES);
  bool new_extents = obs_data_get_bool(s, S_EXTENTS);
  bool new_extends_wrap = obs_data_get_bool(s, S_EXTENTS_WRAP);
  uint32_t n_extents_cx = obs_data_get_uint32(s, S_EXTENTS_CX);
  uint32_t n_extents_cy = obs_data_get_uint32(s, S_EXTENTS_CY);

  const char *font_face = obs_data_get_string(font_obj, "face");
  int font_size = (int)obs_data_get_int(font_obj, "size");
  int64_t font_flags = obs_data_get_int(font_obj, "flags");
  bool new_bold = (font_flags & OBS_FONT_BOLD) != 0;
  bool new_italic = (font_flags & OBS_FONT_ITALIC) != 0;
  bool new_underline = (font_flags & OBS_FONT_UNDERLINE) != 0;
  bool new_strikeout = (font_flags & OBS_FONT_STRIKEOUT) != 0;

  uint32_t new_bk_color = obs_data_get_uint32(s, S_BKCOLOR);
  uint32_t new_bk_opacity = obs_data_get_uint32(s, S_BKOPACITY);

  /* ----------------------------- */

  wrap = new_extends_wrap;

  if (strcmp(align_str, S_ALIGN_CENTER) == 0) {
    if (vertical) {
      valign = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
    } else {
      align = DWRITE_TEXT_ALIGNMENT_CENTER;
    }
  } else if (strcmp(align_str, S_ALIGN_RIGHT) == 0) {
    if (vertical) {
      valign = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
    } else {
      align = DWRITE_TEXT_ALIGNMENT_TRAILING;
    }
  } else {
    if (vertical) {
      valign = DWRITE_PARAGRAPH_ALIGNMENT_FAR;
    } else {
      align = DWRITE_TEXT_ALIGNMENT_LEADING;
    }
  }

  if (strcmp(valign_str, S_VALIGN_CENTER) == 0) {
    valign = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
    if (vertical) {
      align = DWRITE_TEXT_ALIGNMENT_CENTER;
    } else {
      valign = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
    }
  } else if (strcmp(valign_str, S_VALIGN_BOTTOM) == 0) {
    if (vertical) {
      align = DWRITE_TEXT_ALIGNMENT_TRAILING;
    } else {
      valign = DWRITE_PARAGRAPH_ALIGNMENT_FAR;
    }
  } else {
    if (vertical) {
      align = DWRITE_TEXT_ALIGNMENT_LEADING;
    } else {
      valign = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
    }
  }

  wstring new_face = to_wide(font_face);

  if (new_face != face || face_size != font_size || new_bold != bold ||
      new_italic != italic || new_underline != underline ||
      new_strikeout != strikeout || vertical != new_vertical) {
    face = new_face;
    face_size = font_size;
    bold = new_bold;
    italic = new_italic;
    underline = new_underline;
    strikeout = new_strikeout;

    vertical = new_vertical;

    UpdateFont();
  }

  /* ----------------------------- */

  new_color = rgb_to_bgr(new_color);
  new_color2 = rgb_to_bgr(new_color2);
  new_color3 = rgb_to_bgr(new_color3);
  new_color4 = rgb_to_bgr(new_color4);
  new_o_color = rgb_to_bgr(new_o_color);
  new_bk_color = rgb_to_bgr(new_bk_color);

  color = new_color;
  opacity = new_opacity;
  color2 = new_color2;
  color3 = new_color3;
  color4 = new_color4;
  opacity2 = new_opacity2;
  gradient_dir = new_grad_dir;

  if (strcmp(gradient_str, S_GRADIENT_NONE) == 0) {
    gradient_count = 0;
  } else if (strcmp(gradient_str, S_GRADIENT_TWO) == 0) {
    gradient_count = 2;
  } else if (strcmp(gradient_str, S_GRADIENT_THREE) == 0) {
    gradient_count = 3;
  } else {
    gradient_count = 4;
  }

  bk_color = new_bk_color;
  bk_opacity = new_bk_opacity;
  use_extents = new_extents;
  extents_cx = n_extents_cx;
  extents_cy = n_extents_cy;

  read_from_file = new_use_file;

  chatlog_mode = new_chat_mode;
  chatlog_lines = new_chat_lines;

  if (read_from_file) {
    file = new_file;
    file_timestamp = get_modified_timestamp(new_file);
    LoadFileText();
  } else {
    text = to_wide(GetMainString(new_text));
  }

  use_outline = new_outline;
  outline_color = new_o_color;
  outline_opacity = new_o_opacity;
  outline_size = roundf(float(new_o_size));

  RenderText();
  update_time_elapsed = 0.0f;

  /* ----------------------------- */

  obs_data_release(font_obj);
}

inline void TextSource::Tick(float seconds) {
  if (!read_from_file) return;

  update_time_elapsed += seconds;

  if (update_time_elapsed >= 1.f) {
    time_t t = get_modified_timestamp(file.c_str());
    update_time_elapsed = 0.f;

    if (file_timestamp != t) {
      LoadFileText();
      RenderText();
      file_timestamp = t;
    }
  }
}

inline void TextSource::Render() {
  if (!tex) return;
  gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
  gs_technique_t *tech = gs_effect_get_technique(effect, "Draw");
  gs_technique_begin(tech);
  gs_technique_begin_pass(tech, 0);

  gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), tex);
  gs_draw_sprite(tex, 0, cx, cy);

  gs_technique_end_pass(tech);
  gs_technique_end(tech);
}

/* ------------------------------------------------------------------------- */

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-text", "en-US")

#define set_vis(var, val, show)               \
  do {                                        \
    p = obs_properties_get(props, val);       \
    obs_property_set_visible(p, var == show); \
  } while (false)

static bool use_file_changed(obs_properties_t *props, obs_property_t *p,
                             obs_data_t *s) {
  bool use_file = obs_data_get_bool(s, S_USE_FILE);

  set_vis(use_file, S_TEXT, false);
  set_vis(use_file, S_FILE, true);
  return true;
}

static bool outline_changed(obs_properties_t *props, obs_property_t *p,
                            obs_data_t *s) {
  bool outline = obs_data_get_bool(s, S_OUTLINE);

  set_vis(outline, S_OUTLINE_SIZE, true);
  set_vis(outline, S_OUTLINE_COLOR, true);
  set_vis(outline, S_OUTLINE_OPACITY, true);
  return true;
}

static bool chatlog_mode_changed(obs_properties_t *props, obs_property_t *p,
                                 obs_data_t *s) {
  bool chatlog_mode = obs_data_get_bool(s, S_CHATLOG_MODE);

  set_vis(chatlog_mode, S_CHATLOG_LINES, true);
  return true;
}

static bool gradient_changed(obs_properties_t *props, obs_property_t *p,
                             obs_data_t *s) {
  const char *gradient_str = obs_data_get_string(s, S_GRADIENT);
  int mode = 0;
  if (strcmp(gradient_str, S_GRADIENT_NONE) == 0) {
    mode = 0;
  } else if (strcmp(gradient_str, S_GRADIENT_TWO) == 0) {
    mode = 2;
  } else if (strcmp(gradient_str, S_GRADIENT_THREE) == 0) {
    mode = 3;
  } else {
    mode = 4;
  }

  bool gradient_color = (mode > 0);
  bool gradient_color2 = (mode > 2);
  bool gradient_color3 = (mode > 3);

  set_vis(gradient_color, S_GRADIENT_COLOR, true);
  set_vis(gradient_color2, S_GRADIENT_COLOR2, true);
  set_vis(gradient_color3, S_GRADIENT_COLOR3, true);
  set_vis(gradient_color, S_GRADIENT_OPACITY, true);
  set_vis(gradient_color, S_GRADIENT_DIR, true);

  return true;
}

static bool extents_modified(obs_properties_t *props, obs_property_t *p,
                             obs_data_t *s) {
  bool use_extents = obs_data_get_bool(s, S_EXTENTS);

  set_vis(use_extents, S_EXTENTS_CX, true);
  set_vis(use_extents, S_EXTENTS_CY, true);
  set_vis(use_extents, S_EXTENTS_WRAP, true);
  return true;
}

#undef set_vis

static obs_properties_t *get_properties(void *data) {
  TextSource *s = reinterpret_cast<TextSource *>(data);
  string path;

  obs_properties_t *props = obs_properties_create();
  obs_property_t *p;

  obs_properties_add_font(props, S_FONT, T_FONT);

  p = obs_properties_add_bool(props, S_USE_FILE, T_USE_FILE);
  obs_property_set_modified_callback(p, use_file_changed);

  string filter;
  filter += T_FILTER_TEXT_FILES;
  filter += " (*.txt);;";
  filter += T_FILTER_ALL_FILES;
  filter += " (*.*)";

  if (s && !s->file.empty()) {
    const char *slash;

    path = s->file;
    replace(path.begin(), path.end(), '\\', '/');
    slash = strrchr(path.c_str(), '/');
    if (slash) path.resize(slash - path.c_str() + 1);
  }

  obs_properties_add_text(props, S_TEXT, T_TEXT, OBS_TEXT_MULTILINE);
  obs_properties_add_path(props, S_FILE, T_FILE, OBS_PATH_FILE, filter.c_str(),
                          path.c_str());

  obs_properties_add_bool(props, S_VERTICAL, T_VERTICAL);
  obs_properties_add_color(props, S_COLOR, T_COLOR);
  p = obs_properties_add_int_slider(props, S_OPACITY, T_OPACITY, 0, 100, 1);
  obs_property_int_set_suffix(p, "%");

  /*p = obs_properties_add_bool(props, S_GRADIENT, T_GRADIENT);
  obs_property_set_modified_callback(p, gradient_changed);*/

  p = obs_properties_add_list(props, S_GRADIENT, T_GRADIENT,
                              OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

  obs_property_list_add_string(p, T_GRADIENT_NONE, S_GRADIENT_NONE);
  obs_property_list_add_string(p, T_GRADIENT_TWO, S_GRADIENT_TWO);
  obs_property_list_add_string(p, T_GRADIENT_THREE, S_GRADIENT_THREE);
  obs_property_list_add_string(p, T_GRADIENT_FOUR, S_GRADIENT_FOUR);

  obs_property_set_modified_callback(p, gradient_changed);

  obs_properties_add_color(props, S_GRADIENT_COLOR, T_GRADIENT_COLOR);
  obs_properties_add_color(props, S_GRADIENT_COLOR2, T_GRADIENT_COLOR2);
  obs_properties_add_color(props, S_GRADIENT_COLOR3, T_GRADIENT_COLOR3);
  p = obs_properties_add_int_slider(props, S_GRADIENT_OPACITY,
                                    T_GRADIENT_OPACITY, 0, 100, 1);
  obs_property_int_set_suffix(p, "%");
  obs_properties_add_float_slider(props, S_GRADIENT_DIR, T_GRADIENT_DIR, 0, 360,
                                  0.1);

  obs_properties_add_color(props, S_BKCOLOR, T_BKCOLOR);
  p = obs_properties_add_int_slider(props, S_BKOPACITY, T_BKOPACITY, 0, 100, 1);
  obs_property_int_set_suffix(p, "%");

  p = obs_properties_add_list(props, S_ALIGN, T_ALIGN, OBS_COMBO_TYPE_LIST,
                              OBS_COMBO_FORMAT_STRING);
  obs_property_list_add_string(p, T_ALIGN_LEFT, S_ALIGN_LEFT);
  obs_property_list_add_string(p, T_ALIGN_CENTER, S_ALIGN_CENTER);
  obs_property_list_add_string(p, T_ALIGN_RIGHT, S_ALIGN_RIGHT);

  p = obs_properties_add_list(props, S_VALIGN, T_VALIGN, OBS_COMBO_TYPE_LIST,
                              OBS_COMBO_FORMAT_STRING);
  obs_property_list_add_string(p, T_VALIGN_TOP, S_VALIGN_TOP);
  obs_property_list_add_string(p, T_VALIGN_CENTER, S_VALIGN_CENTER);
  obs_property_list_add_string(p, T_VALIGN_BOTTOM, S_VALIGN_BOTTOM);

  p = obs_properties_add_bool(props, S_OUTLINE, T_OUTLINE);
  obs_property_set_modified_callback(p, outline_changed);

  obs_properties_add_int(props, S_OUTLINE_SIZE, T_OUTLINE_SIZE, 1, 20, 1);
  obs_properties_add_color(props, S_OUTLINE_COLOR, T_OUTLINE_COLOR);
  p = obs_properties_add_int_slider(props, S_OUTLINE_OPACITY, T_OUTLINE_OPACITY,
                                    0, 100, 1);
  obs_property_int_set_suffix(p, "%");

  p = obs_properties_add_bool(props, S_CHATLOG_MODE, T_CHATLOG_MODE);
  obs_property_set_modified_callback(p, chatlog_mode_changed);

  obs_properties_add_int(props, S_CHATLOG_LINES, T_CHATLOG_LINES, 1, 1000, 1);

  p = obs_properties_add_bool(props, S_EXTENTS, T_EXTENTS);
  obs_property_set_modified_callback(p, extents_modified);

  obs_properties_add_int(props, S_EXTENTS_CX, T_EXTENTS_CX, 32, 8000, 1);
  obs_properties_add_int(props, S_EXTENTS_CY, T_EXTENTS_CY, 32, 8000, 1);
  obs_properties_add_bool(props, S_EXTENTS_WRAP, T_EXTENTS_WRAP);

  return props;
}

bool obs_module_load(void) {
  obs_source_info si = {};
  si.id = "text_directwrite";
  si.icon_type = OBS_ICON_TYPE_TEXT;
  si.type = OBS_SOURCE_TYPE_INPUT;
  si.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
  si.get_properties = get_properties;

  si.get_name = [](void *) { return obs_module_text("TextDirectWrite"); };
  si.create = [](obs_data_t *settings, obs_source_t *source) -> void * {
    return new TextSource(source, settings);
  };
  si.destroy = [](void *data) { delete reinterpret_cast<TextSource *>(data); };
  si.get_width = [](void *data) {
    return reinterpret_cast<TextSource *>(data)->cx;
  };
  si.get_height = [](void *data) {
    return reinterpret_cast<TextSource *>(data)->cy;
  };
  si.get_defaults = [](obs_data_t *settings) {
    obs_data_t *font_obj = obs_data_create();
    obs_data_set_default_string(font_obj, "face", "Arial");
    obs_data_set_default_int(font_obj, "size", 36);

    obs_data_set_default_obj(settings, S_FONT, font_obj);
    obs_data_set_default_string(settings, S_ALIGN, S_ALIGN_LEFT);
    obs_data_set_default_string(settings, S_VALIGN, S_VALIGN_TOP);
    obs_data_set_default_int(settings, S_COLOR, 0xFFFFFF);
    obs_data_set_default_int(settings, S_OPACITY, 100);
    obs_data_set_default_int(settings, S_GRADIENT_COLOR, 0xFFFFFF);
    obs_data_set_default_int(settings, S_GRADIENT_COLOR2, 0xFFFFFF);
    obs_data_set_default_int(settings, S_GRADIENT_COLOR3, 0xFFFFFF);
    obs_data_set_default_int(settings, S_GRADIENT_OPACITY, 100);
    obs_data_set_default_double(settings, S_GRADIENT_DIR, 90.0);
    obs_data_set_default_int(settings, S_BKCOLOR, 0x000000);
    obs_data_set_default_int(settings, S_BKOPACITY, 0);
    obs_data_set_default_int(settings, S_OUTLINE_SIZE, 2);
    obs_data_set_default_int(settings, S_OUTLINE_COLOR, 0xFFFFFF);
    obs_data_set_default_int(settings, S_OUTLINE_OPACITY, 100);
    obs_data_set_default_int(settings, S_CHATLOG_LINES, 6);
    obs_data_set_default_bool(settings, S_EXTENTS_WRAP, true);
    obs_data_set_default_int(settings, S_EXTENTS_CX, 100);
    obs_data_set_default_int(settings, S_EXTENTS_CY, 100);

    obs_data_release(font_obj);
  };
  si.update = [](void *data, obs_data_t *settings) {
    reinterpret_cast<TextSource *>(data)->Update(settings);
  };
  si.video_tick = [](void *data, float seconds) {
    reinterpret_cast<TextSource *>(data)->Tick(seconds);
  };
  si.video_render = [](void *data, gs_effect_t *effect) {
    reinterpret_cast<TextSource *>(data)->Render();
  };

  obs_register_source(&si);

  return true;
}

void obs_module_unload(void) {}
