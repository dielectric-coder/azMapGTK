/* main.c — GTK4 application entry point for azMapGTK.
 *
 * Replaces the GLFW main loop with a GtkApplication. The map is rendered
 * in a GtkGLArea widget; the sidebar is native GTK4 widgets. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>
#include <time.h>
#include <epoxy/gl.h>
#include <gtk/gtk.h>

#include "projection.h"
#include "map_data.h"
#include "renderer.h"
#include "camera.h"
#include "input.h"
#include "text.h"
#include "grid.h"
#include "config.h"
#include "solar.h"
#include "nightmesh.h"
#include "qrz.h"
#include "overlay.h"
#include "fetch.h"

#define SIDEBAR_WIDTH  260
#define DEFAULT_WIDTH  (800 + SIDEBAR_WIDTH)
#define DEFAULT_HEIGHT 800
#define MARKER_ZOOM_FACTOR 0.005f
#define NIGHT_UPDATE_SEC 60
#define DEFAULT_SHP_REL "data/ne_110m_coastline/ne_110m_coastline.shp"
#define DEFAULT_BORDER_REL "data/ne_110m_admin_0_boundary_lines_land/ne_110m_admin_0_boundary_lines_land.shp"
#define DEFAULT_LAND_REL "data/ne_110m_land/ne_110m_land.shp"
#define DEFAULT_SHADER_REL "shaders"
#define GC_LINE_POINTS 101

/* ── Application state ─────────────────────────────────────────── */

typedef struct {
    GtkApplication *app;
    GtkWidget      *window;
    GtkWidget      *gl_area;

    /* GTK sidebar widgets */
    GtkWidget *lbl_utc;
    GtkWidget *lbl_local;
    GtkWidget *lbl_dist;
    GtkWidget *lbl_az_to;
    GtkWidget *lbl_az_from;
    GtkWidget *lbl_station_info[6];
    GtkWidget *btn_proj;
    GtkWidget *btn_home;
    GtkWidget *btn_aurora;
    GtkWidget *btn_spore;
    GtkWidget *btn_muf;
    GtkWidget *btn_drap;
    GtkWidget *btn_qrz;
    GtkWidget *qrz_entry;
    GtkWidget *qrz_popover;
    GtkWidget *lbl_geomag;
    GtkWidget *lbl_solar;
    GtkWidget *lbl_drap_peak;
    GtkWidget *legend_box;     /* container for E's/MUF/DRAP legends */

    /* Core state */
    Renderer   renderer;
    Camera     cam;
    InputState input;
    int        gl_initialized;

    /* Map data */
    MapData    map;
    MapData    borders;
    int        has_borders;
    MapData    land;
    int        has_land;
    MapData    grid;
    MapData    dist_circles;

    /* Night overlay */
    NightMesh  nightmesh;
    time_t     last_sun_update;

    /* Overlays */
    MufData      muf_data;
    MufData      spore_data;
    AuroraGrid   aurora_grid;
    AuroraMesh   aurora_mesh;
    DrapGrid     drap_grid;
    AuroraMesh   drap_mesh;
    FetchRequest muf_fetch, aurora_fetch, spore_fetch, drap_fetch;
    int          muf_active, aurora_active, spore_active, drap_active;
    int          muf_fetching, aurora_fetching, spore_fetching, drap_fetching;
    time_t       last_muf_fetch, last_aurora_fetch, last_spore_fetch, last_drap_fetch;
    GeomagIndices geomag;
    SolarIndices  solar;
    NoaaScales    scales;
    XrayFlare     xray;
    FetchRequest  kp_fetch, bz_fetch, solar_fetch, scales_fetch, xray_fetch;
    int           kp_fetching, bz_fetching, solar_fetching, scales_fetching, xray_fetching;
    time_t        last_geomag_fetch;

    /* Coordinates */
    double qth_lat, qth_lon;     /* original QTH from config/CLI — never changes */
    double center_lat, center_lon; /* projection center (changes with panning) */
    double target_lat, target_lon;
    double cx, cy, tx, ty;   /* projected coords (km) */
    double dist, az_to, az_from;
    double npx, npy;          /* north pole projected */

    /* Labels */
    char   center_label[128];
    char   target_label[128];
    char   center_name[64];
    char   target_name[64];

    /* Paths */
    char   shader_dir[PATH_MAX];

    /* QRZ */
    int    has_qrz;

    /* FIFO IPC */
    int    fifo_fd;
    char   fifo_buf[512];
    int    fifo_buf_len;

    /* Timers */
    guint  clock_timer_id;
    guint  fetch_timer_id;
    guint  night_timer_id;
    guint  fifo_source_id;
} AppState;

static AppState app_state;

/* ── Utility functions ─────────────────────────────────────────── */

static void resolve_path(const char *exe, const char *rel, char *out, size_t out_size)
{
    char exe_copy[PATH_MAX];
    strncpy(exe_copy, exe, PATH_MAX - 1);
    exe_copy[PATH_MAX - 1] = '\0';
    char *dir = dirname(exe_copy);

    /* Try relative to executable (build tree) */
    snprintf(out, out_size, "%s/../%s", dir, rel);
    if (access(out, F_OK) == 0)
        return;
    /* Try installed azmap-gtk share */
    snprintf(out, out_size, "%s/../share/azmap-gtk/%s", dir, rel);
    if (access(out, F_OK) == 0)
        return;
    /* Fall back to azmap share (shared data) */
    snprintf(out, out_size, "%s/../share/azmap/%s", dir, rel);
}

static int format_coord(char *buf, size_t sz, double lat, double lon)
{
    char ns = lat >= 0 ? 'N' : 'S';
    char ew = lon >= 0 ? 'E' : 'W';
    return snprintf(buf, sz, "%.2f%c, %.2f%c", fabs(lat), ns, fabs(lon), ew);
}

static void build_label(char *buf, size_t sz, const char *name, double lat, double lon)
{
    char coord[64];
    format_coord(coord, sizeof(coord), lat, lon);
    if (name && name[0])
        snprintf(buf, sz, "%s (%s)", name, coord);
    else
        snprintf(buf, sz, "%s", coord);
}

static void str_upper(char *dst, size_t dst_sz, const char *src)
{
    size_t i;
    for (i = 0; i < dst_sz - 1 && src[i]; i++)
        dst[i] = (char)toupper((unsigned char)src[i]);
    dst[i] = '\0';
}

static void km_to_pixel(const float *mvp, float kx, float ky,
                         int fb_w, int fb_h, float *px, float *py)
{
    float cx = mvp[0]*kx + mvp[4]*ky + mvp[12];
    float cy = mvp[1]*kx + mvp[5]*ky + mvp[13];
    float cw = mvp[3]*kx + mvp[7]*ky + mvp[15];
    float nx = cx / cw;
    float ny = cy / cw;
    *px = (nx * 0.5f + 0.5f) * (float)fb_w;
    *py = (-ny * 0.5f + 0.5f) * (float)fb_h;
}

static int build_label_bg(float x, float y, float w, float h, float pad,
                           float *out)
{
    float x0 = x - pad, y0 = y - pad;
    float x1 = x + w + pad, y1 = y + h + pad;
    out[0]  = x0; out[1]  = y0;
    out[2]  = x1; out[3]  = y0;
    out[4]  = x1; out[5]  = y1;
    out[6]  = x0; out[7]  = y0;
    out[8]  = x1; out[9]  = y1;
    out[10] = x0; out[11] = y1;
    return 6;
}

static int build_gc_line(double lat1, double lon1, double lat2, double lon2,
                          float *verts)
{
    double phi1 = lat1 * M_PI / 180.0, lam1 = lon1 * M_PI / 180.0;
    double phi2 = lat2 * M_PI / 180.0, lam2 = lon2 * M_PI / 180.0;
    double cos_d = sin(phi1)*sin(phi2) + cos(phi1)*cos(phi2)*cos(lam2 - lam1);
    if (cos_d > 1.0) cos_d = 1.0;
    if (cos_d < -1.0) cos_d = -1.0;
    double d = acos(cos_d);

    if (d < 1e-10) {
        double x, y;
        projection_forward_clamped(lat1, lon1, &x, &y);
        verts[0] = (float)x;
        verts[1] = (float)y;
        return 1;
    }

    double sin_d = sin(d);
    int n = GC_LINE_POINTS - 1;
    for (int i = 0; i <= n; i++) {
        double t = (double)i / (double)n;
        double a = sin((1.0 - t) * d) / sin_d;
        double b = sin(t * d) / sin_d;
        double x3 = a * cos(phi1)*cos(lam1) + b * cos(phi2)*cos(lam2);
        double y3 = a * cos(phi1)*sin(lam1) + b * cos(phi2)*sin(lam2);
        double z3 = a * sin(phi1)            + b * sin(phi2);
        double lat = atan2(z3, sqrt(x3*x3 + y3*y3)) * 180.0 / M_PI;
        double lon = atan2(y3, x3) * 180.0 / M_PI;
        double px, py;
        projection_forward_clamped(lat, lon, &px, &py);
        verts[i * 2]     = (float)px;
        verts[i * 2 + 1] = (float)py;
    }
    return n + 1;
}

