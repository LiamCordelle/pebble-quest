#include "lab.h"
#include "kit/kit_calc.h"
#include "kit/kit_core.h"
#include "quest_settings.h"
#ifdef LAB_HOST_RENDER
#include <stdio.h>
#include <stdlib.h>
#undef fprintf
#endif

// Quest: a 16-bit JRPG party status screen (BRIEF.md section 8.6).
//
// Four SNES-style windows on black:
//   TIME      the hero clock (Silkscreen 48) with the date
//   AREA      weather as the current "area": icon, condition, temp, rain/UV/sun
//   STATUS    the party member: LV, HP = battery, XP = steps, heart rate, NZ
//   DIALOGUE  one data-driven line of flavour text
// Window fills are a dithered blue gradient written straight into the 8-bit
// framebuffer. Everything else is pixel art on a 2 px grid, anti-aliasing off.

// --- Palette (judged on the colour-corrected screenshot) ---------------------

#define Q_BG GColorBlack
#define Q_WHITE GColorWhite
#define Q_GREY GColorFromHEX(0xaaaaaa)
#define Q_LABEL GColorFromHEX(0x55ffff)
#define Q_DIM GColorFromHEX(0xaaaaaa)
#define Q_SHADOW GColorBlack
#define Q_TRACK GColorBlack
#define Q_WARN GColorFromHEX(0xffaa55)
#define Q_CRIT GColorFromHEX(0xff5555)

// --- Layout (all on a 2 px grid) ----------------------------------------------

#define WIN_X 2
#define WIN_W 196
#define COL_L 12    // content left edge
#define COL_R 188   // content right edge

static const GRect WIN_TIME = {{WIN_X, 2}, {WIN_W, 64}};
static const GRect WIN_AREA = {{WIN_X, 68}, {WIN_W, 36}};
static const GRect WIN_STAT = {{WIN_X, 106}, {WIN_W, 78}};
static const GRect WIN_TALK = {{WIN_X, 186}, {WIN_W, 40}};

// --- Fonts ----------------------------------------------------------------------

typedef enum { QS8, QS16, QS48 } QSize;

static GFont s_time_font;   // Silkscreen Regular 48, digits only

// Per size: px per Silkscreen pixel, cap-top offset inside a text box, text
// box height. Measured on Emery: every glyph carries one font pixel of left
// bearing and one of trailing space, so ink starts one unit right of the box
// and is two units narrower than the measured text width.
static const int8_t Q_UNIT[] = {1, 2, 6};
static const int8_t Q_OFF_Y[] = {3, 6, 18};
static const int8_t Q_BOX_H[] = {16, 26, 60};

static GFont qfont(QSize size) {
  switch (size) {
    case QS8: return lab_font_pixel8();
    case QS16: return lab_font_pixel16();
    default: return s_time_font;
  }
}

static int ink_w(const char *s, QSize size) {
  if (!s || !s[0]) {
    return 0;
  }
  return lab_text_size(s, qfont(size)).w - 2 * Q_UNIT[size];
}

#define Q_TEXT_SHADOW 1
#define Q_GAP 6   // Silkscreen's own word space at 8 px

// Text with its ink's left edge at x and its cap top at cap_y, clipped (with an
// ellipsis) to max_w. Small text gets a one-font-pixel SNES drop shadow.
static void put_w(GContext *ctx, const char *s, QSize size, int x, int cap_y, int max_w,
                  GColor c) {
  GRect box = GRect(x - Q_UNIT[size], cap_y - Q_OFF_Y[size], max_w + 2 * Q_UNIT[size],
                    Q_BOX_H[size]);
#if Q_TEXT_SHADOW
  if (size != QS48) {
    const int o = Q_UNIT[size] > 1 ? 2 : 1;
    lab_text(ctx, s, qfont(size), GRect(box.origin.x + o, box.origin.y + o, box.size.w,
                                        box.size.h), GTextAlignmentLeft, Q_SHADOW);
  }
#endif
  lab_text(ctx, s, qfont(size), box, GTextAlignmentLeft, c);
}

