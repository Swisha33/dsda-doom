/*
 * dsda-doom PS Vita launcher
 *
 * Lets the player pick an IWAD, then PWADs (in load order), then DEH/BEX
 * patches, and starts the engine (app0:dsda-doom.bin).
 *
 * The chosen command line is written to ux0:data/dsda-doom/launch_args.txt
 * (one argument per line) and read back by the engine at startup, so paths
 * with spaces work and nothing depends on how argv is forwarded by the OS.
 *
 * Controls:
 *   Up/Down       move          Left/Right   page up/down
 *   Cross         select / toggle
 *   Circle        back          Triangle     next step
 *   Square        move a selected PWAD/DEH up in the load order
 *   Start         start game (from any step once an IWAD is chosen)
 *
 * This file is part of the dsda-doom PS Vita port and is licensed under the
 * GNU General Public License, version 2 or later (same as dsda-doom).
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <psp2/appmgr.h>
#include <psp2/ctrl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>

#include <vita2d.h>

#ifndef DATA_DIR
#define DATA_DIR      "ux0:data/dsda-doom"
#endif
#define WAD_DIR       DATA_DIR "/wads"
#define CFG_FILE      DATA_DIR "/launcher.cfg"
#define ARGS_FILE     DATA_DIR "/launch_args.txt"
#define EXTRA_FILE    DATA_DIR "/extra_args.txt"
#define ENGINE_PATH   "app0:dsda-doom.bin"

#define MAX_FILES     512
#define MAX_PATH_LEN  512
#define VISIBLE_ROWS  17

#define COL_BG        RGBA8(0x14, 0x10, 0x10, 0xFF)
#define COL_PANEL     RGBA8(0x24, 0x1C, 0x1A, 0xFF)
#define COL_CURSOR    RGBA8(0x7A, 0x1E, 0x14, 0xFF)
#define COL_TEXT      RGBA8(0xE8, 0xE0, 0xD8, 0xFF)
#define COL_DIM       RGBA8(0x9A, 0x90, 0x88, 0xFF)
#define COL_ACCENT    RGBA8(0xF0, 0xA0, 0x30, 0xFF)
#define COL_OK        RGBA8(0x80, 0xD0, 0x70, 0xFF)

typedef enum { KIND_IWAD, KIND_PWAD, KIND_DEH } file_kind_t;

typedef struct {
  char name[256];      /* file name shown in the list */
  char path[MAX_PATH_LEN];
  file_kind_t kind;
  int order;           /* 0 = not selected, otherwise 1-based load order */
} entry_t;

typedef struct {
  entry_t items[MAX_FILES];
  int count;
  int cursor;
  int scroll;
} list_t;

enum { STEP_IWAD, STEP_PWAD, STEP_DEH, STEP_START, STEP_COUNT };

static list_t iwads, pwads, dehs;
static int chosen_iwad = -1;
static int step = STEP_IWAD;
static int start_cursor = 0;
static char status_msg[256];

static vita2d_pgf *font;

/* ------------------------------------------------------------------ */
/* small helpers                                                       */
/* ------------------------------------------------------------------ */

static int has_ext(const char *name, const char *ext)
{
  size_t n = strlen(name), e = strlen(ext);
  size_t i;

  if (n < e)
    return 0;
  for (i = 0; i < e; i++)
    if (tolower((unsigned char) name[n - e + i]) != tolower((unsigned char) ext[i]))
      return 0;
  return 1;
}

static int is_iwad(const char *path)
{
  char magic[4];
  SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
  int result = 0;

  if (fd < 0)
    return 0;
  if (sceIoRead(fd, magic, 4) == 4 && !memcmp(magic, "IWAD", 4))
    result = 1;
  sceIoClose(fd);
  return result;
}

static int cmp_entries(const void *a, const void *b)
{
  return strcasecmp(((const entry_t *) a)->name, ((const entry_t *) b)->name);
}

static void list_add(list_t *list, const char *dir, const char *name, file_kind_t kind)
{
  entry_t *e;

  if (list->count >= MAX_FILES)
    return;
  e = &list->items[list->count++];
  memset(e, 0, sizeof(*e));
  snprintf(e->name, sizeof(e->name), "%s", name);
  snprintf(e->path, sizeof(e->path), "%s/%s", dir, name);
  e->kind = kind;
}