static void reproject_all(AppState *s)
{
    map_data_reproject(&s->map);
    renderer_upload_map(&s->renderer, &s->map);
    if (s->has_borders) {
        map_data_reproject(&s->borders);
        renderer_upload_borders(&s->renderer, &s->borders);
    }
    if (s->has_land) {
        map_data_reproject_nosplit(&s->land);
        renderer_upload_land(&s->renderer, &s->land);
    }
}

static void update_target_geometry(AppState *s, int recompute_dist)
{
    if (recompute_dist) {
        s->dist = projection_distance(s->qth_lat, s->qth_lon,
                                       s->target_lat, s->target_lon);
        s->az_to = projection_azimuth(s->qth_lat, s->qth_lon,
                                       s->target_lat, s->target_lon);
        s->az_from = projection_azimuth(s->target_lat, s->target_lon,
                                         s->qth_lat, s->qth_lon);
    }
    projection_forward(s->qth_lat, s->qth_lon, &s->cx, &s->cy);
    projection_forward(s->target_lat, s->target_lon, &s->tx, &s->ty);
    float gc_verts[GC_LINE_POINTS * 2];
    int gc_n = build_gc_line(s->qth_lat, s->qth_lon,
                              s->target_lat, s->target_lon, gc_verts);
    renderer_upload_target_line(&s->renderer, gc_verts, gc_n);
}

/* ── Sidebar update ────────────────────────────────────────────── */

static void update_sidebar_labels(AppState *s)
{
    time_t now = time(NULL);
    struct tm gt_buf, lt_buf;
    struct tm *gt = gmtime_r(&now, &gt_buf);
    struct tm *lt = localtime_r(&now, &lt_buf);
    if (!gt || !lt) return;

    char buf[64];
    snprintf(buf, sizeof(buf), "UTC  %02d:%02d:%02d",
             gt->tm_hour, gt->tm_min, gt->tm_sec);
    gtk_label_set_text(GTK_LABEL(s->lbl_utc), buf);

    snprintf(buf, sizeof(buf), "LOC  %02d:%02d:%02d",
             lt->tm_hour, lt->tm_min, lt->tm_sec);
    gtk_label_set_text(GTK_LABEL(s->lbl_local), buf);

    if (s->dist > 0.0) {
        snprintf(buf, sizeof(buf), "DIST  %.1f KM", s->dist);
        gtk_label_set_text(GTK_LABEL(s->lbl_dist), buf);
        snprintf(buf, sizeof(buf), "AZ TO  %.1f\u00b0", s->az_to);
        gtk_label_set_text(GTK_LABEL(s->lbl_az_to), buf);
        snprintf(buf, sizeof(buf), "AZ FROM  %.1f\u00b0", s->az_from);
        gtk_label_set_text(GTK_LABEL(s->lbl_az_from), buf);
        gtk_widget_set_visible(s->lbl_dist, TRUE);
        gtk_widget_set_visible(s->lbl_az_to, TRUE);
        gtk_widget_set_visible(s->lbl_az_from, TRUE);
    } else {
        gtk_widget_set_visible(s->lbl_dist, FALSE);
        gtk_widget_set_visible(s->lbl_az_to, FALSE);
        gtk_widget_set_visible(s->lbl_az_from, FALSE);
    }

    /* Geomag indices — always visible */
    if (s->geomag.valid) {
        snprintf(buf, sizeof(buf), "Kp %.1f  Bz %.1f nT",
                 s->geomag.kp, s->geomag.bz);
        gtk_label_set_text(GTK_LABEL(s->lbl_geomag), buf);
    } else {
        gtk_label_set_text(GTK_LABEL(s->lbl_geomag), "Kp --  Bz -- nT");
    }

    /* Solar indices + X-ray flare — always visible */
    {
        const char *xr = s->xray.valid ? s->xray.max_class : "--";
        if (s->solar.valid)
            snprintf(buf, sizeof(buf), "SFU %d  SSN %d  %s",
                     s->solar.sfu, s->solar.ssn, xr);
        else
            snprintf(buf, sizeof(buf), "SFU --  SSN --  %s", xr);
        gtk_label_set_text(GTK_LABEL(s->lbl_solar), buf);
    }

    /* DRAP peak + NOAA scales — always visible, single line */
    {
        const char *rsg = s->scales.valid ?
            (snprintf(buf, sizeof(buf), "R%d S%d G%d",
                      s->scales.r, s->scales.s, s->scales.g), buf) : "R- S- G-";
        char line[128];
        if (s->drap_grid.valid)
            snprintf(line, sizeof(line), "DRAP %.1f MHz  %s", s->drap_grid.peak_mhz, rsg);
        else
            snprintf(line, sizeof(line), "DRAP -- MHz  %s", rsg);
        gtk_label_set_text(GTK_LABEL(s->lbl_drap_peak), line);
    }
}

/* ── GL area callbacks ─────────────────────────────────────────── */

static void on_unrealize(GtkGLArea *area, gpointer data)
{
    (void)data;
    AppState *s = &app_state;
    gtk_gl_area_make_current(area);
    if (s->gl_initialized) {
        renderer_destroy(&s->renderer);
        s->gl_initialized = 0;
    }
}

static void on_realize(GtkGLArea *area, gpointer data)
{
    (void)data;
    AppState *s = &app_state;

    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area) != NULL) return;

    gtk_gl_area_set_has_stencil_buffer(area, TRUE);

    if (renderer_init(&s->renderer, s->shader_dir) != 0) {
        fprintf(stderr, "Error: renderer init failed\n");
        return;
    }

    /* Upload initial geometry */
    renderer_upload_map(&s->renderer, &s->map);
    if (s->has_borders)
        renderer_upload_borders(&s->renderer, &s->borders);
    if (s->has_land)
        renderer_upload_land(&s->renderer, &s->land);
    renderer_upload_grid(&s->renderer, &s->grid);
    renderer_upload_dist_circles(&s->renderer, &s->dist_circles);
    renderer_upload_earth_circle(&s->renderer, projection_get_radius());

    {
        float gc_verts[GC_LINE_POINTS * 2];
        int gc_n = build_gc_line(s->qth_lat, s->qth_lon,
                                  s->target_lat, s->target_lon, gc_verts);
        renderer_upload_target_line(&s->renderer, gc_verts, gc_n);
    }

    float marker_size = 300.0f;
    renderer_upload_markers(&s->renderer, (float)s->cx, (float)s->cy,
                            (float)s->tx, (float)s->ty, marker_size);

    /* Night overlay */
    {
        time_t now = time(NULL);
        s->last_sun_update = now;
        SubsolarPoint sun = solar_subsolar_point(now);
        nightmesh_build(&s->nightmesh, &sun);
        renderer_upload_night(&s->renderer, s->nightmesh.vertices,
                              s->nightmesh.vertex_count);
    }

    s->gl_initialized = 1;
}