static void put(GContext *ctx, const char *s, QSize size, int x, int cap_y, GColor c) {
#ifdef LAB_HOST_RENDER
  if(size==QS16 && (x<10 || x+ink_w(s,size)>190 || cap_y+12>225)) {
    fprintf(stderr,"Quest text overflow: %s at %d,%d width %d\n",s,x,cap_y,ink_w(s,size));
    exit(8);
  }
#endif
  put_w(ctx, s, size, x, cap_y, 200, c);
}

// Right-aligned: the last ink column is right - 1.
static int put_r(GContext *ctx, const char *s, QSize size, int right, int cap_y, GColor c) {
  int x = right - ink_w(s, size);
  put(ctx, s, size, x, cap_y, c);
  return x;
}

static void put_c(GContext *ctx, const char *s, QSize size, int cx, int cap_y, GColor c) {
  put(ctx, s, size, cx - ink_w(s, size) / 2, cap_y, c);
}

// --- Primitives -------------------------------------------------------------------

static void fill(GContext *ctx, int x, int y, int w, int h, GColor c) {
  graphics_context_set_fill_color(ctx, c);
  graphics_fill_rect(ctx, GRect(x, y, w, h), 0, GCornerNone);
}

static GColor sprite_color(char ch) {
  switch (ch) {
    case 'W': return GColorWhite;
    case 'G': return GColorFromHEX(0xaaaaaa);
    case 'D': return GColorFromHEX(0x555555);
    case 'Y': return GColorFromHEX(0xffff55);
    case 'O': return GColorFromHEX(0xffaa00);
    case 'B': return GColorFromHEX(0x55ffff);
    case 'L': return GColorFromHEX(0xffff00);
    case 'M': return GColorFromHEX(0xffffaa);
    case 'R': return GColorFromHEX(0xff0055);
    default: return GColorBlack;
  }
}

// Pixel-art sprite from strings ('.' = transparent), drawn as row runs.
// A non-clear tint paints every cell in that one colour (dimmed states).
static void sprite(GContext *ctx, const char *const *rows, int n, int x, int y, int unit,
                   GColor tint) {
  bool tinted = !gcolor_equal(tint, GColorClear);
  for (int j = 0; j < n; j++) {
    const char *r = rows[j];
    int i = 0;
    while (r[i]) {
      char ch = r[i];
      int start = i;
      while (r[i] == ch) {
        i++;
      }
      if (ch != '.') {
        fill(ctx, x + start * unit, y + j * unit, (i - start) * unit, unit,
             tinted ? tint : sprite_color(ch));
      }
    }
  }
}

#define SPRITE(ctx, rows, x, y, unit, tint) \
  sprite(ctx, rows, (int)(sizeof(rows) / sizeof(rows[0])), x, y, unit, tint)

// --- Sprites (2 px units unless noted) ----------------------------------------------

