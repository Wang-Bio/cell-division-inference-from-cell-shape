from __future__ import annotations

import csv
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Set, Tuple


# =========================
# Parameters to edit
# =========================
# Edit only this section when changing the input file, feature column, threshold,
# or output names. No command-line parser/arguments are used.

SCRIPT_DIR = Path(__file__).resolve().parent

INPUT_CSV = SCRIPT_DIR / "batch_neighbor_pair_geometry.csv"
RESULT_DIR = SCRIPT_DIR / "result"

GEOMETRY_COLUMN = "junctionAngleAverageDegrees"  # case-insensitive lookup works
THRESHOLD = 145.0
OPERATOR = ">="
EXCEPTION_COLUMN = "exception_label"  # read directly from INPUT_CSV
EXCEPTION_POSITIVE_VALUE = 1

OUTPUT_PERFORMANCE_CSV = RESULT_DIR / "performance_matrix.csv"
OUTPUT_PERFORMANCE_FIGURE = RESULT_DIR / "performance_matrix.png"
OUTPUT_ALL_PAIRS_CSV = RESULT_DIR / "all_pairs.csv"
OUTPUT_TP_CSV = RESULT_DIR / "true_positive_pairs.csv"
OUTPUT_FP_CSV = RESULT_DIR / "false_positive_pairs.csv"
OUTPUT_FN_CSV = RESULT_DIR / "false_negative_pairs.csv"
OUTPUT_TN_CSV = RESULT_DIR / "true_negative_pairs.csv"

OBSERVED_POSITIVE_VALUE = 1
BY_FILE_PERFORMANCE = True
SKIP_MISSING_OBSERVED = True
SKIP_EXCEPTION_IN_EXPORTS = True
ENCODING = "utf-8"


@dataclass
class FullRow:
    data: Dict[str, Any]


def _to_number_if_possible(s: str) -> Any:
    if s is None:
        return None
    s = s.strip()
    if s == "":
        return ""
    try:
        if "." not in s and "e" not in s.lower():
            return int(s)
    except ValueError:
        pass
    try:
        return float(s)
    except ValueError:
        return s


def read_all_rows(input_csv: str | Path, encoding: str = "utf-8") -> List[FullRow]:
    input_csv = Path(input_csv)
    out: List[FullRow] = []
    with input_csv.open("r", encoding=encoding, newline="") as f:
        reader = csv.DictReader(f)
        if not reader.fieldnames:
            raise ValueError("No header found in CSV.")
        for row in reader:
            out.append(FullRow({k: _to_number_if_possible(v) for k, v in row.items()}))
    return out


def _resolve_col_name(container: List[FullRow], wanted: str) -> str:
    """Resolve column name robustly with case-insensitive lookup."""
    if not container:
        raise ValueError("Empty container.")
    if wanted in container[0].data:
        return wanted
    wanted_l = wanted.lower()
    for k in container[0].data.keys():
        if k.lower() == wanted_l:
            return k
    raise ValueError(
        f"Column '{wanted}' not found. Available columns include: {list(container[0].data.keys())}"
    )


def _ops() -> Dict[str, Callable[[float, float], bool]]:
    return {
        ">": lambda a, b: a > b,
        ">=": lambda a, b: a >= b,
        "<": lambda a, b: a < b,
        "<=": lambda a, b: a <= b,
        "==": lambda a, b: a == b,
        "!=": lambda a, b: a != b,
    }


# =========================
# Exception handling
# =========================
def _get_exception_bool(
    r: FullRow,
    exception_col: str,
    exception_positive_value: int = 1,
) -> bool:
    """Return True when this row is labelled as an exception in the input CSV."""
    raw = r.data.get(exception_col, None)
    if raw in (None, ""):
        return False
    try:
        return float(raw) == float(exception_positive_value)
    except (TypeError, ValueError):
        return str(raw).strip().casefold() == str(exception_positive_value).strip().casefold()