static gboolean on_render(GtkGLArea *area, GdkGLContext *context, gpointer data)
{
    (void)context; (void)data;
    AppState *s = &app_state;
    if (!s->gl_initialized) return FALSE;

    /* Handle projection center change from input */
    if (s->input.center_dirty) {
        s->input.center_dirty = 0;
        projection_set_center(s->input.center_lat, s->input.center_lon);
        reproject_all(s);
        update_target_geometry(s, 0);
        projection_forward(90.0, 0.0, &s->npx, &s->npy);

        if (projection_get_mode() == PROJ_ORTHO) {
            grid_build_geo(&s->grid);
            renderer_upload_grid(&s->renderer, &s->grid);
        }
        grid_build_dist_circles(&s->dist_circles, s->qth_lat, s->qth_lon);
        renderer_upload_dist_circles(&s->renderer, &s->dist_circles);

        /* Rebuild night overlay for new projection center */
        {
            time_t now = time(NULL);
            s->last_sun_update = now;
            SubsolarPoint sun = solar_subsolar_point(now);
            nightmesh_build(&s->nightmesh, &sun);
            renderer_upload_night(&s->renderer, s->nightmesh.vertices,
                                  s->nightmesh.vertex_count);
        }

        /* Reproject overlays */
        if (s->muf_active && s->muf_data.raw_count > 0) {
            muf_reproject(&s->muf_data);
            renderer_upload_muf(&s->renderer, &s->muf_data);
        }
        if (s->spore_active && s->spore_data.raw_count > 0) {
            muf_reproject(&s->spore_data);
            renderer_upload_spore(&s->renderer, &s->spore_data);
        }
        if (s->aurora_active && s->aurora_grid.valid) {
            aurora_mesh_build(&s->aurora_mesh, &s->aurora_grid);
            renderer_upload_aurora(&s->renderer, &s->aurora_mesh);
        }
        if (s->drap_active && s->drap_grid.valid) {
            drap_mesh_build(&s->drap_mesh, &s->drap_grid);
            renderer_upload_drap(&s->renderer, &s->drap_mesh);
        }
    }

    int fb_w = gtk_widget_get_width(GTK_WIDGET(area));
    int fb_h = gtk_widget_get_height(GTK_WIDGET(area));
    int scale = gtk_widget_get_scale_factor(GTK_WIDGET(area));
    fb_w *= scale;
    fb_h *= scale;

    if (fb_w <= 0 || fb_h <= 0) return FALSE;

    s->cam.aspect = (float)fb_w / (float)fb_h;

    /* Marker size proportional to zoom */
    float marker_size = s->cam.zoom_km * MARKER_ZOOM_FACTOR;
    if (s->dist > 0.0)
        renderer_upload_markers(&s->renderer, (float)s->cx, (float)s->cy,
                                (float)s->tx, (float)s->ty, marker_size);
    else
        renderer_upload_markers(&s->renderer, (float)s->cx, (float)s->cy,
                                (float)s->cx, (float)s->cy, 0.0f);

    /* North pole */
    renderer_upload_npole(&s->renderer, (float)s->npx, (float)s->npy, marker_size);

    /* Build labels in pixel-space */
    float mvp[16];
    camera_get_mvp(&s->cam, mvp);

    {
        float label_verts[8192];
        float label_bg_verts[256];
        int lvc = 0, lbg_vc = 0;
        float label_size = 16.0f;

        /* Center label */
        float px, py;
        km_to_pixel(mvp, (float)s->cx, (float)s->cy, fb_w, fb_h, &px, &py);
        char upper_center[128];
        str_upper(upper_center, sizeof(upper_center), s->center_label);
        float cw = text_width(upper_center, label_size);
        float lx = px - cw * 0.5f;
        float ly = py + marker_size * 0.01f + 8.0f;
        lvc += text_build(upper_center, lx, ly, label_size,
                          label_verts + lvc * 2, 4096 - lvc);
        int label_split = lvc;
        lbg_vc += build_label_bg(lx, ly, cw, label_size, 4.0f,
                                  label_bg_verts + lbg_vc * 2);
        int bg_split = lbg_vc;

        /* Target label */
        km_to_pixel(mvp, (float)s->tx, (float)s->ty, fb_w, fb_h, &px, &py);
        char upper_target[128];
        str_upper(upper_target, sizeof(upper_target), s->target_label);
        float tw = text_width(upper_target, label_size);
        lx = px - tw * 0.5f;
        ly = py + marker_size * 0.01f + 8.0f;
        lvc += text_build(upper_target, lx, ly, label_size,
                          label_verts + lvc * 2, 4096 - lvc);
        lbg_vc += build_label_bg(lx, ly, tw, label_size, 4.0f,
                                  label_bg_verts + lbg_vc * 2);

        renderer_upload_labels(&s->renderer, label_verts, lvc, label_split);
        renderer_upload_label_bgs(&s->renderer, label_bg_verts, lbg_vc, bg_split);
    }

    /* Distance circle labels */
    {
        float dl_verts[4096];
        int dlvc = 0;
        float dsz = 11.0f;
        for (int i = 0; i < s->dist_circles.num_segments; i++) {
            int km = (i + 1) * 2000;
            /* Position at due-north point of circle */
            double dest_lat, dest_lon;
            double clat_r = s->qth_lat * M_PI / 180.0;
            double clon_r = s->qth_lon * M_PI / 180.0;
            double d_r = (double)km / 6371.0;
            dest_lat = asin(sin(clat_r)*cos(d_r) + cos(clat_r)*sin(d_r)) * 180.0/M_PI;
            dest_lon = (clon_r + atan2(0.0, cos(clat_r)*cos(d_r) - sin(clat_r)*sin(d_r))) * 180.0/M_PI;
            double kx, ky;
            projection_forward(dest_lat, dest_lon, &kx, &ky);
            float px, py;
            km_to_pixel(mvp, (float)kx, (float)ky, fb_w, fb_h, &px, &py);
            char dlbl[32];
            snprintf(dlbl, sizeof(dlbl), "%d km", km);
            float dw = text_width(dlbl, dsz);
            dlvc += text_build(dlbl, px - dw*0.5f, py - dsz - 2.0f, dsz,
                               dl_verts + dlvc * 2, 2048 - dlvc);
        }
        renderer_upload_dist_labels(&s->renderer, dl_verts, dlvc);
    }

    glViewport(0, 0, fb_w, fb_h);
    renderer_draw(&s->renderer, mvp, fb_w, fb_h);

    return TRUE;
}

static void on_resize(GtkGLArea *area, int width, int height, gpointer data)
{
    (void)area; (void)width; (void)height; (void)data;
    /* GTK handles viewport; we update aspect in render callback */
}

/* ── Timer callbacks ───────────────────────────────────────────── */

static gboolean on_clock_tick(gpointer data)
{
    AppState *s = (AppState *)data;
    update_sidebar_labels(s);
    return G_SOURCE_CONTINUE;
}

static gboolean on_night_update(gpointer data)
{
    AppState *s = (AppState *)data;
    if (!s->gl_initialized) return G_SOURCE_CONTINUE;

    gtk_gl_area_make_current(GTK_GL_AREA(s->gl_area));

    time_t now = time(NULL);
    s->last_sun_update = now;
    SubsolarPoint sun = solar_subsolar_point(now);
    nightmesh_build(&s->nightmesh, &sun);
    renderer_upload_night(&s->renderer, s->nightmesh.vertices,
                          s->nightmesh.vertex_count);
    gtk_widget_queue_draw(s->gl_area);
    return G_SOURCE_CONTINUE;
}

/* ── Legend rebuild ────────────────────────────────────────────── */

static GtkCssProvider *legend_css_provider = NULL;

static void legend_add_colored_label(GtkWidget *row, const char *text,
                                     float r, float g, float b, float a,
                                     GString *css_buf, int id)
{
    char cls[32];
    snprintf(cls, sizeof(cls), "lc%d", id);

    GtkWidget *lbl = gtk_label_new(text);
    gtk_widget_add_css_class(lbl, cls);
    gtk_box_append(GTK_BOX(row), lbl);

    g_string_append_printf(css_buf,
        ".%s { color: rgba(%d,%d,%d,%.2f); font-family: monospace; font-size: 11px; } ",
        cls, (int)(r * 255), (int)(g * 255), (int)(b * 255), a);
}

static void rebuild_legends(AppState *s)
{
    /* Clear existing children */
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(s->legend_box)) != NULL)
        gtk_box_remove(GTK_BOX(s->legend_box), child);

    /* Remove old legend CSS provider */
    if (legend_css_provider) {
        gtk_style_context_remove_provider_for_display(
            gdk_display_get_default(),
            GTK_STYLE_PROVIDER(legend_css_provider));
        g_object_unref(legend_css_provider);
        legend_css_provider = NULL;
    }

    GString *css_buf = g_string_new("");
    int color_id = 0;

    /* E's legend */
    if (s->spore_data.legend_count > 0) {
        GtkWidget *title = gtk_label_new("E's (foEs MHz)");
        gtk_widget_add_css_class(title, "section-label");
        gtk_box_append(GTK_BOX(s->legend_box), title);
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_halign(row, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(s->legend_box), row);
        for (int i = 0; i < s->spore_data.legend_count; i++) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.0f", s->spore_data.legend[i].mhz);
            legend_add_colored_label(row, buf,
                s->spore_data.legend[i].color[0],
                s->spore_data.legend[i].color[1],
                s->spore_data.legend[i].color[2],
                1.0f, css_buf, color_id++);
        }
    }

    /* MUF legend */
    if (s->muf_data.legend_count > 0) {
        GtkWidget *title = gtk_label_new("MUF (MHz)");
        gtk_widget_add_css_class(title, "section-label");
        gtk_box_append(GTK_BOX(s->legend_box), title);
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_halign(row, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(s->legend_box), row);
        for (int i = 0; i < s->muf_data.legend_count; i++) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.0f", s->muf_data.legend[i].mhz);
            legend_add_colored_label(row, buf,
                s->muf_data.legend[i].color[0],
                s->muf_data.legend[i].color[1],
                s->muf_data.legend[i].color[2],
                1.0f, css_buf, color_id++);
        }
    }

    /* DRAP legend */
    {
        GtkWidget *title = gtk_label_new("DRAP");
        gtk_widget_add_css_class(title, "section-label");
        gtk_box_append(GTK_BOX(s->legend_box), title);
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_halign(row, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(s->legend_box), row);

        const char *drap_levels[] = {"Low", "Med", "High"};
        const float drap_alphas[] = {0.4f, 0.7f, 1.0f};
        for (int i = 0; i < 3; i++) {
            legend_add_colored_label(row, drap_levels[i],
                0.85f, 0.2f, 0.05f, drap_alphas[i],
                css_buf, color_id++);
        }
    }

    /* Install new legend CSS provider */
    legend_css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(legend_css_provider, css_buf->str);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(legend_css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_string_free(css_buf, TRUE);
}

