#!/usr/bin/env python3
"""
Random-estimator control analysis and table generation.

This script:
1. Reads the input and analysis settings from estimate_by_single_threshold.py.
2. Assigns random scores to all non-exception neighboring cell pairs.
3. Calls the shared script for thresholding, exact maximum-weight matching,
   classification, and metric calculation.
4. Runs 100 independent randomizations and records the best F1/precision/recall per seed.
5. Exports the analysis results as CSV tables.

Required input files:
    estimate_by_single_threshold.py
    the geometry CSV configured by that shared script

The geometry CSV must contain the configured exception_label column. Rows with
exception_label == 1 are removed before random scores are assigned, before
thresholding/global matching, and before all metrics and exports.

Required packages:
    pip install pandas numpy networkx
"""

from __future__ import annotations

from decimal import Decimal, ROUND_HALF_UP
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence, Set, Tuple

import numpy as np
import pandas as pd


try:
    import estimate_by_single_threshold as single_threshold
except ModuleNotFoundError as exc:
    raise ModuleNotFoundError(
        "Place estimate_by_single_threshold.py in the same folder as "
        "random_estimator.py. The random estimator reuses its thresholding, "
        "global matching, classification, and metric functions."
    ) from exc

import sys
import time

try:
    from tqdm.auto import tqdm
except ImportError:
    tqdm = None


def progress_iter(iterable, total=None, desc="Progress"):
    """
    Progress iterator.

    If tqdm is installed, use a normal terminal progress bar.
    If tqdm is not installed, print simple progress messages.
    """
    if tqdm is not None:
        return tqdm(iterable, total=total, desc=desc, file=sys.stdout, mininterval=0.5)

    class SimpleProgress:
        def __init__(self, iterable, total=None, desc="Progress"):
            self.iterable = iterable
            self.total = total
            self.desc = desc
            self.count = 0
            self.last_print = time.time()

        def __iter__(self):
            print(f"[{self.desc}] started", flush=True)
            for item in self.iterable:
                self.count += 1
                now = time.time()
                if self.total is not None and (self.count == 1 or self.count == self.total or now - self.last_print > 5):
                    print(f"[{self.desc}] {self.count}/{self.total}", flush=True)
                    self.last_print = now
                yield item
            print(f"[{self.desc}] done", flush=True)

    return SimpleProgress(iterable, total=total, desc=desc)


# =============================================================================
# Default configuration
# =============================================================================
# Edit these parameters directly at the beginning of the script.
DATA_CSV = Path(single_threshold.INPUT_CSV)
OUTPUT_DIR = Path(single_threshold.SCRIPT_DIR) / "FigS_random_estimator_outputs"

EXCEPTION_COLUMN = single_threshold.EXCEPTION_COLUMN
EXCEPTION_POSITIVE_VALUE = single_threshold.EXCEPTION_POSITIVE_VALUE

RANDOM_COLUMN = "randomScore"
RANDOM_SCOPE = "pair"  # "pair": same random value for the same file/pair; "row": independent per row

OPERATOR = single_threshold.OPERATOR

THRESHOLD_MIN = 0.05
THRESHOLD_MAX = 0.95
THRESHOLD_STEP_SINGLE = 0.01
THRESHOLD_STEP_100 = 0.01

N_RANDOMIZATIONS = 100
OBSERVED_POSITIVE_VALUE = single_threshold.OBSERVED_POSITIVE_VALUE






# =============================================================================
# Basic utilities
# =============================================================================
def ensure_dirs(*paths: Path) -> None:
    for path in paths:
        path.mkdir(parents=True, exist_ok=True)


def normalize_filename_key(name: Any) -> str:
    if name is None:
        return ""
    text = str(name).strip()
    if not text:
        return ""
    p = Path(text)
    return p.stem if p.suffix else p.name


def edge_key(file_name: Any, first_id: Any, second_id: Any) -> Optional[Tuple[str, int, int]]:
    if pd.isna(file_name) or pd.isna(first_id) or pd.isna(second_id):
        return None
    try:
        a = int(first_id)
        b = int(second_id)
    except (TypeError, ValueError):
        return None
    x, y = (a, b) if a < b else (b, a)
    return normalize_filename_key(file_name), x, y