static const char *const ICON_SUN[] = {
  ".....O......",
  "..O.....O...",
  "....YYY.....",
  "...YYYYY....",
  "O.YYYYYYY.O.",
  "...YYYYY....",
  "....YYY.....",
  "..O.....O...",
  ".....O......",
};
static const char *const ICON_MOON[] = {
  "....MMM.....",
  "..MMMM......",
  ".MMMM.......",
  ".MMMM.......",
  ".MMMM.......",
  ".MMMMM......",
  "..MMMMMMM...",
  "....MMMM....",
  "............",
};
static const char *const ICON_PARTLY[] = {
  ".......O....",
  "....O.....O.",
  "......YYY...",
  ".....YYYYY..",
  "...WWWYYYYYO",
  ".WWWWWWWYY..",
  "WWWWWWWWWO.O",
  "WWWWWWWWW...",
  ".GGGGGGGG...",
};
static const char *const ICON_PARTLY_NIGHT[] = {
  "......MMM...",
  "....MMMM....",
  "...MMMM.....",
  "...WWWM.....",
  ".WWWWWWWM...",
  "WWWWWWWWWMM.",
  "WWWWWWWWWWM.",
  "WWWWWWWWW...",
  ".GGGGGGGG...",
};
static const char *const ICON_CLOUD[] = {
  "............",
  "....WWW.....",
  "..WWWWWWW...",
  ".WWWWWWWWWW.",
  "WWWWWWWWWWWW",
  "WWWWWWWWWWWW",
  ".GGGGGGGGGG.",
  "............",
  "............",
};
static const char *const ICON_RAIN[] = {
  "....WWW.....",
  "..WWWWWWW...",
  ".WWWWWWWWWW.",
  "WWWWWWWWWWWW",
  ".GGGGGGGGGG.",
  "............",
  "..B...B...B.",
  ".B...B...B..",
  "............",
};
static const char *const ICON_SNOW[] = {
  "....WWW.....",
  "..WWWWWWW...",
  ".WWWWWWWWWW.",
  "WWWWWWWWWWWW",
  ".GGGGGGGGGG.",
  "............",
  "...W....W...",
  "..WWW..WWW..",
  "...W....W...",
};
static const char *const ICON_STORM[] = {
  "....GGG.....",
  "..GGGGGGG...",
  ".GGGGGGGGGG.",
  "GGGGGGGGGGGG",
  ".DDDDLLDDDD.",
  ".....LL.....",
  "....LL......",
  ".....L......",
  "............",
};
static const char *const ICON_FOG[] = {
  "............",
  ".WWWWWWWWW..",
  "............",
  "..WWWWWWWWWW",
  "............",
  ".WWWWWWWWW..",
  "............",
  "...GGGGGGGG.",
  "............",
};
static const char *const ICON_UNKNOWN[] = {
  "....GGGG....",
  "...GG..GG...",
  ".......GG...",
  "......GG....",
  ".....GG.....",
  ".....GG.....",
  "............",
  ".....GG.....",
  "............",
};
static const char *const HEART[] = {
  ".RR.RR.",
  "RWRRRRR",
  "RRRRRRR",
  ".RRRRR.",
  "..RRR..",
  "...R...",
};

static const char *const DROP[] = {     // 1 px units
  "..B..",
  "..B..",
  ".BBB.",
  "BBBBB",
  "BBBBB",
  ".BBB.",
};


// --- Windows ----------------------------------------------------------------------

// 2x2 ordered dither: a 4-level ramp between neighbouring palette blues.
static const uint8_t BAYER2[2][2] = {{0, 2}, {3, 1}};

static void fb_gradient(GBitmap *fb, GRect r) {
  const uint8_t stops[3] = {
    GColorFromHEX(0x0000ff).argb, GColorFromHEX(0x0000aa).argb, GColorFromHEX(0x000055).argb
  };
  const int segs = 2;
  for (int yy = 0; yy < r.size.h; yy++) {
    const int y = r.origin.y + yy;
    GBitmapDataRowInfo row = gbitmap_get_data_row_info(fb, y);
    const int t = yy * segs * 4 / r.size.h;
    const int level = t % 4;
    const uint8_t a = stops[t / 4];
    const uint8_t b = stops[t / 4 + 1];
    int x0 = r.origin.x > row.min_x ? r.origin.x : row.min_x;
    int x1 = r.origin.x + r.size.w - 1;
    if (x1 > row.max_x) {
      x1 = row.max_x;
    }
    for (int x = x0; x <= x1; x++) {
      row.data[x] = BAYER2[y & 1][x & 1] < level ? b : a;
    }
  }
}

static void ring(GContext *ctx, GRect r, int t, GColor c) {
  fill(ctx, r.origin.x, r.origin.y, r.size.w, t, c);
  fill(ctx, r.origin.x, r.origin.y + r.size.h - t, r.size.w, t, c);
  fill(ctx, r.origin.x, r.origin.y, t, r.size.h, c);
  fill(ctx, r.origin.x + r.size.w - t, r.origin.y, t, r.size.h, c);
}