static gboolean on_fetch_poll(gpointer data)
{
    AppState *s = (AppState *)data;
    if (!s->gl_initialized) return G_SOURCE_CONTINUE;

    gtk_gl_area_make_current(GTK_GL_AREA(s->gl_area));
    time_t now = time(NULL);
    int need_redraw = 0;

    /* Poll MUF fetch */
    if (s->muf_fetching) {
        int st = fetch_check(&s->muf_fetch);
        if (st != 0) {
            s->muf_fetching = 0;
            if (st == 1) {
                char *json = fetch_take_response(&s->muf_fetch);
                if (json) {
                    muf_data_free(&s->muf_data);
                    muf_data_init(&s->muf_data);
                    muf_parse_geojson(json, &s->muf_data);
                    free(json);
                    if (s->muf_active && s->muf_data.num_segments > 0) {
                        renderer_upload_muf(&s->renderer, &s->muf_data);
                        need_redraw = 1;
                    }
                    rebuild_legends(s);
                }
            }
            fetch_cleanup(&s->muf_fetch);
        }
    }

    /* Poll Sporadic E fetch */
    if (s->spore_fetching) {
        int st = fetch_check(&s->spore_fetch);
        if (st != 0) {
            s->spore_fetching = 0;
            if (st == 1) {
                char *json = fetch_take_response(&s->spore_fetch);
                if (json) {
                    muf_data_free(&s->spore_data);
                    muf_data_init(&s->spore_data);
                    spore_parse_json(json, &s->spore_data);
                    free(json);
                    if (s->spore_active && s->spore_data.num_segments > 0) {
                        renderer_upload_spore(&s->renderer, &s->spore_data);
                        need_redraw = 1;
                    }
                    rebuild_legends(s);
                }
            }
            fetch_cleanup(&s->spore_fetch);
        }
    }

    /* Poll Aurora fetch */
    if (s->aurora_fetching) {
        int st = fetch_check(&s->aurora_fetch);
        if (st != 0) {
            s->aurora_fetching = 0;
            if (st == 1) {
                char *json = fetch_take_response(&s->aurora_fetch);
                if (json) {
                    aurora_parse_json(json, &s->aurora_grid);
                    free(json);
                    if (s->aurora_active && s->aurora_grid.valid) {
                        aurora_mesh_build(&s->aurora_mesh, &s->aurora_grid);
                        renderer_upload_aurora(&s->renderer, &s->aurora_mesh);
                        need_redraw = 1;
                    }
                }
            }
            fetch_cleanup(&s->aurora_fetch);
        }
    }

    /* Poll DRAP fetch */
    if (s->drap_fetching) {
        int st = fetch_check(&s->drap_fetch);
        if (st != 0) {
            s->drap_fetching = 0;
            if (st == 1) {
                char *txt = fetch_take_response(&s->drap_fetch);
                if (txt) {
                    drap_parse_text(txt, &s->drap_grid);
                    free(txt);
                    if (s->drap_active && s->drap_grid.valid) {
                        drap_mesh_build(&s->drap_mesh, &s->drap_grid);
                        renderer_upload_drap(&s->renderer, &s->drap_mesh);
                        need_redraw = 1;
                    }
                }
            }
            fetch_cleanup(&s->drap_fetch);
        }
    }

    /* Poll Kp/Bz */
    if (s->kp_fetching) {
        int st = fetch_check(&s->kp_fetch);
        if (st != 0) {
            s->kp_fetching = 0;
            if (st == 1) {
                char *json = fetch_take_response(&s->kp_fetch);
                if (json) { geomag_parse_kp(json, &s->geomag); free(json); }
            }
            fetch_cleanup(&s->kp_fetch);
        }
    }
    if (s->bz_fetching) {
        int st = fetch_check(&s->bz_fetch);
        if (st != 0) {
            s->bz_fetching = 0;
            if (st == 1) {
                char *json = fetch_take_response(&s->bz_fetch);
                if (json) { geomag_parse_bz(json, &s->geomag); free(json); }
            }
            fetch_cleanup(&s->bz_fetch);
        }
    }
    if (s->solar_fetching) {
        int st = fetch_check(&s->solar_fetch);
        if (st != 0) {
            s->solar_fetching = 0;
            if (st == 1) {
                char *txt = fetch_take_response(&s->solar_fetch);
                if (txt) { solar_parse_text(txt, &s->solar); free(txt); }
            }
            fetch_cleanup(&s->solar_fetch);
        }
    }
    if (s->scales_fetching) {
        int st = fetch_check(&s->scales_fetch);
        if (st != 0) {
            s->scales_fetching = 0;
            if (st == 1) {
                char *json = fetch_take_response(&s->scales_fetch);
                if (json) { scales_parse_json(json, &s->scales); free(json); }
            }
            fetch_cleanup(&s->scales_fetch);
        }
    }
    if (s->xray_fetching) {
        int st = fetch_check(&s->xray_fetch);
        if (st != 0) {
            s->xray_fetching = 0;
            if (st == 1) {
                char *json = fetch_take_response(&s->xray_fetch);
                if (json) { xray_parse_json(json, &s->xray); free(json); }
            }
            fetch_cleanup(&s->xray_fetch);
        }
    }

    /* Auto-refresh overlays every OVERLAY_UPDATE_SEC */
    if (s->muf_active && !s->muf_fetching &&
        now - s->last_muf_fetch >= OVERLAY_UPDATE_SEC) {
        fetch_start(&s->muf_fetch, MUF_URL);
        s->muf_fetching = 1;
        s->last_muf_fetch = now;
    }
    if (s->spore_active && !s->spore_fetching &&
        now - s->last_spore_fetch >= OVERLAY_UPDATE_SEC) {
        fetch_start(&s->spore_fetch, SPORE_URL);
        s->spore_fetching = 1;
        s->last_spore_fetch = now;
    }
    if (s->aurora_active && !s->aurora_fetching &&
        now - s->last_aurora_fetch >= OVERLAY_UPDATE_SEC) {
        fetch_start(&s->aurora_fetch, AURORA_URL);
        s->aurora_fetching = 1;
        s->last_aurora_fetch = now;
    }
    if (!s->drap_fetching &&
        now - s->last_drap_fetch >= OVERLAY_UPDATE_SEC) {
        fetch_start(&s->drap_fetch, DRAP_URL);
        s->drap_fetching = 1;
        s->last_drap_fetch = now;
    }
    if (!s->kp_fetching && !s->bz_fetching &&
        now - s->last_geomag_fetch >= OVERLAY_UPDATE_SEC) {
        fetch_start(&s->kp_fetch, KP_URL);
        s->kp_fetching = 1;
        fetch_start(&s->bz_fetch, BZ_URL);
        s->bz_fetching = 1;
        if (!s->solar_fetching) {
            fetch_start(&s->solar_fetch, SOLAR_URL);
            s->solar_fetching = 1;
        }
        if (!s->scales_fetching) {
            fetch_start(&s->scales_fetch, SCALES_URL);
            s->scales_fetching = 1;
        }
        if (!s->xray_fetching) {
            fetch_start(&s->xray_fetch, XRAY_URL);
            s->xray_fetching = 1;
        }
        s->last_geomag_fetch = now;
    }

    if (need_redraw)
        gtk_widget_queue_draw(s->gl_area);

    return G_SOURCE_CONTINUE;
}