def require_columns(df: pd.DataFrame, columns: Sequence[str]) -> None:
    missing = [col for col in columns if col not in df.columns]
    if missing:
        raise ValueError(f"Missing required column(s): {missing}. Available columns: {list(df.columns)}")


def load_pairs_excluding_exceptions(
    input_csv: Path,
    exception_column: str = "exception_label",
    exception_positive_value: int | float | str = 1,
) -> Tuple[pd.DataFrame, int, int]:
    """Load and filter rows using the shared estimator's CSV/exception logic."""
    all_rows = single_threshold.read_all_rows(
        input_csv,
        encoding=single_threshold.ENCODING,
    )
    exception_col = single_threshold._resolve_col_name(all_rows, exception_column)
    kept_records = [
        dict(row.data)
        for row in all_rows
        if not single_threshold._get_exception_bool(
            row,
            exception_col=exception_col,
            exception_positive_value=exception_positive_value,
        )
    ]
    excluded_count = len(all_rows) - len(kept_records)
    return pd.DataFrame(kept_records), excluded_count, len(all_rows)


def generate_thresholds(start: float, end: float, step: float) -> List[float]:
    """Inclusive threshold generator without floating-point drift."""
    d_start = Decimal(str(start))
    d_end = Decimal(str(end))
    d_step = Decimal(str(step))
    if d_step <= 0:
        raise ValueError("threshold step must be > 0")

    places = max(-d.as_tuple().exponent for d in (d_start, d_end, d_step))
    quant = Decimal("1") if places == 0 else Decimal("1").scaleb(-places)

    values: List[float] = []
    current = d_start
    while current <= d_end + Decimal("1e-12"):
        values.append(float(current.quantize(quant, rounding=ROUND_HALF_UP)))
        current += d_step
    return values


# =============================================================================
# Random scores and shared single-threshold inference
# =============================================================================
def assign_random_scores(
    source_df: pd.DataFrame,
    random_col: str,
    seed: Optional[int],
    random_scope: str,
) -> pd.DataFrame:
    """Copy a filtered pair table and add one random score in [0, 1)."""
    df = source_df.copy()
    require_columns(df, ["fileName", "firstPolygonId", "secondPolygonId", "observed_division"])

    rng = np.random.default_rng(seed)
    random_scope = random_scope.lower()

    if random_scope == "row":
        df[random_col] = rng.random(len(df))
        return df

    if random_scope != "pair":
        raise ValueError("random_scope must be either 'pair' or 'row'")

    cache: Dict[Tuple[str, int, int], float] = {}
    values: List[float] = []
    for row in df.itertuples(index=False):
        key = edge_key(row.fileName, row.firstPolygonId, row.secondPolygonId)
        if key is None:
            values.append(float(rng.random()))
            continue
        if key not in cache:
            cache[key] = float(rng.random())
        values.append(cache[key])

    df[random_col] = values
    return df


def dataframe_to_full_rows(df: pd.DataFrame) -> List[single_threshold.FullRow]:
    """Adapt a randomized DataFrame to the shared estimator's row format."""
    return [single_threshold.FullRow(dict(record)) for record in df.to_dict(orient="records")]