static void scan_dir(const char *dir)
{
  SceIoDirent ent;
  SceUID dfd = sceIoDopen(dir);

  if (dfd < 0)
    return;

  memset(&ent, 0, sizeof(ent));
  while (sceIoDread(dfd, &ent) > 0)
  {
    char path[MAX_PATH_LEN];
    const char *name = ent.d_name;

    if (SCE_S_ISDIR(ent.d_stat.st_mode))
      continue;

    if (has_ext(name, ".wad"))
    {
      snprintf(path, sizeof(path), "%s/%s", dir, name);
      if (!strcasecmp(name, "dsda-doom.wad"))
        continue;   /* engine resource WAD, loaded automatically */
      if (is_iwad(path))
        list_add(&iwads, dir, name, KIND_IWAD);
      else
        list_add(&pwads, dir, name, KIND_PWAD);
    }
    else if (has_ext(name, ".zip") || has_ext(name, ".pk3"))
    {
      list_add(&pwads, dir, name, KIND_PWAD);
    }
    else if (has_ext(name, ".deh") || has_ext(name, ".bex"))
    {
      list_add(&dehs, dir, name, KIND_DEH);
    }
    memset(&ent, 0, sizeof(ent));
  }
  sceIoDclose(dfd);
}

static void scan_all(void)
{
  memset(&iwads, 0, sizeof(iwads));
  memset(&pwads, 0, sizeof(pwads));
  memset(&dehs, 0, sizeof(dehs));

  sceIoMkdir(DATA_DIR, 0777);
  sceIoMkdir(WAD_DIR, 0777);

  /* IWADs may also sit directly in the data folder (common for other ports) */
  scan_dir(DATA_DIR);
  scan_dir(WAD_DIR);

  qsort(iwads.items, iwads.count, sizeof(entry_t), cmp_entries);
  qsort(pwads.items, pwads.count, sizeof(entry_t), cmp_entries);
  qsort(dehs.items, dehs.count, sizeof(entry_t), cmp_entries);
}

static int max_order(const list_t *list)
{
  int i, m = 0;

  for (i = 0; i < list->count; i++)
    if (list->items[i].order > m)
      m = list->items[i].order;
  return m;
}

static void toggle(list_t *list, int index)
{
  entry_t *e = &list->items[index];
  int i;

  if (e->order)
  {
    int removed = e->order;

    e->order = 0;
    for (i = 0; i < list->count; i++)
      if (list->items[i].order > removed)
        list->items[i].order--;
  }
  else
  {
    e->order = max_order(list) + 1;
  }
}

/* Swap the selected entry with the one loaded right before it. */
static void move_earlier(list_t *list, int index)
{
  entry_t *e = &list->items[index];
  int i;

  if (e->order <= 1)
    return;
  for (i = 0; i < list->count; i++)
    if (list->items[i].order == e->order - 1)
    {
      list->items[i].order++;
      e->order--;
      return;
    }
}

static int find_by_path(const list_t *list, const char *path)
{
  int i;

  for (i = 0; i < list->count; i++)
    if (!strcmp(list->items[i].path, path))
      return i;
  return -1;
}

static void set_status(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(status_msg, sizeof(status_msg), fmt, ap);
  va_end(ap);
}

/* ------------------------------------------------------------------ */
/* remembering the last selection                                      */
/* ------------------------------------------------------------------ */

static void chomp(char *s)
{
  size_t n = strlen(s);

  while (n && (s[n - 1] == '\n' || s[n - 1] == '\r'))
    s[--n] = 0;
}

static void load_config(void)
{
  FILE *f = fopen(CFG_FILE, "r");
  char line[MAX_PATH_LEN + 16];

  if (!f)
    return;

  while (fgets(line, sizeof(line), f))
  {
    int i;

    chomp(line);
    if (!strncmp(line, "iwad=", 5))
    {
      if ((i = find_by_path(&iwads, line + 5)) >= 0)
      {
        chosen_iwad = i;
        iwads.cursor = i;
      }
    }
    else if (!strncmp(line, "pwad=", 5))
    {
      if ((i = find_by_path(&pwads, line + 5)) >= 0 && !pwads.items[i].order)
        toggle(&pwads, i);
    }
    else if (!strncmp(line, "deh=", 4))
    {
      if ((i = find_by_path(&dehs, line + 4)) >= 0 && !dehs.items[i].order)
        toggle(&dehs, i);
    }
  }
  fclose(f);
}

