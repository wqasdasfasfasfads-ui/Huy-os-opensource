/* Huy OS 0.4 - 64-бітне (long mode) графічне ядро: вигляд Windows 11, розкладка macOS (верхнє меню + док) */
typedef unsigned char u8; typedef signed char i8; typedef unsigned short u16; typedef unsigned int u32; typedef unsigned long u64;

static inline void outb(u16 p, u8 v) { __asm__ volatile("outb %0,%1" :: "a"(v), "Nd"(p)); }
static inline void outw(u16 p, u16 v) { __asm__ volatile("outw %0,%1" :: "a"(v), "Nd"(p)); }
static inline u8 inb(u16 p) { u8 v; __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p)); return v; }

void *memcpy(void *d, const void *s, u64 n) { void *r = d; __asm__ volatile("cld; rep movsb" : "+D"(d), "+S"(s), "+c"(n) :: "memory"); return r; }
void *memset(void *d, int c, u64 n) { void *r = d; __asm__ volatile("cld; rep stosb" : "+D"(d), "+c"(n) : "a"(c) : "memory"); return r; }
static void copy32(u32 *d, const u32 *s, u64 n) { __asm__ volatile("cld; rep movsl" : "+D"(d), "+S"(s), "+c"(n) :: "memory"); }

/* ---------- рядки ---------- */
static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int streq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static int startswith(const char *s, const char *p) { while (*p) if (*s++ != *p++) return 0; return 1; }
static void scat(char *d, const char *s) { while (*d) d++; while ((*d++ = *s++)); }
static char *utoa(u32 n, char *b) { char t[12]; int i = 0, j = 0; if (!n) t[i++] = '0'; while (n) { t[i++] = '0' + n % 10; n /= 10; } while (i) b[j++] = t[--i]; b[j] = 0; return b; }
static void hex32(u32 v, char *b) { const char *h = "0123456789ABCDEF"; b[0] = '0'; b[1] = 'x'; for (int i = 0; i < 8; i++) b[2 + i] = h[(v >> (28 - 4 * i)) & 15]; b[10] = 0; }

/* ---------- графіка ---------- */
#define RGB(r, g, b) ((u32)(((r) << 16) | ((g) << 8) | (b)))
static int W, H, PITCH, BPP;
static u8 *FB;
static u32 *back = (u32 *)0x1000000, *wall = (u32 *)0x1400000;
static const u8 *font = (const u8 *)0x6000;

static __attribute__((optimize("O2"))) u32 mix(u32 d, u32 s, int a) {
    if (a >= 255) return s; if (a <= 0) return d;
    int dr = (d >> 16) & 255, dg = (d >> 8) & 255, db = d & 255, sr = (s >> 16) & 255, sg = (s >> 8) & 255, sb = s & 255;
    return RGB(dr + (((sr - dr) * a) >> 8), dg + (((sg - dg) * a) >> 8), db + (((sb - db) * a) >> 8));
}
static __attribute__((optimize("O2"))) void fill_rect(int x, int y, int w, int h, u32 c, int a) {
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0; if (y < 0) y = 0; if (x1 > W) x1 = W; if (y1 > H) y1 = H;
    if (x >= x1 || y >= y1) return;
    for (int j = y; j < y1; j++) { u32 *p = back + j * W; for (int i = x; i < x1; i++) p[i] = a >= 255 ? c : mix(p[i], c, a); }
}
static int isqrt(int n) { int r = 0; while ((r + 1) * (r + 1) <= n) r++; return r; }
static void fill_rrect_rows(int x, int y, int w, int h, int r, u32 c, int a, int r0, int r1) {
    for (int j = r0; j < r1; j++) {
        int inset = 0, k = j < h - 1 - j ? j : h - 1 - j;
        if (k < r) { int dy = r - k; inset = r - isqrt(r * r - (dy - 1) * (dy - 1)); }
        fill_rect(x + inset, y + j, w - 2 * inset, 1, c, a);
    }
}
static void fill_rrect(int x, int y, int w, int h, int r, u32 c, int a) { fill_rrect_rows(x, y, w, h, r, c, a, 0, h); }
static void fill_circle(int cx, int cy, int r, u32 c, int a) {
    for (int dy = -r; dy <= r; dy++) { int dx = isqrt(r * r - dy * dy); fill_rect(cx - dx, cy + dy, 2 * dx + 1, 1, c, a); }
}
static void draw_char(int x, int y, u8 ch, u32 c, int sc) {
    const u8 *g = font + ch * 16;
    for (int r = 0; r < 16; r++) {
        u8 b = g[r]; if (!b) continue;
        for (int k = 0; k < 8; k++) if (b & (0x80 >> k)) {
            if (sc == 1) { int px = x + k, py = y + r; if (px >= 0 && px < W && py >= 0 && py < H) back[py * W + px] = c; }
            else fill_rect(x + k * sc, y + r * sc, sc, sc, c, 255);
        }
    }
}
static void text(int x, int y, const char *s, u32 c) { for (; *s; s++, x += 8) draw_char(x, y, *s, c, 1); }
static void textb(int x, int y, const char *s, u32 c) { text(x, y, s, c); text(x + 1, y, s, c); }
static void textc(int cx, int y, const char *s, u32 c) { text(cx - slen(s) * 4, y, s, c); }

/* ---------- логотип: круг з "Хуй" (штрихи на сітці 0..324 x 0..135) ---------- */
static void texts(int cx, int y, const char *s, u32 c, int sc) { int x = cx - slen(s) * 4 * sc; for (; *s; s++, x += 8 * sc) draw_char(x, y, *s, c, sc); }
static void stroke(int x0, int y0, int x1, int y1, int r, u32 c) {
    int dx = x1 - x0, dy = y1 - y0, ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, n = ax > ay ? ax : ay;
    if (!n) n = 1;
    for (int i = 0; i <= n; i++) fill_circle(x0 + dx * i / n, y0 + dy * i / n, r, c, 255);
}
static const u16 hg[][4] = {
    { 8, 0, 92, 100 }, { 92, 0, 8, 100 },                           /* Х */
    { 120, 35, 164, 98 }, { 204, 35, 140, 135 },                    /* у */
    { 236, 35, 236, 100 }, { 312, 35, 312, 100 }, { 236, 100, 312, 35 }, /* й */
    { 254, 6, 262, 16 }, { 262, 16, 286, 16 }, { 286, 16, 294, 6 } };
static void draw_huy(int cx, int cy, int R, u32 fg) {
    int r = R / 28;
    for (int k = 0; k < 10; k++)
        stroke(cx + ((int)hg[k][0] - 162) * R / 213, cy + ((int)hg[k][1] - 67) * R / 213,
               cx + ((int)hg[k][2] - 162) * R / 213, cy + ((int)hg[k][3] - 67) * R / 213, r, fg);
}
static void draw_huy_icon(int cx, int cy, int R, u32 bg, u32 fg) { fill_circle(cx, cy, R, bg, 255); draw_huy(cx, cy, R, fg); }

static void flush_rows(int y0, int y1) {
    if (y0 < 0) y0 = 0; if (y1 > H) y1 = H;
    if (BPP == 32) { for (int y = y0; y < y1; y++) copy32((u32 *)(FB + (u64)y * PITCH), back + y * W, W); }
    else if (BPP == 24) {
        for (int y = y0; y < y1; y++) { u8 *d = FB + (u64)y * PITCH; u32 *s = back + y * W; for (int x = 0; x < W; x++) { d[0] = s[x]; d[1] = s[x] >> 8; d[2] = s[x] >> 16; d += 3; } }
    }
}
static void flush(void) { flush_rows(0, H); }
static void fb_put(int x, int y, u32 c) {
    if (BPP == 32) *(u32 *)(FB + (u64)y * PITCH + x * 4) = c;
    else { u8 *d = FB + (u64)y * PITCH + x * 3; d[0] = c; d[1] = c >> 8; d[2] = c >> 16; }
}

static void make_wall(void) {
    int bx[3] = { W * 60 / 100, W * 30 / 100, W * 80 / 100 }, by[3] = { H * 40 / 100, H * 65 / 100, H * 85 / 100 };
    int br[3] = { 380, 320, 260 };
    u32 bc[3] = { RGB(70, 175, 255), RGB(160, 90, 230), RGB(0, 190, 200) };
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int t = (x * 256 / W + y * 256 / H) / 2;
        int r = 6 + 14 * t / 256, g = 16 + 54 * t / 256, b = 46 + 124 * t / 256;
        for (int k = 0; k < 3; k++) {
            int dx = x - bx[k], dy = y - by[k], d2 = dx * dx + dy * dy, R2 = br[k] * br[k];
            if (d2 < R2) {
                int f = 255 - d2 * 255 / R2; f = f * f / 255; f = f * 150 / 255;
                r += (((bc[k] >> 16) & 255) - r) * f / 256; g += (((bc[k] >> 8) & 255) - g) * f / 256; b += ((bc[k] & 255) - b) * f / 256;
            }
        }
        wall[y * W + x] = RGB(r, g, b);
    }
}

/* ---------- тема ---------- */
static int dark = 1, acc_i = 0, dirty = 1;
static const u32 accents[5] = { RGB(0, 103, 192), RGB(0, 150, 136), RGB(156, 39, 176), RGB(230, 81, 0), RGB(216, 27, 96) };
#define ACCENT (accents[acc_i])
#define C_BODY (dark ? RGB(32, 32, 32) : RGB(243, 243, 243))
#define C_TITLE (dark ? RGB(46, 46, 46) : RGB(230, 230, 232))
#define C_TEXT (dark ? RGB(240, 240, 240) : RGB(28, 28, 28))
#define C_SUB (dark ? RGB(160, 160, 165) : RGB(95, 95, 100))
#define C_CARD (dark ? RGB(47, 47, 50) : RGB(255, 255, 255))
#define WHITE RGB(255, 255, 255)

/* ---------- файли та розпізнавання форматів ---------- */
typedef struct { const char *name; int dir; u8 *data; u32 size; int isdir, parent; u32 cap; } File;   /* parent: -1 = Home, -2 = Desktop, >=0 = індекс папки */
enum { FT_UNK, FT_TXT, FT_PE, FT_ELF, FT_MACHO, FT_BAT, FT_SH, FT_DIR };
#define NF 64
static File files[NF]; static int nf;
extern u8 payload[];   /* папка files/ вбудована в ядро під час збірки */

static u16 rd16(const u8 *p) { return p[0] | (p[1] << 8); }
static u32 rd32(const u8 *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24); }
static u64 rd64(const u8 *p) { return rd32(p) | ((u64)rd32(p + 4) << 32); }

typedef struct { int ft, bits, has_entry; const char *fmt, *arch, *kind; u32 entry; } Info;
static void analyze(const File *f, Info *i) {
    const u8 *d = f->data; memset(i, 0, sizeof(*i)); i->fmt = "Unknown"; i->arch = "-"; i->kind = "-";
    if (f->isdir) { i->ft = FT_DIR; i->fmt = "Folder"; i->kind = "Folder"; return; }
    if (f->size > 0x90 && d[0] == 'M' && d[1] == 'Z') {
        u32 pe = rd32(d + 0x3C);
        if (pe + 0x20 < f->size && d[pe] == 'P' && d[pe + 1] == 'E') {
            u16 m = rd16(d + pe + 4);
            i->ft = FT_PE; i->fmt = "Windows PE (.exe)";
            i->arch = m == 0x14C ? "x86 (i386)" : m == 0x8664 ? "x86-64" : m == 0xAA64 ? "ARM64" : "other";
            i->bits = rd16(d + pe + 24) == 0x20B ? 64 : 32;
            i->kind = (rd16(d + pe + 22) & 0x2000) ? "DLL library" : "Executable";
            i->entry = rd32(d + pe + 40); i->has_entry = 1; return;
        }
    }
    if (f->size > 24 && d[0] == 0x7F && d[1] == 'E' && d[2] == 'L' && d[3] == 'F') {
        u16 m = rd16(d + 18), t = rd16(d + 16);
        i->ft = FT_ELF; i->fmt = "Linux ELF"; i->bits = d[4] == 2 ? 64 : 32;
        i->arch = m == 3 ? "x86 (i386)" : m == 0x3E ? "x86-64" : m == 0xB7 ? "ARM64" : m == 0x28 ? "ARM" : m == 0xF3 ? "RISC-V" : "other";
        i->kind = t == 2 ? "Executable" : t == 3 ? "PIE / shared object" : t == 1 ? "Object file" : "other";
        i->entry = rd32(d + 24); i->has_entry = 1; return;
    }
    if (f->size > 16) {
        u32 mg = rd32(d);
        if (mg == 0xFEEDFACF || mg == 0xFEEDFACE) {
            u32 cpu = rd32(d + 4), ty = rd32(d + 12);
            i->ft = FT_MACHO; i->fmt = "macOS Mach-O (.app)"; i->bits = mg == 0xFEEDFACF ? 64 : 32;
            i->arch = cpu == 7 ? "x86 (i386)" : cpu == 0x01000007 ? "x86-64" : cpu == 0x0100000C ? "ARM64 (Apple)" : "other";
            i->kind = ty == 2 ? "Executable" : ty == 6 ? "Dylib" : ty == 8 ? "Bundle" : "other"; return;
        }
    }
    int ok = 1; for (u32 k = 0; k < f->size; k++) if (f->data[k] >= 128 || (f->data[k] < 32 && f->data[k] != '\n' && f->data[k] != '\t' && f->data[k] != '\r')) ok = 0;
    if (ok) {
        int l = slen(f->name);
        i->ft = FT_TXT; i->fmt = "Plain text"; i->kind = "Document";
        if (l > 4 && (streq(f->name + l - 4, ".bat") || streq(f->name + l - 4, ".cmd"))) { i->ft = FT_BAT; i->fmt = "Windows batch (.bat)"; i->kind = "Script"; }
        else if (l > 3 && streq(f->name + l - 3, ".sh")) { i->ft = FT_SH; i->fmt = "Shell script (.sh)"; i->kind = "Script"; }
    }
}

static void init_files(void) {
    u8 *p = payload;
    while (p[0] && nf < NF) {
        u32 sz = rd32(p + 32); Info in;
        files[nf] = (File){ (const char *)p, 1, p + 40, sz, 0, -1 };
        analyze(&files[nf], &in); files[nf].dir = (in.ft == FT_TXT || in.ft == FT_BAT || in.ft == FT_SH || in.ft == FT_UNK) ? 1 : 2;
        p += 40 + ((sz + 7) & ~7u); nf++;
    }
}
static int rec_idx(int k) { int c = 0; for (int i = 0; i < nf; i++) if (files[i].dir == 2) { if (c == k) return i; c++; } return -1; }

/* ---------- вікна ---------- */
typedef struct { int x, y, w, h, open, min, max, sx, sy, sw, sh; const char *title; } Win;
#define NW 9
static Win wins[NW] = {
    { 110, 64, 560, 360, 0, 0, 0, 0, 0, 0, 0, "Terminal" }, { 190, 100, 640, 420, 0, 0, 0, 0, 0, 0, 0, "Files" },
    { 250, 80, 520, 380, 0, 0, 0, 0, 0, 0, 0, "Settings" }, { 300, 130, 480, 360, 0, 0, 0, 0, 0, 0, 0, "File Inspector" }, { 230, 90, 560, 400, 0, 0, 0, 0, 0, 0, 0, "Editor" },
    { 150, 60, 520, 420, 0, 0, 0, 0, 0, 0, 0, "3D Viewer" }, { 200, 90, 500, 380, 0, 0, 0, 0, 0, 0, 0, "3D Space" }, { 170, 70, 520, 400, 0, 0, 0, 0, 0, 0, 0, "3D Maze" },
    { 180, 56, 640, 440, 0, 0, 0, 0, 0, 0, 0, "Huygle" } };
static int zo[NW] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
#define TB 34   /* висота нижньої панелі з відкритими вікнами */
static int start_open, start_was, fsel = -1, ffilter, fcur = -1, insp_file, dragw = -1, dragdx, dragdy;
static int dsel = -1, ctx_open, ctx_x, ctx_y, ctx_par, ctx_src, ctx_item = -1, ctx_cnt, ctx_act[4];
static int edit_file = -1, elen, ecur, escroll, emsg, ren_open, ren_idx, ren_n, ren_msg;
static char ebuf[4096], ren_buf[24];
static u8 epool[65536]; static u32 epused;     /* пам'ять для збережених змін у файлах */
static int mx, my, mb, pmb;
static int dock_x[4], dock_s[4], dock_bx, dock_bw;