def infer_one_threshold(
    inference_rows: List[single_threshold.FullRow],
    threshold: float,
    op: str,
    excluded_exception_count: int,
) -> Tuple[Set[int], List[Dict[str, Any]], Dict[str, int], Dict[str, Any]]:
    """Run one threshold entirely through estimate_by_single_threshold.py."""
    selected = single_threshold.select_indices_by_threshold_then_max_weight_matching(
        inference_rows,
        geometry_col=RANDOM_COLUMN,
        threshold=float(threshold),
        op=op,
        exception_col=EXCEPTION_COLUMN,
        exception_positive_value=EXCEPTION_POSITIVE_VALUE,
    )
    classified_rows, skipped = single_threshold.classify_pairs_after_matching(
        inference_rows,
        selected_idx=selected,
        geometry_col=RANDOM_COLUMN,
        threshold=float(threshold),
        op=op,
        observed_positive_value=OBSERVED_POSITIVE_VALUE,
        skip_missing_observed=single_threshold.SKIP_MISSING_OBSERVED,
        exception_col=EXCEPTION_COLUMN,
        exception_positive_value=EXCEPTION_POSITIVE_VALUE,
        skip_exception_in_exports=single_threshold.SKIP_EXCEPTION_IN_EXPORTS,
    )
    metrics = single_threshold.summarize_classified_rows(classified_rows)
    metrics.update({
        "pred_pos": int(metrics["TP"] + metrics["FP"]),
        "pred_neg": int(metrics["TN"] + metrics["FN"]),
        "selected_pairs_raw": len(selected),
        "selected_pairs_evaluated": int(metrics["TP"] + metrics["FP"]),
        "skipped_exception": int(excluded_exception_count),
        "skipped_missing_observed": int(skipped["skipped_missing_observed"]),
    })
    return selected, classified_rows, skipped, metrics


def build_threshold_summary(
    inference_rows: List[single_threshold.FullRow],
    thresholds: Iterable[float],
    op: str,
    excluded_exception_count: int,
    desc: str = "Threshold sweep",
) -> pd.DataFrame:
    rows: List[Dict[str, Any]] = []
    thresholds = list(thresholds)

    for threshold in progress_iter(thresholds, total=len(thresholds), desc=desc):
        _, _, _, metrics = infer_one_threshold(
            inference_rows,
            float(threshold),
            op,
            excluded_exception_count,
        )
        rows.append({
            "feature": RANDOM_COLUMN,
            "operator": op,
            "threshold": float(threshold),
            **metrics,
        })
    return pd.DataFrame(rows)


def run_randomization_analysis(
    base_df: pd.DataFrame,
    excluded_exception_count: int,
    thresholds: Sequence[float],
    n_randomizations: int,
    random_scope: str,
    op: str,
) -> pd.DataFrame:
    """Run N randomizations and collect the best threshold by F1 for each seed."""
    best_rows: List[Dict[str, Any]] = []

    for seed in progress_iter(range(n_randomizations), total=n_randomizations, desc="Running randomizations"):
        df_seed = assign_random_scores(base_df, RANDOM_COLUMN, seed, random_scope)
        inference_rows = dataframe_to_full_rows(df_seed)
        summary_seed = build_threshold_summary(
            inference_rows,
            thresholds,
            op,
            excluded_exception_count,
            desc=f"Seed {seed}: threshold sweep",
        )
        best = summary_seed.sort_values(["f1", "precision", "recall"], ascending=False).iloc[0]

        best_rows.append({
            "seed": seed,
            "inference_engine": "estimate_by_single_threshold.py",
            "best_threshold": float(best["threshold"]),
            "precision": float(best["precision"]),
            "recall": float(best["recall"]),
            "f1": float(best["f1"]),
            "pred_pos": int(best["pred_pos"]),
            "TP": int(best["TP"]),
            "FP": int(best["FP"]),
            "FN": int(best["FN"]),
            "TN": int(best["TN"]),
            "analysis_rows": int(len(base_df)),
            "excluded_exception_rows": int(excluded_exception_count),
            "exception_column": EXCEPTION_COLUMN,
            "exception_positive_value": EXCEPTION_POSITIVE_VALUE,
            "random_scope": random_scope,
            "operator": op,
            "threshold_min": float(min(thresholds)),
            "threshold_max": float(max(thresholds)),
            "threshold_count": int(len(thresholds)),
        })

        if (seed + 1) % 10 == 0:
            print(f"[100-randomization] completed {seed + 1}/{n_randomizations}")

    return pd.DataFrame(best_rows)