static void write_ordered(FILE *f, const list_t *list, const char *prefix)
{
  int n, i;

  for (n = 1; n <= max_order(list); n++)
    for (i = 0; i < list->count; i++)
      if (list->items[i].order == n)
        fprintf(f, "%s%s\n", prefix, list->items[i].path);
}

static void save_config(void)
{
  FILE *f = fopen(CFG_FILE, "w");

  if (!f)
    return;
  if (chosen_iwad >= 0)
    fprintf(f, "iwad=%s\n", iwads.items[chosen_iwad].path);
  write_ordered(f, &pwads, "pwad=");
  write_ordered(f, &dehs, "deh=");
  fclose(f);
}

/* ------------------------------------------------------------------ */
/* launching                                                           */
/* ------------------------------------------------------------------ */

static void write_ordered_args(FILE *f, const list_t *list)
{
  int n, i;

  for (n = 1; n <= max_order(list); n++)
    for (i = 0; i < list->count; i++)
      if (list->items[i].order == n)
        fprintf(f, "%s\n", list->items[i].path);
}

static int write_args(void)
{
  FILE *f = fopen(ARGS_FILE, "w");
  FILE *extra;
  char line[MAX_PATH_LEN];

  if (!f)
    return 0;

  fprintf(f, "-iwad\n%s\n", iwads.items[chosen_iwad].path);

  if (max_order(&pwads))
  {
    fprintf(f, "-file\n");
    write_ordered_args(f, &pwads);
  }
  if (max_order(&dehs))
  {
    fprintf(f, "-deh\n");
    write_ordered_args(f, &dehs);
  }

  /* Optional extra arguments, one per line (e.g. -complevel 9, -warp 1) */
  extra = fopen(EXTRA_FILE, "r");
  if (extra)
  {
    while (fgets(line, sizeof(line), extra))
    {
      chomp(line);
      if (line[0] && line[0] != '#')
        fprintf(f, "%s\n", line);
    }
    fclose(extra);
  }

  fclose(f);
  return 1;
}

static void launch(void)
{
  if (chosen_iwad < 0)
  {
    set_status("Choose an IWAD first.");
    step = STEP_IWAD;
    return;
  }

  save_config();
  if (!write_args())
  {
    set_status("Could not write " ARGS_FILE);
    return;
  }

  vita2d_fini();
  vita2d_free_pgf(font);

  sceAppMgrLoadExec(ENGINE_PATH, NULL, NULL);

  /* only reached if the engine could not be started */
  sceKernelExitProcess(0);
}

/* ------------------------------------------------------------------ */
/* drawing                                                             */
/* ------------------------------------------------------------------ */

static void text(int x, int y, unsigned int color, float scale, const char *fmt, ...)
{
  char buf[512];
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  vita2d_pgf_draw_text(font, x, y, color, scale, buf);
}

static const char *step_title(int s)
{
  switch (s)
  {
    case STEP_IWAD: return "1. Choose IWAD";
    case STEP_PWAD: return "2. Choose PWADs (load order)";
    case STEP_DEH:  return "3. Choose DEH / BEX patches";
    default:        return "4. Start";
  }
}

static void draw_header(void)
{
  int s;

  vita2d_draw_rectangle(0, 0, 960, 44, COL_PANEL);
  text(20, 30, COL_ACCENT, 1.1f, "dsda-doom launcher");
  for (s = 0; s < STEP_COUNT; s++)
    text(330 + s * 160, 30, s == step ? COL_TEXT : COL_DIM, 0.8f, "%s",
         s == STEP_IWAD ? "IWAD" : s == STEP_PWAD ? "PWADs" : s == STEP_DEH ? "DEH/BEX" : "Start");
}

static void draw_footer(const char *hint)
{
  vita2d_draw_rectangle(0, 504, 960, 40, COL_PANEL);
  text(20, 530, COL_DIM, 0.8f, "%s", hint);
  if (status_msg[0])
    text(620, 530, COL_ACCENT, 0.8f, "%s", status_msg);
}