static int top_win(void) { for (int k = NW - 1; k >= 0; k--) { int i = zo[k]; if (wins[i].open && !wins[i].min) return i; } return -1; }
static void bring_front(int i) { int n[NW], k = 0; for (int j = 0; j < NW; j++) if (zo[j] != i) n[k++] = zo[j]; n[NW - 1] = i; for (int j = 0; j < NW; j++) zo[j] = n[j]; }
static void open_win(int i) { wins[i].open = 1; wins[i].min = 0; bring_front(i); }
static void open_inspector(int f) { insp_file = f; open_win(3); }
static void open_item(int fi) {
    if (files[fi].isdir) { int p = fi; while (files[p].parent >= 0) p = files[p].parent; ffilter = files[p].parent == -2 ? 3 : 0; fcur = fi; fsel = -1; open_win(1); }
    else open_inspector(fi);
}
static void go_up(void) { int p = files[fcur].parent; if (p >= 0) fcur = p; else { fcur = -1; ffilter = p == -2 ? 3 : 0; } fsel = -1; }
static void reboot(void) { while (inb(0x64) & 2); outb(0x64, 0xFE); for (;;) __asm__ volatile("cli; hlt"); }
static void power_off(void) { outw(0x604, 0x2000); outw(0xB004, 0x2000); outw(0x4004, 0x3400); for (;;) __asm__ volatile("cli; hlt"); }

/* ---------- термінал ---------- */
#define TC 66
#define TR 19
static char tb[TR][TC], tin[96];
static int trow, tcol, tn;
static void t_nl(void) { tcol = 0; trow++; if (trow >= TR) { memcpy(tb[0], tb[1], TC * (TR - 1)); memset(tb[TR - 1], ' ', TC); trow = TR - 1; } }
static void t_putc(char c) { if (c == '\n') { t_nl(); return; } if (tcol >= TC) t_nl(); tb[trow][tcol++] = c; }
static void t_puts(const char *s) { while (*s) t_putc(*s++); }
static void t_prompt(void) { t_puts("huy@pc:~$ "); }
static int parse(const char **p) { int n = 0, neg = 0; while (**p == ' ') (*p)++; if (**p == '-') { neg = 1; (*p)++; } while (**p >= '0' && **p <= '9') n = n * 10 + (*(*p)++ - '0'); return neg ? -n : n; }