/* ── FIFO IPC ──────────────────────────────────────────────────── */

static gboolean on_fifo_readable(GIOChannel *source, GIOCondition cond, gpointer data)
{
    (void)source; (void)cond;
    AppState *s = (AppState *)data;

    ssize_t nr = read(s->fifo_fd, s->fifo_buf + s->fifo_buf_len,
                      sizeof(s->fifo_buf) - 1 - s->fifo_buf_len);
    if (nr <= 0) return TRUE;

    s->fifo_buf_len += (int)nr;
    s->fifo_buf[s->fifo_buf_len] = '\0';

    char *last_nl = strrchr(s->fifo_buf, '\n');
    if (!last_nl && s->fifo_buf_len >= (int)sizeof(s->fifo_buf) - 2) {
        s->fifo_buf_len = 0;
        return TRUE;
    }
    if (!last_nl) return TRUE;

    *last_nl = '\0';
    char *line = strrchr(s->fifo_buf, '\n');
    line = line ? line + 1 : s->fifo_buf;

    double new_lat, new_lon;
    char new_name[64] = {0};
    if (sscanf(line, "%lf,%lf,", &new_lat, &new_lon) == 2) {
        char *p = strchr(line, ',');
        if (p) p = strchr(p + 1, ',');
        if (p && *(p + 1)) {
            p++;
            char *pipe = strchr(p, '|');
            if (pipe) {
                size_t nlen = (size_t)(pipe - p);
                if (nlen >= sizeof(new_name)) nlen = sizeof(new_name) - 1;
                memcpy(new_name, p, nlen);
                new_name[nlen] = '\0';
                /* Parse station detail fields */
                static const char *labels[] = {"STN", "FREQ", "CTRY", "SITE", "LANG", "TGT"};
                char dbuf[256];
                strncpy(dbuf, pipe + 1, sizeof(dbuf) - 1);
                dbuf[sizeof(dbuf) - 1] = '\0';
                char *field = dbuf;
                int si_count = 0;
                for (int i = 0; i < 6 && field; i++) {
                    char *next = strchr(field, '|');
                    if (next) *next = '\0';
                    if (*field) {
                        char upper[40];
                        str_upper(upper, sizeof(upper), field);
                        char info[48];
                        snprintf(info, sizeof(info), "%s: %s", labels[i], upper);
                        gtk_label_set_text(GTK_LABEL(s->lbl_station_info[si_count]), info);
                        gtk_widget_set_visible(s->lbl_station_info[si_count], TRUE);
                        si_count++;
                    }
                    field = next ? next + 1 : NULL;
                }
                for (int i = si_count; i < 6; i++)
                    gtk_widget_set_visible(s->lbl_station_info[i], FALSE);
            } else {
                strncpy(new_name, p, sizeof(new_name) - 1);
                new_name[sizeof(new_name) - 1] = '\0';
            }
        }

        s->target_lat = new_lat;
        s->target_lon = new_lon;
        strncpy(s->target_name, new_name, sizeof(s->target_name) - 1);
        build_label(s->target_label, sizeof(s->target_label),
                    s->target_name, s->target_lat, s->target_lon);

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s->btn_qrz)))
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->btn_qrz), FALSE);

        if (s->gl_initialized) {
            gtk_gl_area_make_current(GTK_GL_AREA(s->gl_area));
            update_target_geometry(s, 1);
        }
        update_sidebar_labels(s);
        gtk_widget_queue_draw(s->gl_area);
    }

    s->fifo_buf_len = 0;
    return TRUE;
}

/* ── Button callbacks ──────────────────────────────────────────── */

static void on_proj_toggle(GtkToggleButton *btn, gpointer data)
{
    AppState *s = (AppState *)data;
    if (!s->gl_initialized) return;

    gtk_gl_area_make_current(GTK_GL_AREA(s->gl_area));

    if (gtk_toggle_button_get_active(btn)) {
        projection_set_mode(PROJ_ORTHO);
    } else {
        projection_set_mode(PROJ_AZEQ);
        /* Reset view (same as pressing R) */
        camera_reset(&s->cam);
        s->input.center_lat = s->input.original_center_lat;
        s->input.center_lon = s->input.original_center_lon;
        projection_set_center(s->input.center_lat, s->input.center_lon);
    }

    renderer_upload_earth_circle(&s->renderer, projection_get_radius());
    reproject_all(s);
    update_target_geometry(s, 0);
    projection_forward(90.0, 0.0, &s->npx, &s->npy);

    if (projection_get_mode() == PROJ_ORTHO)
        grid_build_geo(&s->grid);
    else
        grid_build(&s->grid);
    renderer_upload_grid(&s->renderer, &s->grid);
    grid_build_dist_circles(&s->dist_circles, s->qth_lat, s->qth_lon);
    renderer_upload_dist_circles(&s->renderer, &s->dist_circles);

    /* Rebuild night overlay for new projection */
    {
        time_t now = time(NULL);
        s->last_sun_update = now;
        SubsolarPoint sun = solar_subsolar_point(now);
        nightmesh_build(&s->nightmesh, &sun);
        renderer_upload_night(&s->renderer, s->nightmesh.vertices,
                              s->nightmesh.vertex_count);
    }
    /* Reproject overlays */
    if (s->muf_active && s->muf_data.raw_count > 0) {
        muf_reproject(&s->muf_data);
        renderer_upload_muf(&s->renderer, &s->muf_data);
    }
    if (s->spore_active && s->spore_data.raw_count > 0) {
        muf_reproject(&s->spore_data);
        renderer_upload_spore(&s->renderer, &s->spore_data);
    }
    if (s->aurora_active && s->aurora_grid.valid) {
        aurora_mesh_build(&s->aurora_mesh, &s->aurora_grid);
        renderer_upload_aurora(&s->renderer, &s->aurora_mesh);
    }
    if (s->drap_active && s->drap_grid.valid) {
        drap_mesh_build(&s->drap_mesh, &s->drap_grid);
        renderer_upload_drap(&s->renderer, &s->drap_mesh);
    }

    gtk_widget_queue_draw(s->gl_area);
}

static void on_home_clicked(GtkButton *btn, gpointer data)
{
    (void)btn;
    AppState *s = (AppState *)data;
    s->input.center_lat = s->input.original_center_lat;
    s->input.center_lon = s->input.original_center_lon;
    s->cam.pan_x = 0.0f;
    s->cam.pan_y = 0.0f;
    s->input.center_dirty = 1;
    gtk_widget_queue_draw(s->gl_area);
}

static void toggle_overlay(GtkToggleButton *btn, AppState *s,
                            int *active, int *fetching, FetchRequest *fetch,
                            time_t *last_fetch, const char *url)
{
    *active = gtk_toggle_button_get_active(btn);
    if (*active && !*fetching) {
        fetch_start(fetch, url);
        *fetching = 1;
        *last_fetch = time(NULL);
    }
    gtk_widget_queue_draw(s->gl_area);
}

static void on_aurora_toggle(GtkToggleButton *btn, gpointer data)
{
    AppState *s = (AppState *)data;
    toggle_overlay(btn, s, &s->aurora_active, &s->aurora_fetching,
                   &s->aurora_fetch, &s->last_aurora_fetch, AURORA_URL);
    if (s->aurora_active) {
        /* Also start Kp/Bz fetch */
        if (!s->kp_fetching) {
            fetch_start(&s->kp_fetch, KP_URL);
            s->kp_fetching = 1;
            fetch_start(&s->bz_fetch, BZ_URL);
            s->bz_fetching = 1;
            s->last_geomag_fetch = time(NULL);
        }
    } else {
        s->renderer.aurora_vertex_count = 0;
    }
}

static void on_spore_toggle(GtkToggleButton *btn, gpointer data)
{
    AppState *s = (AppState *)data;
    toggle_overlay(btn, s, &s->spore_active, &s->spore_fetching,
                   &s->spore_fetch, &s->last_spore_fetch, SPORE_URL);
    if (!s->spore_active)
        s->renderer.spore_num_segments = 0;
}

static void on_muf_toggle(GtkToggleButton *btn, gpointer data)
{
    AppState *s = (AppState *)data;
    toggle_overlay(btn, s, &s->muf_active, &s->muf_fetching,
                   &s->muf_fetch, &s->last_muf_fetch, MUF_URL);
    if (!s->muf_active)
        s->renderer.muf_num_segments = 0;
}