static void draw_list(list_t *list, int multi)
{
  int i, y = 100;

  text(20, 76, COL_TEXT, 1.0f, "%s", step_title(step));

  if (!list->count)
  {
    text(40, 130, COL_DIM, 0.9f, multi ? "Nothing found - this step is optional." : "No IWAD found.");
    if (!multi)
    {
      text(40, 170, COL_TEXT, 0.9f, "Copy doom.wad, doom2.wad, freedoom1.wad, ... to:");
      text(40, 200, COL_ACCENT, 0.9f, WAD_DIR);
    }
    return;
  }

  if (list->cursor < list->scroll)
    list->scroll = list->cursor;
  if (list->cursor >= list->scroll + VISIBLE_ROWS)
    list->scroll = list->cursor - VISIBLE_ROWS + 1;

  for (i = list->scroll; i < list->count && i < list->scroll + VISIBLE_ROWS; i++, y += 23)
  {
    entry_t *e = &list->items[i];
    int selected = multi ? e->order > 0 : i == chosen_iwad;

    if (i == list->cursor)
      vita2d_draw_rectangle(10, y - 2, 940, 23, COL_CURSOR);

    if (multi)
      text(20, y + 16, selected ? COL_OK : COL_DIM, 0.85f, selected ? "[%2d]" : "[  ]", e->order);
    else
      text(20, y + 16, selected ? COL_OK : COL_DIM, 0.85f, selected ? "(*)" : "( )");

    text(80, y + 16, selected ? COL_TEXT : COL_DIM, 0.85f, "%s", e->name);
  }

  if (list->count > VISIBLE_ROWS)
    text(860, 76, COL_DIM, 0.8f, "%d/%d", list->cursor + 1, list->count);
}

static void draw_selected(const list_t *list, int *y)
{
  int n, i;

  for (n = 1; n <= max_order(list); n++)
    for (i = 0; i < list->count; i++)
      if (list->items[i].order == n)
      {
        text(60, *y, COL_TEXT, 0.85f, "%d. %s", n, list->items[i].name);
        *y += 22;
      }
}

static void draw_start(void)
{
  int y = 110;

  text(20, 76, COL_TEXT, 1.0f, "%s", step_title(step));

  text(40, y, COL_ACCENT, 0.9f, "IWAD");
  y += 24;
  text(60, y, COL_TEXT, 0.85f, "%s", chosen_iwad >= 0 ? iwads.items[chosen_iwad].name : "(none)");
  y += 34;

  text(40, y, COL_ACCENT, 0.9f, "PWADs");
  y += 24;
  if (max_order(&pwads))
    draw_selected(&pwads, &y);
  else
  {
    text(60, y, COL_DIM, 0.85f, "(none)");
    y += 22;
  }
  y += 12;

  text(40, y, COL_ACCENT, 0.9f, "DEH / BEX");
  y += 24;
  if (max_order(&dehs))
    draw_selected(&dehs, &y);
  else
  {
    text(60, y, COL_DIM, 0.85f, "(none)");
    y += 22;
  }

  vita2d_draw_rectangle(620, 110, 300, 40, start_cursor == 0 ? COL_CURSOR : COL_PANEL);
  text(640, 137, COL_TEXT, 1.0f, "Start game");
  vita2d_draw_rectangle(620, 160, 300, 40, start_cursor == 1 ? COL_CURSOR : COL_PANEL);
  text(640, 187, COL_TEXT, 1.0f, "Clear PWADs and patches");
  vita2d_draw_rectangle(620, 210, 300, 40, start_cursor == 2 ? COL_CURSOR : COL_PANEL);
  text(640, 237, COL_TEXT, 1.0f, "Rescan folder");

  text(620, 290, COL_DIM, 0.75f, "Extra arguments (one per line):");
  text(620, 312, COL_DIM, 0.75f, EXTRA_FILE);
}

/* ------------------------------------------------------------------ */
/* input                                                               */
/* ------------------------------------------------------------------ */