// White 2 px rim + grey 1 px inner line, with stepped rounded corners.
static void window_frame(GContext *ctx, GRect r) {
  static const int8_t CUT[][2] = {{0, 0}, {1, 0}, {2, 0}, {0, 1}, {0, 2}, {1, 1}};
  static const int8_t RIM[][2] = {{2, 1}, {1, 2}, {2, 2}, {3, 1}, {1, 3}};
  ring(ctx, r, 2, Q_WHITE);
  ring(ctx, grect_inset(r, GEdgeInsets(2)), 1, Q_GREY);
  for (int k = 0; k < 4; k++) {
    const int cx = (k & 1) ? r.origin.x + r.size.w - 1 : r.origin.x;
    const int cy = (k & 2) ? r.origin.y + r.size.h - 1 : r.origin.y;
    const int dx = (k & 1) ? -1 : 1;
    const int dy = (k & 2) ? -1 : 1;
    for (int i = 0; i < 6; i++) {
      fill(ctx, cx + dx * CUT[i][0], cy + dy * CUT[i][1], 1, 1, Q_BG);
    }
    for (int i = 0; i < 5; i++) {
      fill(ctx, cx + dx * RIM[i][0], cy + dy * RIM[i][1], 1, 1, Q_WHITE);
    }
    fill(ctx, cx + dx * 3, cy + dy * 3, 1, 1, Q_GREY);
  }
}

// --- Gauges -----------------------------------------------------------------------

// A 6 px gauge: black track, fill with a 2 px highlight on top, optional
// 1 px notches that split it into `segs` equal cells.
static void gauge(GContext *ctx, int x, int y, int w, int num, int den, GColor body,
                  GColor shine, int segs) {
  fill(ctx, x, y, w, 6, Q_TRACK);
  if (den <= 0) {
    den = 1;
  }
  int fw = (int)((int64_t)w * lab_clamp(num, 0, den) / den);
  fill(ctx, x, y, fw, 6, body);
  fill(ctx, x, y, fw, 2, shine);
  for (int k = 1; k < segs; k++) {
    fill(ctx, x + w * k / segs, y, 1, 6, Q_TRACK);
  }
}

// --- Time -------------------------------------------------------------------------

static void draw_time(GContext *ctx, const LabData *d, int cx, int cap_y) {
  char hh[4];
  char mm[4];
  int hour = d->now.tm_hour;
  if (d->is_24h) {
    snprintf(hh, sizeof(hh), "%02d", hour);
  } else {
    hour %= 12;
    snprintf(hh, sizeof(hh), "%d", hour == 0 ? 12 : hour);
  }
  snprintf(mm, sizeof(mm), "%02d", d->now.tm_min);

  const int u = Q_UNIT[QS48];
  const int wh = ink_w(hh, QS48);
  const int wm = ink_w(mm, QS48);
  // Colon gaps match Silkscreen's own two-unit digit spacing.
  const int x = (cx - (wh + 5 * u + wm) / 2) & ~1;
  for (int pass = 0; pass < 2; pass++) {
    const int o = pass == 0 ? 2 : 0;
    const GColor c = pass == 0 ? Q_SHADOW : Q_WHITE;
    put(ctx, hh, QS48, x + o, cap_y + o, c);
    fill(ctx, x + wh + 2 * u + o, cap_y + u + o, u, u, c);       // hand-placed colon
    fill(ctx, x + wh + 2 * u + o, cap_y + 3 * u + o, u, u, c);
    put(ctx, mm, QS48, x + wh + 5 * u + o, cap_y + o, c);
  }
}

static void draw_time_window(GContext *ctx, GRect w, const LabData *d) {
  draw_time(ctx, d, 100, w.origin.y + 8);
  char date[16];
  snprintf(date, sizeof(date), "%s %d %s", lab_day_abbr(d->now.tm_wday), d->now.tm_mday,
           lab_month_abbr(d->now.tm_mon));
  lab_upper(date);
  if (d->is_24h) put_c(ctx, date, QS16, 100, w.origin.y + 44, Q_WHITE);
  else put(ctx, date, QS16, COL_L, w.origin.y + 44, Q_WHITE);
  if (!d->is_24h) {
    put_r(ctx, d->now.tm_hour < 12 ? "AM" : "PM", QS16, COL_R, w.origin.y + 44, Q_LABEL);
  }
}