static char clk[8], dat[8];
static void t_run(char *cmd);
static char lc(char c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static int ieq(const char *a, const char *b) { while (*a && lc(*a) == lc(*b)) { a++; b++; } return *a == *b; }
static int starti(const char *s, const char *p) { while (*p) if (lc(*s++) != lc(*p++)) return 0; return 1; }
static File *find_file(const char *n) { if (n[0] == '.' && n[1] == '/') n += 2; for (int i = 0; i < nf; i++) if (ieq(files[i].name, n)) return &files[i]; return 0; }
static char nstore[NF][24];
static int new_item(int isdir, int parent) {
    if (nf >= NF) return -1;
    const char *base = isdir ? "New folder" : "New file", *ext = isdir ? "" : ".txt";
    char nm[24]; int n = 1;
    for (;;) {
        nm[0] = 0; scat(nm, base);
        if (n > 1) { char t[12]; scat(nm, " "); scat(nm, utoa(n, t)); }
        scat(nm, ext);
        int clash = 0; for (int i = 0; i < nf; i++) if (files[i].parent == parent && ieq(files[i].name, nm)) clash = 1;
        if (!clash || n > 99) break;
        n++;
    }
    memcpy(nstore[nf], nm, slen(nm) + 1);
    files[nf] = (File){ nstore[nf], isdir ? 3 : 1, (u8 *)"", 0, isdir, parent };
    return nf++;
}
static int is_script_name(const char *n) { int l = slen(n); return (l > 3 && ieq(n + l - 3, ".sh")) || (l > 4 && (ieq(n + l - 4, ".bat") || ieq(n + l - 4, ".cmd"))); }

static int is_textft(int fi) { Info in; analyze(&files[fi], &in); return in.ft == FT_TXT || in.ft == FT_BAT || in.ft == FT_SH; }

/* ---------- текстовий редактор ---------- */
static void open_editor(int fi) {
    File *f = &files[fi]; u32 n = 0;
    for (u32 k = 0; k < f->size && n < 4095; k++) if (f->data[k] != '\r') ebuf[n++] = f->data[k];
    ebuf[n] = 0; elen = (int)n; ecur = elen; escroll = 0; emsg = 0; edit_file = fi; open_win(4);
}
static void ed_fix(void) {
    int ln = 0, rows = (wins[4].h - 112) / 16; if (rows < 1) rows = 1;
    for (int k = 0; k < ecur; k++) if (ebuf[k] == '\n') ln++;
    if (ln < escroll) escroll = ln;
    if (ln >= escroll + rows) escroll = ln - rows + 1;
}
static void ed_key(char c) {
    if (edit_file < 0) return;
    if (c == '\b') { if (ecur > 0) { for (int k = ecur - 1; k < elen; k++) ebuf[k] = ebuf[k + 1]; ecur--; elen--; } }
    else {
        if (c == '\t') c = ' ';
        if ((c >= ' ' || c == '\n') && elen < 4094) { for (int k = elen; k >= ecur; k--) ebuf[k + 1] = ebuf[k]; ebuf[ecur++] = c; elen++; }
    }
    emsg = 0; ed_fix(); dirty = 1;
}
static void ed_save(void) {
    if (edit_file < 0) return;
    File *f = &files[edit_file]; u32 n = (u32)elen;
    if (f->cap < n) {
        u32 need = (n + 64 + 7) & ~7u;
        if (epused + need > sizeof epool) { emsg = 2; return; }
        f->data = epool + epused; f->cap = need; epused += need;
    }
    memcpy(f->data, ebuf, n); f->size = n; emsg = 1;
}
static void ed_click(int lx, int ly) {
    if (edit_file < 0) return;
    int row = ly < 92 ? 0 : (ly - 92) / 16, col = lx < 22 ? 0 : (lx - 22) / 8, line = escroll + row, idx = 0, l = 0;
    while (l < line && idx < elen) { if (ebuf[idx] == '\n') l++; idx++; }
    if (l < line) idx = elen;
    else { int k = 0; while (k < col && idx < elen && ebuf[idx] != '\n') { idx++; k++; } }
    ecur = idx; emsg = 0; ed_fix();
}

/* ---------- перейменування ---------- */
static void ren_start(int fi) {
    ren_idx = fi; ren_n = 0; const char *s = files[fi].name;
    while (s[ren_n] && ren_n < 23) { ren_buf[ren_n] = s[ren_n]; ren_n++; }
    ren_buf[ren_n] = 0; ren_msg = 0; ren_open = 1;
}
static void ren_apply(void) {
    if (ren_n == 0) { ren_msg = 1; return; }
    for (int i = 0; i < nf; i++) if (i != ren_idx && files[i].parent == files[ren_idx].parent && ieq(files[i].name, ren_buf)) { ren_msg = 2; return; }
    memcpy(nstore[ren_idx], ren_buf, ren_n + 1); files[ren_idx].name = nstore[ren_idx];
    if (!files[ren_idx].isdir) { Info in; analyze(&files[ren_idx], &in); files[ren_idx].dir = (in.ft == FT_TXT || in.ft == FT_BAT || in.ft == FT_SH || in.ft == FT_UNK) ? 1 : 2; }
    ren_open = 0;
}
static void ren_key(char c) {
    if (c == '\n') { ren_apply(); }
    else if (c == '\b') { if (ren_n > 0) { ren_buf[--ren_n] = 0; ren_msg = 0; } }
    else if (c >= ' ' && ren_n < 23) { ren_buf[ren_n++] = c; ren_buf[ren_n] = 0; ren_msg = 0; }
    dirty = 1;
}

/* --- .bat / .sh інтерпретатор: змінні, echo, set, виклик команд термінала та інших файлів --- */
static char vname[8][16], vval[8][48]; static int nvars, depth;
static void scpy(char *d, const char *s, int max) { int i = 0; while (s[i] && i < max - 1) { d[i] = s[i]; i++; } d[i] = 0; }
static void setvar(const char *n, const char *v) {
    int i; for (i = 0; i < nvars; i++) if (ieq(vname[i], n)) break;
    if (i == nvars) { if (nvars >= 8) return; nvars++; }
    scpy(vname[i], n, 16); scpy(vval[i], v, 48);
}
static int isid(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; }
static void expand(const char *s, char *o, int max) {
    int n = 0;
    while (*s && n < max - 1) {
        if (*s == '%' || *s == '$') {
            const char *p = s + 1; int br = 0, k = 0; char nm[16];
            if (*s == '$' && *p == '{') { br = 1; p++; }
            while (isid(*p) && k < 15) nm[k++] = *p++;
            nm[k] = 0;
            if (*s == '%') { if (*p == '%') p++; else k = 0; }
            if (br) { if (*p == '}') p++; else k = 0; }
            if (k > 0) { for (int i = 0; i < nvars; i++) if (ieq(vname[i], nm)) for (const char *v = vval[i]; *v && n < max - 1; v++) o[n++] = *v; s = p; continue; }
        }
        o[n++] = *s++;
    }
    o[n] = 0;
}
static int assign(char *l) {
    int k = 0; while (isid(l[k])) k++;
    if (!k || l[k] != '=') return 0;
    char nm[16], *v = l + k + 1; int L = slen(v);
    if (L >= 2 && (v[0] == '"' || v[0] == '\'') && v[L - 1] == v[0]) { v[L - 1] = 0; v++; }
    if (k > 15) k = 15; memcpy(nm, l, k); nm[k] = 0; setvar(nm, v); return 1;
}
static int word(const char *s, const char *w) { int n = slen(w); return starti(s, w) && (s[n] == 0 || s[n] == ' ' || s[n] == '.'); }
static void run_script(File *f) {
    if (depth >= 3) { t_puts("script nesting too deep\n"); return; }
    depth++;
    char line[100], ex[160]; u32 p = 0;
    while (p < f->size) {
        int n = 0;
        while (p < f->size && f->data[p] != '\n') { if (f->data[p] != '\r' && n < 99) line[n++] = f->data[p]; p++; }
        p++; line[n] = 0;
        char *l = line; while (*l == ' ' || *l == '\t' || *l == '@') l++;
        if (!*l || *l == '#' || *l == ':' || word(l, "rem") || starti(l, "echo off") || starti(l, "echo on")) continue;
        expand(l, ex, sizeof ex);
        if (starti(ex, "set ")) { char *a = ex + 4; assign(a); }
        else if (starti(ex, "export ")) { char *a = ex + 7; assign(a); }
        else if (assign(ex)) { }
        else if (word(ex, "echo")) { char *t = ex + 4; if (*t == '.' || *t == ' ') t++; int L = slen(t); if (L >= 2 && (t[0] == '"' || t[0] == '\'') && t[L - 1] == t[0]) { t[L - 1] = 0; t++; } t_puts(t); t_putc('\n'); }
        else if (ieq(ex, "cls") || ieq(ex, "clear")) t_run("clear");
        else if (ieq(ex, "dir")) t_run("ls");
        else if (ieq(ex, "ver")) t_run("uname");
        else if (ieq(ex, "pause")) t_puts("Press any key to continue . . .\n");
        else if (word(ex, "exit")) break;
        else if (word(ex, "timeout") || word(ex, "sleep") || word(ex, "title") || word(ex, "color") || word(ex, "chcp") || word(ex, "cd") || word(ex, "mkdir") || word(ex, "setlocal") || word(ex, "endlocal")) continue;
        else if (starti(ex, "call ")) t_run(ex + 5);
        else if (starti(ex, "bash ")) t_run(ex + 5);
        else if (starti(ex, "sh ")) t_run(ex + 3);
        else t_run(ex);
    }
    depth--;
}

/* --- завантажувачі ELF64 (Linux, static) та PE64 (Windows) ---
   Програми працюють у ring 0 на тому ж стеку, без ізоляції. Linux: syscall write/exit/getpid/...
   Windows: імпорти kernel32 (GetStdHandle, WriteFile, WriteConsoleA, ExitProcess) та msvcrt (puts, putchar, exit). */
extern void run_prog(u64 entry, u64 sp, u64 mode);
extern void prog_exit(void) __attribute__((noreturn));
#define IMG_LO 0x400000UL
#define IMG_HI 0xE00000UL
#define STK_TOP 0xF00000UL
#ifdef HOST
#define CLI()
#define STI()
#else
#define CLI() __asm__ volatile("cli")
#define STI() __asm__ volatile("sti")
#endif
static int prog_code;
static void prog_write(const char *b, u64 n) { for (u64 i = 0; i < n; i++) if (b[i] != '\r') t_putc(b[i]); }
u64 sys_handler(u64 n, u64 a, u64 b, u64 c) {
    switch (n) {
    case 1: if (a == 1 || a == 2) { prog_write((const char *)b, c); return c; } return (u64)-9;
    case 0: return 0;
    case 60: case 231: prog_code = (int)a; prog_exit();
    case 39: case 218: return 1;
    case 158: case 12: return 0;
    default: return (u64)-38;
    }
}
static void run_it(u64 entry, u64 sp, u64 mode) {
    prog_code = 0; CLI(); run_prog(entry, sp, mode); STI(); dirty = 1;
    if (prog_code) { char b[12]; t_puts("[exit code "); t_puts(utoa((u32)prog_code, b)); t_puts("]\n"); }
}
static void run_elf(File *f) {
    const u8 *d = f->data;
    if (f->size < 64 || d[4] != 2 || rd16(d + 18) != 0x3E) { t_puts("ELF: only x86-64 is supported\n"); return; }
    if (rd16(d + 16) != 2) { t_puts("ELF: only static non-PIE programs (ET_EXEC)\n"); return; }
    u64 entry = rd64(d + 24), phoff = rd64(d + 32); u32 phn = rd16(d + 56), phs = rd16(d + 54);
    if (phs < 56 || phoff + (u64)phn * phs > f->size) { t_puts("ELF: bad program headers\n"); return; }
    for (u32 i = 0; i < phn; i++) {
        const u8 *ph = d + phoff + (u64)i * phs;
        if (rd32(ph) != 1) continue;
        u64 off = rd64(ph + 8), va = rd64(ph + 16), fs = rd64(ph + 32), ms = rd64(ph + 40);
        if (va < IMG_LO || va + ms > IMG_HI || off + fs > f->size || fs > ms) { t_puts("ELF: segment outside 0x400000-0xE00000\n"); return; }
        memset((void *)va, 0, ms); memcpy((void *)va, d + off, fs);
    }
    u64 sp = STK_TOP - 64; memset((void *)sp, 0, 64);
    run_it(entry, sp, 0);
}
#define MS __attribute__((ms_abi))
static MS void *w_GetStdHandle(u32 n) { return (void *)(u64)(0x100 + n); }
static MS int w_WriteFile(void *h, const char *b, u32 n, u32 *w, void *ov) { (void)h; (void)ov; prog_write(b, n); if (w) *w = n; return 1; }
static MS void w_ExitProcess(u32 c) { prog_code = (int)c; prog_exit(); }
static MS int w_puts(const char *s) { t_puts(s); t_putc('\n'); return 1; }
static MS int w_putchar(int c) { t_putc((char)c); return c; }
static MS void w_unknown(void) { t_puts("[huyos] program called an unsupported Windows API\n"); prog_exit(); }
static const struct { const char *n; void *f; } imp_tab[] = {
    { "GetStdHandle", (void *)w_GetStdHandle }, { "WriteFile", (void *)w_WriteFile }, { "WriteConsoleA", (void *)w_WriteFile },
    { "ExitProcess", (void *)w_ExitProcess }, { "exit", (void *)w_ExitProcess }, { "_exit", (void *)w_ExitProcess },
    { "puts", (void *)w_puts }, { "putchar", (void *)w_putchar } };
static void run_pe(File *f) {
    const u8 *d = f->data;
    u32 lf = rd32(d + 0x3C);
    if (f->size < 0x200 || lf + 0x108 > f->size) { t_puts("PE: bad header\n"); return; }
    const u8 *pe = d + lf, *opt = pe + 24;
    if (rd16(pe + 4) != 0x8664 || rd16(opt) != 0x20B) { t_puts("PE: only 64-bit (x86-64) .exe is supported\n"); return; }
    u32 ns = rd16(pe + 6), osz = rd16(pe + 20), ep = rd32(opt + 16), soi = rd32(opt + 56), soh = rd32(opt + 60), ndd = rd32(opt + 108);
    u64 ib = rd64(opt + 24), base = IMG_LO; u8 *B = (u8 *)base;
    if (soi > IMG_HI - IMG_LO || soh > f->size || soh > soi) { t_puts("PE: image too large\n"); return; }
    memset(B, 0, soi); memcpy(B, d, soh);
    const u8 *sh = opt + osz;
    for (u32 i = 0; i < ns; i++) {
        const u8 *sc = sh + 40 * i; u32 va = rd32(sc + 12), rs = rd32(sc + 16), rp = rd32(sc + 20);
        if (va + rs > soi || rp + rs > f->size) { t_puts("PE: bad section\n"); return; }
        memcpy(B + va, d + rp, rs);
    }
    const u8 *dd = opt + 112; u64 delta = base - ib;
    u32 rrva = ndd > 5 ? rd32(dd + 40) : 0, rsz = ndd > 5 ? rd32(dd + 44) : 0;
    if (delta && !rrva) { t_puts("PE: image base is outside memory and there is no .reloc\n"); return; }
    if (delta) for (u32 o = 0; o + 8 <= rsz;) {
        u32 pg = rd32(B + rrva + o), bs = rd32(B + rrva + o + 4); if (bs < 8) break;
        for (u32 k = 8; k + 2 <= bs; k += 2) { u16 e = rd16(B + rrva + o + k); u32 of = pg + (e & 0xFFF); if ((e >> 12) == 10) *(u64 *)(B + of) += delta; else if ((e >> 12) == 3) *(u32 *)(B + of) += (u32)delta; }
        o += bs;
    }
    u32 irva = ndd > 1 ? rd32(dd + 8) : 0;
    if (irva) for (const u8 *im = B + irva; rd32(im + 12); im += 20) {
        u32 oft = rd32(im), ft = rd32(im + 16); u64 *th = (u64 *)(B + (oft ? oft : ft)), *iat = (u64 *)(B + ft);
        for (int k = 0; th[k]; k++) {
            void *fn = (void *)w_unknown;
            if (!(th[k] >> 63)) {
                const char *nm = (const char *)(B + (u32)th[k] + 2);
                for (unsigned t = 0; t < sizeof imp_tab / sizeof imp_tab[0]; t++) if (streq(nm, imp_tab[t].n)) fn = imp_tab[t].f;
                if (fn == (void *)w_unknown) { t_puts("unresolved import: "); t_puts(nm); t_putc('\n'); }
            }
            iat[k] = (u64)fn;
        }
    }
    run_it(base + ep, STK_TOP - 64, 1);
}
static void exec_file(File *f) {
    Info in; analyze(f, &in);
    if (in.ft == FT_BAT || in.ft == FT_SH) run_script(f);
    else if (in.ft == FT_ELF) run_elf(f);
    else if (in.ft == FT_PE) run_pe(f);
    else if (in.ft == FT_MACHO) t_puts("macOS apps (Mach-O) can't run here yet.\n");
    else t_puts("not an executable file\n");
}
static void run_in_terminal(int fi) { open_win(0); tn = 0; t_puts("run "); t_puts(files[fi].name); t_putc('\n'); exec_file(&files[fi]); t_prompt(); dirty = 1; }

static void t_run(char *cmd) {
    char b[16]; Info in;
    if (!*cmd) return;
    if (streq(cmd, "help")) t_puts("help clear about whoami uname ls cat run <file> echo calc info date time reboot shutdown\n(type a file name to run .bat .sh .exe or ELF)\n");
    else if (streq(cmd, "clear")) { memset(tb, ' ', sizeof tb); trow = tcol = 0; }
    else if (streq(cmd, "about")) t_puts("Huy OS 0.4 - 64-bit, Windows 11 look, macOS layout, own kernel.\n");
    else if (streq(cmd, "whoami")) t_puts("huy-user\n");
    else if (streq(cmd, "uname")) t_puts("HuyOS 0.4 x86_64 (custom 64-bit kernel)\n");
    else if (streq(cmd, "ls")) { for (int i = 0; i < nf; i++) { analyze(&files[i], &in); t_puts(files[i].name); t_puts("  ["); t_puts(in.fmt); t_puts("]\n"); } }
    else if (startswith(cmd, "echo ")) { t_puts(cmd + 5); t_putc('\n'); }
    else if (startswith(cmd, "calc ")) {
        const char *p = cmd + 5; int a = parse(&p); while (*p == ' ') p++; char op = *p++; int c = parse(&p); int r = 0, ok = 1;
        if (op == '+') r = a + c; else if (op == '-') r = a - c; else if (op == '*') r = a * c; else if (op == '/' && c) r = a / c; else ok = 0;
        if (!ok) t_puts("error"); else { if (r < 0) { t_putc('-'); r = -r; } t_puts(utoa(r, b)); }
        t_putc('\n');
    } else if (startswith(cmd, "info ")) {
        int found = -1; for (int i = 0; i < nf; i++) if (streq(files[i].name, cmd + 5)) found = i;
        if (found < 0) t_puts("no such file\n");
        else {
            analyze(&files[found], &in);
            t_puts("Format: "); t_puts(in.fmt); t_puts("\nArch:   "); t_puts(in.arch); t_puts("\nBits:   "); t_puts(utoa(in.bits, b));
            t_puts("\nType:   "); t_puts(in.kind); t_putc('\n');
            if (in.has_entry) { hex32(in.entry, b); t_puts("Entry:  "); t_puts(b); t_putc('\n'); }
        }
    } else if (streq(cmd, "date")) { t_puts(dat); t_putc('\n'); }
    else if (streq(cmd, "time")) { t_puts(clk); t_putc('\n'); }
    else if (streq(cmd, "pwd")) t_puts("/home/huy\n");
    else if (streq(cmd, "hostname")) t_puts("huy-pc\n");
    else if (startswith(cmd, "cat ") || startswith(cmd, "type ")) {
        File *ff = find_file(cmd[1] == 'a' ? cmd + 4 : cmd + 5);
        if (!ff) t_puts("no such file\n"); else { analyze(ff, &in); if (in.ft == FT_TXT || in.ft == FT_BAT || in.ft == FT_SH) prog_write((const char *)ff->data, ff->size); else t_puts("binary file\n"); }
    }
    else if (startswith(cmd, "run ")) { File *ff = find_file(cmd + 4); if (ff) exec_file(ff); else t_puts("no such file\n"); }
    else if (streq(cmd, "reboot")) reboot();
    else if (streq(cmd, "shutdown")) power_off();
    else {
        char w[40]; int k = 0; while (cmd[k] && cmd[k] != ' ' && k < 39) { w[k] = cmd[k]; k++; } w[k] = 0;
        File *ff = find_file(w);
        if (ff) exec_file(ff); else { t_puts("unknown command: "); t_puts(cmd); t_putc('\n'); }
    }
}
static void term_key(char c) {
    if (c == '\n') { t_putc('\n'); tin[tn] = 0; t_run(tin); tn = 0; t_prompt(); }
    else if (c == '\b') { if (tn > 0 && tcol > 0) { tn--; tcol--; tb[trow][tcol] = ' '; } }
    else if (c >= ' ' && tn < 90) { tin[tn++] = c; t_putc(c); }
    dirty = 1;
}

/* ---------- годинник ---------- */
static int last_min = -1;
static u8 cmos(u8 r) { outb(0x70, r); return inb(0x71); }
static void read_clock(void) {
    u8 sb = cmos(0x0B), h = cmos(4), m = cmos(2), d = cmos(7), mo = cmos(8);
    if (!(sb & 4)) { h = (h & 15) + (h >> 4) * 10; m = (m & 15) + (m >> 4) * 10; d = (d & 15) + (d >> 4) * 10; mo = (mo & 15) + (mo >> 4) * 10; }
    clk[0] = '0' + h / 10; clk[1] = '0' + h % 10; clk[2] = ':'; clk[3] = '0' + m / 10; clk[4] = '0' + m % 10; clk[5] = 0;
    dat[0] = '0' + d / 10; dat[1] = '0' + d % 10; dat[2] = '.'; dat[3] = '0' + mo / 10; dat[4] = '0' + mo % 10; dat[5] = 0;
    if (m != last_min) { last_min = m; dirty = 1; }
}

/* ---------- Huy AI: вбудований помічник у меню Start (працює офлайн, правила, не нейромережа) ---------- */
#define AIL 4
#define AIC 56
static char ai_in[64], ai_ans[AIL][AIC + 2]; static int ai_n, ai_lines;
static int has(const char *s, const char *p) { for (; *s; s++) if (startswith(s, p)) return 1; return 0; }
static void ai_say(const char *s) {
    ai_lines = 0; memset(ai_ans, 0, sizeof ai_ans);
    while (*s && ai_lines < AIL) {
        int n = slen(s), cut = n;
        if (n > AIC) { cut = AIC; while (cut > 0 && s[cut] != ' ') cut--; if (!cut) cut = AIC; }
        memcpy(ai_ans[ai_lines], s, cut); ai_ans[ai_lines][cut] = 0; ai_lines++;
        s += cut; while (*s == ' ') s++;
    }
}
static void ai_run(void) {
    char q[66], b[64], t[12]; int n = ai_n;
    for (int i = 0; i < n; i++) { char c = ai_in[i]; q[i] = (c >= 'A' && c <= 'Z') ? c + 32 : c; }
    q[n] = 0; ai_n = 0; ai_in[0] = 0; b[0] = 0;
    if (!n) return;
    if ((q[0] >= '0' && q[0] <= '9') || q[0] == '-') {
        const char *p = q; int a = parse(&p); while (*p == ' ') p++; char op = *p++; int c = parse(&p), r = 0, ok = 1;
        if (op == '+') r = a + c; else if (op == '-') r = a - c; else if (op == '*' || op == 'x') r = a * c;
        else if (op == '/') { if (c) r = a / c; else { ai_say("I can't divide by zero, even in 64-bit mode."); return; } } else ok = 0;
        if (ok) { scat(b, "That's "); if (r < 0) { scat(b, "-"); r = -r; } scat(b, utoa(r, t)); ai_say(b); return; }
    }
    if (has(q, "huygle") || has(q, "search") || has(q, "google")) { open_win(8); start_open = 0; ai_say("Opening Huygle."); }
    else if (has(q, "terminal") || has(q, "console") || has(q, "shell")) { open_win(0); start_open = 0; ai_say("Opening Terminal."); }
    else if (has(q, "file")) { open_win(1); start_open = 0; ai_say("Opening Files."); }
    else if (has(q, "setting")) { open_win(2); start_open = 0; ai_say("Opening Settings."); }
    else if (has(q, "dark") || has(q, "night")) { dark = 1; ai_say("Dark mode is on."); }
    else if (has(q, "light") || has(q, "bright")) { dark = 0; ai_say("Light mode is on."); }
    else if (has(q, "accent") || has(q, "color") || has(q, "colour")) { acc_i = (acc_i + 1) % 5; ai_say("Switched to the next accent color."); }
    else if (has(q, "time") || has(q, "clock")) { scat(b, "It is "); scat(b, clk); scat(b, "."); ai_say(b); }
    else if (has(q, "date") || has(q, "today")) { scat(b, "Today is "); scat(b, dat); scat(b, " (day.month)."); ai_say(b); }
    else if (has(q, "shut") || has(q, "power off") || has(q, "turn off")) power_off();
    else if (has(q, "restart") || has(q, "reboot")) reboot();
    else if (has(q, "who") || has(q, "your name") || has(q, "what are you")) ai_say("I'm Huy AI, a tiny assistant that lives in the Huy OS Start menu. I run fully offline inside the kernel.");
    else if (streq(q, "hi") || has(q, "hello") || has(q, "hey")) ai_say("Hello! Ask me to open an app, switch the theme, tell the time, or do quick math like 12*7.");
    else if (has(q, "64") || has(q, "bit")) ai_say("Huy OS runs in 64-bit x86-64 long mode.");
    else if (has(q, "joke")) ai_say("Why do programmers prefer dark mode? Because light attracts bugs.");
    else if (has(q, "help") || has(q, "what can")) ai_say("Try: open terminal, open files, settings, dark mode, light mode, next color, time, date, 12*7, restart, shut down.");
    else ai_say("I don't know that yet. Type 'help' to see what I can do.");
}
static void ai_key(char c) {
    if (c == '\n') ai_run();
    else if (c == '\b') { if (ai_n > 0) ai_in[--ai_n] = 0; }
    else if (c >= ' ' && ai_n < 60) { ai_in[ai_n++] = c; ai_in[ai_n] = 0; }
    dirty = 1;
}

/* ---------- 3D: математика і растеризатор (цілі числа, без FPU) ---------- */
static volatile u32 ticks;
static int sintab[1024];
static void trig_init(void) {                       /* sin за наближенням Бхаскари, Q12, 1024 одиниці = коло */
    for (int a = 0; a < 1024; a++) { int x = a & 511; long p = (long)x * (512 - x); int v = (int)(65536L * p / (1310720L - 4 * p)); sintab[a] = a < 512 ? v : -v; }
}
#define SIN(a) sintab[(a) & 1023]
#define COS(a) sintab[((a) + 256) & 1023]
static u64 isq(u64 n) { u64 r = 0, b = 1UL << 62; while (b > n) b >>= 2; while (b) { if (n >= r + b) { n -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; } return r; }
static int cx0, cy0, cx1, cy1;                      /* прямокутник відсікання */
static __attribute__((optimize("O2"))) void hline(int y, int xa, int xb, u32 c) {
    if (y < cy0 || y >= cy1) return;
    if (xa > xb) { int t = xa; xa = xb; xb = t; }
    if (xa < cx0) xa = cx0; if (xb >= cx1) xb = cx1 - 1;
    u32 *p = back + y * W; for (int x = xa; x <= xb; x++) p[x] = c;
}
static __attribute__((optimize("O2"))) void tri(int x0, int y0, int x1, int y1, int x2, int y2, u32 c) {
    int t;
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
    if (y1 > y2) { t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
    if (y2 == y0) return;
    int ya = y0 < cy0 ? cy0 : y0, yb = y2 >= cy1 ? cy1 - 1 : y2;
    for (int y = ya; y <= yb; y++) {
        int xa = x0 + (x2 - x0) * (y - y0) / (y2 - y0), xb;
        if (y < y1) xb = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
        else if (y2 > y1) xb = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
        else xb = x1;
        hline(y, xa, xb, c);
    }
}
static void line(int x0, int y0, int x1, int y1, u32 c) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (int n = 0; n < 4096; n++) {
        if (x0 >= cx0 && x0 < cx1 && y0 >= cy0 && y0 < cy1) back[y0 * W + x0] = c;
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * e; if (e2 >= dy) { e += dy; x0 += sx; } if (e2 <= dx) { e += dx; y0 += sy; }
    }
}
static u32 shade(u32 c, int k) {
    int r = ((c >> 16) & 255) * k / 256, g = ((c >> 8) & 255) * k / 256, b = (c & 255) * k / 256;
    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    return RGB(r, g, b);
}

/* ---------- іконки ---------- */
static void draw_app_icon(int kind, int x, int y, int s);
static void pg_shape(int x, int y, int pw, int ph, int fold, u32 c) { for (int j = 0; j < ph; j++) { int r = j < fold ? pw - (fold - j) : pw; fill_rect(x, y + j, r, 1, c, 255); } }
static void draw_page(int x, int y, int s) {
    int pw = s * 3 / 4, fold = s / 4, px = x + s / 8;
    pg_shape(px - 1, y - 1, pw + 2, s + 2, fold + 1, RGB(140, 146, 160));
    pg_shape(px, y, pw, s, fold, RGB(252, 252, 254));
    for (int j = 0; j < fold; j++) fill_rect(px + pw - fold, y + j, j + 1, 1, RGB(205, 212, 226), 255);
}
static void band_icon(int x, int y, int s, u32 col, const char *lab) {
    int px = x + s / 8, pw = s * 3 / 4, by = y + s * 3 / 5, bh = s * 2 / 5;
    draw_page(x, y, s);
    fill_rect(px + pw / 6, y + s * 3 / 10, pw * 2 / 3, 2, RGB(150, 160, 180), 255);
    fill_rect(px + pw / 6, y + s * 2 / 5, pw / 2, 2, RGB(150, 160, 180), 255);
    fill_rect(px, by, pw, bh, col, 255);
    if (s >= 44) textc(px + pw / 2, by + (bh - 16) / 2, lab, WHITE);
}
static void draw_file_icon(int x, int y, int s, int ft) {
    int px = x + s / 8, pw = s * 3 / 4;
    if (ft == FT_DIR) {                                   /* папка як у Windows 11 */
        fill_rrect(x, y + s / 8, s * 2 / 5, s / 4, s / 12, RGB(222, 160, 20), 255);
        fill_rrect(x, y + s / 5, s, s * 3 / 4, s / 12, RGB(235, 178, 36), 255);
        fill_rect(x + s / 10, y + s * 3 / 10, s * 8 / 10, s / 6, RGB(250, 250, 252), 255);
        fill_rrect(x, y + s * 11 / 20, s, s * 2 / 5, s / 12, RGB(255, 212, 84), 255);
    } else if (ft == FT_PE) {                             /* .exe: вікно з логотипом */
        int bx = x + s / 16, by = y + s / 8, bw = s * 7 / 8, bh = s * 3 / 4, q = s / 9, qx = x + s * 3 / 10, qy = y + s * 9 / 20;
        fill_rrect(bx, by, bw, bh, s / 10, RGB(130, 138, 155), 255);
        fill_rrect(bx + 1, by + 1, bw - 2, bh - 2, s / 10, RGB(250, 250, 253), 255);
        fill_rrect_rows(bx + 1, by + 1, bw - 2, bh - 2, s / 10, RGB(0, 120, 215), 255, 0, s / 5);
        fill_rect(qx, qy, q, q, RGB(0, 120, 215), 255); fill_rect(qx + q + 2, qy, q, q, RGB(40, 150, 235), 255);
        fill_rect(qx, qy + q + 2, q, q, RGB(40, 150, 235), 255); fill_rect(qx + q + 2, qy + q + 2, q, q, RGB(0, 120, 215), 255);
    } else if (ft == FT_BAT) {                            /* .bat: сторінка з консоллю */
        draw_page(x, y, s);
        fill_rect(px + pw / 8, y + s * 3 / 10, pw * 3 / 4, s * 2 / 5, RGB(30, 30, 32), 255);
        stroke(px + pw * 22 / 100, y + s * 2 / 5, px + pw * 42 / 100, y + s / 2, 1, WHITE);
        stroke(px + pw * 42 / 100, y + s / 2, px + pw * 22 / 100, y + s * 3 / 5, 1, WHITE);
        fill_rect(px + pw / 2, y + s * 3 / 5, pw / 4, 2, WHITE, 255);
    } else if (ft == FT_SH) band_icon(x, y, s, RGB(40, 150, 80), "SH");
    else if (ft == FT_ELF) band_icon(x, y, s, RGB(235, 130, 20), "ELF");
    else if (ft == FT_MACHO) band_icon(x, y, s, RGB(120, 124, 135), "APP");
    else {                                                /* .txt: біла сторінка з рядками тексту */
        draw_page(x, y, s);
        for (int k = 0; k < 4; k++) fill_rect(px + pw / 6, y + s * 2 / 5 + k * (s / 8), k == 3 ? pw / 2 : pw * 2 / 3, 2, RGB(150, 160, 180), 255);
    }
}
static void draw_app_icon(int kind, int x, int y, int s) {
    int cx = x + s / 2, cy = y + s / 2;
    if (kind == 0) {
        draw_huy_icon(cx, cy, s / 2, RGB(0, 120, 215), WHITE);
    } else if (kind == 1) {
        fill_rrect(x, y, s, s, s / 5, RGB(28, 28, 32), 255); fill_rrect(x + 1, y + 1, s - 2, 3, 1, RGB(70, 70, 78), 255);
        draw_char(cx - 16, cy - 16, '>', WHITE, 2); draw_char(cx, cy - 16, '_', WHITE, 2);
    } else if (kind == 2) {
        fill_rrect(x, y + s / 8, s * 2 / 5, s / 4, s / 10, RGB(225, 150, 15), 255);
        fill_rrect(x, y + s / 5, s, s * 3 / 4, s / 8, RGB(255, 196, 50), 255); fill_rrect(x, y + s * 2 / 5, s, s * 11 / 20, s / 8, RGB(255, 214, 90), 255);
    } else if (kind == 4) {                                   /* 3D Viewer: куб */
        int r = s * 34 / 100, q = r * 87 / 100; cx0 = 0; cy0 = 0; cx1 = W; cy1 = H;
        fill_rrect(x, y, s, s, s / 5, RGB(28, 34, 52), 255);
        tri(cx, cy - r, cx + q, cy - r / 2, cx, cy, RGB(120, 200, 255)); tri(cx, cy - r, cx, cy, cx - q, cy - r / 2, RGB(120, 200, 255));
        tri(cx - q, cy - r / 2, cx, cy, cx, cy + r, RGB(60, 140, 230)); tri(cx - q, cy - r / 2, cx, cy + r, cx - q, cy + r / 2, RGB(60, 140, 230));
        tri(cx, cy, cx + q, cy - r / 2, cx + q, cy + r / 2, RGB(30, 90, 190)); tri(cx, cy, cx + q, cy + r / 2, cx, cy + r, RGB(30, 90, 190));
    } else if (kind == 5) {                                   /* 3D Space: зорі */
        cx0 = 0; cy0 = 0; cx1 = W; cy1 = H;
        fill_rrect(x, y, s, s, s / 5, RGB(10, 12, 28), 255);
        line(cx, cy, cx + s / 3, cy - s / 4, RGB(200, 220, 255)); line(cx - s / 6, cy + s / 8, cx - s * 5 / 12, cy + s / 3, RGB(200, 220, 255));
        line(cx + s / 12, cy + s / 6, cx + s / 4, cy + s * 5 / 12, RGB(200, 220, 255)); line(cx - s / 8, cy - s / 8, cx - s / 3, cy - s * 5 / 12, RGB(160, 190, 255));
        fill_rect(cx + s / 3 - 1, cy - s / 4 - 1, 3, 3, WHITE, 255); fill_rect(cx - s * 5 / 12 - 1, cy + s / 3 - 1, 3, 3, WHITE, 255);
        fill_rect(cx + s / 4 - 1, cy + s * 5 / 12 - 1, 3, 3, WHITE, 255); fill_rect(cx - s / 3 - 1, cy - s * 5 / 12 - 1, 3, 3, WHITE, 255);
    } else if (kind == 6) {                                   /* 3D Maze: коридор */
        int a = s / 6, l = x + s / 8, r2 = x + s * 7 / 8, t = y + s / 8, b = y + s * 7 / 8; cx0 = 0; cy0 = 0; cx1 = W; cy1 = H;
        fill_rrect(x, y, s, s, s / 5, RGB(40, 26, 22), 255);
        tri(l, t, cx - a, cy - a, cx - a, cy + a, RGB(200, 100, 60)); tri(l, t, cx - a, cy + a, l, b, RGB(200, 100, 60));
        tri(r2, t, cx + a, cy - a, cx + a, cy + a, RGB(150, 70, 45)); tri(r2, t, cx + a, cy + a, r2, b, RGB(150, 70, 45));
        tri(l, t, r2, t, cx + a, cy - a, RGB(70, 76, 120)); tri(l, t, cx + a, cy - a, cx - a, cy - a, RGB(70, 76, 120));
        tri(l, b, r2, b, cx + a, cy + a, RGB(90, 82, 76)); tri(l, b, cx + a, cy + a, cx - a, cy + a, RGB(90, 82, 76));
        fill_rect(cx - a, cy - a, 2 * a + 1, 2 * a + 1, RGB(30, 20, 16), 255);
    } else if (kind == 7) {                                   /* Huygle: лупа */
        fill_rrect(x, y, s, s, s / 5, WHITE, 255);
        int gx = cx - s / 12, gy = cy - s / 12, rr = s * 28 / 100;
        fill_circle(gx, gy, rr, RGB(66, 133, 244), 255); fill_circle(gx, gy, rr * 6 / 10, WHITE, 255);
        stroke(gx + rr * 7 / 10, gy + rr * 7 / 10, x + s * 85 / 100, y + s * 85 / 100, s / 14 + 1, RGB(234, 67, 53));
        fill_circle(gx - rr * 6 / 10, gy - rr * 6 / 10, s / 18 + 1, RGB(251, 188, 5), 255); fill_circle(gx + rr * 6 / 10, gy - rr * 6 / 10, s / 18 + 1, RGB(52, 168, 83), 255);
    } else {
        fill_rrect(x, y, s, s, s / 5, RGB(105, 110, 122), 255);
        fill_rect(cx - s / 3, cy - 2, s * 2 / 3, 5, WHITE, 255); fill_rect(cx - 2, cy - s / 3, 5, s * 2 / 3, WHITE, 255);
        fill_circle(cx, cy, s * 3 / 10, WHITE, 255); fill_circle(cx, cy, s / 6, RGB(105, 110, 122), 255);
    }
}

/* ---------- 3D програми: Viewer, Space, Maze ---------- */
#define MV 160
#define MF 160
static int vx[MV], vy[MV], vz[MV], nv, nfc, rx[MV], ry[MV], rz[MV], px_[MV], py_[MV], order[MF], fkey[MF];
static short fi[MF][4]; static u32 fcol[MF];
static int v_shape, v_wire, v_spin = 1, v_ax = 90, v_ay, v_drag, v_lx, v_ly;
static void mv(int x, int y, int z) { vx[nv] = x; vy[nv] = y; vz[nv] = z; nv++; }
static void mf(int a, int b, int c, int d, u32 col) { fi[nfc][0] = a; fi[nfc][1] = b; fi[nfc][2] = c; fi[nfc][3] = d; fcol[nfc++] = col; }
static void mesh_build(int shape) {
    nv = nfc = 0;
    if (shape == 0) {                                      /* куб */
        static const short q[6][4] = { {0,1,3,2}, {4,5,7,6}, {0,1,5,4}, {2,3,7,6}, {0,2,6,4}, {1,3,7,5} };
        static const u32 cc[6] = { RGB(230,70,70), RGB(70,200,90), RGB(70,120,235), RGB(240,200,50), RGB(190,80,220), RGB(60,200,210) };
        for (int i = 0; i < 8; i++) mv(i & 1 ? 70 : -70, i & 2 ? 70 : -70, i & 4 ? 70 : -70);
        for (int f = 0; f < 6; f++) mf(q[f][0], q[f][1], q[f][2], q[f][3], cc[f]);
    } else if (shape == 1) {                               /* піраміда */
        mv(0, -95, 0); mv(-85, 60, -85); mv(85, 60, -85); mv(85, 60, 85); mv(-85, 60, 85);
        static const u32 cc[4] = { RGB(240,170,50), RGB(230,90,70), RGB(80,170,235), RGB(120,210,110) };
        for (int f = 0; f < 4; f++) mf(0, 1 + f, 1 + (f + 1) % 4, 0, cc[f]);
        mf(1, 2, 3, 4, RGB(150, 150, 160));
    } else if (shape == 2) {                               /* тор */
        for (int i = 0; i < 16; i++) for (int j = 0; j < 8; j++) {
            int u = i * 64, v = j * 128, ring = 85 + ((36 * COS(v)) >> 12);
            mv((ring * COS(u)) >> 12, (36 * SIN(v)) >> 12, (ring * SIN(u)) >> 12);
        }
        for (int i = 0; i < 16; i++) for (int j = 0; j < 8; j++) {
            int i1 = (i + 1) % 16, j1 = (j + 1) % 8;
            mf(i * 8 + j, i1 * 8 + j, i1 * 8 + j1, i * 8 + j1, RGB(140 + (SIN(i * 64) >> 6), 100 + (COS(i * 64) >> 7), (i & 1) ? 235 : 195));
        }
    } else {                                               /* сфера */
        mv(0, -105, 0); mv(0, 105, 0);
        for (int k = 1; k <= 7; k++) for (int j = 0; j < 12; j++) {
            int th = -256 + k * 64, ph = j * 1024 / 12, rr = (105 * COS(th)) >> 12;
            mv((rr * COS(ph)) >> 12, (105 * SIN(th)) >> 12, (rr * SIN(ph)) >> 12);
        }
        for (int j = 0; j < 12; j++) {
            int j1 = (j + 1) % 12;
            mf(0, 2 + j1, 2 + j, 0, (j & 1) ? RGB(60, 190, 200) : RGB(60, 130, 230));
            mf(1, 2 + 6 * 12 + j, 2 + 6 * 12 + j1, 1, (j & 1) ? RGB(60, 130, 230) : RGB(60, 190, 200));
            for (int k = 1; k < 7; k++) mf(2 + (k - 1) * 12 + j, 2 + (k - 1) * 12 + j1, 2 + k * 12 + j1, 2 + k * 12 + j, ((j + k) & 1) ? RGB(60, 190, 200) : RGB(70, 120, 235));
        }
    }
}
static void draw_model(int ox, int oy, int vh) {
    int F = vh * 4 / 3, D = 420, sa = SIN(v_ay), ca = COS(v_ay), sb = SIN(v_ax), cb = COS(v_ax);
    for (int i = 0; i < nv; i++) {
        int x = vx[i], y = vy[i], z = vz[i];
        int x1 = (x * ca + z * sa) >> 12, z1 = (z * ca - x * sa) >> 12;
        int y2 = (y * cb - z1 * sb) >> 12, z2 = (y * sb + z1 * cb) >> 12, d = z2 + D;
        if (d < 60) d = 60;
        rx[i] = x1; ry[i] = y2; rz[i] = z2; px_[i] = ox + x1 * F / d; py_[i] = oy + y2 * F / d;
    }
    for (int f = 0; f < nfc; f++) { fkey[f] = rz[fi[f][0]] + rz[fi[f][1]] + rz[fi[f][2]] + rz[fi[f][3]]; order[f] = f; }
    for (int i = 1; i < nfc; i++) { int t = order[i], j = i - 1; while (j >= 0 && fkey[order[j]] < fkey[t]) { order[j + 1] = order[j]; j--; } order[j + 1] = t; }
    for (int q = 0; q < nfc; q++) {
        int f = order[q], a = fi[f][0], b = fi[f][1], c = fi[f][2], d = fi[f][3];
        if (v_wire) {
            u32 wc = RGB(90, 200, 255);
            line(px_[a], py_[a], px_[b], py_[b], wc); line(px_[b], py_[b], px_[c], py_[c], wc);
            line(px_[c], py_[c], px_[d], py_[d], wc); line(px_[d], py_[d], px_[a], py_[a], wc);
            continue;
        }
        long ux = rx[b] - rx[a], uy = ry[b] - ry[a], uz = rz[b] - rz[a], wx = rx[c] - rx[a], wy = ry[c] - ry[a], wz = rz[c] - rz[a];
        long nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx;
        long len = (long)isq((u64)(nx * nx + ny * ny + nz * nz));
        long dot = len ? (nx * -1229 + ny * -2048 + nz * -3318) / len : 0;
        if (dot < 0) dot = -dot;
        int k = 80 + (int)(dot * 175 / 4096);
        u32 col = shade(fcol[f], k), ec = shade(fcol[f], k * 3 / 4);
        tri(px_[a], py_[a], px_[b], py_[b], px_[c], py_[c], col); tri(px_[a], py_[a], px_[c], py_[c], px_[d], py_[d], col);
        line(px_[a], py_[a], px_[b], py_[b], ec); line(px_[b], py_[b], px_[c], py_[c], ec);
        line(px_[c], py_[c], px_[d], py_[d], ec); line(px_[d], py_[d], px_[a], py_[a], ec);
    }
}

/* --- Space: політ крізь зорі --- */
#define NS 140
static int s_x[NS], s_y[NS], s_z[NS], s_speed = 4, s_col;
static u32 rnd_s = 12345;
static u32 rnd(void) { rnd_s = rnd_s * 1103515245u + 12345u; return (rnd_s >> 16) & 0x7FFF; }
static void star_new(int i, int far) { s_x[i] = (int)(rnd() % 1600) - 800; s_y[i] = (int)(rnd() % 1200) - 600; s_z[i] = far ? 200 + (int)(rnd() % 800) : 1000; }
static void stars_init(void) { for (int i = 0; i < NS; i++) star_new(i, 1); }
static void stars_step(void) { for (int i = 0; i < NS; i++) { s_z[i] -= s_speed * 6; if (s_z[i] < 40) star_new(i, 0); } }
static void draw_space(int ox, int oy, int vw) {
    int F = vw / 2;
    for (int i = 0; i < NS; i++) {
        int z = s_z[i], z2 = z + s_speed * 18, sx = ox + s_x[i] * F / z, sy = oy + s_y[i] * F / z, tx = ox + s_x[i] * F / z2, ty = oy + s_y[i] * F / z2;
        int b = 255 - z * 200 / 1000; if (b < 40) b = 40;
        u32 c = s_col == 0 ? RGB(b, b, b) : s_col == 1 ? RGB(b / 4, b * 3 / 4, b) : s_col == 2 ? RGB(b, b * 6 / 10, b / 6) : (i % 3 == 0 ? RGB(b, b / 3, b / 3) : i % 3 == 1 ? RGB(b / 3, b, b / 3) : RGB(b / 3, b / 3, b));
        line(tx, ty, sx, sy, c);
        if (z < 400) { hline(sy, sx, sx + 1, c); hline(sy + 1, sx, sx + 1, c); }
    }
}

/* --- Maze: 3D-лабіринт (raycasting), керування W/S/A/D --- */
#define MZ 15
static char mz[MZ][MZ]; static int mz_x, mz_y, mz_a, mz_map = 1, mz_win;
static void maze_new(void) {
    static const int ox[4] = { 2, -2, 0, 0 }, oy[4] = { 0, 0, 2, -2 };
    int st[MZ * MZ], sp = 0;
    memset(mz, 1, sizeof mz); rnd_s ^= ticks * 2654435761u;
    mz[1][1] = 0; st[sp++] = MZ + 1;
    while (sp) {
        int cur = st[sp - 1], x = cur % MZ, y = cur / MZ, d[4], nd = 0;
        for (int k = 0; k < 4; k++) { int nx = x + ox[k], ny = y + oy[k]; if (nx > 0 && ny > 0 && nx < MZ - 1 && ny < MZ - 1 && mz[ny][nx] == 1) d[nd++] = k; }
        if (!nd) { sp--; continue; }
        int k = d[rnd() % nd];
        mz[y + oy[k] / 2][x + ox[k] / 2] = 0; mz[y + oy[k]][x + ox[k]] = 0; st[sp++] = (y + oy[k]) * MZ + x + ox[k];
    }
    mz[MZ - 2][MZ - 2] = 3; mz_x = mz_y = 384; mz_a = 0; mz_win = 0;
}
static int mz_free(int x, int y) {
    for (int dx = -48; dx <= 48; dx += 96) for (int dy = -48; dy <= 48; dy += 96) if (mz[(y + dy) >> 8][(x + dx) >> 8] == 1) return 0;
    return 1;
}
static void maze_key(char c) {
    if (c >= 'A' && c <= 'Z') c += 32;
    if (c == 'a') mz_a -= 24; else if (c == 'd') mz_a += 24;
    else if (c == 'w' || c == 's') {
        int sg = c == 'w' ? 1 : -1, nx = mz_x + sg * ((COS(mz_a) * 44) >> 12), ny = mz_y + sg * ((SIN(mz_a) * 44) >> 12);
        if (mz_free(nx, ny)) { mz_x = nx; mz_y = ny; } else if (mz_free(nx, mz_y)) mz_x = nx; else if (mz_free(mz_x, ny)) mz_y = ny;
    } else return;
    if (mz[mz_y >> 8][mz_x >> 8] == 3) mz_win = 1;
    dirty = 1;
}
static void draw_maze(int ix, int iy, int iw, int ih) {
    int nc = iw / 2, half = ih / 2;
    fill_rect(ix, iy, iw, half, RGB(34, 38, 70), 255); fill_rect(ix, iy + half, iw, ih - half, RGB(52, 46, 42), 255);
    for (int c = 0; c < nc; c++) {
        int ang = mz_a + (c - nc / 2) * 171 / nc, dx = COS(ang), dy = SIN(ang), n, hit = 0, side = 0, cell = 1;
        for (n = 1; n < 700; n++) {
            int X = mz_x + ((dx * n * 10) >> 12), Y = mz_y + ((dy * n * 10) >> 12), gx = X >> 8, gy = Y >> 8;
            if (gx < 0 || gy < 0 || gx >= MZ || gy >= MZ) break;
            cell = mz[gy][gx];
            if (cell == 1 || cell == 3) { side = ((mz_x + ((dx * (n - 1) * 10) >> 12)) >> 8) != gx; hit = 1; break; }
        }
        if (!hit) continue;
        int perp = (n * 10 * COS(ang - mz_a)) >> 12; if (perp < 16) perp = 16;
        int hh = ih * 256 / perp; if (hh > ih) hh = ih;
        int b = 255 - perp * 255 / 2304; if (b < 30) b = 30;
        u32 col = cell == 3 ? shade(RGB(60, 220, 110), b) : shade(side ? RGB(200, 95, 60) : RGB(150, 70, 48), b);
        fill_rect(ix + c * 2, iy + half - hh / 2, 2, hh, col, 255);
    }
    if (mz_map) {
        int mx0 = ix + iw - MZ * 4 - 6, my0 = iy + 6;
        fill_rect(mx0 - 2, my0 - 2, MZ * 4 + 4, MZ * 4 + 4, 0, 150);
        for (int y = 0; y < MZ; y++) for (int x = 0; x < MZ; x++) if (mz[y][x] == 1 || mz[y][x] == 3) fill_rect(mx0 + x * 4, my0 + y * 4, 4, 4, mz[y][x] == 3 ? RGB(60, 220, 110) : RGB(150, 150, 165), 255);
        int pxm = mx0 + mz_x * 4 / 256, pym = my0 + mz_y * 4 / 256;
        line(pxm, pym, pxm + ((COS(mz_a) * 7) >> 12), pym + ((SIN(mz_a) * 7) >> 12), RGB(255, 230, 90)); fill_rect(pxm - 1, pym - 1, 3, 3, RGB(255, 230, 90), 255);
    }
    text(ix + 10, iy + ih - 22, "W/S move   A/D turn", RGB(200, 200, 210));
    if (mz_win) { fill_rrect(ix + iw / 2 - 130, iy + ih / 2 - 24, 260, 48, 10, 0, 170); textc(ix + iw / 2, iy + ih / 2 - 8, "You escaped the maze!", RGB(255, 230, 90)); }
}

/* --- меню програм і малювання --- */
static int menu_n(int app) { return app == 5 ? 6 : app == 6 ? 4 : 2; }
static const char *menu_lab(int app, int k) {
    static const char *t5[4] = { "Cube", "Pyramid", "Torus", "Sphere" }, *t6[4] = { "Slower", "Faster", "Color", "Reset" };
    if (app == 5) return k < 4 ? t5[k] : k == 4 ? (v_wire ? "Wire" : "Solid") : (v_spin ? "Spin: On" : "Spin: Off");
    if (app == 6) return t6[k];
    return k == 0 ? "New maze" : (mz_map ? "Map: On" : "Map: Off");
}
static int menu_on(int app, int k) { if (app == 5) return k == v_shape || (k == 4 && v_wire) || (k == 5 && v_spin); if (app == 7) return k == 1 && mz_map; return 0; }
static void menu_geom(int app, int k, int *bx, int *bw) { int xx = 12; for (int j = 0; j < k; j++) xx += slen(menu_lab(app, j)) * 8 + 26; *bx = xx; *bw = slen(menu_lab(app, k)) * 8 + 20; }
static void menu_act(int app, int k) {
    if (app == 5) { if (k < 4) { v_shape = k; mesh_build(k); } else if (k == 4) v_wire = !v_wire; else v_spin = !v_spin; }
    else if (app == 6) { if (k == 0 && s_speed > 1) s_speed--; else if (k == 1 && s_speed < 12) s_speed++; else if (k == 2) s_col = (s_col + 1) % 4; else if (k == 3) { stars_init(); s_speed = 4; } }
    else { if (k == 0) maze_new(); else mz_map = !mz_map; }
}
static void app_click(int app, int lx, int ly) {
    if (ly >= 40 && ly < 66) { for (int k = 0; k < menu_n(app); k++) { int bx, bw; menu_geom(app, k, &bx, &bw); if (lx >= bx && lx < bx + bw) { menu_act(app, k); return; } } }
    else if (app == 5 && ly >= 76) { v_drag = 1; v_lx = mx; v_ly = my; }
}
static void draw_app(int app, int x, int y, int w, int h) {
    char b[12];
    for (int k = 0; k < menu_n(app); k++) {
        int bx, bw, on = menu_on(app, k); menu_geom(app, k, &bx, &bw);
        fill_rrect(x + bx, y + 40, bw, 26, 6, on ? ACCENT : C_CARD, 255); text(x + bx + 10, y + 45, menu_lab(app, k), on ? WHITE : C_TEXT);
    }
    int vx0 = x + 12, vy0 = y + 76, vw = w - 24, vh = h - 88;
    if (vh < 40 || vw < 80) return;
    fill_rrect(vx0, vy0, vw, vh, 8, RGB(10, 12, 20), 255);
    int ix = vx0 + 2, iy = vy0 + 2, iw = vw - 4, ih = vh - 4;
    cx0 = ix; cy0 = iy; cx1 = ix + iw; cy1 = iy + ih;
    if (app == 5) { draw_model(ix + iw / 2, iy + ih / 2, ih); text(ix + 10, iy + ih - 22, "Drag with the mouse to rotate", RGB(130, 136, 155)); }
    else if (app == 6) { draw_space(ix + iw / 2, iy + ih / 2, iw); b[0] = 0; scat(b, "Speed "); { char t[12]; scat(b, utoa(s_speed, t)); } text(ix + 10, iy + ih - 22, b, RGB(130, 136, 155)); }
    else draw_maze(ix, iy, iw, ih);
}

/* ---------- Huygle: пошук у стилі Google (програми, файли, вбудовані відповіді, калькулятор, час) ---------- */
static void activate_item(int fi);
#define SQ 44
#define SRN 8
static char sq_in[SQ + 2]; static int sq_n, sq_res;
typedef struct { int kind, idx; const char *title; char url[48], snip[64]; } SR;   /* kind: 0 програма, 1 файл, 2 довідка */
static SR sr[SRN]; static int srn; static char sq_ans[24], sq_alab[24], sq_q[SQ + 2];
static const struct { const char *n; int w; const char *d; } sapps[6] = {
    { "Terminal", 0, "Command line. Try help, ls, cat, run, calc." }, { "Files", 1, "Browse folders, open and run files." },
    { "Settings", 2, "Dark mode, accent colors and system info." }, { "3D Viewer", 5, "Cube, pyramid, torus, sphere. Drag to rotate." },
    { "3D Space", 6, "Fly through a field of stars." }, { "3D Maze", 7, "Find the way out of a 3D maze. W/S/A/D." } };
static const struct { const char *key, *title, *url, *snip; } sinfo[7] = {
    { "huy os kernel 64 bit long mode x86", "About Huy OS", "huy.os > about", "A custom 64-bit OS written from scratch: own bootloader and kernel." },
    { "mouse keyboard input ps2 arrows click", "Mouse and keyboard", "huy.os > input", "PS/2 mouse on IRQ12. No mouse? Arrows move, F2 = left click, F1 = right." },
    { "3d graphics render integer fpu cube", "3D graphics without an FPU", "huy.os > 3d", "Integer-only rasterizer with a sine table powers Viewer, Space, Maze." },
    { "run exe elf bat sh program script", "Running programs", "huy.os > run", "Runs .bat/.sh scripts, Linux ELF64 and Windows PE64 .exe (small API subset)." },
    { "search google huygle find web internet", "About Huygle", "huy.os > huygle", "You are using it. It searches apps, files and a few built-in answers. No internet yet." },
    { "virtualbox vmware qemu iso boot install", "How to boot Huy OS", "huy.os > boot", "VirtualBox: Other/Unknown 64-bit, PS/2 mouse, no EFI, attach huyos.iso." },
    { "theme dark light accent color", "Change the theme", "huy.os > theme", "Settings > Dark mode and Accent color. Or ask Huy AI: dark mode." } };


static int ci_at(const char *s, const char *w, int n) { for (int i = 0; i < n; i++) if (lc(s[i]) != lc(w[i])) return 0; return 1; }
static int ci_find(const char *h, int hn, const char *w, int wn) { for (int i = 0; i + wn <= hn; i++) if (ci_at(h + i, w, wn)) return i; return -1; }
/* any=0: усі слова запиту є в тексті; any=1: хоч одне слово (>=3 букв) */
static int q_match(const char *h, int hn, const char *q, int any, int *pos) {
    int first = -1, miss = 0; const char *p = q;
    while (*p) {
        while (*p == ' ') p++; if (!*p) break;
        const char *e = p; while (*e && *e != ' ') e++;
        int wl = e - p, k = (any && wl < 3) ? -1 : ci_find(h, hn, p, wl);
        if (k < 0) miss++; else if (first < 0) first = k;
        p = e;
    }
    if (pos) *pos = first < 0 ? 0 : first;
    return any ? first >= 0 : (first >= 0 && !miss);
}
static void sr_add(int kind, int idx, const char *title, const char *pre, const char *nm, const char *snip, int sn) {
    if (srn >= SRN) return; SR *r = &sr[srn++]; r->kind = kind; r->idx = idx; r->title = title;
    r->url[0] = 0; scat(r->url, pre); if (nm) scat(r->url, nm); r->url[47] = 0;
    int n = 0; while (n < 62 && (sn < 0 ? snip[n] : n < sn) && snip[n]) { char c = snip[n]; r->snip[n] = c >= ' ' ? c : ' '; n++; } r->snip[n] = 0;
}
static void sq_calc(const char *q) {
    sq_ans[0] = 0; sq_alab[0] = 0;
    if ((q[0] >= '0' && q[0] <= '9') || q[0] == '-') {
        const char *p = q; int a = parse(&p); while (*p == ' ') p++; char op = *p++; int c = parse(&p), r = 0, ok = 1;
        if (op == '+') r = a + c; else if (op == '-') r = a - c; else if (op == '*' || op == 'x') r = a * c;
        else if (op == '/') { if (!c) { scat(sq_ans, "undefined"); scat(sq_alab, "Calculator"); return; } r = a / c; } else ok = 0;
        if (ok) { char t[12]; if (r < 0) { scat(sq_ans, "-"); r = -r; } scat(sq_ans, utoa(r, t)); scat(sq_alab, "Calculator"); }
    } else if (ci_find(q, slen(q), "time", 4) >= 0 || ci_find(q, slen(q), "clock", 5) >= 0) { scat(sq_ans, clk); scat(sq_alab, "Time now"); }
    else if (ci_find(q, slen(q), "date", 4) >= 0 || ci_find(q, slen(q), "today", 5) >= 0) { scat(sq_ans, dat); scat(sq_alab, "Date (day.month)"); }
}
static void sq_search(void) {
    int n = 0; srn = 0; memset(sq_q, 0, sizeof sq_q);
    while (n < sq_n) { sq_q[n] = sq_in[n]; n++; }
    sq_res = 1; sq_calc(sq_q);
    for (int k = 0; k < 7; k++) if (q_match(sinfo[k].key, slen(sinfo[k].key), sq_q, 1, 0)) sr_add(2, k, sinfo[k].title, sinfo[k].url, 0, sinfo[k].snip, -1);
    for (int k = 0; k < 6; k++) { char h[96]; h[0] = 0; scat(h, sapps[k].n); scat(h, " "); scat(h, sapps[k].d);
        if (q_match(h, slen(h), sq_q, 0, 0)) sr_add(0, sapps[k].w, sapps[k].n, "huy.os > apps > ", sapps[k].n, sapps[k].d, -1); }
    for (int i = 0; i < nf; i++) {
        File *f = &files[i]; int p = 0, hit = q_match(f->name, slen(f->name), sq_q, 0, 0); Info in;
        if (f->isdir) { if (hit) sr_add(1, i, f->name, "huy.os > files > ", f->name, "Folder", -1); continue; }
        analyze(f, &in);
        if (in.ft == FT_TXT || in.ft == FT_BAT || in.ft == FT_SH) {
            int tp = q_match((const char *)f->data, f->size, sq_q, 0, &p);
            if (hit || tp) { int s = tp ? (p > 12 ? p - 12 : 0) : 0; sr_add(1, i, f->name, "huy.os > files > ", f->name, (const char *)f->data + s, f->size - s < 62 ? f->size - s : 62); }
        } else if (hit) sr_add(1, i, f->name, "huy.os > files > ", f->name, in.fmt, -1);
    }
}
static void sq_open(int k) {
    if (k < 0 || k >= srn) return;
    if (sr[k].kind == 0) open_win(sr[k].idx); else if (sr[k].kind == 1) activate_item(sr[k].idx);
}
static void sq_key(char c) {
    if (c == '\n') { if (sq_n) sq_search(); }
    else if (c == '\b') { if (sq_n > 0) sq_in[--sq_n] = 0; }
    else if (c >= ' ' && sq_n < SQ) { sq_in[sq_n++] = c; sq_in[sq_n] = 0; }
    dirty = 1;
}
/* геометрія: головна сторінка та сторінка результатів */
static int sq_oy(int h) { int o = (h - 34 - 230) / 2; return 34 + (o < 8 ? 8 : o); }
static int sq_top(void) { return 120 + (sq_ans[0] ? 78 : 0); }
static void sq_glass(int cx, int cy) { fill_circle(cx, cy, 7, C_SUB, 255); fill_circle(cx, cy, 5, C_CARD, 255); stroke(cx + 5, cy + 5, cx + 10, cy + 10, 1, C_SUB); }
static void sq_logo(int cx, int y, int sc) {
    static const u32 lc_[6] = { RGB(66, 133, 244), RGB(234, 67, 53), RGB(251, 188, 5), RGB(66, 133, 244), RGB(52, 168, 83), RGB(234, 67, 53) };
    const char *s = "Huygle"; int x = cx - 3 * 8 * sc;
    for (int k = 0; k < 6; k++, x += 8 * sc) draw_char(x, y, s[k], lc_[k], sc);
}
static void sq_box(int x, int y, int w, int h) {
    fill_rrect(x - 1, y - 1, w + 2, h + 2, h / 2 + 1, C_SUB, 90); fill_rrect(x, y, w, h, h / 2, C_CARD, 255);
    sq_glass(x + h / 2, y + h / 2);
    if (sq_n) text(x + h + 4, y + h / 2 - 8, sq_in, C_TEXT); else text(x + h + 4, y + h / 2 - 8, "Search apps, files and Huy OS...", C_SUB);
    fill_rect(x + h + 4 + sq_n * 8, y + h / 2 - 8, 2, 16, ACCENT, 255);
}
static void sq_btn(int x, int y, int w, const char *s) { fill_rrect(x, y, w, 36, 6, C_CARD, 255); fill_rrect(x, y, w, 36, 6, C_SUB, 30); textc(x + w / 2, y + 10, s, C_TEXT); }
static void draw_search(int x, int y, int w, int h) {
    int cx = x + w / 2;
    if (!sq_res) {
        int oy = y + sq_oy(h);
        sq_logo(cx, oy, 6);
        sq_box(cx - 220, oy + 120, 440, 44);
        sq_btn(cx - 146, oy + 186, 124, "Huygle Search"); sq_btn(cx - 10, oy + 186, 156, "I'm Feeling Lucky");
        textc(cx, y + h - 36, "Huy OS has no network yet - results come from this computer.", C_SUB);
        return;
    }
    sq_logo(x + 68, y + 46, 2); sq_box(x + 150, y + 42, w - 170, 36);
    fill_rect(x, y + 90, w, 1, C_SUB, 70);
    char b[40], t[12]; b[0] = 0; scat(b, "About "); scat(b, utoa(srn, t)); scat(b, srn == 1 ? " result" : " results");
    text(x + 150, y + 100, b, C_SUB);
    int ty = y + sq_top();
    if (sq_ans[0]) {
        fill_rrect(x + 150, y + 120, w - 170, 66, 10, C_CARD, 255);
        text(x + 168, y + 130, sq_alab, C_SUB); texts(x + 168 + slen(sq_ans) * 12, y + 150, sq_ans, ACCENT, 3);
    }
    if (!srn && !sq_ans[0]) { text(x + 150, y + 130, "No results found. Try: terminal, 3d, readme, kernel, 12*7", C_SUB); return; }
    for (int k = 0; k < srn; k++) {
        int ry = ty + k * 62; if (ry + 58 > y + h) break;
        text(x + 150, ry, sr[k].url, RGB(52, 168, 83));
        textb(x + 150, ry + 18, sr[k].title, dark ? RGB(138, 180, 248) : RGB(26, 13, 171));
        text(x + 150, ry + 38, sr[k].snip, C_TEXT);
    }
}
static void sq_click(int lx, int ly) {
    int w = wins[8].w, h = wins[8].h, cx = w / 2;
    if (!sq_res) {
        int oy = sq_oy(h) - 34 + 34 - 34; oy = sq_oy(h);
        if (ly >= oy + 186 && ly < oy + 222) {
            if (lx >= cx - 146 && lx < cx - 22 && sq_n) sq_search();
            else if (lx >= cx - 10 && lx < cx + 146 && sq_n) { sq_search(); sq_open(0); }
        }
        return;
    }
    if (lx >= 14 && lx < 126 && ly >= 40 && ly < 76) { sq_res = 0; return; }
    if (lx >= 150 && lx < 190 && ly >= 42 && ly < 78 && sq_n) { sq_search(); return; }
    int ty = sq_top();
    for (int k = 0; k < srn; k++) { int ry = ty + k * 62; if (ry + 58 > h) break; if (lx >= 150 && lx < w - 20 && ly >= ry && ly < ry + 58) { sq_open(k); return; } }
}

/* ---------- малювання вікон ---------- */
static const char *fnames[4] = { "Home", "Documents", "Apps", "Desktop" };
static const char *fpath[4] = { "Home", "Home  >  Documents", "Home  >  Apps", "Desktop" };
static int vis_files(int *out) {
    int n = 0;
    for (int i = 0; i < nf; i++) {
        int ok;
        if (fcur >= 0) ok = files[i].parent == fcur;
        else if (ffilter == 3) ok = files[i].parent == -2;
        else if (ffilter == 0) ok = files[i].parent == -1;
        else ok = files[i].parent == -1 && !files[i].isdir && files[i].dir == ffilter;
        if (ok) out[n++] = i;
    }
    return n;
}
static void files_path(char *o) {
    int st[6], n = 0, p = fcur;
    o[0] = 0;
    if (fcur < 0) { scat(o, fpath[ffilter]); return; }
    while (p >= 0 && n < 6) { st[n++] = p; p = files[p].parent; }
    scat(o, p == -2 ? "Desktop" : "Home");
    for (int k = n - 1; k >= 0; k--) { scat(o, "  >  "); scat(o, files[st[k]].name); }
}

static void row(int x, int y, const char *k, const char *v, u32 vc) { text(x, y, k, C_SUB); text(x + 126, y, v, vc); }

static void draw_content(int i, int x, int y, int w, int h) {
    char b[48], pb[160];
    if (i == 0) {
        for (int r = 0; r < TR; r++) for (int c = 0; c < TC; c++) if (tb[r][c] != ' ') draw_char(x + 14 + c * 8, y + 44 + r * 16, tb[r][c], RGB(204, 204, 204), 1);
        if (top_win() == 0) fill_rect(x + 14 + tcol * 8, y + 44 + trow * 16 + 1, 8, 14, ACCENT == RGB(0, 103, 192) ? RGB(76, 194, 255) : ACCENT, 200);
    } else if (i == 1) {
        int vis[NF], n = vis_files(vis);
        fill_rrect(x + 12, y + 44, 140, h - 56, 8, C_CARD, 255);
        for (int k = 0; k < 4; k++) { int ey = y + 54 + k * 34; if (ffilter == k) fill_rrect(x + 18, ey, 128, 30, 6, ACCENT, 255); text(x + 34, ey + 7, fnames[k], ffilter == k ? WHITE : C_TEXT); }
        files_path(pb); text(x + 168, y + 54, pb, C_SUB);
        fill_rrect(x + w - 92, y + 46, 80, 28, 6, fsel >= 0 ? ACCENT : C_CARD, 255); textc(x + w - 52, y + 52, "Open", fsel >= 0 ? WHITE : C_SUB);
        { int rn = fsel >= 0 && (files[fsel].dir == 2 || is_script_name(files[fsel].name));
          fill_rrect(x + w - 182, y + 46, 80, 28, 6, rn ? RGB(40, 150, 80) : C_CARD, 255); textc(x + w - 142, y + 52, "Run", rn ? WHITE : C_SUB); }
        if (fcur >= 0) { fill_rrect(x + w - 272, y + 46, 80, 28, 6, C_CARD, 255); textc(x + w - 232, y + 52, "Up", C_TEXT); }
        if (n == 0) text(x + 176, y + 100, "This folder is empty", C_SUB);
        int cols = (w - 178) / 112; if (cols < 1) cols = 1;
        for (int k = 0; k < n; k++) {
            int fi = vis[k], cx = x + 166 + (k % cols) * 112, cy = y + 92 + (k / cols) * 100; Info in; analyze(&files[fi], &in);
            if (fsel == fi) fill_rrect(cx + 4, cy, 104, 92, 8, ACCENT, 80);
            draw_file_icon(cx + 32, cy + 6, 48, in.ft);
            int l = slen(files[fi].name); char t[16]; if (l > 12) { memcpy(t, files[fi].name, 10); t[10] = '.'; t[11] = '.'; t[12] = 0; } else { memcpy(t, files[fi].name, l); t[l] = 0; }
            textc(cx + 56, cy + 64, t, C_TEXT);
        }
    } else if (i == 2) {
        textb(x + 24, y + 50, "System", C_TEXT);
        text(x + 24, y + 76, "Huy OS 0.4  (custom kernel, x86-64 long mode)", C_SUB);
        b[0] = 0; scat(b, "Display: "); { char t[12]; scat(b, utoa(W, t)); scat(b, "x"); scat(b, utoa(H, t)); scat(b, ", "); scat(b, utoa(BPP, t)); scat(b, "-bit"); }
        text(x + 24, y + 96, b, C_SUB);
        text(x + 24, y + 116, "Files: embedded in the kernel from files/ at build time", C_SUB);
        textb(x + 24, y + 156, "Appearance", C_TEXT);
        text(x + 24, y + 190, "Dark mode", C_TEXT);
        fill_rrect(x + w - 90, y + 186, 44, 22, 11, dark ? ACCENT : RGB(150, 150, 155), 255);
        fill_circle(x + w - 90 + (dark ? 33 : 11), y + 197, 8, WHITE, 255);
        text(x + 24, y + 232, "Accent color", C_TEXT);
        for (int k = 0; k < 5; k++) { int cx = x + 40 + k * 44, cy = y + 280; if (k == acc_i) fill_circle(cx, cy, 18, C_TEXT, 255); fill_circle(cx, cy, 14, accents[k], 255); }
    } else if (i == 4) {
        if (edit_file >= 0) {
            File *f = &files[edit_file];
            textb(x + 24, y + 52, f->name, C_TEXT);
            if (emsg == 1) text(x + 24 + slen(f->name) * 8 + 16, y + 52, "Saved", RGB(90, 220, 120));
            else if (emsg == 2) text(x + 24 + slen(f->name) * 8 + 16, y + 52, "Not enough space to save", RGB(255, 110, 100));
            fill_rrect(x + w - 92, y + 46, 80, 28, 6, ACCENT, 255); textc(x + w - 52, y + 52, "Save", WHITE);
            fill_rrect(x + 12, y + 84, w - 24, h - 96, 8, C_CARD, 255);
            int rows = (h - 112) / 16, maxc = (w - 44) / 8, ln = 0, cc = 0, cl = 0, ccol = 0;
            for (int k = 0; k <= elen; k++) {
                if (k == ecur) { cl = ln; ccol = cc; }
                if (k == elen) break;
                char ch = ebuf[k];
                if (ch == '\n') { ln++; cc = 0; continue; }
                if (ln >= escroll && ln < escroll + rows && cc < maxc && ch >= ' ') draw_char(x + 22 + cc * 8, y + 92 + (ln - escroll) * 16, ch, C_TEXT, 1);
                cc++;
            }
            if (cl >= escroll && cl < escroll + rows && ccol <= maxc) fill_rect(x + 22 + ccol * 8, y + 93 + (cl - escroll) * 16, 2, 14, ACCENT, 255);
        }
    } else if (i == 8) {
        draw_search(x, y, w, h);
    } else if (i >= 5) {
        draw_app(i, x, y, w, h);
    } else {
        File *f = &files[insp_file]; Info in; analyze(f, &in);
        draw_file_icon(x + 24, y + 50, 56, in.ft);
        textb(x + 96, y + 54, f->name, C_TEXT); text(x + 96, y + 76, in.fmt, C_SUB);
        if (in.ft == FT_TXT || in.ft == FT_BAT || in.ft == FT_SH) {
            int ly = y + 112, lx = x + 24; for (u32 k = 0; k < f->size && ly < y + h - 20; k++) { char c = f->data[k]; if (c == '\n') { ly += 16; lx = x + 24; } else if (c >= ' ') { draw_char(lx, ly, c, C_TEXT, 1); lx += 8; } }
        } else {
            int ly = y + 116;
            row(x + 24, ly, "Format", in.fmt, C_TEXT); ly += 24;
            row(x + 24, ly, "Architecture", in.arch, C_TEXT); ly += 24;
            row(x + 24, ly, "Bits", utoa(in.bits, b), C_TEXT); ly += 24;
            row(x + 24, ly, "Type", in.kind, C_TEXT); ly += 24;
            if (in.has_entry) { hex32(in.entry, b); row(x + 24, ly, "Entry point", b, C_TEXT); ly += 24; }
            row(x + 24, ly, "Size", utoa(f->size, b), C_TEXT); scat(b, " bytes"); row(x + 24, ly, "Size", b, C_TEXT); ly += 30;
            fill_rrect(x + 24, ly, w - 48, 54, 8, C_CARD, 255);
            if (in.ft == FT_MACHO) { text(x + 38, ly + 8, "Recognized, but cannot run yet:", RGB(255, 175, 40)); text(x + 38, ly + 28, "needs a Mach-O loader + macOS API layer.", C_SUB); }
            else { text(x + 38, ly + 8, "Runnable on Huy OS (small API subset):", RGB(90, 220, 120)); text(x + 38, ly + 28, "use Run in Files, or type the name in Terminal.", C_SUB); }
        }
    }
}

static void draw_window(int i) {
    Win *w = &wins[i]; int x = w->x, y = w->y, ww = w->w, hh = w->h, foc = top_win() == i;
    for (int s = 0; s < 6; s++) fill_rrect(x - s, y - s + 6, ww + 2 * s, hh + 2 * s, 12 + s, 0, 14);
    u32 body = i == 0 ? RGB(12, 12, 12) : C_BODY;
    fill_rrect_rows(x, y, ww, hh, 10, body, 255, 34, hh);
    fill_rrect_rows(x, y, ww, hh, 10, i == 0 ? RGB(32, 32, 32) : C_TITLE, 255, 0, 34);
    fill_circle(x + 20, y + 17, 6, foc ? RGB(255, 95, 87) : RGB(110, 110, 112), 255);
    fill_circle(x + 40, y + 17, 6, foc ? RGB(254, 188, 46) : RGB(110, 110, 112), 255);
    fill_circle(x + 60, y + 17, 6, foc ? RGB(40, 200, 64) : RGB(110, 110, 112), 255);
    u32 tc = i == 0 ? RGB(230, 230, 230) : C_TEXT;
    text(x + (ww - slen(w->title) * 8) / 2, y + 9, w->title, foc ? tc : RGB(140, 140, 145));
    draw_content(i, x, y, ww, hh);
}

static void draw_menubar(void) {
    fill_rect(0, 0, W, 28, RGB(18, 18, 24), 175);
    draw_huy_icon(21, 14, 10, RGB(0, 120, 215), WHITE);
    int t = top_win(), x = 42;
    const char *name = t >= 0 ? wins[t].title : "Desktop";
    textb(x, 6, name, WHITE); x += slen(name) * 8 + 24;
    const char *m[5] = { "File", "Edit", "View", "Window", "Help" };
    for (int k = 0; k < 5; k++) { text(x, 6, m[k], RGB(225, 225, 230)); x += slen(m[k]) * 8 + 20; }
    int rx = W - 14 - 40; text(rx, 6, clk, WHITE); rx -= 56; text(rx, 6, dat, RGB(200, 200, 205)); rx -= 40;
    fill_rrect(rx, 9, 22, 11, 3, WHITE, 255); fill_rrect(rx + 2, 11, 18, 7, 2, RGB(40, 40, 46), 255); fill_rrect(rx + 2, 11, 14, 7, 2, RGB(90, 220, 120), 255); fill_rect(rx + 23, 12, 2, 5, WHITE, 255);
    rx -= 30; fill_rect(rx, 17, 3, 3, WHITE, 255); fill_rect(rx + 5, 13, 3, 7, WHITE, 255); fill_rect(rx + 10, 9, 3, 11, WHITE, 255);
}

static void dock_layout(void) {
    int base = 48, inzone = my > H - TB - 130, x0 = (W - (4 * base + 3 * 12)) / 2, total = 0;
    for (int i = 0; i < 4; i++) {
        int c = x0 + i * (base + 12) + base / 2, d = mx - c; if (d < 0) d = -d;
        int s = base; if (inzone && d < 120) s += 24 * (120 - d) / 120;
        dock_s[i] = s; total += s;
    }
    total += 36; int x = (W - total) / 2;
    for (int i = 0; i < 4; i++) { dock_x[i] = x; x += dock_s[i] + 12; }
    dock_bx = (W - total) / 2 - 14; dock_bw = total + 28;
}
static void draw_dock(void) {
    int dy = H - TB - 78;
    fill_rrect(dock_bx - 1, dy - 1, dock_bw + 2, 66, 19, WHITE, 40);
    fill_rrect(dock_bx, dy, dock_bw, 64, 18, dark ? RGB(30, 30, 36) : RGB(250, 250, 252), 200);
    for (int i = 0; i < 4; i++) {
        draw_app_icon(i, dock_x[i], H - TB - 22 - dock_s[i], dock_s[i]);
        if (i > 0 && wins[i - 1].open) fill_circle(dock_x[i] + dock_s[i] / 2, H - TB - 15, 2, WHITE, 255);
        if (i == 0 && start_open) fill_circle(dock_x[i] + dock_s[i] / 2, H - TB - 15, 2, WHITE, 255);
    }
}

#define SW 560
#define SH 500
static void draw_start(void) {
    int sx = (W - SW) / 2, sy = H - TB - 78 - 12 - SH;
    fill_rrect(sx - 1, sy - 1, SW + 2, SH + 2, 13, WHITE, 40);
    fill_rrect(sx, sy, SW, SH, 12, dark ? RGB(36, 38, 46) : RGB(245, 245, 248), 238);
    fill_rrect(sx + 24, sy + 22, SW - 48, 36, 18, C_CARD, 255);
    fill_rrect(sx + 30, sy + 28, 32, 24, 12, ACCENT, 255); textc(sx + 46, sy + 32, "AI", WHITE);
    if (ai_n) text(sx + 76, sy + 31, ai_in, C_TEXT); else text(sx + 76, sy + 31, "Ask Huy AI anything...", C_SUB);
    fill_rect(sx + 76 + ai_n * 8, sy + 31, 2, 16, C_TEXT, 255);
    textb(sx + 28, sy + 78, "Pinned", C_TEXT);
    const char *pn[7] = { "Terminal", "Files", "Settings", "3D Viewer", "3D Space", "3D Maze", "Huygle" };
    for (int k = 0; k < 7; k++) { int px = sx + 28 + k * 74; draw_app_icon(k + 1, px + 12, sy + 108, 48); textc(px + 36, sy + 164, pn[k], C_TEXT); }
    if (ai_lines) {
        textb(sx + 28, sy + 216, "Huy AI", C_TEXT);
        fill_rrect(sx + 24, sy + 244, SW - 48, 24 + ai_lines * 20, 10, C_CARD, 255);
        for (int k = 0; k < ai_lines; k++) text(sx + 38, sy + 256 + k * 20, ai_ans[k], C_TEXT);
    } else {
        textb(sx + 28, sy + 216, "Recommended", C_TEXT);
        for (int k = 0; k < 3; k++) {
            int ry = sy + 246 + k * 52, ri = rec_idx(k); if (ri < 0) continue; File *f = &files[ri]; Info in; analyze(f, &in);
            draw_file_icon(sx + 32, ry + 4, 36, in.ft); text(sx + 84, ry + 4, f->name, C_TEXT); text(sx + 84, ry + 24, in.fmt, C_SUB);
        }
    }
    text(sx + 28, sy + SH - 88, "Try: open terminal | time | dark mode | 12*7 | help", C_SUB);
    fill_rrect_rows(sx, sy, SW, SH, 12, dark ? RGB(28, 30, 36) : RGB(232, 232, 236), 255, SH - 64, SH);
    fill_circle(sx + 48, sy + SH - 32, 14, ACCENT, 255); textc(sx + 48, sy + SH - 40, "H", WHITE);
    text(sx + 72, sy + SH - 40, "Huy User", C_TEXT);
    int px = sx + SW - 48, py = sy + SH - 32;
    fill_circle(px, py, 16, C_CARD, 255); fill_circle(px, py, 8, C_TEXT, 255); fill_circle(px, py, 6, C_CARD, 255);
    fill_rect(px - 4, py - 10, 9, 8, C_CARD, 255); fill_rect(px - 1, py - 10, 3, 10, C_TEXT, 255);
}

static const char *cur[19] = {
    "X           ", "XX          ", "X.X         ", "X..X        ", "X...X       ", "X....X      ", "X.....X     ", "X......X    ", "X.......X   ",
    "X........X  ", "X.....XXXXX ", "X..X..X     ", "X.X X..X    ", "XX  X..X    ", "X    X..X   ", "     X..X   ", "      X..X  ", "      X..X  ", "       XX   " };
static int cvx, cvy, cvis;
static void cursor_erase(void) {
    if (!cvis) return; cvis = 0;
    for (int r = 0; r < 19; r++) for (int c = 0; c < 12; c++) {
        int px = cvx + c, py = cvy + r;
        if (cur[r][c] != ' ' && px >= 0 && px < W && py >= 0 && py < H) fb_put(px, py, back[py * W + px]);
    }
}
static void cursor_draw(void) {
    for (int r = 0; r < 19; r++) for (int c = 0; c < 12; c++) {
        char ch = cur[r][c]; int px = mx + c, py = my + r;
        if (ch != ' ' && px >= 0 && px < W && py >= 0 && py < H) fb_put(px, py, ch == 'X' ? RGB(0, 0, 0) : WHITE);
    }
    cvx = mx; cvy = my; cvis = 1;
}

/* ---------- подвійний клік: відкрити як у Windows ---------- */
static volatile u32 ticks;                        /* визначається нижче (tentative) */
static int lc_i = -1, lc_k; static u32 lc_t;
static int is_dbl(int kind, int item) {
    int d = lc_k == kind && lc_i == item && (u32)(ticks - lc_t) <= 50;   /* 0.5 с */
    if (d) lc_i = -1; else { lc_k = kind; lc_i = item; lc_t = ticks; }
    return d;
}
static void activate_item(int fi) {
    Info in; analyze(&files[fi], &in);
    if (files[fi].isdir) open_item(fi);
    else if (in.ft == FT_TXT) open_editor(fi);                                   /* .txt -> редактор */
    else if (in.ft == FT_BAT || in.ft == FT_SH || in.ft == FT_ELF || in.ft == FT_PE) run_in_terminal(fi);   /* скрипти й програми запускаються */
    else open_inspector(fi);
}

/* ---------- робочий стіл: іконки папок/файлів ---------- */
static int dk_list(int *out) { int n = 0; for (int i = 0; i < nf; i++) if (files[i].parent == -2) out[n++] = i; return n; }
static void dk_pos(int k, int *x, int *y) { int r = (H - TB - 60) / 100; if (r < 1) r = 1; *x = 16 + (k / r) * 104; *y = 40 + (k % r) * 100; }
static int desk_hit(void) {
    int v[NF], n = dk_list(v);
    for (int k = 0; k < n; k++) { int x, y; dk_pos(k, &x, &y); if (mx >= x && mx < x + 100 && my >= y && my < y + 92) return v[k]; }
    return -1;
}
static void draw_desktop(void) {
    int v[NF], n = dk_list(v);
    for (int k = 0; k < n; k++) {
        int fi = v[k], x, y; Info in; analyze(&files[fi], &in);
        dk_pos(k, &x, &y);
        if (dsel == fi) fill_rrect(x, y, 100, 92, 8, ACCENT, 110);
        draw_file_icon(x + 26, y + 6, 48, in.ft);
        int l = slen(files[fi].name); char t[16];
        if (l > 12) { memcpy(t, files[fi].name, 10); t[10] = '.'; t[11] = '.'; t[12] = 0; } else memcpy(t, files[fi].name, l + 1);
        textc(x + 51, y + 65, t, RGB(0, 0, 0)); textc(x + 50, y + 64, t, WHITE);
    }
}

/* ---------- нижня панель: кнопки відкритих вікон ---------- */
#define TBW 150
static int tb_x(int i) { if (!wins[i].open) return -1; int k = 0; for (int j = 0; j < i; j++) if (wins[j].open) k++; return 10 + k * (TBW + 6); }
static void draw_taskbar(void) {
    int ty = H - TB, foc = top_win();
    fill_rect(0, ty, W, TB, dark ? RGB(18, 18, 24) : RGB(238, 238, 242), 225);
    fill_rect(0, ty, W, 1, WHITE, 40);
    for (int i = 0; i < NW; i++) {
        int bx = tb_x(i); if (bx < 0) continue;
        int act = foc == i;
        fill_rrect(bx, ty + 4, TBW, 26, 6, act ? ACCENT : C_CARD, act ? 255 : 230);
        if (i == 0) { fill_rrect(bx + 6, ty + 8, 18, 18, 4, RGB(28, 28, 32), 255); draw_char(bx + 11, ty + 9, '>', WHITE, 1); }
        else if (i == 1) { fill_rrect(bx + 6, ty + 9, 9, 5, 2, RGB(225, 150, 15), 255); fill_rrect(bx + 6, ty + 11, 18, 14, 3, RGB(255, 196, 50), 255); }
        else if (i == 2) { fill_circle(bx + 15, ty + 17, 9, RGB(105, 110, 122), 255); fill_circle(bx + 15, ty + 17, 4, WHITE, 255); }
        else if (i >= 5) draw_app_icon(i - 1, bx + 6, ty + 8, 18);
        else { fill_rrect(bx + 8, ty + 8, 14, 18, 3, RGB(225, 225, 230), 255); fill_rect(bx + 11, ty + 13, 8, 2, RGB(120, 120, 125), 255); fill_rect(bx + 11, ty + 18, 8, 2, RGB(120, 120, 125), 255); }
        text(bx + 30, ty + 9, wins[i].title, act ? WHITE : (wins[i].min ? C_SUB : C_TEXT));
        if (!wins[i].min) fill_rrect(bx + TBW / 2 - 8, ty + 28, 16, 2, 1, act ? WHITE : ACCENT, 255);
    }
}
static int taskbar_click(void) {
    if (my < H - TB) return 0;
    for (int i = 0; i < NW; i++) {
        int bx = tb_x(i); if (bx < 0) continue;
        if (mx >= bx && mx < bx + TBW && my >= H - TB + 4 && my < H - TB + 30) {
            if (wins[i].min) { wins[i].min = 0; bring_front(i); }
            else if (top_win() == i) wins[i].min = 1;
            else bring_front(i);
            return 1;
        }
    }
    return 1;
}

/* ---------- контекстне меню (права кнопка миші) ---------- */
#define CTXW 176
enum { CA_FOLDER, CA_TXT, CA_OPEN, CA_EDIT, CA_REN };
static const char *ca_lab[5] = { "New folder", "New text file", "Open", "Edit", "Rename" };
static int ctx_hit(void) {
    if (mx < ctx_x + 4 || mx >= ctx_x + CTXW - 4 || my < ctx_y + 4 || my >= ctx_y + 4 + ctx_cnt * 32) return -1;
    return (my - ctx_y - 4) / 32;
}
static void draw_ctx(void) {
    if (!ctx_open) return;
    int ch = 8 + ctx_cnt * 32;
    fill_rrect(ctx_x - 1, ctx_y - 1, CTXW + 2, ch + 2, 9, WHITE, 40);
    fill_rrect(ctx_x, ctx_y, CTXW, ch, 8, dark ? RGB(44, 44, 50) : RGB(250, 250, 252), 250);
    int hk = ctx_hit();
    for (int k = 0; k < ctx_cnt; k++) {
        int iy = ctx_y + 4 + k * 32;
        if (hk == k) fill_rrect(ctx_x + 4, iy, CTXW - 8, 32, 6, ACCENT, 255);
        text(ctx_x + 16, iy + 8, ca_lab[ctx_act[k]], hk == k ? WHITE : C_TEXT);
    }
}
static void ctx_do(int k) {
    int a = ctx_act[k];
    if (a == CA_OPEN) { activate_item(ctx_item); dirty = 1; return; }
    if (a == CA_EDIT) { open_editor(ctx_item); dirty = 1; return; }
    if (a == CA_REN) { ren_start(ctx_item); dirty = 1; return; }
    int idx = new_item(a == CA_FOLDER, ctx_par);
    if (idx < 0) return;
    if (ctx_src == 0) dsel = idx; else { fsel = idx; if (fcur < 0 && ffilter != 3) ffilter = 0; }
    dirty = 1;
}
static void ctx_place(void) {
    int ch = 8 + ctx_cnt * 32;
    ctx_x = mx; ctx_y = my;
    if (ctx_x + CTXW > W) ctx_x = W - CTXW;
    if (ctx_y + ch > H - TB) ctx_y = H - TB - ch;
    ctx_open = 1;
}
static int files_item_at(int lx, int ly) {
    int vis[NF], n = vis_files(vis), cols = (wins[1].w - 178) / 112; if (cols < 1) cols = 1;
    for (int k = 0; k < n; k++) {
        int cx = 166 + (k % cols) * 112, cy = 92 + (k / cols) * 100;
        if (lx >= cx && lx < cx + 112 && ly >= cy && ly < cy + 96) return vis[k];
    }
    return -1;
}

/* ---------- вікно перейменування ---------- */
#define RW 380
#define RH 150
static int ren_btn(void) {
    int rx = (W - RW) / 2, ry = (H - TB - RH) / 2;
    if (my >= ry + 104 && my < ry + 134) { if (mx >= rx + RW - 190 && mx < rx + RW - 110) return 1; if (mx >= rx + RW - 100 && mx < rx + RW - 20) return 2; }
    return 0;
}
static void draw_ren(void) {
    if (!ren_open) return;
    int rx = (W - RW) / 2, ry = (H - TB - RH) / 2;
    fill_rect(0, 0, W, H, 0, 90);
    fill_rrect(rx - 1, ry - 1, RW + 2, RH + 2, 11, WHITE, 40);
    fill_rrect(rx, ry, RW, RH, 10, dark ? RGB(44, 44, 50) : RGB(250, 250, 252), 255);
    textb(rx + 20, ry + 16, "Rename", C_TEXT);
    fill_rrect(rx + 19, ry + 47, RW - 38, 34, 6, ACCENT, 255);
    fill_rrect(rx + 20, ry + 48, RW - 40, 32, 6, C_CARD, 255);
    text(rx + 30, ry + 56, ren_buf, C_TEXT);
    fill_rect(rx + 30 + ren_n * 8, ry + 55, 2, 18, C_TEXT, 255);
    if (ren_msg == 1) text(rx + 20, ry + 86, "Name cannot be empty", RGB(255, 110, 100));
    else if (ren_msg == 2) text(rx + 20, ry + 86, "A file with this name already exists", RGB(255, 110, 100));
    fill_rrect(rx + RW - 190, ry + 104, 80, 30, 6, ACCENT, 255); textc(rx + RW - 150, ry + 112, "OK", WHITE);
    fill_rrect(rx + RW - 100, ry + 104, 80, 30, 6, C_CARD, 255); textc(rx + RW - 60, ry + 112, "Cancel", C_TEXT);
}

static void render(void) {
    copy32(back, wall, W * H);
    draw_desktop();
    for (int k = 0; k < NW; k++) { int i = zo[k]; if (wins[i].open && !wins[i].min) draw_window(i); }
    draw_menubar(); draw_taskbar(); dock_layout(); draw_dock();
    if (start_open) draw_start();
    draw_ctx();
    draw_ren();
    flush(); cvis = 0; cursor_draw();
}

/* ---------- кліки ---------- */
static void win_click(int i, int lx, int ly) {
    Win *w = &wins[i];
    if (ly < 34) {
        if (ly >= 8 && ly < 26) {
            if (lx >= 11 && lx < 29) { w->open = 0; return; }
            if (lx >= 31 && lx < 49) { w->min = 1; return; }
            if (lx >= 51 && lx < 69) {
                if (!w->max) { w->sx = w->x; w->sy = w->y; w->sw = w->w; w->sh = w->h; w->x = 8; w->y = 36; w->w = W - 16; w->h = H - 36 - 100 - TB; w->max = 1; }
                else { w->x = w->sx; w->y = w->sy; w->w = w->sw; w->h = w->sh; w->max = 0; }
                return;
            }
        }
        if (!w->max) { dragw = i; dragdx = lx; dragdy = ly; }
        return;
    }
    if (i == 1) {
        if (lx >= 12 && lx < 152) { int k = (ly - 54) / 34; if (ly >= 54 && k >= 0 && k < 4) { ffilter = k; fcur = -1; fsel = -1; } }
        if (lx >= w->w - 92 && lx < w->w - 12 && ly >= 46 && ly < 74 && fsel >= 0) open_item(fsel);
        if (fcur >= 0 && lx >= w->w - 272 && lx < w->w - 192 && ly >= 46 && ly < 74) go_up();
        if (lx >= w->w - 182 && lx < w->w - 102 && ly >= 46 && ly < 74 && fsel >= 0 && (files[fsel].dir == 2 || is_script_name(files[fsel].name))) run_in_terminal(fsel);
        int vis[NF], n = vis_files(vis), cols = (w->w - 178) / 112; if (cols < 1) cols = 1;
        int hit = 0;
        for (int k = 0; k < n; k++) {
            int cx = 166 + (k % cols) * 112, cy = 92 + (k / cols) * 100;
            if (lx >= cx && lx < cx + 112 && ly >= cy && ly < cy + 96) { hit = 1; fsel = vis[k]; if (is_dbl(2, vis[k])) activate_item(vis[k]); }
        }
        if (!hit && lx >= 160 && ly >= 84) fsel = -1;
    } else if (i == 2) {
        if (lx >= w->w - 90 && lx < w->w - 46 && ly >= 186 && ly < 208) dark = !dark;
        for (int k = 0; k < 5; k++) { int dx = lx - (40 + k * 44), dy = ly - 280; if (dx * dx + dy * dy <= 18 * 18) acc_i = k; }
    } else if (i == 4) {
        if (lx >= w->w - 92 && lx < w->w - 12 && ly >= 46 && ly < 74) ed_save();
        else if (ly >= 84) ed_click(lx, ly);
    } else if (i == 8) sq_click(lx, ly);
    else if (i >= 5) app_click(i, lx, ly);
}

static int start_click(void) {
    int sx = (W - SW) / 2, sy = H - TB - 78 - 12 - SH;
    if (mx < sx || mx >= sx + SW || my < sy || my >= sy + SH) return 0;
    for (int k = 0; k < 7; k++) { int px = sx + 28 + k * 74; if (mx >= px && mx < px + 74 && my >= sy + 108 && my < sy + 192) { start_open = 0; open_win(k < 3 ? k : k + 2); return 1; } }
    for (int k = 0; k < 3; k++) { int ry = sy + 246 + k * 52; if (my >= ry && my < ry + 48 && mx >= sx + 20 && mx < sx + SW - 20) { if (rec_idx(k) >= 0) { start_open = 0; open_inspector(rec_idx(k)); } return 1; } }
    int dx = mx - (sx + SW - 48), dy = my - (sy + SH - 32); if (dx * dx + dy * dy < 16 * 16) power_off();
    return 1;
}
static int dock_click(void) {
    for (int i = 0; i < 4; i++) if (mx >= dock_x[i] && mx < dock_x[i] + dock_s[i] && my >= H - TB - 22 - dock_s[i] && my < H - TB - 22) {
        if (i == 0) { start_open = !start_was; if (start_open) { ai_n = 0; ai_in[0] = 0; ai_lines = 0; } }
        else { start_open = 0; int win = i - 1; if (wins[win].open && !wins[win].min && top_win() == win) wins[win].min = 1; else open_win(win); }
        return 1;
    }
    if (mx >= dock_bx && mx < dock_bx + dock_bw && my >= H - TB - 78 && my < H - TB - 14) return 1;
    return 0;
}
static void on_press(void) {
    start_was = start_open;
    if (ren_open) { int b = ren_btn(); if (b == 1) ren_apply(); else if (b == 2) ren_open = 0; return; }
    if (ctx_open) { int k = ctx_hit(); ctx_open = 0; if (k >= 0) ctx_do(k); return; }
    if (start_open) { if (start_click()) return; start_open = 0; }
    dock_layout();
    if (taskbar_click()) return;
    if (dock_click()) return;
    for (int k = NW - 1; k >= 0; k--) {
        int i = zo[k]; Win *w = &wins[i];
        if (!w->open || w->min) continue;
        if (mx >= w->x && mx < w->x + w->w && my >= w->y && my < w->y + w->h) { bring_front(i); win_click(i, mx - w->x, my - w->y); return; }
    }
    if (my >= 28 && my < H - TB) {                /* клік по робочому столу */
        int h = desk_hit();
        if (h >= 0) { dsel = h; if (is_dbl(1, h)) activate_item(h); } else { dsel = -1; lc_i = -1; }
    }
}
static void on_rpress(void) {
    if (ren_open) return;
    ctx_open = 0;
    if (start_open) { start_open = 0; return; }
    dock_layout();
    if (my < 28 || my >= H - TB) return;
    if (mx >= dock_bx && mx < dock_bx + dock_bw && my >= H - TB - 78 && my < H - TB - 14) return;
    int src = -1, item = -1;
    for (int k = NW - 1; k >= 0; k--) {
        int i = zo[k]; Win *w = &wins[i];
        if (!w->open || w->min) continue;
        if (mx >= w->x && mx < w->x + w->w && my >= w->y && my < w->y + w->h) {
            bring_front(i);
            if (i == 1 && my - w->y >= 34 && mx - w->x >= 160) { src = 1; item = files_item_at(mx - w->x, my - w->y); } else return;
            break;
        }
    }
    if (src < 0) { src = 0; item = desk_hit(); dsel = item; }
    if (src == 1 && item >= 0) fsel = item;
    ctx_src = src; ctx_item = item; ctx_par = src == 0 ? -2 : (fcur >= 0 ? fcur : (ffilter == 3 ? -2 : -1));
    if (item >= 0) {
        ctx_cnt = 0; ctx_act[ctx_cnt++] = CA_OPEN;
        if (!files[item].isdir && is_textft(item)) ctx_act[ctx_cnt++] = CA_EDIT;
        ctx_act[ctx_cnt++] = CA_REN;
    } else { ctx_cnt = 2; ctx_act[0] = CA_FOLDER; ctx_act[1] = CA_TXT; }
    ctx_place();
}

/* ---------- переривання, миша та клавіатура ---------- */
static volatile u8 mq[256], kq[64]; static volatile u32 mqh, mqt, kqh, kqt, ticks;
static void pump(void) {
    u8 st; int g = 0;
    while (((st = inb(0x64)) & 1) && g++ < 32) {
        u8 d = inb(0x60);
        if (st & 0x20) { u32 n = (mqh + 1) & 255; if (n != mqt) { mq[mqh] = d; mqh = n; } }
        else { u32 n = (kqh + 1) & 63; if (n != kqt) { kq[kqh] = d; kqh = n; } }
    }
}
void irq_handler(u64 n) {
    if (n == 0) ticks++; else if (n == 1 || n == 12) pump();
    if (n >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}
extern u64 irq_stubs[16]; extern char exc_stub[];
typedef struct { u16 lo, sel; u8 ist, attr; u16 mid; u32 hi, zero; } __attribute__((packed)) Gate;
static Gate idt[256];
static void set_gate(int v, u64 h) { idt[v] = (Gate){ (u16)h, 0x08, 0, 0x8E, (u16)(h >> 16), (u32)(h >> 32), 0 }; }
static void wrmsr(u32 m, u64 v) { __asm__ volatile("wrmsr" :: "c"(m), "a"((u32)v), "d"((u32)(v >> 32))); }
static u64 rdmsr(u32 m) { u32 lo, hi; __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(m)); return lo | ((u64)hi << 32); }
extern void syscall_entry(void);
static void idt_init(void) {
    struct { u16 lim; u64 base; } __attribute__((packed)) p = { sizeof idt - 1, (u64)idt };
    for (int i = 0; i < 32; i++) set_gate(i, (u64)exc_stub);
    for (int i = 0; i < 16; i++) set_gate(32 + i, irq_stubs[i]);
    __asm__ volatile("lidt %0" :: "m"(p));
}
static void pic_pit_init(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11); outb(0x21, 0x20); outb(0xA1, 0x28); outb(0x21, 4); outb(0xA1, 2); outb(0x21, 1); outb(0xA1, 1);
    outb(0x21, 0xF8); outb(0xA1, 0xEF);          /* дозволені: IRQ0 таймер, IRQ1 клавіатура, IRQ2 каскад, IRQ12 миша */
    outb(0x43, 0x36); outb(0x40, 11932 & 255); outb(0x40, 11932 >> 8);   /* таймер ~100 Гц */
}

static int kbc_wr(void) { for (int i = 0; i < 100000; i++) if (!(inb(0x64) & 2)) return 1; return 0; }
static int kbc_rd(void) { for (int i = 0; i < 100000; i++) if (inb(0x64) & 1) return inb(0x60); return -1; }
static int mouse_cmd(u8 v) { kbc_wr(); outb(0x64, 0xD4); kbc_wr(); outb(0x60, v); return kbc_rd(); }
static void mouse_init(void) {
    while (inb(0x64) & 1) inb(0x60);
    kbc_wr(); outb(0x64, 0xA8);
    kbc_wr(); outb(0x64, 0x20); int s = kbc_rd(); if (s < 0) s = 0x47;
    s = (s | 3) & ~0x30;                          /* IRQ1+IRQ12 увімкнено, годинники клавіатури і миші увімкнено */
    kbc_wr(); outb(0x64, 0x60); kbc_wr(); outb(0x60, s);
    if (mouse_cmd(0xFF) == 0xFA) { kbc_rd(); kbc_rd(); }   /* reset: ACK, 0xAA, 0x00 */
    mouse_cmd(0xF6);                              /* defaults */
    mouse_cmd(0xF3); mouse_cmd(100);              /* 100 пакетів/с */
    mouse_cmd(0xF4);                              /* потік даних увімкнено */
    while (inb(0x64) & 1) inb(0x60);
}

static int mstate, dockkey = -1; static u8 mp[3];
static void apply_motion(void) {
    if (dragw >= 0) { Win *w = &wins[dragw]; w->x = mx - dragdx; w->y = my - dragdy; if (w->y < 28) w->y = 28; if (w->y > H - TB - 60) w->y = H - TB - 60; if (w->x < 80 - w->w) w->x = 80 - w->w; if (w->x > W - 80) w->x = W - 80; dirty = 1; }
    if (ctx_open) dirty = 1;
    if (v_drag) { v_ay += (mx - v_lx) * 3; v_ax += (my - v_ly) * 3; v_lx = mx; v_ly = my; dirty = 1; }
    int dk = my > H - TB - 130 ? mx / 3 : -1; if (dk != dockkey) { dockkey = dk; dirty = 1; }   /* док збільшується під курсором */
}
static int mouse_byte(u8 d) {
    if (mstate == 0 && !(d & 8)) return 0;        /* синхронізація пакета */
    mp[mstate++] = d; if (mstate < 3) return 0; mstate = 0;
    if (mp[0] & 0xC0) return 0;                   /* переповнення - відкидаємо */
    int dx = mp[1] - ((mp[0] << 4) & 0x100), dy = mp[2] - ((mp[0] << 3) & 0x100);
    mx += dx; my -= dy;
    if (mx < 0) mx = 0; if (my < 0) my = 0; if (mx >= W) mx = W - 1; if (my >= H) my = H - 1;
    mb = mp[0] & 7;
    if ((mb & 1) && !(pmb & 1)) { on_press(); dirty = 1; }
    if ((mb & 2) && !(pmb & 2)) { on_rpress(); dirty = 1; }
    if (!(mb & 1)) { if (dragw >= 0) dirty = 1; dragw = -1; v_drag = 0; }
    apply_motion();
    pmb = mb; return 1;
}
static const char kmap[] = "\0\0331234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
static const char ksmap[] = "\0\033!@#$%^&*()_+\b\tQWERTYUIOP{}\n\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";
static int kb_l;
static void kb_arrow(u8 s) {
    int dx = 0, dy = 0;
    if (s == 0x48) dy = -10; else if (s == 0x50) dy = 10; else if (s == 0x4B) dx = -10; else if (s == 0x4D) dx = 10; else return;
    mx += dx; my += dy;
    if (mx < 0) mx = 0; if (my < 0) my = 0; if (mx >= W) mx = W - 1; if (my >= H) my = H - 1;
    apply_motion(); dirty = 1;
}
static void key_byte(u8 sc) {
    static int shift, ext;
    if (sc == 0xE0) { ext = 1; return; }
    if (ext) { ext = 0; if (!(sc & 0x80)) kb_arrow(sc); return; }
    if (sc == 0x3C) { if (!kb_l) { kb_l = 1; mb |= 1; on_press(); pmb = mb; dirty = 1; } return; }          /* F2 = ліва кнопка (тримай + стрілки = тягнути) */
    if (sc == 0xBC) { kb_l = 0; mb &= ~1; pmb = mb; dragw = -1; v_drag = 0; dirty = 1; return; }
    if (sc == 0x3B) { on_rpress(); dirty = 1; return; }                                                       /* F1 = права кнопка */
    if (sc == 0x2A || sc == 0x36) { shift = 1; return; } if (sc == 0xAA || sc == 0xB6) { shift = 0; return; }
    if (sc & 0x80) return;
    if (sc == 1) { if (start_open) { start_open = 0; dirty = 1; } if (ctx_open) { ctx_open = 0; dirty = 1; } if (ren_open) { ren_open = 0; dirty = 1; } return; }
    if (sc >= sizeof(kmap)) return;
    char c = shift ? ksmap[sc] : kmap[sc]; if (!c) return;
    if (ren_open) ren_key(c); else if (start_open) ai_key(c); else if (top_win() == 0) term_key(c); else if (top_win() == 4) ed_key(c); else if (top_win() == 7) maze_key(c); else if (top_win() == 8) sq_key(c);
}

/* ---------- екран завантаження ---------- */
static void splash_bg(void) {
    int cx = W / 2, cy = H / 2 - 70, R = 84;
    for (int y = 0; y < H; y++) fill_rect(0, y, W, 1, RGB(5 + y * 9 / H, 9 + y * 20 / H, 24 + y * 40 / H), 255);
    fill_circle(cx, cy, R + 48, RGB(0, 120, 215), 16); fill_circle(cx, cy, R + 30, RGB(0, 120, 215), 26); fill_circle(cx, cy, R + 14, RGB(0, 120, 215), 44);
    fill_circle(cx, cy, R + 3, WHITE, 255);
    draw_huy_icon(cx, cy, R, RGB(0, 120, 215), WHITE);
    texts(cx, cy + R + 28, "Huy OS", WHITE, 3);
    texts(cx, cy + R + 84, "x86-64 edition", RGB(130, 136, 155), 1);
    copy32(wall, back, (u64)W * H);
}
static void splash_frame(int pct, int first) {
    int cx = W / 2, cy = H / 2 - 70, by = cy + 84 + 140, bw = 260, bx = cx - bw / 2, y0 = by - 4, y1 = by + 44;
    if (first) { copy32(back, wall, (u64)W * H); y0 = 0; y1 = H; }
    else for (int y = y0; y < y1; y++) memcpy(back + y * W, wall + y * W, (u64)W * 4);
    const char *m = pct < 25 ? "Starting Huy OS (64-bit)..." : pct < 55 ? "Loading kernel modules..." : pct < 80 ? "Initializing display and input..." : "Preparing your desktop...";
    fill_rrect(bx, by, bw, 6, 3, RGB(60, 66, 84), 255);
    int fw = bw * pct / 100; if (fw >= 6) fill_rrect(bx, by, fw, 6, 3, RGB(76, 194, 255), 255);
    textc(cx, by + 22, m, RGB(150, 156, 175));
    flush_rows(y0, y1);
}
static void splash(void) {
    splash_bg();
    u32 t0 = ticks, total = 300; int first = 1;
    for (;;) {
        u32 e = ticks - t0; if (e > total) e = total;
        splash_frame((int)(e * 100 / total), first); first = 0;
        if (e >= total) break;
        __asm__ volatile("hlt");
    }
}

void kmain(void) {
    u8 *mi = (u8 *)0x5000;
    PITCH = *(u16 *)(mi + 0x10); W = *(u16 *)(mi + 0x12); H = *(u16 *)(mi + 0x14); BPP = mi[0x19]; FB = (u8 *)(u64)*(u32 *)(mi + 0x28);
    idt_init(); pic_pit_init(); mouse_init();
    wrmsr(0xC0000080, rdmsr(0xC0000080) | 1); wrmsr(0xC0000081, 0x0008ULL << 32); wrmsr(0xC0000082, (u64)syscall_entry); wrmsr(0xC0000084, 0);   /* SYSCALL для Linux-програм */
    __asm__ volatile("sti");
    splash();
    make_wall(); init_files(); trig_init(); mesh_build(0); stars_init(); maze_new();
    memset(tb, ' ', sizeof tb); t_puts("Huy OS 0.4 terminal\nType 'help' for commands.\n\n"); t_prompt();
    open_win(0);
    mx = W / 2; my = H / 2; read_clock();
    __asm__ volatile("cli"); mqt = mqh; kqt = kqh; mstate = 0; __asm__ volatile("sti");
    unsigned last_clk = ticks, last_anim = ticks;
    for (;;) {
        int moved = 0;
        __asm__ volatile("cli"); pump(); __asm__ volatile("sti");   /* запасне опитування, якщо IRQ12 не приходить */
        while (mqt != mqh) { u8 d = mq[mqt]; mqt = (mqt + 1) & 255; if (mouse_byte(d)) moved = 1; }
        while (kqt != kqh) { u8 d = kq[kqt]; kqt = (kqt + 1) & 63; key_byte(d); }
        if (ticks - last_clk >= 100) { last_clk = ticks; read_clock(); }
        if (ticks - last_anim >= 4) {                 /* ~25 кадрів/с для 3D-програм */
            last_anim = ticks;
            if (wins[5].open && !wins[5].min && v_spin && !v_drag) { v_ay += 8; dirty = 1; }
            if (wins[6].open && !wins[6].min) { stars_step(); dirty = 1; }
        }
        if (dirty) { dirty = 0; render(); }
        else if (moved) { cursor_erase(); cursor_draw(); }
        else __asm__ volatile("hlt");
    }
}