static unsigned int pressed(void)
{
  static unsigned int old_buttons;
  static int repeat_timer;
  SceCtrlData pad;
  unsigned int now, down;

  sceCtrlPeekBufferPositive(0, &pad, 1);

  /* left analog stick as d-pad */
  if (pad.ly < 40)  pad.buttons |= SCE_CTRL_UP;
  if (pad.ly > 215) pad.buttons |= SCE_CTRL_DOWN;
  if (pad.lx < 40)  pad.buttons |= SCE_CTRL_LEFT;
  if (pad.lx > 215) pad.buttons |= SCE_CTRL_RIGHT;

  now = pad.buttons;
  down = now & ~old_buttons;

  /* key repeat for scrolling */
  if (now & (SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT))
  {
    if (down)
      repeat_timer = 20;
    else if (--repeat_timer <= 0)
    {
      down |= now & (SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT);
      repeat_timer = 4;
    }
  }

  old_buttons = now;
  return down;
}

static void move_cursor(list_t *list, unsigned int b)
{
  if (!list->count)
    return;
  if (b & SCE_CTRL_UP)    list->cursor--;
  if (b & SCE_CTRL_DOWN)  list->cursor++;
  if (b & SCE_CTRL_LEFT)  list->cursor -= VISIBLE_ROWS;
  if (b & SCE_CTRL_RIGHT) list->cursor += VISIBLE_ROWS;
  if (list->cursor < 0) list->cursor = 0;
  if (list->cursor >= list->count) list->cursor = list->count - 1;
}

static void clear_selection(list_t *list)
{
  int i;

  for (i = 0; i < list->count; i++)
    list->items[i].order = 0;
}

static void handle_input(unsigned int b)
{
  list_t *list = step == STEP_IWAD ? &iwads : step == STEP_PWAD ? &pwads : step == STEP_DEH ? &dehs : NULL;

  if (b)
    status_msg[0] = 0;

  if (b & SCE_CTRL_START)
  {
    launch();
    return;
  }

  if (b & SCE_CTRL_CIRCLE)
  {
    if (step > STEP_IWAD)
      step--;
    return;
  }

  if (b & SCE_CTRL_TRIANGLE)
  {
    if (step == STEP_IWAD && chosen_iwad < 0)
      set_status("Choose an IWAD first.");
    else if (step < STEP_START)
      step++;
    return;
  }

  if (list)
  {
    move_cursor(list, b);

    if (b & SCE_CTRL_CROSS && list->count)
    {
      if (step == STEP_IWAD)
      {
        chosen_iwad = list->cursor;
        step = STEP_PWAD;
      }
      else
        toggle(list, list->cursor);
    }

    if (b & SCE_CTRL_SQUARE && step != STEP_IWAD && list->count)
      move_earlier(list, list->cursor);
    return;
  }

  /* start page */
  if (b & SCE_CTRL_UP && start_cursor > 0)
    start_cursor--;
  if (b & SCE_CTRL_DOWN && start_cursor < 2)
    start_cursor++;

  if (b & SCE_CTRL_CROSS)
  {
    if (start_cursor == 0)
      launch();
    else if (start_cursor == 1)
    {
      clear_selection(&pwads);
      clear_selection(&dehs);
      set_status("Selection cleared.");
    }
    else
    {
      save_config();
      chosen_iwad = -1;
      scan_all();
      load_config();
      set_status("Found %d IWADs, %d PWADs, %d patches.", iwads.count, pwads.count, dehs.count);
    }
  }
}

/* ------------------------------------------------------------------ */

int main(void)
{
  sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

  vita2d_init();
  vita2d_set_clear_color(COL_BG);
  font = vita2d_load_default_pgf();

  scan_all();
  load_config();
  if (chosen_iwad >= 0)
    step = STEP_START;   /* last session remembered: one press to play */

  for (;;)
  {
    handle_input(pressed());

    vita2d_start_drawing();
    vita2d_clear_screen();

    draw_header();
    if (step == STEP_START)
    {
      draw_start();
      draw_footer("X select   O back   START play");
    }
    else
    {
      draw_list(step == STEP_IWAD ? &iwads : step == STEP_PWAD ? &pwads : &dehs, step != STEP_IWAD);
      draw_footer(step == STEP_IWAD
                  ? "X choose   /\\ next   START play"
                  : "X toggle   [] earlier   /\\ next   O back   START play");
    }

    vita2d_end_drawing();
    vita2d_swap_buffers();
  }

  return 0;
}
