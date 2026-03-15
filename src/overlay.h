/* overlay.h — Radio propagation and space weather overlay data.
 *
 * Manages four overlay layers parsed from external data sources:
 * - MUF contour lines (KC2G GeoJSON, frequency-colored polylines)
 * - Sporadic E contours (KC2G stations JSON, IDW interpolation + marching squares)
 * - Aurora heatmap (NOAA OVATION JSON, probability-to-alpha polar mesh)
 * - DRAP absorption (NOAA SWPC text, HAF-to-alpha polar mesh)
 * Plus geomagnetic indices (Kp + Bz) for the sidebar legend.
 * All overlays auto-refresh every 15 minutes via async fetch. */

#ifndef OVERLAY_H
#define OVERLAY_H

#include <time.h>

#define OVERLAY_UPDATE_SEC  900  /* 15 minutes */
#define MUF_URL    "https://prop.kc2g.com/renders/current/mufd-normal-now.geojson"
#define SPORE_URL  "https://prop.kc2g.com/api/stations.json"
#define AURORA_URL "https://services.swpc.noaa.gov/json/ovation_aurora_latest.json"
#define KP_URL     "https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json"
#define BZ_URL     "https://services.swpc.noaa.gov/products/summary/solar-wind-mag-field.json"
#define DRAP_URL   "https://services.swpc.noaa.gov/text/drap_global_frequencies.txt"
#define SOLAR_URL  "https://services.swpc.noaa.gov/text/daily-solar-indices.txt"
#define SFU_URL    "https://services.swpc.noaa.gov/products/summary/10cm-flux.json"
#define SCALES_URL "https://services.swpc.noaa.gov/products/noaa-scales.json"
#define XRAY_URL   "https://services.swpc.noaa.gov/json/goes/primary/xray-flares-latest.json"
#define WIND_URL   "https://services.swpc.noaa.gov/products/summary/solar-wind-speed.json"
#define DISCUSS_URL "https://services.swpc.noaa.gov/text/discussion.txt"

#define MUF_MAX_SEGMENTS 256
#define MUF_MAX_LEGEND   16

/* MUF legend entry: MHz level + color */
typedef struct {
    float mhz;
    float color[4]; /* RGBA */
} MufLegendEntry;

/* MUF contour line data — raw lat/lon + projected vertices with per-segment color */
typedef struct {
    /* Raw lat/lon for reprojection */
    double *raw_lats;
    double *raw_lons;
    int     raw_count;
    int     raw_seg_starts[MUF_MAX_SEGMENTS];
    int     raw_seg_counts[MUF_MAX_SEGMENTS];
    float   raw_seg_colors[MUF_MAX_SEGMENTS][4]; /* RGBA per raw segment */
    int     raw_num_segments;

    /* Projected vertices (after split at large jumps) */
    float  *vertices;              /* x,y pairs in km-space */
    int     vertex_count;
    int     segment_starts[MUF_MAX_SEGMENTS];
    int     segment_counts[MUF_MAX_SEGMENTS];
    float   segment_colors[MUF_MAX_SEGMENTS][4]; /* RGBA per projected segment */
    int     num_segments;

    /* Legend: unique (mhz, color) pairs for sidebar display */
    MufLegendEntry legend[MUF_MAX_LEGEND];
    int            legend_count;
    time_t         ts;             /* source timestamp (spore stations) */
} MufData;

/* Aurora heatmap mesh (same vertex format as NightMesh: x, y, alpha) */
typedef struct {
    float *vertices;    /* interleaved x, y, alpha */
    int    vertex_count;
    int    capacity;
} AuroraMesh;

/* Aurora raw grid (parsed from JSON, used to build mesh) */
typedef struct {
    int    *values;     /* aurora probability 0-100, indexed [lon * 181 + (lat+90)] */
    int     valid;      /* 1 if data loaded successfully */
    time_t  ts;         /* source observation time */
} AuroraGrid;