// --- Area (weather) --------------------------------------------------------------

static bool is_night(const LabData *d) {
  return kit_daylight_state(d->weather_ok,d->minute_of_day,d->sunrise_min,d->sunset_min)==KitDaylightNight;
}


static void draw_weather_icon(GContext *ctx, const LabData *d, int x, int y) {
  if (!d->weather_ok) { SPRITE(ctx, ICON_UNKNOWN, x, y, 2, GColorClear); return; }
  bool night = is_night(d);
  LabWxKind kind = lab_weather_kind(d->weather_code);
  if (kind == LabWxClear) {
    if (night) { SPRITE(ctx, ICON_MOON, x, y, 2, GColorClear); }
    else { SPRITE(ctx, ICON_SUN, x, y, 2, GColorClear); }
    return;
  }
  if (kind == LabWxPartly) {
    if (night) { SPRITE(ctx, ICON_PARTLY_NIGHT, x, y, 2, GColorClear); }
    else { SPRITE(ctx, ICON_PARTLY, x, y, 2, GColorClear); }
    return;
  }
  // Keep precipitation/visibility symbols; night clouds are silver, with a moon.
  switch (kind) {
    case LabWxFog: SPRITE(ctx, ICON_FOG, x, y, 2, GColorClear); break;
    case LabWxRain: SPRITE(ctx, ICON_RAIN, x, y, 2, GColorClear); break;
    case LabWxSnow: SPRITE(ctx, ICON_SNOW, x, y, 2, GColorClear); break;
    case LabWxStorm: SPRITE(ctx, ICON_STORM, x, y, 2, GColorClear); break;
    default: SPRITE(ctx, ICON_CLOUD, x, y, 2, GColorClear); break;
  }
  if (night) {
    fill(ctx,x+18,y,6,2,GColorFromHEX(0xffffaa));
    fill(ctx,x+18,y+2,4,2,GColorFromHEX(0xffffaa));
    fill(ctx,x+20,y+4,4,2,GColorFromHEX(0xffffaa));
  }
}

static const char *condition(const LabData *d) {
  if (!d->weather_ok) return "NO DATA";
  switch (lab_weather_kind(d->weather_code)) {
    case LabWxClear: return "CLEAR";
    case LabWxPartly: return "PARTLY";
    case LabWxCloudy: return "CLOUD";
    case LabWxFog: return "FOG";
    case LabWxRain: return "RAIN";
    case LabWxSnow: return "SNOW";
    case LabWxStorm: return "STORM";
    default: return "UNKNOWN";
  }
}

static void draw_area_window(GContext *ctx, GRect w, const LabData *d) {
  char temp[12],rain[16],uv[12];
  GColor color=d->weather_ok ? Q_WHITE : Q_DIM;
  draw_weather_icon(ctx,d,COL_L,w.origin.y+8);
  put(ctx,condition(d),QS16,42,w.origin.y+6,color);
  if (d->weather_ok) {
    int t=d->temp_fahrenheit ? kit_temperature(d->temp,true) : d->temp;
    snprintf(temp,sizeof(temp),"%d",t);
    snprintf(rain,sizeof(rain),"%d%%",lab_clamp(d->rain_pct,0,100));
    snprintf(uv,sizeof(uv),"UV %d",is_night(d)?0:lab_clamp((d->uv_x10+5)/10,0,99));
  } else {
    snprintf(temp,sizeof(temp),"--");
    snprintf(rain,sizeof(rain),"--%%"); snprintf(uv,sizeof(uv),"UV --");
  }
  put_r(ctx,temp,QS16,COL_R-8,w.origin.y+6,color);
  fill(ctx,COL_R-4,w.origin.y+6,2,2,color);
  fill(ctx,COL_R-6,w.origin.y+8,2,2,color);
  fill(ctx,COL_R-2,w.origin.y+8,2,2,color);
  fill(ctx,COL_R-4,w.origin.y+10,2,2,color);
  SPRITE(ctx,DROP,42,w.origin.y+21,1,d->weather_ok?GColorClear:Q_DIM);
  put(ctx,rain,QS16,54,w.origin.y+20,color);
  put_r(ctx,uv,QS16,COL_R,w.origin.y+20,color);
}