# ==========================================================
# Threshold + maximum-weight matching, per fileName
# Matching weight = margin past threshold:
#   - Above-threshold operators: value - threshold
#   - Below-threshold operators: threshold - value
# ==========================================================
def select_indices_by_threshold_then_max_weight_matching(
    container: List[FullRow],
    geometry_col: str = "junctionAngleAverageDegrees",
    threshold: float = 145.0,
    op: str = ">=",
    exception_col: str = "exception_label",
    exception_positive_value: int = 1,
) -> Set[int]:
    """
    Apply the threshold and exact maximum-weight matching independently per file.

    Exception-labelled rows are removed before the matching graph is built, in
    agreement with the Qt C++ analysis. They therefore cannot occupy a polygon
    vertex or change which non-exception pairs are selected.
    """
    if not container:
        return set()

    try:
        import networkx as nx
    except ImportError as e:
        raise ImportError(
            "This step needs networkx for exact maximum-weight matching.\n"
            "Install it with: pip install networkx"
        ) from e

    ops = _ops()
    if op not in ops:
        raise ValueError(f"Unsupported op '{op}'. Use one of {list(ops.keys())}")
    if op not in (">", ">=", "<", "<="):
        raise ValueError("Margin scoring is defined only for: >, >=, <, <=")

    file_col = _resolve_col_name(container, "fileName")
    a_col = _resolve_col_name(container, "firstPolygonId")
    b_col = _resolve_col_name(container, "secondPolygonId")
    geo_col = _resolve_col_name(container, geometry_col)
    exc_col = _resolve_col_name(container, exception_col)

    per_file_best: Dict[str, Dict[Tuple[int, int], Tuple[float, int]]] = {}
    thr = float(threshold)

    for i, r in enumerate(container):
        # Match the Qt C++ order of operations: discard exceptions first, then
        # apply the threshold and construct the global matching graph.
        if _get_exception_bool(
            r,
            exception_col=exc_col,
            exception_positive_value=exception_positive_value,
        ):
            continue

        fn = r.data.get(file_col, None)
        a = r.data.get(a_col, None)
        b = r.data.get(b_col, None)
        v = r.data.get(geo_col, None)
        if fn in (None, "") or a in (None, "") or b in (None, "") or v in (None, ""):
            continue
        try:
            u = int(a)
            w = int(b)
            val = float(v)
        except (TypeError, ValueError):
            continue
        if u == w:
            continue
        if not ops[op](val, thr):
            continue

        score = (val - thr) if op in (">", ">=") else (thr - val)
        if score < 0:
            continue

        key = (u, w) if u < w else (w, u)
        bucket = per_file_best.setdefault(str(fn), {})
        prev = bucket.get(key)
        if prev is None or score > prev[0]:
            bucket[key] = (score, i)

    selected: Set[int] = set()
    for best_edges in per_file_best.values():
        graph = nx.Graph()
        for (u, v), (score, row_idx) in best_edges.items():
            graph.add_edge(u, v, weight=score, row_idx=row_idx)

        matching = nx.algorithms.matching.max_weight_matching(
            graph, maxcardinality=False, weight="weight"
        )
        for u, v in matching:
            key = (u, v) if u < v else (v, u)
            score_row = best_edges.get(key)
            if score_row is not None:
                selected.add(score_row[1])

    return selected


# =========================
# Post-matching evaluation
# =========================
def _get_observed_bool(
    container: List[FullRow],
    r: FullRow,
    observed_positive_value: int = 1,
) -> Optional[bool]:
    obs_col = _resolve_col_name(container, "observed_division")
    obs_raw = r.data.get(obs_col, None)
    if obs_raw in (None, ""):
        return None
    try:
        return int(obs_raw) == int(observed_positive_value)
    except (TypeError, ValueError):
        return None


def _classification_label(pred: bool, obs: bool) -> str:
    if pred and obs:
        return "TP"
    if pred and not obs:
        return "FP"
    if (not pred) and obs:
        return "FN"
    return "TN"