static void on_drap_toggle(GtkToggleButton *btn, gpointer data)
{
    AppState *s = (AppState *)data;
    toggle_overlay(btn, s, &s->drap_active, &s->drap_fetching,
                   &s->drap_fetch, &s->last_drap_fetch, DRAP_URL);
    if (!s->drap_active) {
        s->renderer.drap_vertex_count = 0;
    }
}

static void on_qrz_btn_destroy(GtkWidget *btn, gpointer data)
{
    (void)btn;
    AppState *s = data;
    if (s->qrz_popover) {
        gtk_widget_unparent(s->qrz_popover);
        s->qrz_popover = NULL;
    }
}

static void on_qrz_activate(GtkEntry *entry, gpointer data)
{
    AppState *s = (AppState *)data;
    const char *callsign = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (!callsign || !callsign[0] || !s->has_qrz) return;

    QRZResult result;
    char qrz_err[128] = {0};
    if (qrz_lookup(callsign, &result, qrz_err, sizeof(qrz_err)) == 0) {
        s->target_lat = result.lat;
        s->target_lon = result.lon;
        strncpy(s->target_name, result.call, sizeof(s->target_name) - 1);
        build_label(s->target_label, sizeof(s->target_label),
                    s->target_name, s->target_lat, s->target_lon);

        /* Update station info labels */
        char buf[48];
        int si = 0;
        if (result.call[0]) {
            snprintf(buf, sizeof(buf), "CALL: %s", result.call);
            gtk_label_set_text(GTK_LABEL(s->lbl_station_info[si]), buf);
            gtk_widget_set_visible(s->lbl_station_info[si], TRUE);
            si++;
        }
        if (result.name[0]) {
            snprintf(buf, sizeof(buf), "NAME: %.41s", result.name);
            gtk_label_set_text(GTK_LABEL(s->lbl_station_info[si]), buf);
            gtk_widget_set_visible(s->lbl_station_info[si], TRUE);
            si++;
        }
        if (result.grid[0]) {
            snprintf(buf, sizeof(buf), "GRID: %s", result.grid);
            gtk_label_set_text(GTK_LABEL(s->lbl_station_info[si]), buf);
            gtk_widget_set_visible(s->lbl_station_info[si], TRUE);
            si++;
        }
        for (int i = si; i < 6; i++)
            gtk_widget_set_visible(s->lbl_station_info[i], FALSE);

        if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s->btn_qrz)))
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->btn_qrz), TRUE);

        if (s->gl_initialized) {
            gtk_gl_area_make_current(GTK_GL_AREA(s->gl_area));
            update_target_geometry(s, 1);
        }
        update_sidebar_labels(s);
        gtk_widget_queue_draw(s->gl_area);
    }

    gtk_popover_popdown(GTK_POPOVER(s->qrz_popover));
}

/* ── CSS styling ───────────────────────────────────────────────── */

static void load_css(void)
{
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider,
        "window { background-color: #0d0d1e; }"
        ".sidebar { background-color: #10101a; padding: 12px; padding-bottom: 5px; }"
        ".sidebar label { color: #b3cce6; font-family: monospace; font-size: 13px; }"
        ".sidebar .clock { font-size: 16px; font-weight: bold; color: #ddeeff; }"
        ".sidebar .section-label { font-size: 11px; color: #667788; margin-top: 8px; }"
        ".sidebar .info-label { font-size: 12px; color: #99bbdd; }"
        ".sidebar button { "
        "  background-color: #1a1a2e; color: #ccc; border: 1px solid #334; "
        "  border-radius: 6px; padding: 4px 10px; font-size: 11px; "
        "  font-family: monospace; min-height: 24px; }"
        ".sidebar button:hover { background-color: #252540; border-color: #558; }"
        ".sidebar button:checked { background-color: #2a4570; border-color: #4a7ab5; color: #fff; }"
        ".btn-sep { background-color: #ffffff; min-height: 1px; margin: 6px 0; }"
        ".info-sep { background-color: #667788; min-height: 1px; margin: 6px 0; }"
        ".map-area { background-color: #0d0d1e; margin: 8px; }"
    );
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

/* ── Application activate ──────────────────────────────────────── */