static int goal_days(const LabData *d) {
  return kit_goal_days(d->steps_by_day,28,d->step_goal>0?d->step_goal:KIT_DEFAULT_STEP_GOAL);
}
static void status_chip(GContext *ctx,const char *text,int right,int cap_y,GColor color) {
  int width=ink_w(text,QS16)+8, x=right-width;
  fill(ctx,x,cap_y-2,width,14,color);
  lab_text(ctx,text,qfont(QS16),GRect(x+4-2,cap_y-6,width,26),GTextAlignmentLeft,Q_BG);
}
static void draw_status_window(GContext *ctx,GRect w,const LabData *d) {
  int y=w.origin.y+10; char value[24];
  put(ctx,quest_hero_name(),QS16,COL_L,y,Q_WHITE);
  if(d->steps_ok) snprintf(value,sizeof(value),"LV %d",lab_clamp(1+(int)((int64_t)d->steps*10/(d->step_goal>0?d->step_goal:KIT_DEFAULT_STEP_GOAL)),1,99));
  else snprintf(value,sizeof(value),"LV --");
  put_r(ctx,value,QS16,COL_R,y,Q_LABEL);
  int pct=lab_clamp(d->battery_pct,0,100);
  put(ctx,"HP",QS16,COL_L,y+16,Q_LABEL);
  gauge(ctx,44,y+18,56,pct,100,lab_battery_color(pct),
    pct<10?Q_CRIT:pct<40?GColorFromHEX(0xffff55):GColorFromHEX(0x55ff55),0);
  snprintf(value,sizeof(value),"%d/100",pct);
  put_r(ctx,value,QS16,COL_R,y+16,pct<10?Q_CRIT:pct<40?Q_WARN:Q_WHITE);
  put(ctx,"XP",QS16,COL_L,y+32,Q_LABEL);
  bool cleared=d->steps_ok && d->steps>=d->step_goal;
  gauge(ctx,44,y+34,56,d->steps_ok?d->steps:0,d->step_goal,
    cleared?GColorFromHEX(0xffff55):GColorFromHEX(0x00aaff),
    cleared?Q_WHITE:Q_LABEL,10);
  if(d->steps_ok) snprintf(value,sizeof(value),"%ld",(long)d->steps);
  else snprintf(value,sizeof(value),"--");
  put_r(ctx,value,QS16,COL_R,y+32,d->steps_ok?Q_WHITE:Q_DIM);
  SPRITE(ctx,HEART,COL_L,y+48,2,d->hr_ok?GColorClear:Q_DIM);
  if(d->hr_ok) snprintf(value,sizeof(value),"%d",d->heart_rate);
  else snprintf(value,sizeof(value),"--");
  put(ctx,value,QS16,30,y+48,d->hr_ok?Q_WHITE:Q_DIM);
  if(!d->bt_connected) status_chip(ctx,"SILENCE",COL_R,y+48,GColorFromHEX(0xffaaff));
  else if(d->charging) status_chip(ctx,"REGEN",COL_R,y+48,GColorFromHEX(0x55ff55));
  else if(d->hr_ok && d->heart_rate>=110) status_chip(ctx,"HASTE",COL_R,y+48,GColorFromHEX(0xffff55));
}