def classify_pairs_after_matching(
    container: List[FullRow],
    selected_idx: Set[int],
    geometry_col: str,
    threshold: float,
    op: str,
    observed_positive_value: int = 1,
    skip_missing_observed: bool = True,
    exception_col: str = "exception_label",
    exception_positive_value: int = 1,
    skip_exception_in_exports: bool = True,
) -> Tuple[List[Dict[str, Any]], Dict[str, int]]:
    """
    Returns concise row information for every evaluated pair after matching.
    Rows are classified as TP/FP/FN/TN using post-matching predictions only.
    """
    if not container:
        raise ValueError("Container is empty; nothing to classify.")

    file_col = _resolve_col_name(container, "fileName")
    a_col = _resolve_col_name(container, "firstPolygonId")
    b_col = _resolve_col_name(container, "secondPolygonId")
    obs_col = _resolve_col_name(container, "observed_division")
    geo_col = _resolve_col_name(container, geometry_col)
    exc_col = _resolve_col_name(container, exception_col)

    pair_index_col = None
    division_timing_col = None
    for candidate in ("pairIndex", "pair_index"):
        try:
            pair_index_col = _resolve_col_name(container, candidate)
            break
        except ValueError:
            pass
    try:
        division_timing_col = _resolve_col_name(container, "division_timing")
    except ValueError:
        division_timing_col = None

    out_rows: List[Dict[str, Any]] = []
    counters = {"skipped_missing_observed": 0, "skipped_exception": 0}

    for i, r in enumerate(container):
        is_exception = _get_exception_bool(
            r,
            exception_col=exc_col,
            exception_positive_value=exception_positive_value,
        )
        if skip_exception_in_exports and is_exception:
            counters["skipped_exception"] += 1
            continue

        obs = _get_observed_bool(container, r, observed_positive_value)
        if obs is None:
            if skip_missing_observed:
                counters["skipped_missing_observed"] += 1
                continue
            obs = False

        pred = i in selected_idx
        label = _classification_label(pred, obs)

        row = {
            "fileName": r.data.get(file_col, ""),
            "firstPolygonId": r.data.get(a_col, ""),
            "secondPolygonId": r.data.get(b_col, ""),
            "observed_division": r.data.get(obs_col, ""),
            "predicted": int(pred),
            "classification": label,
            geometry_col: r.data.get(geo_col, ""),
            "threshold_op": op,
            "threshold": threshold,
        }
        if pair_index_col is not None:
            row = {"pairIndex": r.data.get(pair_index_col, ""), **row}
        if division_timing_col is not None:
            row["division_timing"] = r.data.get(division_timing_col, "")
        out_rows.append(row)

    return out_rows, counters


def _safe_div(a: float, b: float) -> float:
    return (a / b) if b != 0 else 0.0


def summarize_classified_rows(rows: List[Dict[str, Any]]) -> Dict[str, Any]:
    tp = sum(1 for r in rows if r["classification"] == "TP")
    fp = sum(1 for r in rows if r["classification"] == "FP")
    fn = sum(1 for r in rows if r["classification"] == "FN")
    tn = sum(1 for r in rows if r["classification"] == "TN")

    precision = _safe_div(tp, tp + fp)
    recall = _safe_div(tp, tp + fn)
    specificity = _safe_div(tn, tn + fp)
    accuracy = _safe_div(tp + tn, tp + fp + fn + tn)
    f1 = _safe_div(2 * precision * recall, precision + recall)
    balanced_accuracy = 0.5 * (recall + specificity)

    return {
        "TP": tp,
        "FP": fp,
        "FN": fn,
        "TN": tn,
        "precision": precision,
        "recall": recall,
        "specificity": specificity,
        "f1": f1,
        "accuracy": accuracy,
        "balanced_accuracy": balanced_accuracy,
        "support_pos": tp + fn,
        "support_neg": tn + fp,
        "evaluated_pairs": tp + fp + fn + tn,
    }


