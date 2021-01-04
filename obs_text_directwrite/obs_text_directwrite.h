#pragma once

#include <math.h>
#include <obs-module.h>
#include <sys/stat.h>
#include <util/platform.h>
#include <windows.h>

#include <algorithm>
#include <memory>
#include <string>
#include <util/util.hpp>

#include "CustomTextRenderer.h"

using namespace std;

#ifndef clamp
#define clamp(val, min_val, max_val) \
  if (val < min_val)                 \
    val = min_val;                   \
  else if (val > max_val)            \
    val = max_val;
#endif

#define MIN_SIZE_CX 2.0
#define MIN_SIZE_CY 2.0
#define MAX_SIZE_CX 4096.0
#define MAX_SIZE_CY 4096.0

/* ------------------------------------------------------------------------- */

constexpr auto S_FONT = "font";
constexpr auto S_USE_FILE = "read_from_file";
constexpr auto S_FILE = "file";
constexpr auto S_TEXT = "text";
constexpr auto S_COLOR = "color";
constexpr auto S_GRADIENT = "gradient";
constexpr auto S_GRADIENT_NONE = "none";
constexpr auto S_GRADIENT_TWO = "two_colors";
constexpr auto S_GRADIENT_THREE = "three_colors";
constexpr auto S_GRADIENT_FOUR = "four_colors";
constexpr auto S_GRADIENT_COLOR = "gradient_color";
constexpr auto S_GRADIENT_COLOR2 = "gradient_color2";
constexpr auto S_GRADIENT_COLOR3 = "gradient_color3";
constexpr auto S_GRADIENT_DIR = "gradient_dir";
constexpr auto S_GRADIENT_OPACITY = "gradient_opacity";
constexpr auto S_ALIGN = "align";
constexpr auto S_VALIGN = "valign";
constexpr auto S_OPACITY = "opacity";
constexpr auto S_BKCOLOR = "bk_color";
constexpr auto S_BKOPACITY = "bk_opacity";
//#define S_VERTICAL                      "vertical"
constexpr auto S_OUTLINE = "outline";
constexpr auto S_OUTLINE_SIZE = "outline_size";
constexpr auto S_OUTLINE_COLOR = "outline_color";
constexpr auto S_OUTLINE_OPACITY = "outline_opacity";
constexpr auto S_CHATLOG_MODE = "chatlog";
constexpr auto S_CHATLOG_LINES = "chatlog_lines";
constexpr auto S_EXTENTS = "extents";
constexpr auto S_EXTENTS_CX = "extents_cx";
constexpr auto S_EXTENTS_CY = "extents_cy";

constexpr auto S_ALIGN_LEFT = "left";
constexpr auto S_ALIGN_CENTER = "center";
constexpr auto S_ALIGN_RIGHT = "right";

constexpr auto S_VALIGN_TOP = "top";
constexpr auto S_VALIGN_CENTER = S_ALIGN_CENTER;
constexpr auto S_VALIGN_BOTTOM = "bottom";

#define T_(v) obs_module_text(v)
#define T_FONT T_("Font")
#define T_USE_FILE T_("ReadFromFile")
#define T_FILE T_("TextFile")
#define T_TEXT T_("Text")
#define T_COLOR T_("Color")
#define T_GRADIENT T_("Gradient")
#define T_GRADIENT_NONE T_("Gradient.None")
#define T_GRADIENT_TWO T_("Gradient.TwoColors")
#define T_GRADIENT_THREE T_("Gradient.ThreeColors")
#define T_GRADIENT_FOUR T_("Gradient.FourColors")
#define T_GRADIENT_COLOR T_("Gradient.Color")
#define T_GRADIENT_COLOR2 T_("Gradient.Color2")
#define T_GRADIENT_COLOR3 T_("Gradient.Color3")
#define T_GRADIENT_DIR T_("Gradient.Direction")
#define T_GRADIENT_OPACITY T_("Gradient.Opacity")
#define T_ALIGN T_("Alignment")
#define T_VALIGN T_("VerticalAlignment")
#define T_OPACITY T_("Opacity")
#define T_BKCOLOR T_("BkColor")
#define T_BKOPACITY T_("BkOpacity")
//#define T_VERTICAL                      T_("Vertical")
#define T_OUTLINE T_("Outline")
#define T_OUTLINE_SIZE T_("Outline.Size")
#define T_OUTLINE_COLOR T_("Outline.Color")
#define T_OUTLINE_OPACITY T_("Outline.Opacity")
#define T_CHATLOG_MODE T_("ChatlogMode")
#define T_CHATLOG_LINES T_("ChatlogMode.Lines")
#define T_EXTENTS T_("UseCustomExtents")
#define T_EXTENTS_CX T_("Width")
#define T_EXTENTS_CY T_("Height")