void  muf_data_init(MufData *m);
void  muf_data_free(MufData *m);
int   muf_parse_geojson(const char *json_str, MufData *m);
void  muf_reproject(MufData *m);

int   spore_parse_json(const char *json_str, MufData *m);

void  aurora_grid_init(AuroraGrid *g);
void  aurora_grid_free(AuroraGrid *g);
int   aurora_parse_json(const char *json_str, AuroraGrid *g);

void  aurora_mesh_init(AuroraMesh *m);
void  aurora_mesh_free(AuroraMesh *m);
void  aurora_mesh_build(AuroraMesh *m, const AuroraGrid *g);

/* DRAP grid (D-Region Absorption Prediction — HAF in MHz) */
#define DRAP_GRID_ROWS 90   /* lat: 89 to -89, step -2 */
#define DRAP_GRID_COLS 90   /* lon: -178 to 178, step 4 */

typedef struct {
    float *values;     /* HAF in MHz, row-major [row * DRAP_GRID_COLS + col] */
    float  peak_mhz;  /* peak HAF value across the grid */
    int    valid;
    time_t ts;         /* source valid-at time */
} DrapGrid;

void  drap_grid_init(DrapGrid *g);
void  drap_grid_free(DrapGrid *g);
int   drap_parse_text(const char *text, DrapGrid *g);
void  drap_mesh_build(AuroraMesh *m, const DrapGrid *g);

/* Geomagnetic indices (Kp + Bz) */
typedef struct {
    float kp;       /* Planetary K-index (0-9) */
    float  bz;       /* IMF Bz component (nT, negative = southward) */
    int    valid;    /* 1 if data loaded */
    time_t ts_kp;    /* source timestamp for Kp */
    time_t ts_bz;    /* source timestamp for Bz */
} GeomagIndices;

void  geomag_init(GeomagIndices *g);
int   geomag_parse_kp(const char *json_str, GeomagIndices *g);
int   geomag_parse_bz(const char *json_str, GeomagIndices *g);

/* Solar indices (SFU + SSN) */
typedef struct {
    int sfu;        /* 10.7 cm radio flux (Solar Flux Units) */
    int    ssn;        /* Sunspot number */
    int    valid;      /* 1 if data loaded */
    time_t ts_sfu;     /* source timestamp for SFU */
    time_t ts_ssn;     /* source timestamp for SSN */
} SolarIndices;

void  solar_init(SolarIndices *s);
int   solar_parse_text(const char *text, SolarIndices *s);
int   sfu_parse_json(const char *json_str, SolarIndices *s);

/* NOAA Space Weather Scales (R/S/G) */
typedef struct {
    int r;          /* Radio Blackout level (0-5) */
    int s;          /* Solar Radiation Storm level (0-5) */
    int    g;          /* Geomagnetic Storm level (0-5) */
    int    valid;
    time_t ts;         /* source timestamp */
} NoaaScales;

void  scales_init(NoaaScales *sc);
int   scales_parse_json(const char *json_str, NoaaScales *sc);

/* Latest X-ray flare class */
typedef struct {
    char   max_class[8];   /* e.g. "M5.0", "X1.2", "C3.4" */
    int    valid;
    time_t ts;             /* source timestamp */
} XrayFlare;

void  xray_init(XrayFlare *xf);
int   xray_parse_json(const char *json_str, XrayFlare *xf);

/* Solar wind speed */
typedef struct {
    int    speed;      /* km/s */
    int    valid;
    time_t ts;         /* source timestamp */
} SolarWind;

void  solarwind_init(SolarWind *w);
int   solarwind_parse_json(const char *json_str, SolarWind *w);

/* Coronal Hole HSS prediction (parsed from SWPC discussion text) */
typedef struct {
    int  active;    /* 1 if CH HSS currently influencing solar wind */
    int    forecast;  /* 1 if CH HSS expected in forecast period */
    int    valid;
    time_t ts;         /* source issued time */
} ChHss;

void  chhss_init(ChHss *h);
int   chhss_parse_discussion(const char *text, ChHss *h);

#endif