def export_performance_matrix_from_classified_rows(
    rows: List[Dict[str, Any]],
    output_csv: str | Path,
    geometry_col: str,
    threshold: float,
    op: str,
    by_file: bool = True,
    skipped_missing_observed: int = 0,
    skipped_exception: int = 0,
    encoding: str = "utf-8",
) -> None:
    output_csv = Path(output_csv)
    output_csv.parent.mkdir(parents=True, exist_ok=True)

    scopes: List[Tuple[str, List[Dict[str, Any]]]] = [("__OVERALL__", rows)]
    if by_file:
        buckets: Dict[str, List[Dict[str, Any]]] = {}
        for row in rows:
            buckets.setdefault(str(row.get("fileName", "")), []).append(row)
        for file_name in sorted(buckets):
            scopes.append((file_name, buckets[file_name]))

    fieldnames = [
        "scope",
        "geometry_col",
        "op",
        "threshold",
        "TP",
        "FP",
        "FN",
        "TN",
        "precision",
        "recall",
        "specificity",
        "f1",
        "accuracy",
        "balanced_accuracy",
        "support_pos",
        "support_neg",
        "evaluated_pairs",
        "skipped_missing_observed",
        "skipped_exception",
    ]

    with output_csv.open("w", encoding=encoding, newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for scope_name, scope_rows in scopes:
            metrics = summarize_classified_rows(scope_rows)
            writer.writerow(
                {
                    "scope": scope_name,
                    "geometry_col": f"{geometry_col} (threshold + MWM)",
                    "op": op,
                    "threshold": threshold,
                    **metrics,
                    # Skip counts are only meaningful for the overall row.
                    "skipped_missing_observed": skipped_missing_observed if scope_name == "__OVERALL__" else "",
                    "skipped_exception": skipped_exception if scope_name == "__OVERALL__" else "",
                }
            )


def export_rows(rows: List[Dict[str, Any]], output_csv: str | Path, encoding: str = "utf-8") -> int:
    output_csv = Path(output_csv)
    output_csv.parent.mkdir(parents=True, exist_ok=True)
    if rows:
        fieldnames = list(rows[0].keys())
    else:
        # Stable header even if there are no rows.
        fieldnames = [
            "pairIndex",
            "fileName",
            "firstPolygonId",
            "secondPolygonId",
            "observed_division",
            "predicted",
            "classification",
            GEOMETRY_COLUMN,
            "threshold_op",
            "threshold",
            "division_timing",
        ]
    with output_csv.open("w", encoding=encoding, newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)
    return len(rows)


def export_all_classification_csvs(
    rows: List[Dict[str, Any]],
    out_all: str | Path,
    out_tp: str | Path,
    out_fp: str | Path,
    out_fn: str | Path,
    out_tn: str | Path,
    encoding: str = "utf-8",
) -> Dict[str, int]:
    counts = {}
    counts["all"] = export_rows(rows, out_all, encoding=encoding)
    for label, path in [("TP", out_tp), ("FP", out_fp), ("FN", out_fn), ("TN", out_tn)]:
        counts[label] = export_rows(
            [r for r in rows if r.get("classification") == label],
            path,
            encoding=encoding,
        )
    return counts



# =========================
# Performance matrix figure
# =========================
def _load_font(size: int):
    """Load a clean sans-serif font similar to the reference figure."""
    try:
        from PIL import ImageFont
    except ImportError as e:
        raise ImportError(
            "The performance matrix figure needs Pillow. Install it with: pip install pillow"
        ) from e

    font_candidates = [
        "arial.ttf",
        "Arial.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
    ]
    for font_path in font_candidates:
        try:
            return ImageFont.truetype(font_path, size=size)
        except OSError:
            continue
    return ImageFont.load_default()


def _draw_centered_text(draw, box, text, font, fill=(0, 0, 0), line_spacing: int = 8):
    """Draw one- or multi-line text centered inside a rectangular box."""
    x0, y0, x1, y1 = box
    lines = str(text).split("\n")
    line_boxes = [draw.textbbox((0, 0), line, font=font) for line in lines]
    line_heights = [b[3] - b[1] for b in line_boxes]
    total_h = sum(line_heights) + line_spacing * (len(lines) - 1)
    y = y0 + ((y1 - y0) - total_h) / 2

    for line, bbox, line_h in zip(lines, line_boxes, line_heights):
        line_w = bbox[2] - bbox[0]
        x = x0 + ((x1 - x0) - line_w) / 2
        draw.text((x, y - bbox[1]), line, font=font, fill=fill)
        y += line_h + line_spacing


def _draw_rotated_centered_text(base_image, box, text, font, fill=(0, 0, 0), angle: int = 90):
    """Draw rotated centered text, used for the vertical Estimated label."""
    from PIL import Image, ImageDraw

    x0, y0, x1, y1 = box
    tmp = Image.new("RGBA", (500, 120), (255, 255, 255, 0))
    tmp_draw = ImageDraw.Draw(tmp)
    bbox = tmp_draw.textbbox((0, 0), text, font=font)
    text_w = bbox[2] - bbox[0]
    text_h = bbox[3] - bbox[1]
    tmp_draw.text(((tmp.width - text_w) / 2, (tmp.height - text_h) / 2 - bbox[1]), text, font=font, fill=fill)
    rotated = tmp.rotate(angle, expand=True, resample=Image.Resampling.BICUBIC)

    px = int(x0 + ((x1 - x0) - rotated.width) / 2)
    py = int(y0 + ((y1 - y0) - rotated.height) / 2)
    base_image.alpha_composite(rotated, (px, py))


def export_performance_matrix_figure(
    metrics: Dict[str, Any],
    output_png: str | Path,
) -> None:
    """
    Export a performance matrix figure matching the reference layout.
    The figure is written to RESULT_DIR/performance_matrix.png by default.
    """
    try:
        from PIL import Image, ImageDraw
    except ImportError as e:
        raise ImportError(
            "The performance matrix figure needs Pillow. Install it with: pip install pillow"
        ) from e

    output_png = Path(output_png)
    output_png.parent.mkdir(parents=True, exist_ok=True)

    # Fixed canvas and geometry to reproduce the provided reference figure.
    W, H = 847, 612
    LINE_W = 5

    # Colors sampled from the reference image.
    BLACK = (0, 0, 0)
    WHITE = (255, 255, 255)
    PEACH = (251, 227, 214)
    RED = (218, 103, 82)
    BLUE = (76, 121, 162)
    TP_FILL = (252, 220, 214)
    FP_FILL = (234, 242, 228)
    FN_FILL = (225, 233, 241)
    TN_FILL = (218, 212, 235)
    GREY = (232, 232, 232)

    img = Image.new("RGBA", (W, H), WHITE + (255,))
    draw = ImageDraw.Draw(img)

    def rect(box, fill):
        draw.rectangle(box, fill=fill, outline=BLACK, width=LINE_W)

    # Layout coordinates copied from the reference arrangement.
    observed_box = (350, 15, 840, 98)
    col_daughter_box = (350, 96, 595, 205)
    col_nondaughter_box = (595, 96, 840, 205)

    estimated_box = (15, 203, 110, 485)
    row_daughter_box = (110, 203, 350, 345)
    row_nondaughter_box = (110, 345, 350, 485)

    tp_box = (350, 203, 595, 345)
    fp_box = (595, 203, 840, 345)
    fn_box = (350, 345, 595, 485)
    tn_box = (595, 345, 840, 485)

    precision_box = (15, 518, 290, 600)
    recall_box = (290, 518, 565, 600)
    f1_box = (565, 518, 840, 600)

    for box, fill in [
        (observed_box, PEACH),
        (col_daughter_box, RED),
        (col_nondaughter_box, BLUE),
        (estimated_box, PEACH),
        (row_daughter_box, RED),
        (row_nondaughter_box, BLUE),
        (tp_box, TP_FILL),
        (fp_box, FP_FILL),
        (fn_box, FN_FILL),
        (tn_box, TN_FILL),
        (precision_box, GREY),
        (recall_box, GREY),
        (f1_box, GREY),
    ]:
        rect(box, fill)

    # Fonts chosen to match the reference figure.
    font_main = _load_font(31)
    font_header = _load_font(31)
    font_metric = _load_font(31)
    font_vertical = _load_font(31)

    tp = int(metrics.get("TP", 0))
    fp = int(metrics.get("FP", 0))
    fn = int(metrics.get("FN", 0))
    tn = int(metrics.get("TN", 0))
    precision = float(metrics.get("precision", 0.0)) * 100.0
    recall = float(metrics.get("recall", 0.0)) * 100.0
    f1 = float(metrics.get("f1", 0.0)) * 100.0

    _draw_centered_text(draw, observed_box, "Observed", font_header, BLACK, line_spacing=8)
    _draw_centered_text(draw, col_daughter_box, "Daughter Pair", font_header, WHITE, line_spacing=8)
    _draw_centered_text(draw, col_nondaughter_box, "Non-Daughter\nPair", font_header, WHITE, line_spacing=6)

    _draw_rotated_centered_text(img, estimated_box, "Estimated", font_vertical, BLACK, angle=90)
    _draw_centered_text(draw, row_daughter_box, "Daughter Pair", font_header, WHITE, line_spacing=8)
    _draw_centered_text(draw, row_nondaughter_box, "Non-Daughter\nPair", font_header, WHITE, line_spacing=6)

    _draw_centered_text(draw, tp_box, f"True Positive\n(TP): {tp}", font_main, BLACK, line_spacing=8)
    _draw_centered_text(draw, fp_box, f"False Positive\n(FP): {fp}", font_main, BLACK, line_spacing=8)
    _draw_centered_text(draw, fn_box, f"False Negative\n(FN): {fn}", font_main, BLACK, line_spacing=8)
    _draw_centered_text(draw, tn_box, f"True Negative\n(TN): {tn}", font_main, BLACK, line_spacing=8)

    _draw_centered_text(draw, precision_box, f"Precision: {precision:.1f}%", font_metric, BLACK, line_spacing=8)
    _draw_centered_text(draw, recall_box, f"Recall: {recall:.1f}%", font_metric, BLACK, line_spacing=8)
    _draw_centered_text(draw, f1_box, f"F1 score: {f1:.1f}%", font_metric, BLACK, line_spacing=8)

    img.convert("RGB").save(output_png, quality=100)

def main() -> None:
    """
    Run the whole analysis using only the parameters defined at the top of
    this script. All output CSV files are written into RESULT_DIR.
    """
    all_data = read_all_rows(INPUT_CSV, encoding=ENCODING)

    selected_idx = select_indices_by_threshold_then_max_weight_matching(
        all_data,
        geometry_col=GEOMETRY_COLUMN,
        threshold=THRESHOLD,
        op=OPERATOR,
        exception_col=EXCEPTION_COLUMN,
        exception_positive_value=EXCEPTION_POSITIVE_VALUE,
    )

    classified_rows, skipped = classify_pairs_after_matching(
        all_data,
        selected_idx=selected_idx,
        geometry_col=GEOMETRY_COLUMN,
        threshold=THRESHOLD,
        op=OPERATOR,
        observed_positive_value=OBSERVED_POSITIVE_VALUE,
        skip_missing_observed=SKIP_MISSING_OBSERVED,
        exception_col=EXCEPTION_COLUMN,
        exception_positive_value=EXCEPTION_POSITIVE_VALUE,
        skip_exception_in_exports=SKIP_EXCEPTION_IN_EXPORTS,
    )

    export_performance_matrix_from_classified_rows(
        classified_rows,
        OUTPUT_PERFORMANCE_CSV,
        geometry_col=GEOMETRY_COLUMN,
        threshold=THRESHOLD,
        op=OPERATOR,
        by_file=BY_FILE_PERFORMANCE,
        skipped_missing_observed=skipped["skipped_missing_observed"],
        skipped_exception=skipped["skipped_exception"],
        encoding=ENCODING,
    )

    counts = export_all_classification_csvs(
        classified_rows,
        out_all=OUTPUT_ALL_PAIRS_CSV,
        out_tp=OUTPUT_TP_CSV,
        out_fp=OUTPUT_FP_CSV,
        out_fn=OUTPUT_FN_CSV,
        out_tn=OUTPUT_TN_CSV,
        encoding=ENCODING,
    )

    metrics = summarize_classified_rows(classified_rows)

    export_performance_matrix_figure(
        metrics,
        OUTPUT_PERFORMANCE_FIGURE,
    )

    print("\n[SINGLE THRESHOLD INFERENCE]")
    print(f"  input CSV         = {INPUT_CSV}")
    print(
        f"  exception label   = {EXCEPTION_COLUMN} "
        f"(value={EXCEPTION_POSITIVE_VALUE})"
    )
    print(f"  result folder     = {RESULT_DIR}")
    print(f"  geometry column   = {GEOMETRY_COLUMN}")
    print(f"  threshold         = {OPERATOR} {THRESHOLD}")
    print(f"  selected pairs    = {len(selected_idx)}")
    print(f"  evaluated pairs   = {metrics['evaluated_pairs']}")
    print(f"  TP={metrics['TP']}  FP={metrics['FP']}  FN={metrics['FN']}  TN={metrics['TN']}")
    print(
        f"  precision={metrics['precision']:.6f}  "
        f"recall={metrics['recall']:.6f}  "
        f"f1={metrics['f1']:.6f}"
    )
    print(
        f"  skipped_missing_observed={skipped['skipped_missing_observed']}  "
        f"skipped_exception={skipped['skipped_exception']}"
    )
    print("\n[OUTPUTS]")
    print(f"  performance CSV:    {OUTPUT_PERFORMANCE_CSV}")
    print(f"  performance figure: {OUTPUT_PERFORMANCE_FIGURE}")
    print(f"  all pairs:   {OUTPUT_ALL_PAIRS_CSV} ({counts['all']} rows)")
    print(f"  TP pairs:    {OUTPUT_TP_CSV} ({counts['TP']} rows)")
    print(f"  FP pairs:    {OUTPUT_FP_CSV} ({counts['FP']} rows)")
    print(f"  FN pairs:    {OUTPUT_FN_CSV} ({counts['FN']} rows)")
    print(f"  TN pairs:    {OUTPUT_TN_CSV} ({counts['TN']} rows)")


if __name__ == "__main__":
    main()