#define T_FILTER_TEXT_FILES T_("Filter.TextFiles")
#define T_FILTER_ALL_FILES T_("Filter.AllFiles")

#define T_ALIGN_LEFT T_("Alignment.Left")
#define T_ALIGN_CENTER T_("Alignment.Center")
#define T_ALIGN_RIGHT T_("Alignment.Right")

#define T_VALIGN_TOP T_("VerticalAlignment.Top")
#define T_VALIGN_CENTER T_ALIGN_CENTER
#define T_VALIGN_BOTTOM T_("VerticalAlignment.Bottom")

/* ------------------------------------------------------------------------- */

static inline wstring to_wide(const char *utf8) {
  wstring text;

  size_t len = os_utf8_to_wcs(utf8, 0, nullptr, 0);
  text.resize(len);
  if (len) os_utf8_to_wcs(utf8, 0, &text[0], len + 1);

  return text;
}

static inline uint32_t rgb_to_bgr(uint32_t rgb) {
  return ((rgb & 0xFF) << 16) | (rgb & 0xFF00) | ((rgb & 0xFF0000) >> 16);
}

struct TextSource {
  obs_source_t *source = nullptr;
  gs_texture_t *tex = nullptr;

  uint32_t cx = 0;
  uint32_t cy = 0;

  IDWriteFactory4 *pDWriteFactory = nullptr;
  IDWriteTextFormat *pTextFormat = nullptr;
  ID2D1Factory *pD2DFactory = nullptr;
  ID2D1Brush *pFillBrush = nullptr;
  ID2D1Brush *pOutlineBrush = nullptr;
  ID2D1DCRenderTarget *pRT = nullptr;
  IDWriteTextLayout *pTextLayout = nullptr;
  CustomTextRenderer *pTextRenderer = nullptr;

  D2D1_RENDER_TARGET_PROPERTIES props = {};

  bool read_from_file = false;
  string file;
  time_t file_timestamp = 0;
  float update_time_elapsed = 0.f;

  wstring text;
  wstring face;
  int face_size = 0;
  uint32_t color = 0xFFFFFF;
  uint32_t color2 = 0xFFFFFF;
  uint32_t color3 = 0xFFFFFF;
  uint32_t color4 = 0xFFFFFF;

  int gradient_count = 0;
  D2D1_GRADIENT_STOP gradientStops[4];

  float gradient_dir = 0;
  float gradient_x = 0;
  float gradient_y = 0;
  float gradient2_x = 0;
  float gradient2_y = 0;

  uint32_t opacity = 100;
  uint32_t opacity2 = 100;
  uint32_t bk_color = 0;
  uint32_t bk_opacity = 0;

  DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_REGULAR;
  DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL;
  DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL;
  DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING;
  DWRITE_PARAGRAPH_ALIGNMENT valign = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;

  bool bold = false;
  bool italic = false;
  bool underline = false;
  bool strikeout = false;
  // bool vertical = false;

  bool use_outline = false;
  float outline_size = 0.f;
  uint32_t outline_color = 0;
  uint32_t outline_opacity = 100;

  bool use_extents = false;
  uint32_t extents_cx = 0;
  uint32_t extents_cy = 0;

  bool chatlog_mode = false;
  int chatlog_lines = 6;

  /* --------------------------- */

  inline TextSource(obs_source_t *source_, obs_data_t *settings)
      : source(source_) {
    InitializeDirectWrite();
    obs_source_update(source, settings);
  }

  inline ~TextSource() {
    if (tex) {
      obs_enter_graphics();
      gs_texture_destroy(tex);
      obs_leave_graphics();
    }
    ReleaseResource();
  }

  void UpdateFont();
  void CalculateGradientAxis(float width, float height);
  void InitializeDirectWrite();
  void ReleaseResource();
  void UpdateBrush(ID2D1RenderTarget *pRT, ID2D1Brush **ppOutlineBrush,
                   ID2D1Brush **ppFillBrush, float width, float height);
  void RenderText();
  void LoadFileText();

  const char *GetMainString(const char *str);

  inline void Update(obs_data_t *settings);
  inline void Tick(float seconds);
  inline void Render();
};

static time_t get_modified_timestamp(const char *filename) {
  struct stat stats;
  if (os_stat(filename, &stats) != 0) return -1;
  return stats.st_mtime;
}