// Compact event clocks carry AM/PM and tomorrow explicitly, without a second local clock.
static void event_time(char *out,size_t n,int hour,int minute,bool h24,int delta) {
  kit_format_event_time(out,n,hour*60+minute,h24,delta,KitMeridiemCompact);
}
static int next_wet(const LabData *d) {
  return d->weather_ok ? kit_forecast_rain_next(d->rolling_rain,d->rolling_count,1,50) : -1;
}
static void rain_line(const LabData *d,char *out,size_t n) {
  if(!d->weather_ok) {snprintf(out,n,"RAIN --");return;}
  if(d->rain_pct>=50) {snprintf(out,n,"%s %d%% NOW",lab_weather_kind(d->weather_code)==LabWxSnow?"SNOW":"RAIN",d->rain_pct);return;}
  int wet=next_wet(d);
  if(wet<0) {snprintf(out,n,"NO RAIN AHEAD");return;}
  char t[16];
  if(d->rolling_day[wet]>0) {
    int h=d->rolling_hour[wet];
    if(d->is_24h)snprintf(t,sizeof(t),"%02dH+1",h);
    else snprintf(t,sizeof(t),"%d%c+1",h%12?h%12:12,h<12?'A':'P');
  } else event_time(t,sizeof(t),d->rolling_hour[wet],0,d->is_24h,0);
  snprintf(out,n,"RAIN %d%% %s",d->rolling_rain[wet],t);
}
static void sun_line(const LabData *d,char *out,size_t n) {
  if(!d->weather_ok || d->next_sun_min<0) {snprintf(out,n,"SUN --");return;}
  char t[16];event_time(t,sizeof(t),d->next_sun_min/60,d->next_sun_min%60,d->is_24h,d->next_sun_day_delta);
  snprintf(out,n,"%s %s",d->next_sun_is_set?"SET":"RISE",t);
}
static void compose_quip(const LabData *d,char *l1,char *l2,size_t n) {
  if(d->battery_pct<10 && !d->charging) {
    snprintf(l1,n,"HP CRITICAL!");snprintf(l2,n,"REST AT AN INN.");
  } else if(!d->bt_connected) {
    snprintf(l1,n,"PHONE LOST.");snprintf(l2,n,"SILENCE CAST.");
  } else if(d->weather_ok && lab_weather_kind(d->weather_code)==LabWxStorm) {
    snprintf(l1,n,"THUNDER NEAR.");snprintf(l2,n,"SEEK SHELTER.");
  } else if(d->weather_ok && (d->rain_pct>=50 || next_wet(d)>=0)) {
    rain_line(d,l1,n);snprintf(l2,n,lab_weather_kind(d->weather_code)==LabWxSnow?"WRAP UP WARM.":"PACK A CLOAK.");
  } else if(d->weather_ok && !is_night(d) && d->uv_x10>=60) {
    snprintf(l1,n,"SUN MAGIC HIGH.");snprintf(l2,n,"EQUIP A HAT.");
  } else if(d->steps_ok && d->steps>=d->step_goal) {
    snprintf(l1,n,"QUEST CLEARED!");snprintf(l2,n,"BONUS XP EARNED.");
  } else if(d->now.tm_hour>=21 || d->now.tm_hour<5) {
    snprintf(l1,n,"CAMP FOR NIGHT.");snprintf(l2,n,"TIME TO REST.");
  } else if(d->sleep_minutes>0 && d->now.tm_hour<10) {
    snprintf(l1,n,"REST %dH%02d",d->sleep_minutes/60,d->sleep_minutes%60);snprintf(l2,n,"ONWARD, HERO.");
  } else if(!d->steps_ok) {
    snprintf(l1,n,"NO XP RECORD.");snprintf(l2,n,"ONWARD, HERO.");
  } else {
    snprintf(l1,n,"%ld XP TO GO.",(long)(d->step_goal-d->steps));snprintf(l2,n,"ROAD GOES ON.");
  }
}
static void draw_dialogue_window(GContext *ctx,GRect w,const LabData *d) {
  char a[40],b[40];compose_quip(d,a,b,sizeof(a));
  put(ctx,a,QS16,COL_L,w.origin.y+8,Q_WHITE);
  put(ctx,b,QS16,COL_L,w.origin.y+24,Q_WHITE);
}
static void paint_windows(GContext *ctx,GRect bounds,const GRect *wins,int count) {
  graphics_context_set_antialiased(ctx,false);
  fill(ctx,0,0,bounds.size.w,bounds.size.h,Q_BG);
  GBitmap *fb=graphics_capture_frame_buffer(ctx);
  if(fb) {for(int i=0;i<count;i++)fb_gradient(fb,wins[i]);graphics_release_frame_buffer(ctx,fb);}
  for(int i=0;i<count;i++)window_frame(ctx,wins[i]);
}
static void draw_detail(GContext *ctx,GRect bounds,const LabData *d) {
  const GRect wins[]={{{2,2},{196,36}},{{2,40},{196,80}},{{2,122},{196,66}},{{2,190},{196,36}}};
  paint_windows(ctx,bounds,wins,4);
  char value[48];
  put(ctx,quest_hero_name(),QS16,COL_L,10,Q_WHITE);
  if(d->steps_ok)snprintf(value,sizeof(value),"LV %d",goal_days(d));else snprintf(value,sizeof(value),"LV --");
  put_r(ctx,value,QS16,COL_R,10,Q_LABEL);
  put_c(ctx,"CHARACTER SHEET",QS16,100,24,Q_LABEL);
  if(d->hr_min>0 && d->hr_max>0)snprintf(value,sizeof(value),"VIT %d-%d%s",d->hr_min,d->hr_max,d->hr_range_partial?"*":"");
  else snprintf(value,sizeof(value),"VIT --%s",d->hr_range_partial?"*":"");
  put(ctx,value,QS16,COL_L,50,Q_WHITE);
  if(d->steps_ok)snprintf(value,sizeof(value),"AGI %d MIN",d->active_minutes);else snprintf(value,sizeof(value),"AGI -- MIN");
  put(ctx,value,QS16,COL_L,66,Q_WHITE);
  if(d->steps_ok)snprintf(value,sizeof(value),"DIST %ld.%ldKM",(long)d->distance_m/1000,(long)(d->distance_m%1000)/100);else snprintf(value,sizeof(value),"DIST -- KM");
  put(ctx,value,QS16,COL_L,82,Q_WHITE);
  if(d->sleep_minutes>0)snprintf(value,sizeof(value),"REST %dH%02d",d->sleep_minutes/60,d->sleep_minutes%60);else snprintf(value,sizeof(value),"REST --");
  put(ctx,value,QS16,COL_L,98,Q_WHITE);
  put(ctx,"QUEST LOG",QS16,COL_L,130,Q_LABEL);
  rain_line(d,value,sizeof(value));put(ctx,value,QS16,COL_L,148,Q_WHITE);
  sun_line(d,value,sizeof(value));put(ctx,value,QS16,COL_L,166,Q_WHITE);
  char t[20];event_time(t,sizeof(t),d->world_hour,d->world_min,d->is_24h,0);
  snprintf(value,sizeof(value),"%s %s%s",d->world_label,t,d->world_day_delta>0?" +1":d->world_day_delta<0?" -1":"");
  put_c(ctx,value,QS16,100,198,Q_WHITE);
  put_c(ctx,"LV = GOAL DAYS",QS16,100,212,Q_LABEL);
}

// --- Frame ---------------------------------------------------------------------------

static void draw(GContext *ctx, GRect bounds, const LabData *d) {
  graphics_context_set_antialiased(ctx, false);
  fill(ctx, 0, 0, bounds.size.w, bounds.size.h, Q_BG);

  const GRect wins[] = {WIN_TIME, WIN_AREA, WIN_STAT, WIN_TALK};
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (fb) {
    for (int i = 0; i < 4; i++) {
      fb_gradient(fb, wins[i]);
    }
    graphics_release_frame_buffer(ctx, fb);
  }
  for (int i = 0; i < 4; i++) {
    window_frame(ctx, wins[i]);
  }

  draw_time_window(ctx, WIN_TIME, d);
  draw_area_window(ctx, WIN_AREA, d);
  draw_status_window(ctx, WIN_STAT, d);
  draw_dialogue_window(ctx, WIN_TALK, d);
}

static void load(void) {
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_QUEST_PIXEL_48));
}

static void unload(void) {
  if (s_time_font) {
    fonts_unload_custom_font(s_time_font);
    s_time_font = NULL;
  }
}

const LabDesign design_quest = {"Quest", load, unload, draw, draw_detail};