def cached_randomizations_match_current_analysis(
    cached: pd.DataFrame,
    analysis_rows: int,
    excluded_exception_count: int,
    random_scope: str,
    op: str,
    thresholds: Sequence[float],
) -> bool:
    """Prevent reuse of tables generated with the former exception workflow."""
    required = {
        "inference_engine",
        "analysis_rows",
        "excluded_exception_rows",
        "exception_column",
        "exception_positive_value",
        "random_scope",
        "operator",
        "threshold_min",
        "threshold_max",
        "threshold_count",
    }
    if cached.empty or not required.issubset(cached.columns) or not thresholds:
        return False

    first = cached.iloc[0]
    try:
        return (
            str(first["inference_engine"]) == "estimate_by_single_threshold.py"
            and int(first["analysis_rows"]) == int(analysis_rows)
            and int(first["excluded_exception_rows"]) == int(excluded_exception_count)
            and str(first["exception_column"]) == str(EXCEPTION_COLUMN)
            and str(first["exception_positive_value"]) == str(EXCEPTION_POSITIVE_VALUE)
            and str(first["random_scope"]) == str(random_scope)
            and str(first["operator"]) == str(op)
            and np.isclose(float(first["threshold_min"]), float(min(thresholds)))
            and np.isclose(float(first["threshold_max"]), float(max(thresholds)))
            and int(first["threshold_count"]) == int(len(thresholds))
        )
    except (TypeError, ValueError):
        return False