static void activate(GtkApplication *app, gpointer user_data)
{
    (void)user_data;
    AppState *s = &app_state;

    load_css();

    /* Main window */
    s->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(s->window), "azMap GTK v" AZMAP_VERSION);
    gtk_window_set_default_size(GTK_WINDOW(s->window), DEFAULT_WIDTH, DEFAULT_HEIGHT);

    /* Main horizontal layout: GL area + sidebar */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_window_set_child(GTK_WINDOW(s->window), hbox);

    /* GL Area for the map */
    s->gl_area = gtk_gl_area_new();
    gtk_gl_area_set_required_version(GTK_GL_AREA(s->gl_area), 3, 3);
    gtk_gl_area_set_has_stencil_buffer(GTK_GL_AREA(s->gl_area), TRUE);
    gtk_widget_set_hexpand(s->gl_area, TRUE);
    gtk_widget_set_vexpand(s->gl_area, TRUE);
    gtk_widget_add_css_class(s->gl_area, "map-area");
    gtk_widget_set_focusable(s->gl_area, TRUE);

    g_signal_connect(s->gl_area, "realize", G_CALLBACK(on_realize), NULL);
    g_signal_connect(s->gl_area, "unrealize", G_CALLBACK(on_unrealize), NULL);
    g_signal_connect(s->gl_area, "render", G_CALLBACK(on_render), NULL);
    g_signal_connect(s->gl_area, "resize", G_CALLBACK(on_resize), NULL);

    gtk_box_append(GTK_BOX(hbox), s->gl_area);

    /* Sidebar */
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(sidebar, "sidebar");
    gtk_widget_set_size_request(sidebar, SIDEBAR_WIDTH, -1);
    gtk_box_append(GTK_BOX(hbox), sidebar);

    /* Clock labels */
    s->lbl_utc = gtk_label_new("UTC  --:--:--");
    gtk_widget_add_css_class(s->lbl_utc, "clock");
    gtk_box_append(GTK_BOX(sidebar), s->lbl_utc);

    s->lbl_local = gtk_label_new("LOC  --:--:--");
    gtk_widget_add_css_class(s->lbl_local, "clock");
    gtk_box_append(GTK_BOX(sidebar), s->lbl_local);

    /* Separator */
    gtk_box_append(GTK_BOX(sidebar), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    /* Station info labels (initially hidden) */
    for (int i = 0; i < 6; i++) {
        s->lbl_station_info[i] = gtk_label_new("");
        gtk_widget_add_css_class(s->lbl_station_info[i], "info-label");
        gtk_widget_set_visible(s->lbl_station_info[i], FALSE);
        gtk_box_append(GTK_BOX(sidebar), s->lbl_station_info[i]);
    }

    /* Distance / azimuth labels */
    s->lbl_dist = gtk_label_new("");
    s->lbl_az_to = gtk_label_new("");
    s->lbl_az_from = gtk_label_new("");
    gtk_widget_add_css_class(s->lbl_dist, "info-label");
    gtk_widget_add_css_class(s->lbl_az_to, "info-label");
    gtk_widget_add_css_class(s->lbl_az_from, "info-label");
    gtk_box_append(GTK_BOX(sidebar), s->lbl_dist);
    gtk_box_append(GTK_BOX(sidebar), s->lbl_az_to);
    gtk_box_append(GTK_BOX(sidebar), s->lbl_az_from);

    GtkWidget *info_sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_add_css_class(info_sep, "info-sep");
    gtk_widget_set_halign(info_sep, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(info_sep, SIDEBAR_WIDTH * 3 / 4, -1);
    gtk_box_append(GTK_BOX(sidebar), info_sep);

    /* Geomag + DRAP info */
    s->lbl_geomag = gtk_label_new("Kp --  Bz -- nT");
    gtk_widget_add_css_class(s->lbl_geomag, "info-label");
    gtk_box_append(GTK_BOX(sidebar), s->lbl_geomag);

    s->lbl_solar = gtk_label_new("SFU --  SSN --");
    gtk_widget_add_css_class(s->lbl_solar, "info-label");
    gtk_box_append(GTK_BOX(sidebar), s->lbl_solar);

    s->lbl_drap_peak = gtk_label_new("DRAP peak -- MHz");
    gtk_widget_add_css_class(s->lbl_drap_peak, "info-label");
    gtk_box_append(GTK_BOX(sidebar), s->lbl_drap_peak);

    GtkWidget *legend_sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_add_css_class(legend_sep, "info-sep");
    gtk_widget_set_halign(legend_sep, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(legend_sep, SIDEBAR_WIDTH * 3 / 4, -1);
    gtk_box_append(GTK_BOX(sidebar), legend_sep);

    /* Legends for E's, MUF, DRAP */
    s->legend_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_append(GTK_BOX(sidebar), s->legend_box);
    rebuild_legends(s);

    /* Spacer to push buttons to bottom */
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(sidebar), spacer);

    /* SOURCE section */
    GtkWidget *src_label = gtk_label_new("SOURCE");
    gtk_widget_add_css_class(src_label, "section-label");
    gtk_box_append(GTK_BOX(sidebar), src_label);

    /* QRZ button + popover */
    GtkWidget *src_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(src_box, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(src_box, SIDEBAR_WIDTH / 2, -1);
    gtk_box_append(GTK_BOX(sidebar), src_box);

    s->btn_qrz = gtk_toggle_button_new_with_label("QRZ");
    gtk_box_append(GTK_BOX(src_box), s->btn_qrz);

    /* QRZ popover with entry */
    s->qrz_popover = gtk_popover_new();
    gtk_widget_set_parent(s->qrz_popover, s->btn_qrz);
    s->qrz_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(s->qrz_entry), "Callsign...");
    gtk_popover_set_child(GTK_POPOVER(s->qrz_popover), s->qrz_entry);
    g_signal_connect(s->qrz_entry, "activate", G_CALLBACK(on_qrz_activate), s);
    g_signal_connect_swapped(s->btn_qrz, "clicked",
                              G_CALLBACK(gtk_popover_popup), s->qrz_popover);
    g_signal_connect(s->btn_qrz, "destroy", G_CALLBACK(on_qrz_btn_destroy), s);

    GtkWidget *src_sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_add_css_class(src_sep, "btn-sep");
    gtk_widget_set_halign(src_sep, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(src_sep, SIDEBAR_WIDTH * 3 / 4, -1);
    gtk_box_append(GTK_BOX(sidebar), src_sep);

    /* LAYERS section */
    GtkWidget *layers_label = gtk_label_new("LAYERS");
    gtk_widget_add_css_class(layers_label, "section-label");
    gtk_box_append(GTK_BOX(sidebar), layers_label);

    GtkWidget *layers_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(layers_box, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(layers_box, SIDEBAR_WIDTH / 2, -1);
    gtk_box_append(GTK_BOX(sidebar), layers_box);

    s->btn_aurora = gtk_toggle_button_new_with_label("Aurora");
    s->btn_spore = gtk_toggle_button_new_with_label("E's");
    s->btn_muf = gtk_toggle_button_new_with_label("MUF");
    s->btn_drap = gtk_toggle_button_new_with_label("DRAP");
    gtk_box_append(GTK_BOX(layers_box), s->btn_aurora);
    gtk_box_append(GTK_BOX(layers_box), s->btn_spore);
    gtk_box_append(GTK_BOX(layers_box), s->btn_muf);
    gtk_box_append(GTK_BOX(layers_box), s->btn_drap);

    g_signal_connect(s->btn_aurora, "toggled", G_CALLBACK(on_aurora_toggle), s);
    g_signal_connect(s->btn_spore, "toggled", G_CALLBACK(on_spore_toggle), s);
    g_signal_connect(s->btn_muf, "toggled", G_CALLBACK(on_muf_toggle), s);
    g_signal_connect(s->btn_drap, "toggled", G_CALLBACK(on_drap_toggle), s);

    GtkWidget *btn_sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_add_css_class(btn_sep, "btn-sep");
    gtk_widget_set_halign(btn_sep, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(btn_sep, SIDEBAR_WIDTH * 3 / 4, -1);
    gtk_box_append(GTK_BOX(sidebar), btn_sep);

    GtkWidget *map_label = gtk_label_new("MAP");
    gtk_widget_add_css_class(map_label, "section-label");
    gtk_box_append(GTK_BOX(sidebar), map_label);

    /* PROJ + HOME buttons – stacked vertically, half sidebar width */
    GtkWidget *ctrl_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(ctrl_box, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(ctrl_box, SIDEBAR_WIDTH / 2, -1);
    gtk_box_append(GTK_BOX(sidebar), ctrl_box);

    s->btn_proj = gtk_toggle_button_new_with_label("ORTHO");
    if (projection_get_mode() == PROJ_ORTHO)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s->btn_proj), TRUE);
    s->btn_home = gtk_button_new_with_label("HOME");
    gtk_box_append(GTK_BOX(ctrl_box), s->btn_proj);
    gtk_box_append(GTK_BOX(ctrl_box), s->btn_home);

    g_signal_connect(s->btn_proj, "toggled", G_CALLBACK(on_proj_toggle), s);
    g_signal_connect(s->btn_home, "clicked", G_CALLBACK(on_home_clicked), s);

    GtkWidget *map_sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_add_css_class(map_sep, "btn-sep");
    gtk_widget_set_halign(map_sep, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(map_sep, SIDEBAR_WIDTH * 3 / 4, -1);
    gtk_box_append(GTK_BOX(sidebar), map_sep);

    /* Input controllers */
    input_init(&s->input, s->gl_area, s->window,
               &s->cam, s->qth_lat, s->qth_lon);

    /* Timers */
    s->clock_timer_id = g_timeout_add_seconds(1, on_clock_tick, s);
    s->night_timer_id = g_timeout_add_seconds(NIGHT_UPDATE_SEC, on_night_update, s);
    s->fetch_timer_id = g_timeout_add(500, on_fetch_poll, s);

    /* FIFO IPC — use XDG_RUNTIME_DIR to avoid symlink races in /tmp */
    const char *runtime = g_get_user_runtime_dir();
    char fifo_path[512];
    snprintf(fifo_path, sizeof(fifo_path), "%s/azmap-target.fifo", runtime);
    unlink(fifo_path);  /* remove stale entry so mkfifo succeeds */
    s->fifo_fd = -1;
    if (mkfifo(fifo_path, 0600) == 0) {
        struct stat st;
        if (lstat(fifo_path, &st) == 0 && S_ISFIFO(st.st_mode))
            s->fifo_fd = open(fifo_path, O_RDWR | O_NONBLOCK);
    }
    if (s->fifo_fd >= 0) {
        GIOChannel *fifo_chan = g_io_channel_unix_new(s->fifo_fd);
        s->fifo_source_id = g_io_add_watch(fifo_chan, G_IO_IN, on_fifo_readable, s);
        g_io_channel_unref(fifo_chan);
    }

    /* Initial sidebar update */
    update_sidebar_labels(s);

    gtk_window_present(GTK_WINDOW(s->window));
}

/* ── Shutdown ──────────────────────────────────────────────────── */

static void on_shutdown(GtkApplication *app, gpointer data)
{
    (void)app; (void)data;
    AppState *s = &app_state;

    if (s->clock_timer_id) g_source_remove(s->clock_timer_id);
    if (s->night_timer_id) g_source_remove(s->night_timer_id);
    if (s->fetch_timer_id) g_source_remove(s->fetch_timer_id);
    if (s->fifo_source_id) g_source_remove(s->fifo_source_id);

    /* Save session state */
    config_save_state(s->target_lat, s->target_lon, s->target_name,
                      s->cam.zoom_km, s->cam.pan_x, s->cam.pan_y,
                      (int)projection_get_mode(),
                      s->input.center_lat, s->input.center_lon,
                      DEFAULT_WIDTH, DEFAULT_HEIGHT, 1);

    if (s->fifo_fd >= 0) close(s->fifo_fd);
    {
        const char *rt = g_get_user_runtime_dir();
        char fp[512];
        snprintf(fp, sizeof(fp), "%s/azmap-target.fifo", rt);
        unlink(fp);
    }

    if (s->muf_fetching) fetch_cleanup(&s->muf_fetch);
    if (s->spore_fetching) fetch_cleanup(&s->spore_fetch);
    if (s->aurora_fetching) fetch_cleanup(&s->aurora_fetch);
    if (s->drap_fetching) fetch_cleanup(&s->drap_fetch);
    if (s->kp_fetching) fetch_cleanup(&s->kp_fetch);
    if (s->bz_fetching) fetch_cleanup(&s->bz_fetch);
    if (s->solar_fetching) fetch_cleanup(&s->solar_fetch);
    if (s->scales_fetching) fetch_cleanup(&s->scales_fetch);
    if (s->xray_fetching) fetch_cleanup(&s->xray_fetch);

    if (s->has_qrz) qrz_cleanup();
    map_data_free(&s->map);
    if (s->has_borders) map_data_free(&s->borders);
    if (s->has_land) map_data_free(&s->land);
    free(s->grid.vertices);
    free(s->dist_circles.vertices);
    nightmesh_free(&s->nightmesh);
    muf_data_free(&s->muf_data);
    muf_data_free(&s->spore_data);
    aurora_mesh_free(&s->aurora_mesh);
    aurora_mesh_free(&s->drap_mesh);
}

/* ── Main ──────────────────────────────────────────────────────── */

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s <center_lat> <center_lon> <target_lat> <target_lon> [options]\n"
        "       %s <target_lat> <target_lon> [options]  (center from config)\n"
        "       %s [options]                             (restore saved session)\n"
        "\nOptions:\n"
        "  -c NAME    Center location name\n"
        "  -t NAME    Target location name\n"
        "  -d DETAIL  Station detail string (station|freq|country|site|lang|target)\n"
        "  -s PATH    Shapefile path override\n",
        prog, prog, prog);
}

int main(int argc, char **argv)
{
    AppState *s = &app_state;
    memset(s, 0, sizeof(*s));
    s->fifo_fd = -1;

    /* Load config */
    Config cfg;
    int has_config = (config_load(&cfg) == 0 && cfg.valid);

    double center_lat, center_lon, target_lat, target_lon;
    const char *center_name = NULL;
    const char *target_name = NULL;
    const char *shp_override = NULL;
    const char *detail_arg = NULL;

    /* Parse positional args */
    int npos = 0;
    for (int j = 1; j < argc; j++) {
        if (argv[j][0] == '-' && !isdigit((unsigned char)argv[j][1]) && argv[j][1] != '.')
            break;
        npos++;
    }

    int opt_start;
    int cli_center_given = 0;

    if (npos >= 4) {
        center_lat = strtod(argv[1], NULL);
        center_lon = strtod(argv[2], NULL);
        target_lat = strtod(argv[3], NULL);
        target_lon = strtod(argv[4], NULL);
        opt_start = 5;
        cli_center_given = 1;
    } else if (npos >= 2 && has_config) {
        center_lat = cfg.lat;
        center_lon = cfg.lon;
        if (cfg.name[0]) center_name = cfg.name;
        target_lat = strtod(argv[1], NULL);
        target_lon = strtod(argv[2], NULL);
        opt_start = 3;
    } else if (npos == 0 && has_config && cfg.target_valid) {
        center_lat = cfg.lat;
        center_lon = cfg.lon;
        if (cfg.name[0]) center_name = cfg.name;
        target_lat = cfg.target_lat;
        target_lon = cfg.target_lon;
        if (cfg.target_name[0]) target_name = cfg.target_name;
        opt_start = 1;
    } else {
        print_usage(argv[0]);
        return 1;
    }

    /* Parse flags */
    int argi = opt_start;
    while (argi < argc) {
        if (strcmp(argv[argi], "-c") == 0 && argi + 1 < argc)
            center_name = argv[++argi];
        else if (strcmp(argv[argi], "-t") == 0 && argi + 1 < argc)
            target_name = argv[++argi];
        else if (strcmp(argv[argi], "-d") == 0 && argi + 1 < argc)
            detail_arg = argv[++argi];
        else if (strcmp(argv[argi], "-s") == 0 && argi + 1 < argc)
            shp_override = argv[++argi];
        else if (argv[argi][0] != '-' && !shp_override)
            shp_override = argv[argi];
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[argi]);
            print_usage(argv[0]);
            return 1;
        }
        argi++;
    }

    /* Resolve paths */
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        strncpy(exe_path, argv[0], PATH_MAX - 1);
        exe_path[PATH_MAX - 1] = '\0';
    } else {
        exe_path[len] = '\0';
    }

    char default_shp[PATH_MAX], default_border[PATH_MAX], default_land[PATH_MAX];
    resolve_path(exe_path, DEFAULT_SHP_REL, default_shp, sizeof(default_shp));
    resolve_path(exe_path, DEFAULT_BORDER_REL, default_border, sizeof(default_border));
    resolve_path(exe_path, DEFAULT_LAND_REL, default_land, sizeof(default_land));
    resolve_path(exe_path, DEFAULT_SHADER_REL, s->shader_dir, sizeof(s->shader_dir));

    const char *shp_path = shp_override ? shp_override : default_shp;

    /* Save QTH (home) coords before view restore may override center_lat/lon */
    s->qth_lat = center_lat;
    s->qth_lon = center_lon;

    /* Restore saved view center */
    if (!cli_center_given && cfg.view_valid) {
        center_lat = cfg.view_center_lat;
        center_lon = cfg.view_center_lon;
    }
    if (cfg.view_valid && cfg.view_proj_mode == 1)
        projection_set_mode(PROJ_ORTHO);

    projection_set_center(center_lat, center_lon);

    /* Store coordinates */
    s->center_lat = center_lat;
    s->center_lon = center_lon;
    s->target_lat = target_lat;
    s->target_lon = target_lon;
    if (center_name) strncpy(s->center_name, center_name, sizeof(s->center_name) - 1);
    if (target_name) strncpy(s->target_name, target_name, sizeof(s->target_name) - 1);

    /* Project — use QTH for center marker, not panned view center */
    projection_forward(s->qth_lat, s->qth_lon, &s->cx, &s->cy);
    projection_forward(target_lat, target_lon, &s->tx, &s->ty);
    s->dist = projection_distance(s->qth_lat, s->qth_lon, target_lat, target_lon);
    s->az_to = projection_azimuth(s->qth_lat, s->qth_lon, target_lat, target_lon);
    s->az_from = projection_azimuth(target_lat, target_lon, s->qth_lat, s->qth_lon);
    projection_forward(90.0, 0.0, &s->npx, &s->npy);

    build_label(s->center_label, sizeof(s->center_label), center_name, s->qth_lat, s->qth_lon);
    build_label(s->target_label, sizeof(s->target_label), target_name, target_lat, target_lon);

    printf("Center:   %.4f, %.4f\n", center_lat, center_lon);
    printf("Target:   %.4f, %.4f\n", target_lat, target_lon);
    printf("Distance: %.1f km\n", s->dist);

    /* QRZ */
    if (cfg.qrz_user[0] && cfg.qrz_pass[0]) {
        if (qrz_init(cfg.qrz_user, cfg.qrz_pass) == 0)
            s->has_qrz = 1;
    }

    /* Load map data */
    if (map_data_load(&s->map, shp_path) != 0) {
        fprintf(stderr, "Error: failed to load shapefile: %s\n", shp_path);
        return 1;
    }
    s->has_borders = (map_data_load(&s->borders, default_border) == 0);
    s->has_land = (map_data_load(&s->land, default_land) == 0);
    if (s->has_land) map_data_reproject_nosplit(&s->land);

    /* Build grid */
    memset(&s->grid, 0, sizeof(s->grid));
    if (projection_get_mode() == PROJ_ORTHO)
        grid_build_geo(&s->grid);
    else
        grid_build(&s->grid);

    /* Distance circles */
    memset(&s->dist_circles, 0, sizeof(s->dist_circles));
    grid_build_dist_circles(&s->dist_circles, s->qth_lat, s->qth_lon);

    /* Night mesh */
    nightmesh_init(&s->nightmesh);

    /* Overlays */
    muf_data_init(&s->muf_data);
    muf_data_init(&s->spore_data);
    aurora_grid_init(&s->aurora_grid);
    aurora_mesh_init(&s->aurora_mesh);
    drap_grid_init(&s->drap_grid);
    aurora_mesh_init(&s->drap_mesh);
    geomag_init(&s->geomag);
    solar_init(&s->solar);
    scales_init(&s->scales);
    xray_init(&s->xray);

    /* Text system */
    text_init();

    /* Camera */
    camera_init(&s->cam);
    if (cfg.view_valid) {
        s->cam.zoom_km = cfg.view_zoom_km;
        s->cam.pan_x = cfg.view_pan_x;
        s->cam.pan_y = cfg.view_pan_y;
        double max_diam = 2.0 * projection_get_radius();
        if (s->cam.zoom_km > (float)max_diam) s->cam.zoom_km = (float)max_diam;
        if (s->cam.zoom_km < 10.0f) s->cam.zoom_km = 10.0f;
    }

    /* GTK Application */
    s->app = gtk_application_new("com.azmap.gtk", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(s->app, "activate", G_CALLBACK(activate), NULL);
    g_signal_connect(s->app, "shutdown", G_CALLBACK(on_shutdown), NULL);

    int status = g_application_run(G_APPLICATION(s->app), 0, NULL);
    g_object_unref(s->app);

    (void)detail_arg; /* TODO: parse station detail into sidebar labels at startup */

    return status;
}