# =============================================================================
# Main
# =============================================================================
def main() -> None:
    input_csv = DATA_CSV
    outdir = OUTPUT_DIR
    seed = 0
    n_randomizations = N_RANDOMIZATIONS
    threshold_step_single = THRESHOLD_STEP_SINGLE
    threshold_step_100 = THRESHOLD_STEP_100
    random_scope = RANDOM_SCOPE
    op = OPERATOR

    table_dir = outdir / "tables"
    ensure_dirs(table_dir)

    print("[START] Random-estimator analysis", flush=True)
    print(f"Input data: {input_csv}", flush=True)
    print(
        f"Integrated exception label: {EXCEPTION_COLUMN} "
        f"(value={EXCEPTION_POSITIVE_VALUE})",
        flush=True,
    )
    print(f"Output directory: {outdir}", flush=True)
    print(f"Seed-0 threshold range: {THRESHOLD_MIN:.2f}-{THRESHOLD_MAX:.2f}", flush=True)
    print(f"Seed-0 threshold step: {threshold_step_single}", flush=True)
    print(f"100-run threshold step: {threshold_step_100}", flush=True)
    print(f"Number of randomizations: {n_randomizations}", flush=True)

    base_df, excluded_exception_count, input_row_count = load_pairs_excluding_exceptions(
        input_csv,
        exception_column=EXCEPTION_COLUMN,
        exception_positive_value=EXCEPTION_POSITIVE_VALUE,
    )
    print(
        f"Exception filtering: excluded {excluded_exception_count} of "
        f"{input_row_count} rows before randomization and global matching; "
        f"{len(base_df)} rows remain.",
        flush=True,
    )

    # Single randomization for seed-0 table results.
    print("[STEP] Loading data and assigning seed-0 random scores...", flush=True)
    df_single = assign_random_scores(base_df, RANDOM_COLUMN, seed, random_scope)
    inference_rows_single = dataframe_to_full_rows(df_single)

    thresholds_single = generate_thresholds(THRESHOLD_MIN, THRESHOLD_MAX, threshold_step_single)
    print("[STEP] Running seed-0 threshold sweep...", flush=True)
    summary_single = build_threshold_summary(
        inference_rows_single,
        thresholds_single,
        op,
        excluded_exception_count,
        desc="Seed 0: threshold sweep",
    )

    # Use the best F1 threshold for the seed-0 performance table.
    best_single = summary_single.sort_values(["f1", "precision", "recall"], ascending=False).iloc[0]
    best_threshold = float(best_single["threshold"])
    print(f"[INFO] Best seed-0 threshold by F1 = {best_threshold:.2f}", flush=True)

    selected_a, classified_rows, _, metrics_a = infer_one_threshold(
        inference_rows_single,
        best_threshold,
        op,
        excluded_exception_count,
    )
    metrics_a.update({
        "feature": RANDOM_COLUMN,
        "operator": op,
        "threshold": best_threshold,
    })

    classified = pd.DataFrame(classified_rows).rename(
        columns={"predicted": "estimated_division"}
    )

    # 100 randomizations: skip if already available.
    best100_csv = table_dir / "best_metrics_across_100_randomizations.csv"
    best100_summary_csv = table_dir / "best_metric_summary_100_randomizations.csv"
    thresholds_100 = generate_thresholds(
        THRESHOLD_MIN,
        THRESHOLD_MAX,
        threshold_step_100,
    )

    if best100_csv.exists():
        print(f"[STEP] Found existing 100-randomization results: {best100_csv}", flush=True)
        best_100 = pd.read_csv(best100_csv)
        if "seed" in best_100.columns:
            unique_n = best_100["seed"].nunique()
        else:
            unique_n = len(best_100)
        cache_matches = cached_randomizations_match_current_analysis(
            best_100,
            analysis_rows=len(base_df),
            excluded_exception_count=excluded_exception_count,
            random_scope=random_scope,
            op=op,
            thresholds=thresholds_100,
        )
        if unique_n >= n_randomizations and cache_matches:
            print(f"[SKIP] Reusing existing 100-randomization table ({unique_n} runs found).", flush=True)
        else:
            reason = (
                f"only {unique_n} runs were found"
                if unique_n < n_randomizations
                else "it was generated with different input/exception settings"
            )
            print(
                f"[STEP] Existing table will not be reused because {reason}; "
                "rerunning the full randomization analysis...",
                flush=True,
            )
            best_100 = run_randomization_analysis(
                base_df=base_df,
                excluded_exception_count=excluded_exception_count,
                thresholds=thresholds_100,
                n_randomizations=n_randomizations,
                random_scope=random_scope,
                op=op,
            )
            best_100.to_csv(best100_csv, index=False)
    else:
        print("[STEP] Running 100-randomization analysis...", flush=True)
        best_100 = run_randomization_analysis(
            base_df=base_df,
            excluded_exception_count=excluded_exception_count,
            thresholds=thresholds_100,
            n_randomizations=n_randomizations,
            random_scope=random_scope,
            op=op,
        )
        best_100.to_csv(best100_csv, index=False)

    # Export or refresh tables.
    df_single.to_csv(table_dir / "all_pairs_with_random_score_seed0.csv", index=False)
    threshold_tag = str(best_threshold).replace('.', 'p')
    classified.to_csv(table_dir / f"classified_pairs_seed0_best_threshold_{threshold_tag}.csv", index=False)
    summary_single.to_csv(table_dir / "threshold_performance_summary_seed0.csv", index=False)
    pd.DataFrame([metrics_a]).to_csv(table_dir / f"performance_matrix_seed0_best_threshold_{threshold_tag}.csv", index=False)

    pd.DataFrame([
        {"metric": "Precision", "mean": best_100["precision"].mean() * 100, "sd": best_100["precision"].std(ddof=1) * 100},
        {"metric": "Recall", "mean": best_100["recall"].mean() * 100, "sd": best_100["recall"].std(ddof=1) * 100},
        {"metric": "F1 score", "mean": best_100["f1"].mean() * 100, "sd": best_100["f1"].std(ddof=1) * 100},
    ]).to_csv(best100_summary_csv, index=False)


    print("[DONE] Random-estimator analysis and tables generated.")
    print(f"Seed-0 best-F1 threshold: {best_threshold:.2f}")
    print(
        "Seed-0 metrics: precision={:.3f}, recall={:.3f}, F1={:.3f}".format(
            metrics_a["precision"], metrics_a["recall"], metrics_a["f1"]
        )
    )
    print(
        "100-randomization mean ± SD: precision={:.2f}±{:.2f}%, recall={:.2f}±{:.2f}%, F1={:.2f}±{:.2f}%".format(
            best_100["precision"].mean() * 100,
            best_100["precision"].std(ddof=1) * 100,
            best_100["recall"].mean() * 100,
            best_100["recall"].std(ddof=1) * 100,
            best_100["f1"].mean() * 100,
            best_100["f1"].std(ddof=1) * 100,
        )
    )


if __name__ == "__main__":
    main()
