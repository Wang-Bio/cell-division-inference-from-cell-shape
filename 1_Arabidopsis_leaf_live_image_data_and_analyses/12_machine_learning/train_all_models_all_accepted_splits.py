#!/usr/bin/env python3
"""
Train and score logistic regression, linear SVM, XGBoost, and LightGBM across
all accepted primordium-grouped train/test splits, using either all 49 geometric
features or the four-feature panel described in the manuscript.

This revision is matched to the supplied raw dataset and split table:
- 7,624 raw neighbour-pair rows;
- 51 integrated exception rows removed before every analysis;
- 7,573 analyzable rows (1,114 daughter and 6,459 non-daughter pairs);
- 12 primordia inferred from the fileName prefix;
- 75 accepted partitions containing 68-72% of rows in training and 28-32% in
  testing while keeping each primordium intact. The number of primordia is not
  fixed at 8/4: accepted partitions contain 6-9 training and 3-6 test primordia.

How to use
----------
1. Put these two inputs beside the script (or edit their paths below):
   batch_neighbor_pair_geometry.csv and
   all_satisfying_primordia_splits.csv.
2. Install the required packages:

       python3 -m pip install numpy pandas "scikit-learn>=1.1" networkx xgboost lightgbm joblib tqdm

3. Edit the USER SETTINGS block near the beginning of this file if needed.
4. Run directly:

       python3 train_all_models_all_accepted_splits.py

No command-line parameters are needed.

For the 75-split analysis, the script creates one global queue containing every
(split, model, feature-mode) job. With four models and two feature sets this gives
600 independent jobs, of which 36 run concurrently by default. With the default
restricted grids, those jobs evaluate 2,400 training-only hyperparameter candidates
in total. Each job is kept single-threaded to prevent oversubscription. The global
progress bar reports ETA, and each job writes a separate log inside its model output
directory.

With RUN_ALL_ACCEPTED_SPLITS=True, outputs are written to:
  MULTISPLIT_OUT_DIR/<split_id>/models/<feature_mode>/<model_name>/

The cross-split summaries and parallel job status are written directly under
MULTISPLIT_OUT_DIR.

Main output files in each model folder:
  training_summary.json
  train_scores.csv
  test_scores.csv
  threshold_sweep_matching_train_cv.csv
  matched_edges_train_fit.csv
  matched_edges_test.csv
  matching_summary_train_fit.csv
  matching_summary_test.csv
  model_pipeline.joblib

Scientific workflow
-------------------
- Rows with exception_label == 1 are removed before splitting, training, threshold
  selection, matching, and performance calculation. No separate exception.csv is used.
- Tracked primordia are inferred directly from fileName prefixes (for example,
  sample4_36h.json -> sample4); every snapshot from one primordium remains entirely
  in either the training or held-out test set.
- The accepted split definitions are read from all_satisfying_primordia_splits(1).csv,
  whose train_set and test_set cells contain semicolon-separated primordium prefixes.
- The outer evaluation is repeated primordium-grouped holdout, not conventional
  K-fold cross-validation and not an independent external cohort. For each accepted
  partition, the test observations remain unused during preprocessing, fitting,
  hyperparameter selection, probability calibration, and threshold selection.
- Hyperparameters and probability thresholds are selected only from inner
  primordium-grouped cross-validation of the outer training partition. The selected
  configuration is refitted on the full outer training partition and reused unchanged
  on held-out test primordia.
- Threshold-positive edges are filtered by exact maximum-weight matching within each
  snapshot so that each cell belongs to at most one inferred daughter pair.
- A deliberately small, prespecified hyperparameter grid is used. This adds a useful
  sensitivity analysis without turning the 12-primordium dataset into a large,
  overfit model-search exercise.
- Input hashes, script hash, Python/package versions, validation settings, exact
  candidate grids, seed, and methodological references are written to
  analysis_configuration.json. A concise reviewer_methods_summary.txt is also written.

Methodological basis
--------------------
- Varma & Simon (2006), doi:10.1186/1471-2105-7-91: tuning must be nested inside
  an independent outer evaluation.
- Roberts et al. (2017), doi:10.1111/ecog.02881: dependence/hierarchical structure
  should be respected by blocked or group-aware validation.
- Saeb et al. (2017), doi:10.1093/gigascience/gix019: repeated observations from
  the same biological subject/sample should not cross validation boundaries.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import itertools
import json
import os
import platform
import re
from importlib import metadata as importlib_metadata
from contextlib import redirect_stderr, redirect_stdout
# Keep every individual model/candidate single-threaded. This must be set before
# numpy/scikit-learn/xgboost/lightgbm import native math libraries.
os.environ.setdefault("OMP_NUM_THREADS", "1")
os.environ.setdefault("OPENBLAS_NUM_THREADS", "1")
os.environ.setdefault("MKL_NUM_THREADS", "1")
os.environ.setdefault("NUMEXPR_NUM_THREADS", "1")
os.environ.setdefault("VECLIB_MAXIMUM_THREADS", "1")
import sys
import time
import warnings
import multiprocessing as mp
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path
from types import SimpleNamespace
from typing import Any

import numpy as np
import pandas as pd


from sklearn.impute import SimpleImputer
from sklearn.linear_model import LogisticRegression
from sklearn.metrics import average_precision_score, confusion_matrix, roc_auc_score
from sklearn.model_selection import StratifiedGroupKFold, StratifiedKFold
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler
from sklearn.svm import LinearSVC
from sklearn.calibration import CalibratedClassifierCV
from sklearn.base import clone

try:
    import joblib
except Exception:  # pragma: no cover
    joblib = None

try:
    from tqdm.auto import tqdm as _tqdm
except Exception:  # pragma: no cover
    _tqdm = None

try:
    import networkx as nx
except Exception as e:  # pragma: no cover
    nx = None
    NETWORKX_IMPORT_ERROR = e
else:
    NETWORKX_IMPORT_ERROR = None


# =========================
# Column conventions
# =========================
FILE_COL = "fileName"
ID1_COL = "firstPolygonId"
ID2_COL = "secondPolygonId"
LABEL_COL_DEFAULT = "observed_division"
EXCEPTION_COL_DEFAULT = "exception_label"
EXCEPTION_POSITIVE_VALUE_DEFAULT = 1
EXPECTED_ALL_FEATURE_COUNT = 49

# Exact signature of the supplied raw dataset after integrated exception filtering.
# Set an item to None only when intentionally applying the script to a different dataset.
EXPECTED_RAW_ROW_COUNT = 7624
EXPECTED_EXCEPTION_ROW_COUNT = 51
EXPECTED_ANALYZABLE_ROW_COUNT = 7573
EXPECTED_DAUGHTER_PAIR_COUNT = 1114
EXPECTED_NON_DAUGHTER_PAIR_COUNT = 6459
EXPECTED_PRIMORDIA_COUNT = 12

# Keep for traceability, but NEVER use for training.
EXCLUDE_ALWAYS = [
    "fileName",
    "pairIndex",
    "firstPolygonId",
    "secondPolygonId",
    "division_timing",
    "exception_label",
]

SELECTED_4_FEATURES = [
    "junctionAngleAverageDegrees",
    "normalizedSharedEdgeLength",
    "unionConvexDeficiency",
    "unionCircularity",
]

MODEL_ALIASES = {
    "lr": "lr",
    "logreg": "lr",
    "logistic": "lr",
    "logistic_regression": "lr",
    "svm": "svm",
    "linear_svm": "svm",
    "xgb": "xgboost",
    "xgboost": "xgboost",
    "lgbm": "lightgbm",
    "lightgbm": "lightgbm",
}

FEATURE_MODE_ALIASES = {
    "all": "all",
    "all_features": "all",
    "4": "selected4",
    "4feature": "selected4",
    "4features": "selected4",
    "selected4": "selected4",
    "four": "selected4",
}

METHODOLOGICAL_REFERENCES = [
    {
        "citation": "Varma S, Simon R. BMC Bioinformatics 7, 91 (2006)",
        "doi": "10.1186/1471-2105-7-91",
        "supports": "nested separation of model selection from outer performance evaluation",
    },
    {
        "citation": "Roberts DR et al. Ecography 40, 913-929 (2017)",
        "doi": "10.1111/ecog.02881",
        "supports": "blocked/group-aware validation for hierarchically dependent observations",
    },
    {
        "citation": "Saeb S et al. GigaScience 6(5), 1-9 (2017)",
        "doi": "10.1093/gigascience/gix019",
        "supports": "sample-aware rather than record-wise validation for repeated observations",
    },
    {
        "citation": "Chen T, Guestrin C. KDD (2016)",
        "doi": "10.1145/2939672.2939785",
        "supports": "regularized tree boosting, shrinkage, and row/column subsampling in XGBoost",
    },
    {
        "citation": "Ke G et al. NeurIPS 30 (2017)",
        "url": "https://papers.nips.cc/paper/6907-lightgbm-a-highly-efficient-gradient-boosting-decision-tree",
        "supports": "LightGBM gradient-boosting implementation",
    },
]


# =============================================================================
# USER SETTINGS — EDIT THIS BLOCK ONLY
# =============================================================================
# Put this script in the same folder as your CSV files, or edit these paths.
TRAIN_CSV = "neighbor_pairs_train.csv"
TEST_CSV = "neighbor_pairs_test.csv"
OUT_DIR = "output/combined_all_models_grouped_tuned_parallel_36cores"

# Column containing the ground-truth label.
LABEL_COL = LABEL_COL_DEFAULT

# Integrated exception label. Rows equal to EXCEPTION_POSITIVE_VALUE are excluded
# before every scientific calculation; the column is also blocked from training.
EXCEPTION_COL = EXCEPTION_COL_DEFAULT
EXCEPTION_POSITIVE_VALUE = EXCEPTION_POSITIVE_VALUE_DEFAULT

# Models to train. Available names: "lr", "svm", "xgboost", "lightgbm".
MODELS_TO_RUN = ["lr", "svm", "xgboost", "lightgbm"]

# Feature modes to run. Available names: "all", "selected4".
FEATURE_MODES_TO_RUN = ["all", "selected4"]

# Reproducibility / inner cross-validation.
SEED = 42
# The accepted outer partitions contain 6-9 training primordia. Three grouped
# inner folds provide a modest training-only model-selection procedure without
# assuming a fixed number of primordia per fold; unequal primordium sizes mean
# the fold group counts can vary. No primordium crosses an inner fold boundary.
# This inner CV is unrelated to the outer 70/30 row fraction.
CV_FOLDS = 3
GROUP_AWARE_INNER_CV = True
N_JOBS = 1  # keep this at 1 when parallelizing by hyperparameter candidate
CONTINUE_ON_ERROR = False

# Threshold search after matching.
# Using 201 inclusive points gives the transparent grid 0.000, 0.005, ..., 1.000.
# Threshold is selected from cross-validated TRAIN probabilities only, not from the test set.
BETA = 1.0
N_THRESHOLDS = 201
THR_LOW = 0.00
THR_HIGH = 1.00

# Output options.
SAVE_MODEL = True
MAKE_PLOTS = False  # set True if you also want metrics_vs_threshold_train_cv.png

# Progress display.
# These show estimated time left for the whole run, for CV folds inside each
# hyperparameter candidate, and for the threshold sweep inside each candidate.
SHOW_PROGRESS_BARS = True
SHOW_WHOLE_PROCESS_PROGRESS = True
SHOW_CV_FOLD_PROGRESS = True
SHOW_THRESHOLD_PROGRESS = True
PROGRESS_MININTERVAL_SECONDS = 1.0

# Candidate-level parallelization for the optional single-split workflow.
# The 75-split workflow instead uses the global scheduler configured in the
# MULTI-SPLIT SETTINGS block below.
PARALLEL_HYPERPARAMETER_CANDIDATES = True

# For RUN_ALL_ACCEPTED_SPLITS=False only: make one queue containing candidates
# from every feature mode/model in that single train/test split. The multi-split
# workflow ignores this setting and uses MAX_TOTAL_PARALLEL_JOBS below.
GLOBAL_PARALLEL_ACROSS_ALL_COMBINATIONS = True

MAX_PARALLEL_CANDIDATES = 36
N_JOBS_PER_CANDIDATE = 1
PARALLEL_START_METHOD = "fork"  # best on Linux/Ubuntu; use "spawn" if fork causes issues
SHOW_PER_CANDIDATE_START_FINISH = True

# Class weighting.
CLASS_WEIGHT_BALANCED = True

# Hyperparameter sensitivity search.
# True = test parameter combinations below by CV on the training set only, then
# choose the best hyperparameter + threshold by matching-aware CV F1.
# False = use the single-value parameters below only.
# Keep True for the revised manuscript analysis. The outer held-out primordia are
# never used for candidate or threshold selection.
HYPERPARAMETER_SEARCH = True

# LR / SVM settings.
# If HYPERPARAMETER_SEARCH is True, LR_C_LIST and SVM_C_LIST are used.
# If False, LR_C and SVM_C are used.
LR_C = 1.0
LR_C_LIST = [0.1, 1.0, 10.0]
LR_MAX_ITER = 5000

SVM_C = 1.0
SVM_C_LIST = [0.1, 1.0, 10.0]
SVM_MAX_ITER = 20000
SVM_CALIB_METHOD = "sigmoid"  # "sigmoid" or "isotonic" when HYPERPARAMETER_SEARCH=False
SVM_CALIB_METHOD_LIST = ["sigmoid"]
SVM_CALIB_CV = 3

# Tree-model settings.
TREE_USE_IMPUTER = False

# XGBoost settings.
# Single-value parameters are used when HYPERPARAMETER_SEARCH=False.
# *_LIST parameters are used when HYPERPARAMETER_SEARCH=True.
XGB_N_ESTIMATORS = 600
XGB_LEARNING_RATE = 0.05
XGB_MAX_DEPTH = 4
XGB_MAX_DEPTH_LIST = [2, 4]
XGB_MIN_CHILD_WEIGHT = 1.0
XGB_MIN_CHILD_WEIGHT_LIST = [1.0, 5.0]
XGB_SUBSAMPLE = 0.8
XGB_SUBSAMPLE_LIST = [0.8]
XGB_COLSAMPLE_BYTREE = 0.8
XGB_COLSAMPLE_BYTREE_LIST = [0.8]
XGB_REG_LAMBDA = 1.0
XGB_REG_LAMBDA_LIST = [1.0]
XGB_REG_ALPHA = 0.0
XGB_REG_ALPHA_LIST = [0.0]
XGB_GAMMA = 0.0
XGB_GAMMA_LIST = [0.0]
XGB_TREE_METHOD = "hist"

# LightGBM settings.
# Single-value parameters are used when HYPERPARAMETER_SEARCH=False.
# *_LIST parameters are used when HYPERPARAMETER_SEARCH=True.
LGBM_N_ESTIMATORS = 1500
LGBM_LEARNING_RATE = 0.02
LGBM_NUM_LEAVES = 63
LGBM_NUM_LEAVES_LIST = [15, 31, 63]
LGBM_MAX_DEPTH = -1
LGBM_MAX_DEPTH_LIST = [-1]
LGBM_MIN_CHILD_SAMPLES = 20
LGBM_MIN_CHILD_SAMPLES_LIST = [20, 50]
LGBM_SUBSAMPLE = 0.8
LGBM_SUBSAMPLE_LIST = [0.8]
# LightGBM ignores subsample < 1 when subsample_freq is 0 (its default).
# Setting this to 1 activates row subsampling at every boosting iteration.
LGBM_SUBSAMPLE_FREQ = 1
LGBM_COLSAMPLE_BYTREE = 0.8
LGBM_COLSAMPLE_BYTREE_LIST = [0.8]
LGBM_REG_LAMBDA = 1.0
LGBM_REG_LAMBDA_LIST = [1.0]
LGBM_REG_ALPHA = 0.0
LGBM_REG_ALPHA_LIST = [0.0]
# =============================================================================
# END OF USER SETTINGS
# =============================================================================

# =============================================================================
# MULTI-SPLIT SETTINGS — for evaluating all accepted group-aware splits
# =============================================================================
# True = ignore TRAIN_CSV/TEST_CSV above and instead:
#   1) read ALL_DATA_CSV,
#   2) read ACCEPTED_SPLITS_CSV,
#   3) generate train/test CSVs for every accepted split,
#   4) train all requested model/feature-mode combinations on every split,
#   5) summarize performance across splits as mean ± SD.
RUN_ALL_ACCEPTED_SPLITS = True

# Full unsplit dataset containing all neighbor-pair rows.
ALL_DATA_CSV = "batch_neighbor_pair_geometry.csv"

# Accepted prefix-defined primordium splits. Required columns are split_id,
# train_set, and test_set; group names are separated by semicolons.
ACCEPTED_SPLITS_CSV = "all_satisfying_primordia_splits.csv"

# The supplied split table contains 75 qualifying partitions. This guard prevents
# a truncated or stale split table from silently changing the manuscript analysis.
EXPECTED_ACCEPTED_SPLIT_COUNT = 75

# Prespecified acceptance rules used to generate the supplied split table.
TARGET_TRAIN_FRACTION = 0.70
TRAIN_FRACTION_TOLERANCE = 0.02
MAX_DAUGHTER_FREQUENCY_DIFFERENCE = 0.01

# Root output folder for all split runs.
MULTISPLIT_OUT_DIR = "output/all_satisfying_primordia_splits_grouped_tuned"

# True = flatten all split/model/feature-mode combinations into one process pool.
# With the supplied data this creates 75 x 4 x 2 = 600 independent jobs and lets
# the workstation use up to 36 CPU cores throughout the analysis.
GLOBAL_PARALLEL_ACROSS_ALL_SPLIT_MODEL_FEATURE_JOBS = True
MAX_TOTAL_PARALLEL_JOBS = 36

# Each global job is deliberately single-threaded. Do not increase this while
# MAX_TOTAL_PARALLEL_JOBS is 36, or XGBoost/LightGBM can oversubscribe the CPU.
N_JOBS_PER_GLOBAL_JOB = 1

# Worker stdout/stderr is written to worker.log in each model output folder so
# output from 36 simultaneous processes does not become interleaved in Terminal.
SAVE_GLOBAL_WORKER_LOGS = True

# For debugging, set this to a small integer such as 2.
# For formal analysis, keep None to run all accepted splits.
MAX_SPLITS_TO_RUN = None

# True performs only the raw-data, exception, split, hash, version, and
# reproducibility audits, then exits before model fitting. Keep False for the
# formal manuscript run. This is useful for a quick installation/data check.
PREFLIGHT_ONLY = False

# Generated train/test CSVs are retained under MULTISPLIT_OUT_DIR/split_XXX/
# for a complete audit trail of every reported split.
# =============================================================================
# END MULTI-SPLIT SETTINGS
# =============================================================================



@dataclass
class TrainResult:
    model_name: str
    feature_mode: str
    out_dir: str
    pipeline: Pipeline | None
    feature_cols: list[str]
    chosen_threshold: float
    summary_json: str


# =========================
# General utilities
# =========================
def installed_distribution_version(distribution_name: str) -> str | None:
    """Return an installed package version without importing optional packages."""
    try:
        return importlib_metadata.version(distribution_name)
    except importlib_metadata.PackageNotFoundError:
        return None


def collect_software_versions() -> dict[str, Any]:
    """Capture the exact runtime environment used for a formal analysis run."""
    distributions = {
        "numpy": "numpy",
        "pandas": "pandas",
        "scipy": "scipy",
        "scikit_learn": "scikit-learn",
        "networkx": "networkx",
        "xgboost": "xgboost",
        "lightgbm": "lightgbm",
        "joblib": "joblib",
        "tqdm": "tqdm",
        "matplotlib": "matplotlib",
    }
    return {
        "python": platform.python_version(),
        "python_implementation": platform.python_implementation(),
        "platform": platform.platform(),
        "packages": {
            key: installed_distribution_version(distribution)
            for key, distribution in distributions.items()
        },
    }


def collect_script_provenance() -> dict[str, Any]:
    """Record the executed script name and content hash for auditability."""
    script_path = Path(__file__).resolve()
    try:
        sha256 = hashlib.sha256(script_path.read_bytes()).hexdigest()
    except OSError:
        sha256 = None
    return {
        "script_name": script_path.name,
        "sha256": sha256,
    }


def sha256_file(path: str | Path) -> str:
    """Return a streaming SHA-256 digest for an input or output file."""
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def resolve_input_path(path_value: str) -> str:
    """Resolve an input from the working directory or the script directory."""
    path = Path(path_value).expanduser()
    if path.is_absolute():
        return str(path)
    candidates = [Path.cwd() / path, Path(__file__).resolve().parent / path]
    for candidate in candidates:
        if candidate.exists():
            return str(candidate.resolve())
    # Return the working-directory candidate so the eventual error is explicit.
    return str(candidates[0].resolve())


def _require_expected_count(name: str, observed: int, expected: int | None) -> None:
    """Fail fast if the supplied raw dataset no longer matches the audited version."""
    if expected is not None and int(observed) != int(expected):
        raise ValueError(
            f"Raw-data signature mismatch for {name}: expected {expected}, observed {observed}. "
            "Confirm that ALL_DATA_CSV and ACCEPTED_SPLITS_CSV are the manuscript versions, "
            "or intentionally update the EXPECTED_* constants and regenerate the split table."
        )


def validate_raw_dataset_signature(
    full_df_raw: pd.DataFrame,
    full_df: pd.DataFrame,
    primordia: list[str],
    label_col: str,
    n_exceptions_removed: int,
) -> dict[str, Any]:
    """Validate and return the exact data counts used by the revised analysis."""
    n_daughter = int(full_df[label_col].astype(int).sum())
    n_non_daughter = int(len(full_df) - n_daughter)
    observed = {
        "raw_rows": int(len(full_df_raw)),
        "exception_rows_removed": int(n_exceptions_removed),
        "analyzable_rows": int(len(full_df)),
        "daughter_pairs": n_daughter,
        "non_daughter_pairs": n_non_daughter,
        "primordia": int(len(primordia)),
    }
    expected = {
        "raw_rows": EXPECTED_RAW_ROW_COUNT,
        "exception_rows_removed": EXPECTED_EXCEPTION_ROW_COUNT,
        "analyzable_rows": EXPECTED_ANALYZABLE_ROW_COUNT,
        "daughter_pairs": EXPECTED_DAUGHTER_PAIR_COUNT,
        "non_daughter_pairs": EXPECTED_NON_DAUGHTER_PAIR_COUNT,
        "primordia": EXPECTED_PRIMORDIA_COUNT,
    }
    for key, observed_value in observed.items():
        _require_expected_count(key, observed_value, expected[key])
    return {"observed": observed, "expected": expected, "validated": True}


def _truthy_split_flag(value: Any) -> bool:
    """Parse the optional satisfies_conditions column without string truthiness."""
    if isinstance(value, (bool, np.bool_)):
        return bool(value)
    if isinstance(value, (int, np.integer, float, np.floating)) and not pd.isna(value):
        return bool(int(value))
    text_value = str(value).strip().lower()
    if text_value in {"true", "1", "yes", "y"}:
        return True
    if text_value in {"false", "0", "no", "n", "", "nan"}:
        return False
    raise ValueError(f"Cannot interpret satisfies_conditions value: {value!r}")


def validate_and_summarize_split_table(
    split_table: pd.DataFrame,
    full_df_with_groups: pd.DataFrame,
    label_col: str,
) -> tuple[pd.DataFrame, dict[str, Any]]:
    """Recalculate every accepted 70/30 partition directly from the raw data.

    This is a preflight audit. It confirms that the supplied split table partitions
    all 12 primordia without overlap, satisfies the row-fraction and label-frequency
    criteria, and agrees with its recorded counts. It does not fit any model.
    """
    all_groups = set(full_df_with_groups["_split_group"].astype(str).unique())
    records: list[dict[str, Any]] = []

    for split_index, split_row in split_table.iterrows():
        split_id = str(split_row.get("split_id", f"split_{split_index + 1:03d}")).strip()
        train_groups = set(parse_group_list_cell(split_row.get("train_set", "")))
        test_groups = set(parse_group_list_cell(split_row.get("test_set", "")))

        if not train_groups or not test_groups:
            raise ValueError(f"{split_id}: train_set and test_set must both be non-empty.")
        if train_groups & test_groups:
            raise ValueError(f"{split_id}: a primordium occurs in both train and test.")
        if train_groups | test_groups != all_groups:
            missing = sorted(all_groups - train_groups - test_groups)
            unknown = sorted((train_groups | test_groups) - all_groups)
            raise ValueError(
                f"{split_id}: split does not exactly partition the raw primordia; "
                f"missing={missing}, unknown={unknown}."
            )

        train_df = full_df_with_groups[
            full_df_with_groups["_split_group"].isin(train_groups)
        ]
        test_df = full_df_with_groups[
            full_df_with_groups["_split_group"].isin(test_groups)
        ]
        total_rows = int(len(train_df) + len(test_df))
        train_fraction = float(len(train_df) / total_rows)
        test_fraction = float(len(test_df) / total_rows)
        train_pos = int(train_df[label_col].astype(int).sum())
        test_pos = int(test_df[label_col].astype(int).sum())
        train_rate = float(train_pos / len(train_df))
        test_rate = float(test_pos / len(test_df))
        rate_difference = float(abs(train_rate - test_rate))

        target = float(split_row.get("target_train_fraction", TARGET_TRAIN_FRACTION))
        tolerance = float(split_row.get("fraction_tolerance", TRAIN_FRACTION_TOLERANCE))
        max_difference = float(
            split_row.get(
                "max_daughter_pair_frequency_difference",
                MAX_DAUGHTER_FREQUENCY_DIFFERENCE,
            )
        )
        if not np.isclose(target, TARGET_TRAIN_FRACTION, rtol=0.0, atol=1e-12):
            raise ValueError(f"{split_id}: unexpected target_train_fraction={target}.")
        if not np.isclose(tolerance, TRAIN_FRACTION_TOLERANCE, rtol=0.0, atol=1e-12):
            raise ValueError(f"{split_id}: unexpected fraction_tolerance={tolerance}.")
        if not np.isclose(
            max_difference,
            MAX_DAUGHTER_FREQUENCY_DIFFERENCE,
            rtol=0.0,
            atol=1e-12,
        ):
            raise ValueError(
                f"{split_id}: unexpected max_daughter_pair_frequency_difference={max_difference}."
            )
        if abs(train_fraction - target) > tolerance + 1e-12:
            raise ValueError(f"{split_id}: training fraction is outside 70% +/- 2%.")
        if rate_difference >= max_difference:
            raise ValueError(f"{split_id}: daughter-pair frequency difference is not <1 percentage point.")
        if "satisfies_conditions" in split_row.index and not _truthy_split_flag(
            split_row["satisfies_conditions"]
        ):
            raise ValueError(f"{split_id}: marked satisfies_conditions=False.")

        record = {
            "split_id": split_id,
            "train_set": ";".join(sorted(train_groups)),
            "test_set": ";".join(sorted(test_groups)),
            "n_train_primordia": int(len(train_groups)),
            "n_test_primordia": int(len(test_groups)),
            "train_rows": int(len(train_df)),
            "test_rows": int(len(test_df)),
            "total_rows": total_rows,
            "train_fraction": train_fraction,
            "test_fraction": test_fraction,
            "train_daughter_pairs": train_pos,
            "test_daughter_pairs": test_pos,
            "train_daughter_pair_frequency": train_rate,
            "test_daughter_pair_frequency": test_rate,
            "daughter_pair_frequency_difference": rate_difference,
            "satisfies_conditions_recalculated": True,
        }

        integer_columns = [
            "n_train_primordia",
            "n_test_primordia",
            "train_rows",
            "test_rows",
            "total_rows",
            "train_daughter_pairs",
            "test_daughter_pairs",
        ]
        float_columns = [
            "train_fraction",
            "test_fraction",
            "train_daughter_pair_frequency",
            "test_daughter_pair_frequency",
            "daughter_pair_frequency_difference",
        ]
        for column in integer_columns:
            if column in split_row.index and not pd.isna(split_row[column]):
                if int(split_row[column]) != int(record[column]):
                    raise ValueError(
                        f"{split_id}: {column} disagrees with the raw data "
                        f"({split_row[column]} vs {record[column]})."
                    )
        for column in float_columns:
            if column in split_row.index and not pd.isna(split_row[column]):
                if not np.isclose(
                    float(split_row[column]),
                    float(record[column]),
                    rtol=0.0,
                    atol=1e-9,
                ):
                    raise ValueError(
                        f"{split_id}: {column} disagrees with the raw data "
                        f"({split_row[column]} vs {record[column]})."
                    )
        records.append(record)

    validated = pd.DataFrame(records)
    train_count_distribution = {
        str(int(k)): int(v)
        for k, v in validated["n_train_primordia"].value_counts().sort_index().items()
    }
    test_count_distribution = {
        str(int(k)): int(v)
        for k, v in validated["n_test_primordia"].value_counts().sort_index().items()
    }
    summary = {
        "evaluation_design": "repeated primordium-grouped holdout",
        "independent_external_cohort": False,
        "n_accepted_partitions": int(len(validated)),
        "target_train_row_fraction": TARGET_TRAIN_FRACTION,
        "row_fraction_tolerance": TRAIN_FRACTION_TOLERANCE,
        "observed_train_row_fraction_range": [
            float(validated["train_fraction"].min()),
            float(validated["train_fraction"].max()),
        ],
        "observed_test_row_fraction_range": [
            float(validated["test_fraction"].min()),
            float(validated["test_fraction"].max()),
        ],
        "maximum_allowed_daughter_frequency_difference": MAX_DAUGHTER_FREQUENCY_DIFFERENCE,
        "observed_daughter_frequency_difference_range": [
            float(validated["daughter_pair_frequency_difference"].min()),
            float(validated["daughter_pair_frequency_difference"].max()),
        ],
        "training_primordia_range": [
            int(validated["n_train_primordia"].min()),
            int(validated["n_train_primordia"].max()),
        ],
        "test_primordia_range": [
            int(validated["n_test_primordia"].min()),
            int(validated["n_test_primordia"].max()),
        ],
        "training_primordia_count_distribution": train_count_distribution,
        "test_primordia_count_distribution": test_count_distribution,
        "group_integrity": "all snapshots from each primordium remain in one side of each partition",
        "test_use": "final scoring only within each partition",
    }
    return validated, summary


def write_reviewer_methods_summary(configuration: dict[str, Any], output_path: str) -> None:
    """Write a concise, run-specific description suitable for Methods/response drafting."""
    data = configuration["raw_dataset_signature"]["observed"]
    outer = configuration["outer_split_design"]
    versions = configuration["software_versions"]
    package_text = ", ".join(
        f"{name}={version}" for name, version in versions["packages"].items() if version
    )
    lines = [
        "Reviewer-requested multivariate-model reproducibility summary",
        "",
        (
            f"Raw data: {data['raw_rows']} neighbour-pair rows; "
            f"{data['exception_rows_removed']} integrated exception rows removed; "
            f"{data['analyzable_rows']} analyzable rows "
            f"({data['daughter_pairs']} daughter and {data['non_daughter_pairs']} non-daughter pairs) "
            f"from {data['primordia']} primordia."
        ),
        (
            f"Outer evaluation: {outer['n_accepted_partitions']} prespecified primordium-grouped "
            f"holdout partitions; training contained 70% +/- 2% of rows and "
            f"{outer['training_primordia_range'][0]}-{outer['training_primordia_range'][1]} primordia; "
            f"test contained {outer['test_primordia_range'][0]}-{outer['test_primordia_range'][1]} "
            "primordia."
        ),
        (
            f"Model selection: {configuration['cv']['folds']}-fold StratifiedGroupKFold within each "
            "outer training partition, grouped by primordium. Median imputation, scaling where "
            "applicable, probability calibration, class weighting, hyperparameter selection, and "
            "decision-threshold selection were confined to training data."
        ),
        (
            f"Threshold: {configuration['threshold_selection']['n_thresholds']} values from "
            f"{configuration['threshold_selection']['range'][0]:.3f} to "
            f"{configuration['threshold_selection']['range'][1]:.3f} "
            f"(step {configuration['threshold_selection']['even_spacing']:.3f}), selected by maximum "
            "post-matching F1, then frozen for held-out test evaluation."
        ),
        (
            f"Reproducibility: seed={configuration['cv']['seed']}; "
            f"Python={versions['python']}; {package_text}."
        ),
        "Exact candidate grids, input SHA-256 hashes, script hash, and all principal settings are in analysis_configuration.json.",
    ]
    Path(output_path).write_text("\n".join(lines) + "\n", encoding="utf-8")


def safe_div(a: float, b: float) -> float:
    return float(a) / float(b) if b else 0.0


def fbeta_from_pr(precision: float, recall: float, beta: float) -> float:
    b2 = beta * beta
    return (1.0 + b2) * precision * recall / (b2 * precision + recall + 1e-12)


def normalize_filename_series(s: pd.Series) -> pd.Series:
    return s.astype(str).str.strip().str.replace(r"\.json$", "", regex=True)


def normalize_list_arg(value: str | None, aliases: dict[str, str], arg_name: str) -> list[str]:
    if value is None or str(value).strip() == "":
        return []
    items = []
    for raw in str(value).split(","):
        key = raw.strip().lower().replace("-", "_")
        if not key:
            continue
        if key not in aliases:
            valid = ", ".join(sorted(set(aliases.values())))
            raise ValueError(f"Unknown {arg_name}: {raw!r}. Valid values include: {valid}")
        canonical = aliases[key]
        if canonical not in items:
            items.append(canonical)
    return items


def ensure_dir(path: str) -> None:
    os.makedirs(path, exist_ok=True)


def format_duration(seconds: float | None) -> str:
    """Format seconds as a compact duration string such as 1h23m04s."""
    if seconds is None or not np.isfinite(seconds):
        return "?"
    seconds = max(0, int(round(float(seconds))))
    h, rem = divmod(seconds, 3600)
    m, s = divmod(rem, 60)
    if h:
        return f"{h}h{m:02d}m{s:02d}s"
    if m:
        return f"{m}m{s:02d}s"
    return f"{s}s"


class TextProgressBar:
    """
    Minimal fallback progress reporter used when tqdm is not installed.

    It prints elapsed time and estimated remaining time at a controlled interval.
    tqdm is strongly recommended for nicer live bars: pip install tqdm
    """
    def __init__(self, total: int | None, desc: str, unit: str, mininterval: float = 1.0, leave: bool = True):
        self.total = int(total) if total is not None else None
        self.desc = desc
        self.unit = unit
        self.mininterval = float(mininterval)
        self.leave = leave
        self.n = 0
        self.start_time = time.monotonic()
        self.last_print = 0.0
        self.postfix = ""
        self._print(force=True)

    def set_postfix_str(self, value: str) -> None:
        self.postfix = str(value)

    def set_description(self, value: str) -> None:
        self.desc = str(value)

    def update(self, n: int = 1) -> None:
        self.n += int(n)
        self._print(force=(self.total is not None and self.n >= self.total))

    def _print(self, force: bool = False) -> None:
        now = time.monotonic()
        if not force and (now - self.last_print) < self.mininterval:
            return
        self.last_print = now
        elapsed = now - self.start_time
        rate = self.n / elapsed if elapsed > 0 else 0.0
        if self.total:
            remaining = (self.total - self.n) / rate if rate > 0 else None
            pct = 100.0 * self.n / self.total
            msg = (
                f"{self.desc}: {self.n}/{self.total} {self.unit} "
                f"({pct:5.1f}%) | elapsed {format_duration(elapsed)} | ETA {format_duration(remaining)}"
            )
        else:
            msg = f"{self.desc}: {self.n} {self.unit} | elapsed {format_duration(elapsed)}"
        if self.postfix:
            msg += f" | {self.postfix}"
        print(msg, flush=True)

    def close(self) -> None:
        self._print(force=True)


def make_progress_bar(
    total: int | None,
    desc: str,
    unit: str,
    args: Any,
    leave: bool = True,
    disable: bool = False,
):
    """Create a tqdm progress bar when available, otherwise a text fallback."""
    show = bool(getattr(args, "show_progress_bars", True)) and not disable
    if not show:
        return None
    mininterval = float(getattr(args, "progress_mininterval_seconds", 1.0))
    if _tqdm is not None:
        return _tqdm(
            total=total,
            desc=desc,
            unit=unit,
            leave=leave,
            dynamic_ncols=True,
            mininterval=mininterval,
        )
    return TextProgressBar(total=total, desc=desc, unit=unit, mininterval=mininterval, leave=leave)


def progress_update(bar: Any, n: int = 1, postfix: str | None = None) -> None:
    if bar is None:
        return
    if postfix is not None and hasattr(bar, "set_postfix_str"):
        bar.set_postfix_str(postfix)
    bar.update(n)


def progress_close(bar: Any) -> None:
    if bar is not None:
        bar.close()


def _slice_rows(X: Any, idx: np.ndarray) -> Any:
    """Slice pandas DataFrame/Series or numpy array by row index."""
    if hasattr(X, "iloc"):
        return X.iloc[idx]
    return X[idx]


def apply_fold_specific_class_weight(
    estimator: Pipeline,
    y_train: np.ndarray,
    args: Any,
) -> Pipeline:
    """Ensure tree-model class weights are calculated from that fold's training rows."""
    if not bool(getattr(args, "class_weight_balanced", False)):
        return estimator
    deep_params = estimator.get_params(deep=True)
    if "clf__scale_pos_weight" not in deep_params:
        # LogisticRegression and LinearSVC use class_weight='balanced', which is
        # calculated by scikit-learn from the rows supplied to each fit call.
        return estimator
    n_pos = int((y_train == 1).sum())
    n_neg = int((y_train == 0).sum())
    if n_pos == 0 or n_neg == 0:
        raise ValueError("A CV training fold lacks one class; cannot calculate scale_pos_weight.")
    estimator.set_params(clf__scale_pos_weight=float(n_neg / n_pos))
    return estimator


def cross_val_predict_proba_with_progress(
    estimator: Pipeline,
    X: Any,
    y: np.ndarray,
    cv: Any,
    args: Any,
    desc: str,
    groups: np.ndarray | None = None,
) -> np.ndarray:
    """
    Like cross_val_predict(..., method='predict_proba')[:, 1], but with a
    progress bar over folds so long candidates show their own ETA.
    """
    splits = list(cv.split(X, y, groups)) if groups is not None else list(cv.split(X, y))
    if not splits:
        raise RuntimeError("Cross-validation splitter produced no folds.")

    validation_hits = np.zeros(len(y), dtype=int)
    for fold_index, (train_idx, valid_idx) in enumerate(splits, start=1):
        if np.intersect1d(train_idx, valid_idx).size:
            raise RuntimeError(f"CV fold {fold_index} has overlapping train and validation rows.")
        validation_hits[valid_idx] += 1
        if len(np.unique(y[train_idx])) < 2 or len(np.unique(y[valid_idx])) < 2:
            raise ValueError(
                f"CV fold {fold_index} does not contain both labels in both partitions. "
                "Reduce CV_FOLDS or revise the grouped split."
            )
        if groups is not None:
            train_groups = set(np.asarray(groups)[train_idx].tolist())
            valid_groups = set(np.asarray(groups)[valid_idx].tolist())
            overlap = sorted(train_groups & valid_groups)
            if overlap:
                raise RuntimeError(
                    f"CV fold {fold_index} leaks groups across train/validation: {overlap}"
                )
    if not np.all(validation_hits == 1):
        raise RuntimeError(
            "Cross-validation must assign every row to exactly one validation fold."
        )

    y_prob = np.full(shape=len(y), fill_value=np.nan, dtype=float)
    bar = make_progress_bar(
        total=len(splits),
        desc=desc,
        unit="fold",
        args=args,
        leave=False,
        disable=not bool(getattr(args, "show_cv_fold_progress", True)),
    )
    try:
        for fold_index, (train_idx, valid_idx) in enumerate(splits, start=1):
            fold_start = time.monotonic()
            est = clone(estimator)
            est = apply_fold_specific_class_weight(est, y[train_idx], args)
            est.fit(_slice_rows(X, train_idx), y[train_idx])
            fold_prob = est.predict_proba(_slice_rows(X, valid_idx))[:, 1]
            if not np.isfinite(fold_prob).all():
                raise RuntimeError(f"CV fold {fold_index} produced non-finite probabilities.")
            y_prob[valid_idx] = fold_prob
            progress_update(
                bar,
                1,
                postfix=f"fold {fold_index}/{len(splits)} finished in {format_duration(time.monotonic() - fold_start)}",
            )
    finally:
        progress_close(bar)

    if np.isnan(y_prob).any():
        raise RuntimeError("Cross-validation prediction failed: some rows did not receive a probability.")
    return y_prob


# =========================
# Integrated exception filtering
# =========================
def apply_integrated_exception_filter(
    df: pd.DataFrame,
    exception_col: str,
    exception_positive_value: int | float,
    dataset_name: str,
) -> tuple[pd.DataFrame, int, dict[str, Any]]:
    """Remove integrated exception rows before any scientific calculation.

    The exception indicator remains in the returned dataframe (all retained values
    are non-exception values) for traceability, but it is explicitly excluded from
    every feature set. Missing, non-numeric, or non-binary labels are treated as hard
    errors so an exception row can never silently enter training or evaluation.
    """
    if exception_col not in df.columns:
        raise ValueError(
            f"Required integrated exception column {exception_col!r} was not found "
            f"in {dataset_name}. No separate exception.csv is supported."
        )

    raw = df[exception_col]
    if raw.isna().any():
        raise ValueError(
            f"Column {exception_col!r} contains {int(raw.isna().sum())} missing values "
            f"in {dataset_name}."
        )

    numeric = pd.to_numeric(raw, errors="coerce")
    if numeric.isna().any():
        bad = sorted(raw[numeric.isna()].astype(str).unique())
        raise ValueError(
            f"Column {exception_col!r} contains non-numeric values in {dataset_name}: {bad[:10]}"
        )

    positive_value = float(exception_positive_value)
    allowed_values = {0.0, positive_value}
    observed_values = {float(v) for v in numeric.unique()}
    unexpected = sorted(observed_values - allowed_values)
    if unexpected:
        raise ValueError(
            f"Column {exception_col!r} must contain only 0 and {exception_positive_value}; "
            f"found unexpected values in {dataset_name}: {unexpected}"
        )

    exception_mask = numeric.eq(positive_value)
    removed = int(exception_mask.sum())
    kept = df.loc[~exception_mask].copy()
    kept[exception_col] = numeric.loc[~exception_mask].astype(int)

    debug = {
        "dataset_name": dataset_name,
        "exception_source": "integrated_column",
        "exception_column": exception_col,
        "exception_positive_value": exception_positive_value,
        "n_rows_original": int(len(df)),
        "n_rows_removed_as_exceptions": removed,
        "n_rows_retained": int(len(kept)),
        "observed_exception_value_counts": {
            str(int(k) if float(k).is_integer() else k): int(v)
            for k, v in numeric.value_counts(dropna=False).sort_index().items()
        },
    }
    return kept, removed, debug


# =========================
# Feature selection
# =========================
def coerce_numeric_if_possible(df: pd.DataFrame, cols: list[str]) -> None:
    for c in cols:
        if c in df.columns and not pd.api.types.is_numeric_dtype(df[c]):
            df[c] = pd.to_numeric(df[c], errors="coerce")


def select_feature_columns_for_training(
    df: pd.DataFrame,
    label_col: str,
    feature_mode: str,
    selected_features: list[str],
    exception_col: str = EXCEPTION_COL_DEFAULT,
) -> list[str]:
    feature_mode = FEATURE_MODE_ALIASES.get(feature_mode, feature_mode)
    if feature_mode not in {"all", "selected4"}:
        raise ValueError("feature_mode must be 'all' or 'selected4'")

    if feature_mode == "selected4":
        missing = [c for c in selected_features if c not in df.columns]
        if missing:
            raise ValueError("Missing required selected feature columns: " + ", ".join(missing))
        coerce_numeric_if_possible(df, selected_features)
        still_bad = [c for c in selected_features if not pd.api.types.is_numeric_dtype(df[c])]
        if still_bad:
            raise ValueError("Selected features must be numeric. Non-numeric: " + ", ".join(still_bad))
        return selected_features.copy()

    # all numeric features except traceability columns and label
    exclude = set(EXCLUDE_ALWAYS + [label_col, exception_col])
    candidates = [c for c in df.columns if c not in exclude]

    numeric_cols = df[candidates].select_dtypes(include=[np.number]).columns.tolist()
    non_numeric = [c for c in candidates if c not in numeric_cols]
    for c in non_numeric:
        coerced = pd.to_numeric(df[c], errors="coerce")
        if coerced.notna().any():
            df[c] = coerced
            numeric_cols.append(c)

    numeric_set = set(numeric_cols)
    numeric_cols_ordered = [c for c in candidates if c in numeric_set]
    if not numeric_cols_ordered:
        raise ValueError("No numeric feature columns found after exclusions.")
    if len(numeric_cols_ordered) != EXPECTED_ALL_FEATURE_COUNT:
        raise ValueError(
            f"The manuscript-defined all-feature model requires exactly "
            f"{EXPECTED_ALL_FEATURE_COUNT} geometric features, but "
            f"{len(numeric_cols_ordered)} were selected. Selected columns: "
            f"{numeric_cols_ordered}"
        )
    forbidden = {label_col, exception_col, *EXCLUDE_ALWAYS}
    leakage = [c for c in numeric_cols_ordered if c in forbidden]
    if leakage:
        raise RuntimeError(f"Non-feature columns entered the training matrix: {leakage}")
    return numeric_cols_ordered


def make_scoring_features(df: pd.DataFrame, feature_cols: list[str]) -> pd.DataFrame:
    missing = [c for c in feature_cols if c not in df.columns]
    if missing:
        raise ValueError("Scoring CSV is missing training feature columns: " + ", ".join(missing))
    out = df[feature_cols].copy()
    for c in feature_cols:
        if not pd.api.types.is_numeric_dtype(out[c]):
            out[c] = pd.to_numeric(out[c], errors="coerce")
    return out


# =========================
# Maximum-weight matching
# =========================
def run_matching_one_file(df_file: pd.DataFrame, score_col: str) -> set[tuple[int, int]]:
    """
    Return the exact maximum-weight set of non-overlapping edges for one snapshot.

    This intentionally reproduces the NetworkX/Edmonds matching used by the original
    all-model scripts and described in the manuscript. It is not a greedy approximation.
    """
    if nx is None:
        raise RuntimeError(
            "networkx is required for maximum-weight matching. Install with: "
            "python3 -m pip install networkx"
        ) from NETWORKX_IMPORT_ERROR
    if df_file.empty:
        return set()

    work = df_file[[ID1_COL, ID2_COL, score_col]].copy()
    work[ID1_COL] = work[ID1_COL].astype(int)
    work[ID2_COL] = work[ID2_COL].astype(int)
    work[score_col] = work[score_col].astype(float)
    work = work[work[ID1_COL] != work[ID2_COL]].copy()
    if work.empty:
        return set()

    # Canonicalize duplicated undirected edges and keep their highest score.
    a_vals = np.minimum(work[ID1_COL].to_numpy(), work[ID2_COL].to_numpy())
    b_vals = np.maximum(work[ID1_COL].to_numpy(), work[ID2_COL].to_numpy())
    work["_a"] = a_vals
    work["_b"] = b_vals
    work = (
        work.groupby(["_a", "_b"], as_index=False, sort=False)[score_col]
        .max()
        .sort_values(["_a", "_b"], ascending=[True, True])
    )

    graph = nx.Graph()
    for a_raw, b_raw, score_raw in work[["_a", "_b", score_col]].itertuples(index=False, name=None):
        graph.add_edge(int(a_raw), int(b_raw), weight=float(score_raw))

    matching = nx.algorithms.matching.max_weight_matching(
        graph,
        maxcardinality=False,
        weight="weight",
    )
    selected: set[tuple[int, int]] = set()
    for u_raw, v_raw in matching:
        u = int(u_raw)
        v = int(v_raw)
        selected.add((u, v) if u < v else (v, u))
    return selected


def selected_by_file_after_threshold(
    df_all: pd.DataFrame,
    score_col: str,
    threshold: float,
) -> dict[str, set[tuple[int, int]]]:
    df_work = df_all.copy()
    df_work[FILE_COL] = normalize_filename_series(df_work[FILE_COL])
    df_work[ID1_COL] = df_work[ID1_COL].astype(int)
    df_work[ID2_COL] = df_work[ID2_COL].astype(int)
    df_work[score_col] = df_work[score_col].astype(float)

    df_filt = df_work[df_work[score_col] >= float(threshold)].copy()
    selected: dict[str, set[tuple[int, int]]] = {}
    for file_norm, dff in df_filt.groupby(FILE_COL, sort=False):
        selected[file_norm] = run_matching_one_file(dff, score_col=score_col)
    return selected


def true_positive_sets(df_all: pd.DataFrame, label_col: str) -> dict[str, set[tuple[int, int]]]:
    out: dict[str, set[tuple[int, int]]] = {}
    df_work = df_all.copy()
    df_work[FILE_COL] = normalize_filename_series(df_work[FILE_COL])
    df_work[ID1_COL] = df_work[ID1_COL].astype(int)
    df_work[ID2_COL] = df_work[ID2_COL].astype(int)

    for file_norm, dff in df_work.groupby(FILE_COL, sort=False):
        edges: set[tuple[int, int]] = set()
        for row in dff.itertuples(index=False):
            if int(getattr(row, label_col)) != 1:
                continue
            u = int(getattr(row, ID1_COL))
            v = int(getattr(row, ID2_COL))
            a, b = (u, v) if u < v else (v, u)
            edges.add((a, b))
        out[file_norm] = edges
    return out


def matching_metrics_from_selected(
    df_all: pd.DataFrame,
    selected_by_file: dict[str, set[tuple[int, int]]],
    label_col: str,
) -> dict[str, Any]:
    tp_sets = true_positive_sets(df_all, label_col=label_col)
    tp = fp = fn = 0
    for file_norm in set(tp_sets.keys()) | set(selected_by_file.keys()):
        true_pos = tp_sets.get(file_norm, set())
        selected = selected_by_file.get(file_norm, set())
        tp += len(selected & true_pos)
        fp += len(selected - true_pos)
        fn += len(true_pos - selected)

    precision = safe_div(tp, tp + fp)
    recall = safe_div(tp, tp + fn)
    f1 = safe_div(2.0 * precision * recall, precision + recall)
    return {
        "TP": int(tp),
        "FP": int(fp),
        "FN": int(fn),
        "precision_pos1": float(precision),
        "recall_pos1": float(recall),
        "F1_pos1": float(f1),
    }


def matching_metrics_after_threshold(
    df_all: pd.DataFrame,
    score_col: str,
    label_col: str,
    threshold: float,
) -> tuple[dict[str, Any], dict[str, set[tuple[int, int]]]]:
    selected = selected_by_file_after_threshold(df_all=df_all, score_col=score_col, threshold=threshold)
    metrics = matching_metrics_from_selected(df_all=df_all, selected_by_file=selected, label_col=label_col)
    return metrics, selected


def selected_edge_key_set(selected_by_file: dict[str, set[tuple[int, int]]]) -> set[tuple[str, int, int]]:
    out: set[tuple[str, int, int]] = set()
    for file_norm, edges in selected_by_file.items():
        for a, b in edges:
            out.add((file_norm, int(a), int(b)))
    return out


def positive_class_metrics(y_true: np.ndarray, y_pred: np.ndarray) -> dict[str, Any]:
    tn, fp, fn, tp = confusion_matrix(y_true, y_pred, labels=[0, 1]).ravel()
    precision = safe_div(tp, tp + fp)
    recall = safe_div(tp, tp + fn)
    f1 = safe_div(2.0 * precision * recall, precision + recall)
    return {
        "TP": int(tp),
        "FP": int(fp),
        "FN": int(fn),
        "TN": int(tn),
        "precision_pos1": float(precision),
        "recall_pos1": float(recall),
        "F1_pos1": float(f1),
    }


# =========================
# Model builders
# =========================
def make_calibrated_linear_svm(
    C: float,
    class_weight: str | dict | None,
    max_iter: int,
    seed: int,
    calib_method: str,
    calib_cv: int,
) -> CalibratedClassifierCV:
    base = LinearSVC(
        C=C,
        class_weight=class_weight,
        random_state=seed,
        max_iter=max_iter,
        dual=False,
    )
    # sklearn changed base_estimator -> estimator in newer versions.
    try:
        return CalibratedClassifierCV(estimator=base, method=calib_method, cv=calib_cv)
    except TypeError:  # pragma: no cover, older sklearn
        return CalibratedClassifierCV(base_estimator=base, method=calib_method, cv=calib_cv)


def build_pipeline(model_name: str, y: np.ndarray, args: argparse.Namespace) -> Pipeline:
    model_name = MODEL_ALIASES.get(model_name, model_name)
    if model_name == "lr":
        return Pipeline(steps=[
            ("imputer", SimpleImputer(strategy="median")),
            ("scaler", StandardScaler()),
            ("clf", LogisticRegression(
                solver="liblinear",
                penalty="l2",
                C=args.lr_C,
                class_weight="balanced" if args.class_weight_balanced else None,
                random_state=args.seed,
                max_iter=args.lr_max_iter,
            )),
        ])

    if model_name == "svm":
        cal_svm = make_calibrated_linear_svm(
            C=args.svm_C,
            class_weight="balanced" if args.class_weight_balanced else None,
            max_iter=args.svm_max_iter,
            seed=args.seed,
            calib_method=args.svm_calib_method,
            calib_cv=args.svm_calib_cv,
        )
        return Pipeline(steps=[
            ("imputer", SimpleImputer(strategy="median")),
            ("scaler", StandardScaler()),
            ("clf", cal_svm),
        ])

    n_pos = int((y == 1).sum())
    n_neg = int((y == 0).sum())
    scale_pos_weight = n_neg / max(1, n_pos)

    if model_name == "xgboost":
        try:
            from xgboost import XGBClassifier
        except Exception as e:  # pragma: no cover
            raise RuntimeError("xgboost is required. Install with: pip install xgboost") from e
        clf = XGBClassifier(
            objective="binary:logistic",
            eval_metric="logloss",
            n_estimators=args.xgb_n_estimators,
            learning_rate=args.xgb_learning_rate,
            max_depth=args.xgb_max_depth,
            min_child_weight=args.xgb_min_child_weight,
            subsample=args.xgb_subsample,
            colsample_bytree=args.xgb_colsample_bytree,
            reg_lambda=args.xgb_reg_lambda,
            reg_alpha=args.xgb_reg_alpha,
            gamma=args.xgb_gamma,
            tree_method=args.xgb_tree_method,
            n_jobs=args.n_jobs,
            random_state=args.seed,
            scale_pos_weight=scale_pos_weight,
        )
        if args.tree_use_imputer:
            return Pipeline(steps=[("imputer", SimpleImputer(strategy="median")), ("clf", clf)])
        return Pipeline(steps=[("clf", clf)])

    if model_name == "lightgbm":
        try:
            # LightGBM imports its plotting module from __init__.py. On some headless
            # Linux environments, that optional matplotlib import can hang while
            # building the font cache. We do not need LightGBM plotting here, so
            # force matplotlib to look unavailable before importing LightGBM.
            sys.modules.setdefault("matplotlib", None)
            from lightgbm import LGBMClassifier
        except Exception as e:  # pragma: no cover
            raise RuntimeError("lightgbm is required. Install with: pip install lightgbm") from e
        clf = LGBMClassifier(
            objective="binary",
            n_estimators=args.lgbm_n_estimators,
            learning_rate=args.lgbm_learning_rate,
            num_leaves=args.lgbm_num_leaves,
            max_depth=args.lgbm_max_depth,
            min_child_samples=args.lgbm_min_child_samples,
            subsample=args.lgbm_subsample,
            subsample_freq=args.lgbm_subsample_freq,
            colsample_bytree=args.lgbm_colsample_bytree,
            reg_lambda=args.lgbm_reg_lambda,
            reg_alpha=args.lgbm_reg_alpha,
            n_jobs=args.n_jobs,
            random_state=args.seed,
            scale_pos_weight=scale_pos_weight if args.class_weight_balanced else 1.0,
            verbose=-1,
        )
        if args.tree_use_imputer:
            return Pipeline(steps=[("imputer", SimpleImputer(strategy="median")), ("clf", clf)])
        return Pipeline(steps=[("clf", clf)])

    raise ValueError(f"Unsupported model: {model_name}")


# =========================
# Hyperparameter search helpers
# =========================
def _as_list(value: Any) -> list[Any]:
    """Return value as a list, treating strings and scalars as one item."""
    if isinstance(value, list):
        return value
    if isinstance(value, tuple):
        return list(value)
    return [value]


def _unique_dicts(dicts: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """Deduplicate candidate dictionaries while preserving order."""
    seen: set[str] = set()
    out: list[dict[str, Any]] = []
    for d in dicts:
        key = json.dumps(d, sort_keys=True)
        if key in seen:
            continue
        seen.add(key)
        out.append(d)
    return out


def _copy_args_with_updates(args: argparse.Namespace, updates: dict[str, Any]) -> argparse.Namespace:
    new_args = copy.copy(args)
    for k, v in updates.items():
        setattr(new_args, k, v)
    return new_args


def hyperparameter_candidates(model_name: str, args: argparse.Namespace) -> list[tuple[argparse.Namespace, dict[str, Any]]]:
    """
    Build candidate hyperparameter configurations.

    All candidates are evaluated only by cross-validation on the training set.
    The independent test set is not used to choose hyperparameters or thresholds.
    """
    model_name = MODEL_ALIASES.get(model_name, model_name)

    if not getattr(args, "hyperparameter_search", False):
        if model_name == "lr":
            params = {"lr_C": args.lr_C}
        elif model_name == "svm":
            params = {"svm_C": args.svm_C, "svm_calib_method": args.svm_calib_method}
        elif model_name == "xgboost":
            params = {
                "xgb_n_estimators": args.xgb_n_estimators,
                "xgb_learning_rate": args.xgb_learning_rate,
                "xgb_max_depth": args.xgb_max_depth,
                "xgb_min_child_weight": args.xgb_min_child_weight,
                "xgb_subsample": args.xgb_subsample,
                "xgb_colsample_bytree": args.xgb_colsample_bytree,
                "xgb_reg_lambda": args.xgb_reg_lambda,
                "xgb_reg_alpha": args.xgb_reg_alpha,
                "xgb_gamma": args.xgb_gamma,
            }
        elif model_name == "lightgbm":
            params = {
                "lgbm_n_estimators": args.lgbm_n_estimators,
                "lgbm_learning_rate": args.lgbm_learning_rate,
                "lgbm_num_leaves": args.lgbm_num_leaves,
                "lgbm_max_depth": args.lgbm_max_depth,
                "lgbm_min_child_samples": args.lgbm_min_child_samples,
                "lgbm_subsample": args.lgbm_subsample,
                "lgbm_subsample_freq": args.lgbm_subsample_freq,
                "lgbm_colsample_bytree": args.lgbm_colsample_bytree,
                "lgbm_reg_lambda": args.lgbm_reg_lambda,
                "lgbm_reg_alpha": args.lgbm_reg_alpha,
            }
        else:
            raise ValueError(f"Unsupported model: {model_name}")
        return [(_copy_args_with_updates(args, params), params)]

    candidate_dicts: list[dict[str, Any]] = []

    if model_name == "lr":
        for C in _as_list(args.lr_C_list):
            candidate_dicts.append({"lr_C": float(C)})

    elif model_name == "svm":
        for C, calib_method in itertools.product(_as_list(args.svm_C_list), _as_list(args.svm_calib_method_list)):
            if calib_method not in {"sigmoid", "isotonic"}:
                raise ValueError('Every item in SVM_CALIB_METHOD_LIST must be "sigmoid" or "isotonic"')
            candidate_dicts.append({"svm_C": float(C), "svm_calib_method": str(calib_method)})

    elif model_name == "xgboost":
        for values in itertools.product(
            _as_list(args.xgb_max_depth_list),
            _as_list(args.xgb_min_child_weight_list),
            _as_list(args.xgb_subsample_list),
            _as_list(args.xgb_colsample_bytree_list),
            _as_list(args.xgb_reg_lambda_list),
            _as_list(args.xgb_reg_alpha_list),
            _as_list(args.xgb_gamma_list),
        ):
            max_depth, min_child_weight, subsample, colsample_bytree, reg_lambda, reg_alpha, gamma = values
            candidate_dicts.append({
                "xgb_n_estimators": int(args.xgb_n_estimators),
                "xgb_learning_rate": float(args.xgb_learning_rate),
                "xgb_max_depth": int(max_depth),
                "xgb_min_child_weight": float(min_child_weight),
                "xgb_subsample": float(subsample),
                "xgb_colsample_bytree": float(colsample_bytree),
                "xgb_reg_lambda": float(reg_lambda),
                "xgb_reg_alpha": float(reg_alpha),
                "xgb_gamma": float(gamma),
            })

    elif model_name == "lightgbm":
        for values in itertools.product(
            _as_list(args.lgbm_num_leaves_list),
            _as_list(args.lgbm_max_depth_list),
            _as_list(args.lgbm_min_child_samples_list),
            _as_list(args.lgbm_subsample_list),
            _as_list(args.lgbm_colsample_bytree_list),
            _as_list(args.lgbm_reg_lambda_list),
            _as_list(args.lgbm_reg_alpha_list),
        ):
            num_leaves, max_depth, min_child_samples, subsample, colsample_bytree, reg_lambda, reg_alpha = values
            # Avoid obviously invalid LightGBM combinations: if max_depth is positive,
            # num_leaves should not exceed 2^max_depth.
            if int(max_depth) > 0 and int(num_leaves) > (2 ** int(max_depth)):
                continue
            candidate_dicts.append({
                "lgbm_n_estimators": int(args.lgbm_n_estimators),
                "lgbm_learning_rate": float(args.lgbm_learning_rate),
                "lgbm_num_leaves": int(num_leaves),
                "lgbm_max_depth": int(max_depth),
                "lgbm_min_child_samples": int(min_child_samples),
                "lgbm_subsample": float(subsample),
                "lgbm_subsample_freq": int(args.lgbm_subsample_freq),
                "lgbm_colsample_bytree": float(colsample_bytree),
                "lgbm_reg_lambda": float(reg_lambda),
                "lgbm_reg_alpha": float(reg_alpha),
            })

    else:
        raise ValueError(f"Unsupported model: {model_name}")

    candidate_dicts = _unique_dicts(candidate_dicts)
    if not candidate_dicts:
        raise ValueError(f"No hyperparameter candidates were created for model={model_name}")
    return [(_copy_args_with_updates(args, params), params) for params in candidate_dicts]


def threshold_row_is_better(
    candidate: dict[str, Any],
    incumbent: dict[str, Any] | None,
    fbeta_col: str,
) -> bool:
    """Deterministic threshold ranking: F-beta, precision, recall, then threshold."""
    if incumbent is None:
        return True
    ranking_fields = [fbeta_col, "precision_pos1", "recall_pos1", "threshold"]
    for field in ranking_fields:
        cand = float(candidate[field])
        old = float(incumbent[field])
        if cand > old + 1e-12:
            return True
        if cand < old - 1e-12:
            return False
    return False


def evaluate_probabilities_with_threshold_sweep(
    df_base: pd.DataFrame,
    y_true: np.ndarray,
    y_prob: np.ndarray,
    score_col: str,
    label_col: str,
    args: argparse.Namespace,
    progress_desc: str | None = None,
) -> tuple[pd.DataFrame, dict[str, Any]]:
    """Evaluate a probability vector by dense threshold sweep plus matching."""
    df_eval = df_base.copy()
    df_eval[score_col] = y_prob
    thresholds = np.linspace(args.thr_low, args.thr_high, int(args.n_thresholds))
    sweep_rows = []
    best_row: dict[str, Any] | None = None
    fbeta_col = f"F{args.beta}_pos1"

    bar = make_progress_bar(
        total=len(thresholds),
        desc=progress_desc or "Threshold sweep",
        unit="thr",
        args=args,
        leave=False,
        disable=not bool(getattr(args, "show_threshold_progress", True)),
    )
    try:
        for thr in thresholds:
            metrics, _ = matching_metrics_after_threshold(
                df_all=df_eval,
                score_col=score_col,
                label_col=label_col,
                threshold=float(thr),
            )
            fbeta = fbeta_from_pr(metrics["precision_pos1"], metrics["recall_pos1"], beta=args.beta)
            row = {
                "threshold": float(thr),
                **metrics,
                fbeta_col: float(fbeta),
            }
            sweep_rows.append(row)
            if threshold_row_is_better(row, best_row, fbeta_col):
                best_row = row
            best_f1 = best_row["F1_pos1"] if best_row is not None else float("nan")
            best_thr = best_row["threshold"] if best_row is not None else float("nan")
            progress_update(bar, 1, postfix=f"thr={float(thr):.3f}; best_thr={best_thr:.3f}; best_F1={best_f1:.4f}")
    finally:
        progress_close(bar)

    assert best_row is not None
    return pd.DataFrame(sweep_rows), best_row


def printable_param_summary(params: dict[str, Any]) -> str:
    return ", ".join(f"{k}={v}" for k, v in params.items())


def candidate_row_is_better(candidate_row: dict[str, Any], old_row: dict[str, Any] | None, fbeta_col: str) -> bool:
    """Primary ranking: matching-aware CV F-beta; tie-breakers: F1, then PR-AUC."""
    if old_row is None:
        return True
    if candidate_row[fbeta_col] > old_row[fbeta_col] + 1e-12:
        return True
    if abs(candidate_row[fbeta_col] - old_row[fbeta_col]) <= 1e-12:
        if candidate_row["F1_pos1"] > old_row["F1_pos1"] + 1e-12:
            return True
        if abs(candidate_row["F1_pos1"] - old_row["F1_pos1"]) <= 1e-12:
            if candidate_row["cv_pr_auc"] > old_row["cv_pr_auc"] + 1e-12:
                return True
    return False


def prepare_args_for_worker(args: Any, candidate_args: Any | None = None) -> Any:
    """Return a lightweight, picklable args object for a worker process."""
    src = candidate_args if candidate_args is not None else args
    worker_args = copy.copy(src)

    # Do not pickle tqdm bars or other private runtime objects.
    for name in list(vars(worker_args).keys()):
        if name.startswith("_"):
            try:
                delattr(worker_args, name)
            except Exception:
                pass

    # Critical: one worker = one CPU-ish task. Do not let xgboost/lightgbm/sklearn
    # internally use all cores, otherwise 36 workers will oversubscribe the machine.
    worker_args.n_jobs = int(max(1, getattr(args, "n_jobs_per_candidate", 1)))

    # Nested tqdm bars from many worker processes become unreadable. The parent
    # process shows the global candidate-level ETA instead.
    worker_args.show_progress_bars = False
    worker_args.show_cv_fold_progress = False
    worker_args.show_threshold_progress = False
    worker_args.show_whole_process_progress = False
    return worker_args


def evaluate_one_hyperparameter_candidate_worker(payload: dict[str, Any]) -> dict[str, Any]:
    """Evaluate one hyperparameter candidate in a separate process."""
    # Enforce single-thread behavior inside the child process too.
    os.environ["OMP_NUM_THREADS"] = "1"
    os.environ["OPENBLAS_NUM_THREADS"] = "1"
    os.environ["MKL_NUM_THREADS"] = "1"
    os.environ["NUMEXPR_NUM_THREADS"] = "1"
    os.environ["VECLIB_MAXIMUM_THREADS"] = "1"

    start = time.monotonic()
    model_name = payload["model_name"]
    feature_mode = payload["feature_mode"]
    candidate_index = int(payload["candidate_index"])
    total_candidates = int(payload["total_candidates"])
    candidate_args = payload["candidate_args"]
    candidate_params = payload["candidate_params"]
    X = payload["X"]
    y = payload["y"]
    groups = np.asarray(payload["groups"], dtype=object)
    df = payload["df"]
    cv_folds = int(payload["cv_folds"])
    fbeta_col = payload["fbeta_col"]

    try:
        cv, actual_cv_folds, cv_groups, _cv_method = make_inner_cv(
            y=y,
            groups=groups,
            args=candidate_args,
        )
        if actual_cv_folds != cv_folds:
            raise RuntimeError(
                f"Worker CV-fold mismatch: payload={cv_folds}, recalculated={actual_cv_folds}."
            )
        candidate_pipeline = build_pipeline(model_name, y, candidate_args)

        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            y_prob_cv_candidate = cross_val_predict_proba_with_progress(
                estimator=candidate_pipeline,
                X=X,
                y=y,
                cv=cv,
                args=candidate_args,
                desc=f"CV folds: {feature_mode}/{model_name} candidate {candidate_index}/{total_candidates}",
                groups=cv_groups,
            )

        try:
            cand_pr_auc = average_precision_score(y, y_prob_cv_candidate)
            cand_roc_auc = roc_auc_score(y, y_prob_cv_candidate)
        except Exception:
            cand_pr_auc = np.nan
            cand_roc_auc = np.nan

        cand_sweep_df, cand_best_row = evaluate_probabilities_with_threshold_sweep(
            df_base=df,
            y_true=y,
            y_prob=y_prob_cv_candidate,
            score_col="p_division_cv",
            label_col=candidate_args.label_col,
            args=candidate_args,
            progress_desc=f"Thresholds: {feature_mode}/{model_name} candidate {candidate_index}/{total_candidates}",
        )

        candidate_row = {
            "candidate_index": candidate_index,
            "params_json": json.dumps(candidate_params, sort_keys=True),
            "cv_pr_auc": float(cand_pr_auc),
            "cv_roc_auc": float(cand_roc_auc),
            "chosen_threshold": float(cand_best_row["threshold"]),
            "precision_pos1": float(cand_best_row["precision_pos1"]),
            "recall_pos1": float(cand_best_row["recall_pos1"]),
            "F1_pos1": float(cand_best_row["F1_pos1"]),
            fbeta_col: float(cand_best_row[fbeta_col]),
            "TP": int(cand_best_row["TP"]),
            "FP": int(cand_best_row["FP"]),
            "FN": int(cand_best_row["FN"]),
            "elapsed_seconds": float(time.monotonic() - start),
        }
        candidate_row.update(candidate_params)

        return {
            "ok": True,
            "model_name": model_name,
            "feature_mode": feature_mode,
            "combo_key": payload.get("combo_key", f"{feature_mode}::{model_name}"),
            "global_candidate_index": int(payload.get("global_candidate_index", candidate_index)),
            "total_global_candidates": int(payload.get("total_global_candidates", total_candidates)),
            "candidate_index": candidate_index,
            "candidate_params": candidate_params,
            "candidate_row": candidate_row,
            "sweep_records": cand_sweep_df.to_dict(orient="records"),
            "best_row": cand_best_row,
            "y_prob_cv": y_prob_cv_candidate,
            "cv_pr_auc": float(cand_pr_auc),
            "cv_roc_auc": float(cand_roc_auc),
            "elapsed_seconds": float(time.monotonic() - start),
        }
    except Exception as e:
        import traceback
        return {
            "ok": False,
            "model_name": model_name,
            "feature_mode": feature_mode,
            "combo_key": payload.get("combo_key", f"{feature_mode}::{model_name}"),
            "global_candidate_index": int(payload.get("global_candidate_index", candidate_index)),
            "total_global_candidates": int(payload.get("total_global_candidates", total_candidates)),
            "candidate_index": candidate_index,
            "candidate_params": candidate_params,
            "error": str(e),
            "traceback": traceback.format_exc(),
            "elapsed_seconds": float(time.monotonic() - start),
        }


# =========================
# Exports
# =========================
def export_lr_coefficients(pipeline: Pipeline, feature_cols: list[str], out_csv: str) -> None:
    clf = pipeline.named_steps["clf"]
    rows = [{"feature": "__INTERCEPT__", "coef_standardized": float(clf.intercept_.ravel()[0])}]
    for feature, coef in zip(feature_cols, clf.coef_.ravel()):
        rows.append({"feature": feature, "coef_standardized": float(coef)})
    pd.DataFrame(rows).sort_values(
        by="coef_standardized", ascending=False, key=lambda s: s.where(s.index != 0, np.inf)
    ).to_csv(out_csv, index=False)


def export_svm_coefficients(X: pd.DataFrame, y: np.ndarray, feature_cols: list[str], args: argparse.Namespace, out_csv: str) -> None:
    """
    CalibratedClassifierCV does not expose one simple coefficient vector.
    For interpretability, fit a separate LinearSVC on the same imputed/scaled features and export its coefficients.
    """
    coef_pipe = Pipeline(steps=[
        ("imputer", SimpleImputer(strategy="median")),
        ("scaler", StandardScaler()),
        ("clf", LinearSVC(
            C=args.svm_C,
            class_weight="balanced" if args.class_weight_balanced else None,
            random_state=args.seed,
            max_iter=args.svm_max_iter,
            dual=False,
        )),
    ])
    coef_pipe.fit(X, y)
    clf = coef_pipe.named_steps["clf"]
    rows = [{"feature": "__INTERCEPT__", "coef_standardized": float(clf.intercept_.ravel()[0])}]
    for feature, coef in zip(feature_cols, clf.coef_.ravel()):
        rows.append({"feature": feature, "coef_standardized": float(coef)})
    pd.DataFrame(rows).sort_values(
        by="coef_standardized", ascending=False, key=lambda s: s.where(s.index != 0, np.inf)
    ).to_csv(out_csv, index=False)


def export_xgb_importance(pipeline: Pipeline, feature_cols: list[str], out_csv: str) -> None:
    clf = pipeline.named_steps["clf"]
    booster = clf.get_booster()
    gain = booster.get_score(importance_type="gain")
    weight = booster.get_score(importance_type="weight")

    def map_name(k: str) -> str:
        if k.startswith("f") and k[1:].isdigit():
            idx = int(k[1:])
            if 0 <= idx < len(feature_cols):
                return feature_cols[idx]
        return k

    rows = []
    for k in sorted(set(gain) | set(weight)):
        rows.append({
            "feature": map_name(k),
            "gain": float(gain.get(k, 0.0)),
            "weight": float(weight.get(k, 0.0)),
        })
    pd.DataFrame(rows).sort_values("gain", ascending=False).to_csv(out_csv, index=False)


def export_lgbm_importance(pipeline: Pipeline, feature_cols: list[str], out_csv: str) -> None:
    clf = pipeline.named_steps["clf"]
    booster = clf.booster_
    try:
        names = list(booster.feature_name())
    except Exception:
        names = [f"Column_{i}" for i in range(len(feature_cols))]
    gain = booster.feature_importance(importance_type="gain")
    split = booster.feature_importance(importance_type="split")

    def map_name(name: str) -> str:
        if name.startswith("Column_"):
            idx = name.replace("Column_", "")
            if idx.isdigit() and 0 <= int(idx) < len(feature_cols):
                return feature_cols[int(idx)]
        return name

    rows = []
    for i, name in enumerate(names):
        rows.append({
            "feature": map_name(name),
            "gain": float(gain[i]) if i < len(gain) else 0.0,
            "split": float(split[i]) if i < len(split) else 0.0,
        })
    pd.DataFrame(rows).sort_values("gain", ascending=False).to_csv(out_csv, index=False)


def export_model_interpretation(
    model_name: str,
    pipeline: Pipeline,
    X: pd.DataFrame,
    y: np.ndarray,
    feature_cols: list[str],
    args: argparse.Namespace,
    out_dir: str,
) -> str | None:
    try:
        if model_name == "lr":
            out_csv = os.path.join(out_dir, "lr_coefficients.csv")
            export_lr_coefficients(pipeline, feature_cols, out_csv)
            return out_csv
        if model_name == "svm":
            out_csv = os.path.join(out_dir, "svm_linear_coefficients.csv")
            export_svm_coefficients(X, y, feature_cols, args, out_csv)
            return out_csv
        if model_name == "xgboost":
            out_csv = os.path.join(out_dir, "xgb_feature_importance.csv")
            export_xgb_importance(pipeline, feature_cols, out_csv)
            return out_csv
        if model_name == "lightgbm":
            out_csv = os.path.join(out_dir, "lgbm_feature_importance.csv")
            export_lgbm_importance(pipeline, feature_cols, out_csv)
            return out_csv
    except Exception as e:
        print(f"[WARNING] Could not export interpretation table for {model_name}: {e}")
    return None


def export_matching_outputs(
    df_all: pd.DataFrame,
    selected_by_file: dict[str, set[tuple[int, int]]],
    score_col: str,
    label_col: str,
    out_dir: str,
    tag: str,
) -> tuple[str, str | None]:
    has_label = label_col in df_all.columns
    df_work = df_all.copy()
    df_work[FILE_COL] = normalize_filename_series(df_work[FILE_COL])
    df_work[ID1_COL] = df_work[ID1_COL].astype(int)
    df_work[ID2_COL] = df_work[ID2_COL].astype(int)
    df_work[score_col] = df_work[score_col].astype(float)

    edge_lookup: dict[str, dict[tuple[int, int], dict[str, Any]]] = {}
    for file_norm, dff in df_work.groupby(FILE_COL, sort=False):
        lookup: dict[tuple[int, int], dict[str, Any]] = {}
        for row in dff.itertuples(index=False):
            u = int(getattr(row, ID1_COL))
            v = int(getattr(row, ID2_COL))
            a, b = (u, v) if u < v else (v, u)
            score = float(getattr(row, score_col))
            label = int(getattr(row, label_col)) if has_label else np.nan
            if (a, b) not in lookup or score > lookup[(a, b)]["score"]:
                lookup[(a, b)] = {"score": score, "label": label}
            elif has_label:
                lookup[(a, b)]["label"] = max(int(lookup[(a, b)]["label"]), label)
        edge_lookup[file_norm] = lookup

    matched_rows = []
    summary_rows = []
    all_files = sorted(set(edge_lookup.keys()) | set(selected_by_file.keys()))
    for file_norm in all_files:
        selected = selected_by_file.get(file_norm, set())
        edges = edge_lookup.get(file_norm, {})
        selected_sorted = sorted(selected, key=lambda e: edges.get(e, {}).get("score", -np.inf), reverse=True)

        for a, b in selected_sorted:
            info = edges.get((a, b), {"score": np.nan, "label": np.nan})
            row = {
                FILE_COL: file_norm,
                ID1_COL: int(a),
                ID2_COL: int(b),
                score_col: float(info["score"]),
            }
            if has_label:
                row[label_col] = int(info["label"]) if not pd.isna(info["label"]) else np.nan
            matched_rows.append(row)

        summary = {
            FILE_COL: file_norm,
            "n_edges_input": int((df_work[FILE_COL] == file_norm).sum()),
            "n_selected_matching": int(len(selected)),
            "sum_score_selected": float(sum(edges[e]["score"] for e in selected if e in edges)) if selected else 0.0,
        }
        if has_label:
            true_pos = {e for e, info in edges.items() if int(info.get("label", 0)) == 1}
            tp = len(selected & true_pos)
            fp = len(selected - true_pos)
            fn = len(true_pos - selected)
            prec = safe_div(tp, tp + fp)
            rec = safe_div(tp, tp + fn)
            f1 = safe_div(2.0 * prec * rec, prec + rec)
            summary.update({
                "TP": int(tp),
                "FP": int(fp),
                "FN": int(fn),
                "precision_pos1": float(prec),
                "recall_pos1": float(rec),
                "F1_pos1": float(f1),
            })
        summary_rows.append(summary)

    matched_csv = os.path.join(out_dir, f"matched_edges_{tag}.csv")
    pd.DataFrame(matched_rows).to_csv(matched_csv, index=False)

    summary_csv = None
    if summary_rows:
        summary_csv = os.path.join(out_dir, f"matching_summary_{tag}.csv")
        pd.DataFrame(summary_rows).to_csv(summary_csv, index=False)

    return matched_csv, summary_csv


def add_prediction_columns(
    df_scored: pd.DataFrame,
    score_col: str,
    threshold: float,
    selected_by_file: dict[str, set[tuple[int, int]]],
) -> pd.DataFrame:
    out = df_scored.copy()
    out[FILE_COL] = normalize_filename_series(out[FILE_COL])
    a = np.minimum(out[ID1_COL].astype(int), out[ID2_COL].astype(int))
    b = np.maximum(out[ID1_COL].astype(int), out[ID2_COL].astype(int))
    keys = list(zip(out[FILE_COL], a, b))
    selected_keys = selected_edge_key_set(selected_by_file)
    out["pred_raw"] = (out[score_col].astype(float) >= float(threshold)).astype(int)
    out["pred_matched"] = pd.Series(keys, index=out.index).isin(selected_keys).astype(int)
    return out


def save_scores_csv(
    df_scored: pd.DataFrame,
    score_col: str,
    label_col: str,
    out_csv: str,
) -> None:
    preferred = [
        c for c in EXCLUDE_ALWAYS if c in df_scored.columns
    ]
    cols = preferred.copy()
    if label_col in df_scored.columns:
        cols.append(label_col)
    cols += [score_col, "pred_raw", "pred_matched"]
    # Keep feature and extra columns out of the default scores table to make it compact.
    df_scored[cols].to_csv(out_csv, index=False)


# =========================
# Train / score workflow
# =========================
def score_dataset(
    dataset_name: str,
    csv_path: str | None,
    pipeline: Pipeline,
    feature_cols: list[str],
    chosen_threshold: float,
    label_col: str,
    exception_col: str,
    exception_positive_value: int | float,
    out_dir: str,
) -> dict[str, Any] | None:
    if csv_path is None:
        return None
    if not os.path.exists(csv_path):
        print(f"[WARNING] {dataset_name} CSV not found; skipping scoring: {csv_path}")
        return None

    df_raw = pd.read_csv(csv_path)
    df, removed, ex_debug = apply_integrated_exception_filter(
        df_raw,
        exception_col=exception_col,
        exception_positive_value=exception_positive_value,
        dataset_name=dataset_name,
    )
    X = make_scoring_features(df, feature_cols)
    score_col = f"p_division_{dataset_name}"
    df_scored = df.copy()
    df_scored[score_col] = pipeline.predict_proba(X)[:, 1]

    selected = selected_by_file_after_threshold(df_scored, score_col=score_col, threshold=chosen_threshold)
    df_scored = add_prediction_columns(df_scored, score_col=score_col, threshold=chosen_threshold, selected_by_file=selected)

    scores_csv = os.path.join(out_dir, f"{dataset_name}_scores.csv")
    save_scores_csv(df_scored, score_col=score_col, label_col=label_col, out_csv=scores_csv)

    matched_csv, summary_csv = export_matching_outputs(
        df_all=df_scored,
        selected_by_file=selected,
        score_col=score_col,
        label_col=label_col,
        out_dir=out_dir,
        tag=dataset_name,
    )

    out: dict[str, Any] = {
        "csv": csv_path,
        "n_rows_original": int(len(df_raw)),
        "n_rows_removed_by_integrated_exception_label": int(removed),
        "n_rows_used": int(len(df)),
        "integrated_exception_filter": ex_debug,
        "scores_csv": scores_csv,
        "matched_edges_csv": matched_csv,
        "matching_summary_csv": summary_csv,
    }

    if label_col in df_scored.columns:
        y_true = df_scored[label_col].astype(int).to_numpy()
        y_pred_raw = df_scored["pred_raw"].astype(int).to_numpy()
        y_pred_matched = df_scored["pred_matched"].astype(int).to_numpy()
        try:
            pr_auc = average_precision_score(y_true, df_scored[score_col].to_numpy())
            roc_auc = roc_auc_score(y_true, df_scored[score_col].to_numpy())
        except Exception:
            pr_auc = np.nan
            roc_auc = np.nan
        out["metrics"] = {
            "score_level": {"pr_auc": float(pr_auc), "roc_auc": float(roc_auc)},
            "raw_edge_level": positive_class_metrics(y_true, y_pred_raw),
            "after_matching": matching_metrics_from_selected(df_scored, selected, label_col=label_col),
            "matched_flag_edge_level": positive_class_metrics(y_true, y_pred_matched),
        }

    return out


def training_groups_from_dataframe(df: pd.DataFrame) -> np.ndarray:
    """Return one primordium identifier per training row."""
    if FILE_COL not in df.columns:
        raise ValueError(f"Cannot construct grouped CV without {FILE_COL!r}.")
    if df[FILE_COL].isna().any():
        raise ValueError(f"Cannot construct grouped CV because {FILE_COL!r} contains missing values.")
    groups = df[FILE_COL].astype(str).map(infer_primordium_from_filename).to_numpy(dtype=object)
    if len(groups) != len(df) or any(str(g).strip() == "" for g in groups):
        raise RuntimeError("Failed to assign a valid primordium group to every training row.")
    return groups


def choose_cv_folds(
    y: np.ndarray,
    requested_folds: int,
    groups: np.ndarray | None = None,
) -> int:
    n_pos = int((y == 1).sum())
    n_neg = int((y == 0).sum())
    limits = [int(requested_folds), n_pos, n_neg]
    if groups is not None:
        limits.append(int(len(np.unique(groups))))
    folds = min(limits)
    if folds < 2:
        raise ValueError(
            "Need at least two folds, two positives, two negatives, and—when "
            f"grouping—two primordia. Got positives={n_pos}, negatives={n_neg}, "
            f"groups={len(np.unique(groups)) if groups is not None else 'not used'}."
        )
    return folds


def make_inner_cv(
    y: np.ndarray,
    groups: np.ndarray,
    args: Any,
) -> tuple[Any, int, np.ndarray | None, str]:
    """Construct the training-only CV scheme used for tuning and threshold selection."""
    if bool(getattr(args, "group_aware_inner_cv", True)):
        folds = choose_cv_folds(y, args.cv_folds, groups=groups)
        cv = StratifiedGroupKFold(
            n_splits=folds,
            shuffle=True,
            random_state=int(args.seed),
        )
        return cv, folds, groups, "StratifiedGroupKFold by primordium"

    folds = choose_cv_folds(y, args.cv_folds, groups=None)
    cv = StratifiedKFold(n_splits=folds, shuffle=True, random_state=int(args.seed))
    return cv, folds, None, "StratifiedKFold on rows"


def train_one_model_feature_mode(
    model_name: str,
    feature_mode: str,
    args: argparse.Namespace,
) -> TrainResult:
    model_name = MODEL_ALIASES.get(model_name, model_name)
    feature_mode = FEATURE_MODE_ALIASES.get(feature_mode, feature_mode)

    out_dir = os.path.join(args.out_dir, feature_mode, model_name)
    ensure_dir(out_dir)

    train_csv = args.train_csv
    if not os.path.exists(train_csv):
        raise FileNotFoundError(f"train_csv not found: {train_csv}")

    print("\n" + "=" * 90)
    print(f"Training model={model_name} | feature_mode={feature_mode}")
    print(f"Train CSV:     {train_csv}")
    print(f"Test CSV:      {args.test_csv}")
    print(
        f"Integrated exception filter: {args.exception_col} == "
        f"{args.exception_positive_value}"
    )
    print(f"Output:        {out_dir}")

    df_raw = pd.read_csv(train_csv)
    if args.label_col not in df_raw.columns:
        raise ValueError(f"Label column {args.label_col!r} not found in train_csv: {train_csv}")

    df, removed, ex_debug = apply_integrated_exception_filter(
        df_raw,
        exception_col=args.exception_col,
        exception_positive_value=args.exception_positive_value,
        dataset_name="train",
    )
    feature_cols = select_feature_columns_for_training(
        df=df,
        label_col=args.label_col,
        feature_mode=feature_mode,
        selected_features=SELECTED_4_FEATURES,
        exception_col=args.exception_col,
    )
    X = df[feature_cols].copy()
    y = df[args.label_col].astype(int).to_numpy()
    groups = training_groups_from_dataframe(df)
    cv, cv_folds, cv_groups, cv_method = make_inner_cv(y=y, groups=groups, args=args)

    print(
        f"Rows: original={len(df_raw)} "
        f"removed_by_integrated_exception_label={removed} used={len(df)}"
    )
    print(f"Features used ({len(feature_cols)}): {feature_cols}")
    print(f"Inner CV: {cv_method}; folds={cv_folds}; primordia={len(np.unique(groups))}")
    fbeta_col = f"F{args.beta}_pos1"

    candidates = hyperparameter_candidates(model_name, args)
    print(f"Hyperparameter candidates: {len(candidates)}")

    candidate_rows: list[dict[str, Any]] = []
    best_candidate: dict[str, Any] | None = None

    use_parallel = bool(getattr(args, "parallel_hyperparameter_candidates", False)) and len(candidates) > 1

    if use_parallel:
        max_workers = min(int(getattr(args, "max_parallel_candidates", 1)), len(candidates))
        max_workers = max(1, max_workers)
        start_method = str(getattr(args, "parallel_start_method", "fork"))
        try:
            mp_context = mp.get_context(start_method)
        except ValueError:
            print(f"[WARNING] Unknown PARALLEL_START_METHOD={start_method!r}; using Python default.")
            mp_context = None

        print(
            f"Parallel candidate evaluation: {max_workers} worker processes | "
            f"N_JOBS_PER_CANDIDATE={getattr(args, 'n_jobs_per_candidate', 1)} | "
            f"start_method={start_method}"
        )
        print("Tip: if Ubuntu kills the job because of memory, reduce MAX_PARALLEL_CANDIDATES to 18 or 24.")

        payloads: list[dict[str, Any]] = []
        for i, (candidate_args, candidate_params) in enumerate(candidates, start=1):
            worker_args = prepare_args_for_worker(args, candidate_args)
            payloads.append({
                "model_name": model_name,
                "feature_mode": feature_mode,
                "candidate_index": i,
                "total_candidates": len(candidates),
                "candidate_args": worker_args,
                "candidate_params": candidate_params,
                "X": X,
                "y": y,
                "groups": groups,
                "df": df,
                "cv_folds": cv_folds,
                "fbeta_col": fbeta_col,
            })
            if bool(getattr(args, "show_per_candidate_start_finish", True)):
                print(f"  Submitted candidate {i}/{len(candidates)}: {printable_param_summary(candidate_params)}")

        executor_kwargs = {"max_workers": max_workers}
        if mp_context is not None:
            executor_kwargs["mp_context"] = mp_context

        with ProcessPoolExecutor(**executor_kwargs) as executor:
            future_to_index = {
                executor.submit(evaluate_one_hyperparameter_candidate_worker, payload): payload["candidate_index"]
                for payload in payloads
            }
            for future in as_completed(future_to_index):
                i = future_to_index[future]
                result = future.result()
                elapsed = float(result.get("elapsed_seconds", 0.0))

                if not result.get("ok", False):
                    msg = (
                        f"FAILED candidate {i}/{len(candidates)} for {feature_mode}/{model_name}: "
                        f"{result.get('error')}\n{result.get('traceback', '')}"
                    )
                    print("\n[ERROR] " + msg)
                    if not args.continue_on_error:
                        raise RuntimeError(msg)
                    global_bar = getattr(args, "_global_progress_bar", None)
                    progress_update(global_bar, 1, postfix=f"FAILED {feature_mode}/{model_name} cand {i}")
                    continue

                candidate_row = result["candidate_row"]
                candidate_rows.append(candidate_row)

                if candidate_row_is_better(
                    candidate_row,
                    best_candidate["candidate_row"] if best_candidate is not None else None,
                    fbeta_col,
                ):
                    best_candidate = {
                        "args": _copy_args_with_updates(args, result["candidate_params"]),
                        "params": result["candidate_params"],
                        "candidate_row": candidate_row,
                        "sweep_df": pd.DataFrame(result["sweep_records"]),
                        "best_row": result["best_row"],
                        "y_prob_cv": result["y_prob_cv"],
                        "cv_pr_auc": result["cv_pr_auc"],
                        "cv_roc_auc": result["cv_roc_auc"],
                    }

                if bool(getattr(args, "show_per_candidate_start_finish", True)):
                    print(
                        f"  Finished candidate {i}/{len(candidates)} in {format_duration(elapsed)} | "
                        f"threshold={candidate_row['chosen_threshold']:.6f}, "
                        f"CV F1={candidate_row['F1_pos1']:.4f} | "
                        f"{printable_param_summary(result['candidate_params'])}"
                    )

                global_bar = getattr(args, "_global_progress_bar", None)
                progress_update(
                    global_bar,
                    1,
                    postfix=(
                        f"{feature_mode}/{model_name} cand {i}/{len(candidates)} "
                        f"F1={candidate_row['F1_pos1']:.4f} elapsed={format_duration(elapsed)}"
                    ),
                )

        candidate_rows = sorted(candidate_rows, key=lambda r: int(r["candidate_index"]))

    else:
        for i, (candidate_args, candidate_params) in enumerate(candidates, start=1):
            candidate_label = f"{feature_mode}/{model_name} candidate {i}/{len(candidates)}"
            candidate_start = time.monotonic()
            print(f"  Candidate {i}/{len(candidates)}: {printable_param_summary(candidate_params)}")
            candidate_pipeline = build_pipeline(model_name, y, candidate_args)

            with warnings.catch_warnings():
                warnings.simplefilter("ignore")
                y_prob_cv_candidate = cross_val_predict_proba_with_progress(
                    estimator=candidate_pipeline,
                    X=X,
                    y=y,
                    cv=cv,
                    args=candidate_args,
                    desc=f"CV folds: {candidate_label}",
                    groups=cv_groups,
                )

            try:
                cand_pr_auc = average_precision_score(y, y_prob_cv_candidate)
                cand_roc_auc = roc_auc_score(y, y_prob_cv_candidate)
            except Exception:
                cand_pr_auc = np.nan
                cand_roc_auc = np.nan

            cand_sweep_df, cand_best_row = evaluate_probabilities_with_threshold_sweep(
                df_base=df,
                y_true=y,
                y_prob=y_prob_cv_candidate,
                score_col="p_division_cv",
                label_col=args.label_col,
                args=candidate_args,
                progress_desc=f"Thresholds: {candidate_label}",
            )

            candidate_row = {
                "candidate_index": i,
                "params_json": json.dumps(candidate_params, sort_keys=True),
                "cv_pr_auc": float(cand_pr_auc),
                "cv_roc_auc": float(cand_roc_auc),
                "chosen_threshold": float(cand_best_row["threshold"]),
                "precision_pos1": float(cand_best_row["precision_pos1"]),
                "recall_pos1": float(cand_best_row["recall_pos1"]),
                "F1_pos1": float(cand_best_row["F1_pos1"]),
                fbeta_col: float(cand_best_row[fbeta_col]),
                "TP": int(cand_best_row["TP"]),
                "FP": int(cand_best_row["FP"]),
                "FN": int(cand_best_row["FN"]),
                "elapsed_seconds": float(time.monotonic() - candidate_start),
            }
            candidate_row.update(candidate_params)
            candidate_rows.append(candidate_row)

            if candidate_row_is_better(
                candidate_row,
                best_candidate["candidate_row"] if best_candidate is not None else None,
                fbeta_col,
            ):
                best_candidate = {
                    "args": candidate_args,
                    "params": candidate_params,
                    "candidate_row": candidate_row,
                    "sweep_df": cand_sweep_df,
                    "best_row": cand_best_row,
                    "y_prob_cv": y_prob_cv_candidate,
                    "cv_pr_auc": cand_pr_auc,
                    "cv_roc_auc": cand_roc_auc,
                }

            candidate_elapsed = time.monotonic() - candidate_start
            print(
                f"  Finished candidate {i}/{len(candidates)} in {format_duration(candidate_elapsed)} | "
                f"best threshold={candidate_row['chosen_threshold']:.6f}, "
                f"CV F1={candidate_row['F1_pos1']:.4f}"
            )
            global_bar = getattr(args, "_global_progress_bar", None)
            progress_update(
                global_bar,
                1,
                postfix=(
                    f"{feature_mode}/{model_name} cand {i}/{len(candidates)} "
                    f"F1={candidate_row['F1_pos1']:.4f} elapsed={format_duration(candidate_elapsed)}"
                ),
            )

    assert best_candidate is not None
    best_args = best_candidate["args"]
    best_params = best_candidate["params"]
    best_row = best_candidate["best_row"]
    y_prob_cv = best_candidate["y_prob_cv"]
    cv_pr_auc = best_candidate["cv_pr_auc"]
    cv_roc_auc = best_candidate["cv_roc_auc"]
    chosen_threshold = float(best_row["threshold"])

    hyper_csv = os.path.join(out_dir, "hyperparameter_search_train_cv.csv")
    pd.DataFrame(candidate_rows).to_csv(hyper_csv, index=False)

    sweep_df = best_candidate["sweep_df"]
    sweep_csv = os.path.join(out_dir, "threshold_sweep_matching_train_cv.csv")
    sweep_df.to_csv(sweep_csv, index=False)

    print(f"Selected hyperparameters: {printable_param_summary(best_params)}")

    pipeline = build_pipeline(model_name, y, best_args)
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        pipeline.fit(X, y)
    y_prob_fit = pipeline.predict_proba(X)[:, 1]

    df_cv = df.copy()
    df_cv["p_division_cv"] = y_prob_cv
    df_cv["p_division_fit"] = y_prob_fit

    fig_png = None
    if args.make_plots:
        fig_png = os.path.join(out_dir, "metrics_vs_threshold_train_cv.png")
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        plt.figure(figsize=(7.0, 4.5))
        plt.plot(sweep_df["threshold"], sweep_df["precision_pos1"], label="Precision")
        plt.plot(sweep_df["threshold"], sweep_df["recall_pos1"], label="Recall")
        plt.plot(sweep_df["threshold"], sweep_df["F1_pos1"], label="F1")
        plt.plot(sweep_df["threshold"], sweep_df[fbeta_col], label=f"F{args.beta}")
        plt.scatter([chosen_threshold], [best_row[fbeta_col]], label="Chosen threshold")
        plt.xlabel("Threshold applied before matching")
        plt.ylabel("Metric after matching")
        plt.title(f"{model_name} / {feature_mode}: matching-aware threshold sweep")
        plt.legend()
        plt.tight_layout()
        plt.savefig(fig_png, dpi=300)
        plt.close()

    # Train-fit matching and scores.
    selected_fit = selected_by_file_after_threshold(df_cv, score_col="p_division_fit", threshold=chosen_threshold)
    selected_cv = selected_by_file_after_threshold(df_cv, score_col="p_division_cv", threshold=chosen_threshold)

    df_train_scores = add_prediction_columns(
        df_cv.rename(columns={"p_division_fit": "p_division_train_fit"}),
        score_col="p_division_train_fit",
        threshold=chosen_threshold,
        selected_by_file=selected_fit,
    )
    # Add CV score/prediction columns too, for honest training-set reference.
    cv_keys = selected_edge_key_set(selected_cv)
    a = np.minimum(df_train_scores[ID1_COL].astype(int), df_train_scores[ID2_COL].astype(int))
    b = np.maximum(df_train_scores[ID1_COL].astype(int), df_train_scores[ID2_COL].astype(int))
    keys = list(zip(normalize_filename_series(df_train_scores[FILE_COL]), a, b))
    df_train_scores["p_division_train_cv"] = df_cv["p_division_cv"].to_numpy()
    df_train_scores["pred_raw_cv"] = (df_train_scores["p_division_train_cv"] >= chosen_threshold).astype(int)
    df_train_scores["pred_matched_cv"] = pd.Series(keys, index=df_train_scores.index).isin(cv_keys).astype(int)

    train_scores_csv = os.path.join(out_dir, "train_scores.csv")
    train_score_cols = [c for c in EXCLUDE_ALWAYS if c in df_train_scores.columns]
    train_score_cols += [args.label_col, "p_division_train_fit", "p_division_train_cv", "pred_raw", "pred_matched", "pred_raw_cv", "pred_matched_cv"]
    df_train_scores[train_score_cols].to_csv(train_scores_csv, index=False)

    matched_train_csv, matching_summary_train_csv = export_matching_outputs(
        df_all=df_cv,
        selected_by_file=selected_fit,
        score_col="p_division_fit",
        label_col=args.label_col,
        out_dir=out_dir,
        tag="train_fit",
    )

    interpretation_csv = export_model_interpretation(model_name, pipeline, X, y, feature_cols, best_args, out_dir)

    model_path = None
    if args.save_model:
        if joblib is None:
            print("[WARNING] joblib is unavailable; model was not saved.")
        else:
            model_path = os.path.join(out_dir, "model_pipeline.joblib")
            joblib.dump(pipeline, model_path)

    test_summary = score_dataset(
        dataset_name="test",
        csv_path=args.test_csv,
        pipeline=pipeline,
        feature_cols=feature_cols,
        chosen_threshold=chosen_threshold,
        label_col=args.label_col,
        exception_col=args.exception_col,
        exception_positive_value=args.exception_positive_value,
        out_dir=out_dir,
    )

    raw_metrics_fit = positive_class_metrics(
        df_cv[args.label_col].astype(int).to_numpy(),
        (df_cv["p_division_fit"].to_numpy() >= chosen_threshold).astype(int),
    )
    matched_metrics_fit = matching_metrics_from_selected(df_cv, selected_fit, label_col=args.label_col)
    matched_metrics_cv = matching_metrics_from_selected(df_cv, selected_cv, label_col=args.label_col)

    summary = {
        "model_name": model_name,
        "feature_mode": feature_mode,
        "train_csv": train_csv,
        "test_csv": args.test_csv,
        "exception_column": args.exception_col,
        "exception_positive_value": args.exception_positive_value,
        "exception_source": "integrated_column",
        "label_col": args.label_col,
        "n_rows_train_original": int(len(df_raw)),
        "n_rows_train_removed_by_integrated_exception_label": int(removed),
        "n_rows_train_used": int(len(df)),
        "integrated_exception_filter_train": ex_debug,
        "excluded_columns_from_training": EXCLUDE_ALWAYS,
        "selected_4_features_definition": SELECTED_4_FEATURES,
        "feature_cols_used": feature_cols,
        "n_features_used": int(len(feature_cols)),
        "cv_folds": int(cv_folds),
        "inner_cv": {
            "method": cv_method,
            "group_aware": bool(args.group_aware_inner_cv),
            "group_definition": "primordium inferred from fileName prefix",
            "n_groups": int(len(np.unique(groups))),
            "groups": sorted(map(str, np.unique(groups))),
            "folds": int(cv_folds),
            "shuffle": True,
        },
        "seed": int(args.seed),
        "software_versions": collect_software_versions(),
        "script_provenance": collect_script_provenance(),
        "methodological_references": METHODOLOGICAL_REFERENCES,
        "hyperparameter_search_enabled": bool(args.hyperparameter_search),
        "selected_hyperparameters": best_params,
        "hyperparameter_selection": {
            "method": f"maximize_F{args.beta}_after_matching_on_inner_grouped_cv_probabilities_then_refit_on_full_training_set",
            "outer_test_used_for_selection": False,
            "n_candidates": int(len(candidates)),
            "search_csv": hyper_csv,
            "selected_candidate_index": int(best_candidate["candidate_row"]["candidate_index"]),
        },
        "threshold_selection": {
            "method": f"maximize_F{args.beta}_after_matching_on_inner_grouped_cv_probabilities",
            "outer_test_used_for_selection": False,
            "n_thresholds": int(args.n_thresholds),
            "thr_low": float(args.thr_low),
            "thr_high": float(args.thr_high),
            "even_spacing": float(
                (args.thr_high - args.thr_low) / (args.n_thresholds - 1)
            ),
            "chosen_threshold": float(chosen_threshold),
            f"chosen_F{args.beta}_pos1": float(best_row[fbeta_col]),
            "chosen_precision_pos1": float(best_row["precision_pos1"]),
            "chosen_recall_pos1": float(best_row["recall_pos1"]),
            "chosen_F1_pos1": float(best_row["F1_pos1"]),
            "chosen_TP": int(best_row["TP"]),
            "chosen_FP": int(best_row["FP"]),
            "chosen_FN": int(best_row["FN"]),
        },
        "cv_score_level_train": {
            "pr_auc": float(cv_pr_auc),
            "roc_auc": float(cv_roc_auc),
        },
        "metrics_at_chosen_threshold_train": {
            "raw_edge_level_fit": raw_metrics_fit,
            "after_matching_fit": matched_metrics_fit,
            "after_matching_cv": matched_metrics_cv,
        },
        "test_summary": test_summary,
        "outputs": {
            "out_dir": out_dir,
            "model_pipeline_joblib": model_path,
            "interpretation_csv": interpretation_csv,
            "train_scores_csv": train_scores_csv,
            "hyperparameter_search_train_cv_csv": hyper_csv,
            "threshold_sweep_matching_train_cv_csv": sweep_csv,
            "metrics_vs_threshold_train_cv_png": fig_png,
            "matched_edges_train_fit_csv": matched_train_csv,
            "matching_summary_train_fit_csv": matching_summary_train_csv,
        },
    }

    summary_json = os.path.join(out_dir, "training_summary.json")
    with open(summary_json, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)

    print(f"Chosen threshold: {chosen_threshold:.6f}")
    print(
        f"Train CV after matching: precision={best_row['precision_pos1']:.4f}, "
        f"recall={best_row['recall_pos1']:.4f}, F1={best_row['F1_pos1']:.4f}, "
        f"F{args.beta}={best_row[fbeta_col]:.4f}"
    )
    if test_summary and "metrics" in test_summary:
        mt = test_summary["metrics"]["after_matching"]
        print(
            f"Test after matching:     precision={mt['precision_pos1']:.4f}, "
            f"recall={mt['recall_pos1']:.4f}, F1={mt['F1_pos1']:.4f}"
        )
    print(f"Saved summary: {summary_json}")

    return TrainResult(
        model_name=model_name,
        feature_mode=feature_mode,
        out_dir=out_dir,
        pipeline=pipeline,
        feature_cols=feature_cols,
        chosen_threshold=chosen_threshold,
        summary_json=summary_json,
    )



# =========================
# Global parallel scheduler across all feature modes and all models
# =========================
def prepare_model_feature_context(
    model_name: str,
    feature_mode: str,
    args: argparse.Namespace,
) -> dict[str, Any]:
    """
    Load and prepare data for one (feature_mode, model_name) combination.

    This is the first half of train_one_model_feature_mode(), split out so a
    single global scheduler can mix candidates from all combinations in one
    36-worker queue.
    """
    model_name = MODEL_ALIASES.get(model_name, model_name)
    feature_mode = FEATURE_MODE_ALIASES.get(feature_mode, feature_mode)

    out_dir = os.path.join(args.out_dir, feature_mode, model_name)
    ensure_dir(out_dir)

    train_csv = args.train_csv
    if not os.path.exists(train_csv):
        raise FileNotFoundError(f"train_csv not found: {train_csv}")

    print("\n" + "=" * 90)
    print(f"Preparing model={model_name} | feature_mode={feature_mode}")
    print(f"Train CSV:     {train_csv}")
    print(f"Test CSV:      {args.test_csv}")
    print(
        f"Integrated exception filter: {args.exception_col} == "
        f"{args.exception_positive_value}"
    )
    print(f"Output:        {out_dir}")

    df_raw = pd.read_csv(train_csv)
    if args.label_col not in df_raw.columns:
        raise ValueError(f"Label column {args.label_col!r} not found in train_csv: {train_csv}")

    df, removed, ex_debug = apply_integrated_exception_filter(
        df_raw,
        exception_col=args.exception_col,
        exception_positive_value=args.exception_positive_value,
        dataset_name="train",
    )
    feature_cols = select_feature_columns_for_training(
        df=df,
        label_col=args.label_col,
        feature_mode=feature_mode,
        selected_features=SELECTED_4_FEATURES,
        exception_col=args.exception_col,
    )
    X = df[feature_cols].copy()
    y = df[args.label_col].astype(int).to_numpy()
    groups = training_groups_from_dataframe(df)
    _cv, cv_folds, _cv_groups, cv_method = make_inner_cv(y=y, groups=groups, args=args)
    candidates = hyperparameter_candidates(model_name, args)

    print(
        f"Rows: original={len(df_raw)} "
        f"removed_by_integrated_exception_label={removed} used={len(df)}"
    )
    print(f"Features used ({len(feature_cols)}): {feature_cols}")
    print(f"Inner CV: {cv_method}; folds={cv_folds}; primordia={len(np.unique(groups))}")
    print(f"Hyperparameter candidates: {len(candidates)}")

    combo_key = f"{feature_mode}::{model_name}"
    return {
        "combo_key": combo_key,
        "model_name": model_name,
        "feature_mode": feature_mode,
        "out_dir": out_dir,
        "train_csv": train_csv,
        "df_raw": df_raw,
        "df": df,
        "removed": removed,
        "ex_debug": ex_debug,
        "feature_cols": feature_cols,
        "X": X,
        "y": y,
        "groups": groups,
        "cv_method": cv_method,
        "cv_folds": cv_folds,
        "candidates": candidates,
        "fbeta_col": f"F{args.beta}_pos1",
    }


def finalize_model_feature_context_from_global_search(
    context: dict[str, Any],
    search_state: dict[str, Any],
    args: argparse.Namespace,
) -> TrainResult:
    """
    Given all evaluated hyperparameter candidates for one combination, select
    the best candidate, refit it on the full training set, score train/test,
    and write the same output files as train_one_model_feature_mode().
    """
    model_name = context["model_name"]
    feature_mode = context["feature_mode"]
    out_dir = context["out_dir"]
    train_csv = context["train_csv"]
    df_raw = context["df_raw"]
    df = context["df"]
    removed = context["removed"]
    ex_debug = context["ex_debug"]
    feature_cols = context["feature_cols"]
    X = context["X"]
    y = context["y"]
    groups = context["groups"]
    cv_method = context["cv_method"]
    cv_folds = context["cv_folds"]
    candidates = context["candidates"]
    fbeta_col = context["fbeta_col"]

    candidate_rows = search_state.get("candidate_rows", [])
    best_candidate = search_state.get("best_candidate")
    if best_candidate is None:
        raise RuntimeError(f"No successful candidates for {feature_mode}/{model_name}")

    # Keep per-combination candidate files ordered by local candidate index.
    candidate_rows = sorted(candidate_rows, key=lambda r: int(r["candidate_index"]))

    best_args = best_candidate["args"]
    best_params = best_candidate["params"]
    best_row = best_candidate["best_row"]
    y_prob_cv = best_candidate["y_prob_cv"]
    cv_pr_auc = best_candidate["cv_pr_auc"]
    cv_roc_auc = best_candidate["cv_roc_auc"]
    chosen_threshold = float(best_row["threshold"])

    print("\n" + "=" * 90)
    print(f"Finalizing model={model_name} | feature_mode={feature_mode}")
    print(f"Successful candidates: {len(candidate_rows)}/{len(candidates)}")
    print(f"Selected hyperparameters: {printable_param_summary(best_params)}")

    hyper_csv = os.path.join(out_dir, "hyperparameter_search_train_cv.csv")
    pd.DataFrame(candidate_rows).to_csv(hyper_csv, index=False)

    sweep_df = best_candidate["sweep_df"]
    sweep_csv = os.path.join(out_dir, "threshold_sweep_matching_train_cv.csv")
    sweep_df.to_csv(sweep_csv, index=False)

    pipeline = build_pipeline(model_name, y, best_args)
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        pipeline.fit(X, y)
    y_prob_fit = pipeline.predict_proba(X)[:, 1]

    df_cv = df.copy()
    df_cv["p_division_cv"] = y_prob_cv
    df_cv["p_division_fit"] = y_prob_fit

    fig_png = None
    if args.make_plots:
        fig_png = os.path.join(out_dir, "metrics_vs_threshold_train_cv.png")
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        plt.figure(figsize=(7.0, 4.5))
        plt.plot(sweep_df["threshold"], sweep_df["precision_pos1"], label="Precision")
        plt.plot(sweep_df["threshold"], sweep_df["recall_pos1"], label="Recall")
        plt.plot(sweep_df["threshold"], sweep_df["F1_pos1"], label="F1")
        plt.plot(sweep_df["threshold"], sweep_df[fbeta_col], label=f"F{args.beta}")
        plt.scatter([chosen_threshold], [best_row[fbeta_col]], label="Chosen threshold")
        plt.xlabel("Threshold applied before matching")
        plt.ylabel("Metric after matching")
        plt.title(f"{model_name} / {feature_mode}: matching-aware threshold sweep")
        plt.legend()
        plt.tight_layout()
        plt.savefig(fig_png, dpi=300)
        plt.close()

    selected_fit = selected_by_file_after_threshold(df_cv, score_col="p_division_fit", threshold=chosen_threshold)
    selected_cv = selected_by_file_after_threshold(df_cv, score_col="p_division_cv", threshold=chosen_threshold)

    df_train_scores = add_prediction_columns(
        df_cv.rename(columns={"p_division_fit": "p_division_train_fit"}),
        score_col="p_division_train_fit",
        threshold=chosen_threshold,
        selected_by_file=selected_fit,
    )
    cv_keys = selected_edge_key_set(selected_cv)
    a = np.minimum(df_train_scores[ID1_COL].astype(int), df_train_scores[ID2_COL].astype(int))
    b = np.maximum(df_train_scores[ID1_COL].astype(int), df_train_scores[ID2_COL].astype(int))
    keys = list(zip(normalize_filename_series(df_train_scores[FILE_COL]), a, b))
    df_train_scores["p_division_train_cv"] = df_cv["p_division_cv"].to_numpy()
    df_train_scores["pred_raw_cv"] = (df_train_scores["p_division_train_cv"] >= chosen_threshold).astype(int)
    df_train_scores["pred_matched_cv"] = pd.Series(keys, index=df_train_scores.index).isin(cv_keys).astype(int)

    train_scores_csv = os.path.join(out_dir, "train_scores.csv")
    train_score_cols = [c for c in EXCLUDE_ALWAYS if c in df_train_scores.columns]
    train_score_cols += [args.label_col, "p_division_train_fit", "p_division_train_cv", "pred_raw", "pred_matched", "pred_raw_cv", "pred_matched_cv"]
    df_train_scores[train_score_cols].to_csv(train_scores_csv, index=False)

    matched_train_csv, matching_summary_train_csv = export_matching_outputs(
        df_all=df_cv,
        selected_by_file=selected_fit,
        score_col="p_division_fit",
        label_col=args.label_col,
        out_dir=out_dir,
        tag="train_fit",
    )

    interpretation_csv = export_model_interpretation(model_name, pipeline, X, y, feature_cols, best_args, out_dir)

    model_path = None
    if args.save_model:
        if joblib is None:
            print("[WARNING] joblib is unavailable; model was not saved.")
        else:
            model_path = os.path.join(out_dir, "model_pipeline.joblib")
            joblib.dump(pipeline, model_path)

    test_summary = score_dataset(
        dataset_name="test",
        csv_path=args.test_csv,
        pipeline=pipeline,
        feature_cols=feature_cols,
        chosen_threshold=chosen_threshold,
        label_col=args.label_col,
        exception_col=args.exception_col,
        exception_positive_value=args.exception_positive_value,
        out_dir=out_dir,
    )

    raw_metrics_fit = positive_class_metrics(
        df_cv[args.label_col].astype(int).to_numpy(),
        (df_cv["p_division_fit"].to_numpy() >= chosen_threshold).astype(int),
    )
    matched_metrics_fit = matching_metrics_from_selected(df_cv, selected_fit, label_col=args.label_col)
    matched_metrics_cv = matching_metrics_from_selected(df_cv, selected_cv, label_col=args.label_col)

    summary = {
        "model_name": model_name,
        "feature_mode": feature_mode,
        "train_csv": train_csv,
        "test_csv": args.test_csv,
        "exception_column": args.exception_col,
        "exception_positive_value": args.exception_positive_value,
        "exception_source": "integrated_column",
        "label_col": args.label_col,
        "n_rows_train_original": int(len(df_raw)),
        "n_rows_train_removed_by_integrated_exception_label": int(removed),
        "n_rows_train_used": int(len(df)),
        "integrated_exception_filter_train": ex_debug,
        "excluded_columns_from_training": EXCLUDE_ALWAYS,
        "selected_4_features_definition": SELECTED_4_FEATURES,
        "feature_cols_used": feature_cols,
        "n_features_used": int(len(feature_cols)),
        "cv_folds": int(cv_folds),
        "inner_cv": {
            "method": cv_method,
            "group_aware": bool(args.group_aware_inner_cv),
            "group_definition": "primordium inferred from fileName prefix",
            "n_groups": int(len(np.unique(groups))),
            "groups": sorted(map(str, np.unique(groups))),
            "folds": int(cv_folds),
            "shuffle": True,
        },
        "seed": int(args.seed),
        "software_versions": collect_software_versions(),
        "script_provenance": collect_script_provenance(),
        "methodological_references": METHODOLOGICAL_REFERENCES,
        "hyperparameter_search_enabled": bool(args.hyperparameter_search),
        "global_parallel_across_all_combinations": bool(getattr(args, "global_parallel_across_all_combinations", False)),
        "selected_hyperparameters": best_params,
        "hyperparameter_selection": {
            "method": f"maximize_F{args.beta}_after_matching_on_inner_grouped_cv_probabilities_then_refit_on_full_training_set",
            "outer_test_used_for_selection": False,
            "n_candidates": int(len(candidates)),
            "search_csv": hyper_csv,
            "selected_candidate_index": int(best_candidate["candidate_row"]["candidate_index"]),
        },
        "threshold_selection": {
            "method": f"maximize_F{args.beta}_after_matching_on_inner_grouped_cv_probabilities",
            "outer_test_used_for_selection": False,
            "n_thresholds": int(args.n_thresholds),
            "thr_low": float(args.thr_low),
            "thr_high": float(args.thr_high),
            "even_spacing": float(
                (args.thr_high - args.thr_low) / (args.n_thresholds - 1)
            ),
            "chosen_threshold": float(chosen_threshold),
            f"chosen_F{args.beta}_pos1": float(best_row[fbeta_col]),
            "chosen_precision_pos1": float(best_row["precision_pos1"]),
            "chosen_recall_pos1": float(best_row["recall_pos1"]),
            "chosen_F1_pos1": float(best_row["F1_pos1"]),
            "chosen_TP": int(best_row["TP"]),
            "chosen_FP": int(best_row["FP"]),
            "chosen_FN": int(best_row["FN"]),
        },
        "cv_score_level_train": {
            "pr_auc": float(cv_pr_auc),
            "roc_auc": float(cv_roc_auc),
        },
        "metrics_at_chosen_threshold_train": {
            "raw_edge_level_fit": raw_metrics_fit,
            "after_matching_fit": matched_metrics_fit,
            "after_matching_cv": matched_metrics_cv,
        },
        "test_summary": test_summary,
        "outputs": {
            "out_dir": out_dir,
            "model_pipeline_joblib": model_path,
            "interpretation_csv": interpretation_csv,
            "train_scores_csv": train_scores_csv,
            "hyperparameter_search_train_cv_csv": hyper_csv,
            "threshold_sweep_matching_train_cv_csv": sweep_csv,
            "metrics_vs_threshold_train_cv_png": fig_png,
            "matched_edges_train_fit_csv": matched_train_csv,
            "matching_summary_train_fit_csv": matching_summary_train_csv,
        },
    }

    summary_json = os.path.join(out_dir, "training_summary.json")
    with open(summary_json, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)

    print(f"Chosen threshold: {chosen_threshold:.6f}")
    print(
        f"Train CV after matching: precision={best_row['precision_pos1']:.4f}, "
        f"recall={best_row['recall_pos1']:.4f}, F1={best_row['F1_pos1']:.4f}, "
        f"F{args.beta}={best_row[fbeta_col]:.4f}"
    )
    if test_summary and "metrics" in test_summary:
        mt = test_summary["metrics"]["after_matching"]
        print(
            f"Test after matching:     precision={mt['precision_pos1']:.4f}, "
            f"recall={mt['recall_pos1']:.4f}, F1={mt['F1_pos1']:.4f}"
        )
    print(f"Saved summary: {summary_json}")

    return TrainResult(
        model_name=model_name,
        feature_mode=feature_mode,
        out_dir=out_dir,
        pipeline=pipeline,
        feature_cols=feature_cols,
        chosen_threshold=chosen_threshold,
        summary_json=summary_json,
    )


def run_global_parallel_search_across_all_combinations(args: argparse.Namespace) -> tuple[list[TrainResult], list[dict[str, str]]]:
    """
    Build one global task queue over every (feature_mode, model, hyperparameter)
    candidate, then evaluate that queue with a single process pool.

    This is different from train_one_model_feature_mode(), which parallelizes only
    within one model/feature-mode block. With this global scheduler, if all-feature
    LR has only 9 candidates, the remaining cores are immediately filled by
    selected4 LR, SVM, XGBoost, and LightGBM candidates.
    """
    ensure_dir(args.out_dir)
    failures: list[dict[str, str]] = []
    results: list[TrainResult] = []

    contexts: dict[str, dict[str, Any]] = {}
    combo_order: list[str] = []
    payloads: list[dict[str, Any]] = []

    total_candidates = 0
    for feature_mode in args.feature_modes:
        for model_name in args.models:
            context = prepare_model_feature_context(model_name=model_name, feature_mode=feature_mode, args=args)
            combo_key = context["combo_key"]
            contexts[combo_key] = context
            combo_order.append(combo_key)
            total_candidates += len(context["candidates"])

    print("\n" + "=" * 90)
    print("GLOBAL parallel scheduler is enabled.")
    print(f"Total hyperparameter candidates across ALL feature modes/models: {total_candidates}")
    print(f"MAX_PARALLEL_CANDIDATES={args.max_parallel_candidates}; N_JOBS_PER_CANDIDATE={args.n_jobs_per_candidate}")
    print("This can run selected4 LR/SVM and tree-model candidates while all-feature LR is still running.")

    global_index = 0
    for combo_key in combo_order:
        context = contexts[combo_key]
        model_name = context["model_name"]
        feature_mode = context["feature_mode"]
        candidates = context["candidates"]
        for local_i, (candidate_args, candidate_params) in enumerate(candidates, start=1):
            global_index += 1
            worker_args = prepare_args_for_worker(args, candidate_args)
            payloads.append({
                "combo_key": combo_key,
                "global_candidate_index": global_index,
                "total_global_candidates": total_candidates,
                "model_name": model_name,
                "feature_mode": feature_mode,
                "candidate_index": local_i,
                "total_candidates": len(candidates),
                "candidate_args": worker_args,
                "candidate_params": candidate_params,
                "X": context["X"],
                "y": context["y"],
                "groups": context["groups"],
                "df": context["df"],
                "cv_folds": context["cv_folds"],
                "fbeta_col": context["fbeta_col"],
            })
            if bool(getattr(args, "show_per_candidate_start_finish", True)):
                print(
                    f"  Submitted global {global_index}/{total_candidates}: "
                    f"{feature_mode}/{model_name} candidate {local_i}/{len(candidates)} | "
                    f"{printable_param_summary(candidate_params)}"
                )

    search_states: dict[str, dict[str, Any]] = {
        combo_key: {"candidate_rows": [], "best_candidate": None}
        for combo_key in combo_order
    }

    max_workers = min(int(getattr(args, "max_parallel_candidates", 1)), len(payloads))
    max_workers = max(1, max_workers)
    start_method = str(getattr(args, "parallel_start_method", "fork"))
    try:
        mp_context = mp.get_context(start_method)
    except ValueError:
        print(f"[WARNING] Unknown PARALLEL_START_METHOD={start_method!r}; using Python default.")
        mp_context = None

    global_bar = make_progress_bar(
        total=len(payloads),
        desc="Whole global search",
        unit="candidate",
        args=args,
        leave=True,
        disable=not bool(getattr(args, "show_whole_process_progress", True)),
    )

    executor_kwargs = {"max_workers": max_workers}
    if mp_context is not None:
        executor_kwargs["mp_context"] = mp_context

    try:
        with ProcessPoolExecutor(**executor_kwargs) as executor:
            future_to_payload = {
                executor.submit(evaluate_one_hyperparameter_candidate_worker, payload): payload
                for payload in payloads
            }
            for future in as_completed(future_to_payload):
                payload = future_to_payload[future]
                combo_key = payload["combo_key"]
                feature_mode = payload["feature_mode"]
                model_name = payload["model_name"]
                local_i = payload["candidate_index"]
                local_n = payload["total_candidates"]
                global_i = payload["global_candidate_index"]

                try:
                    result = future.result()
                except Exception as e:
                    result = {
                        "ok": False,
                        "error": str(e),
                        "traceback": "",
                        "elapsed_seconds": 0.0,
                        "combo_key": combo_key,
                        "feature_mode": feature_mode,
                        "model_name": model_name,
                        "candidate_index": local_i,
                        "global_candidate_index": global_i,
                        "candidate_params": payload.get("candidate_params", {}),
                    }

                elapsed = float(result.get("elapsed_seconds", 0.0))

                if not result.get("ok", False):
                    msg = (
                        f"FAILED global candidate {global_i}/{len(payloads)} "
                        f"for {feature_mode}/{model_name} local candidate {local_i}/{local_n}: "
                        f"{result.get('error')}\n{result.get('traceback', '')}"
                    )
                    print("\n[ERROR] " + msg)
                    failures.append({
                        "model_name": model_name,
                        "feature_mode": feature_mode,
                        "candidate_index": str(local_i),
                        "global_candidate_index": str(global_i),
                        "error": str(result.get("error")),
                    })
                    progress_update(global_bar, 1, postfix=f"FAILED {feature_mode}/{model_name} cand {local_i}")
                    if not args.continue_on_error:
                        raise RuntimeError(msg)
                    continue

                candidate_row = result["candidate_row"]
                # Store both local and global identity in the per-combination CSV.
                candidate_row["global_candidate_index"] = int(result.get("global_candidate_index", global_i))
                candidate_row["feature_mode"] = feature_mode
                candidate_row["model_name"] = model_name
                search_states[combo_key]["candidate_rows"].append(candidate_row)

                fbeta_col = contexts[combo_key]["fbeta_col"]
                old_best = search_states[combo_key]["best_candidate"]
                if candidate_row_is_better(
                    candidate_row,
                    old_best["candidate_row"] if old_best is not None else None,
                    fbeta_col,
                ):
                    search_states[combo_key]["best_candidate"] = {
                        "args": _copy_args_with_updates(args, result["candidate_params"]),
                        "params": result["candidate_params"],
                        "candidate_row": candidate_row,
                        "sweep_df": pd.DataFrame(result["sweep_records"]),
                        "best_row": result["best_row"],
                        "y_prob_cv": result["y_prob_cv"],
                        "cv_pr_auc": result["cv_pr_auc"],
                        "cv_roc_auc": result["cv_roc_auc"],
                    }

                if bool(getattr(args, "show_per_candidate_start_finish", True)):
                    print(
                        f"  Finished global {global_i}/{len(payloads)} | "
                        f"{feature_mode}/{model_name} candidate {local_i}/{local_n} "
                        f"in {format_duration(elapsed)} | "
                        f"threshold={candidate_row['chosen_threshold']:.6f}, "
                        f"CV F1={candidate_row['F1_pos1']:.4f} | "
                        f"{printable_param_summary(result['candidate_params'])}"
                    )

                progress_update(
                    global_bar,
                    1,
                    postfix=(
                        f"{feature_mode}/{model_name} cand {local_i}/{local_n} "
                        f"F1={candidate_row['F1_pos1']:.4f} elapsed={format_duration(elapsed)}"
                    ),
                )
    finally:
        progress_close(global_bar)

    # Refit and score the selected candidate for each model/feature-mode.
    for combo_key in combo_order:
        try:
            result = finalize_model_feature_context_from_global_search(
                context=contexts[combo_key],
                search_state=search_states[combo_key],
                args=args,
            )
            results.append(result)
        except Exception as e:
            context = contexts[combo_key]
            msg = f"FAILED finalization for {context['feature_mode']}/{context['model_name']}: {e}"
            print("\n[ERROR] " + msg)
            failures.append({
                "model_name": context["model_name"],
                "feature_mode": context["feature_mode"],
                "error": str(e),
            })
            if not args.continue_on_error:
                raise

    return results, failures

def write_overall_summary(results: list[TrainResult], args: argparse.Namespace) -> str:
    rows = []
    details = []
    for result in results:
        with open(result.summary_json, "r", encoding="utf-8") as f:
            summary = json.load(f)
        details.append(summary)
        row = {
            "feature_mode": result.feature_mode,
            "model_name": result.model_name,
            "n_features": summary["n_features_used"],
            "chosen_threshold": summary["threshold_selection"]["chosen_threshold"],
            "selected_hyperparameters": json.dumps(summary.get("selected_hyperparameters", {}), sort_keys=True),
            "hyperparameter_search_csv": summary.get("hyperparameter_selection", {}).get("search_csv"),
            "train_cv_pr_auc": summary["cv_score_level_train"]["pr_auc"],
            "train_cv_roc_auc": summary["cv_score_level_train"]["roc_auc"],
            "train_cv_precision_after_matching": summary["threshold_selection"]["chosen_precision_pos1"],
            "train_cv_recall_after_matching": summary["threshold_selection"]["chosen_recall_pos1"],
            "train_cv_F1_after_matching": summary["threshold_selection"]["chosen_F1_pos1"],
            "summary_json": result.summary_json,
        }
        test_summary = summary.get("test_summary") or {}
        test_metrics = test_summary.get("metrics", {}).get("after_matching")
        if test_metrics:
            row.update({
                "test_precision_after_matching": test_metrics["precision_pos1"],
                "test_recall_after_matching": test_metrics["recall_pos1"],
                "test_F1_after_matching": test_metrics["F1_pos1"],
                "test_TP": test_metrics["TP"],
                "test_FP": test_metrics["FP"],
                "test_FN": test_metrics["FN"],
            })
        rows.append(row)

    ensure_dir(args.out_dir)
    overall_csv = os.path.join(args.out_dir, "overall_model_summary.csv")
    pd.DataFrame(rows).to_csv(overall_csv, index=False)

    overall_json = os.path.join(args.out_dir, "overall_model_summary.json")
    with open(overall_json, "w", encoding="utf-8") as f:
        json.dump({
            "train_csv": args.train_csv,
            "test_csv": args.test_csv,
            "exception_source": "integrated_column",
            "exception_column": args.exception_col,
            "exception_positive_value": args.exception_positive_value,
            "models_run": [r.model_name for r in results],
            "feature_modes_run": [r.feature_mode for r in results],
            "summary_csv": overall_csv,
            "details": details,
        }, f, indent=2)

    print("\n" + "=" * 90)
    print(f"Overall summary CSV:  {overall_csv}")
    print(f"Overall summary JSON: {overall_json}")
    return overall_csv


def _normalize_config_list(value: Any, aliases: dict[str, str], setting_name: str) -> list[str]:
    """Normalize a list such as ["lr", "svm"] or a comma string such as "lr,svm"."""
    if value is None:
        return []
    if isinstance(value, str):
        raw_items = [x.strip() for x in value.split(",")]
    else:
        raw_items = [str(x).strip() for x in value]

    items: list[str] = []
    for raw in raw_items:
        if not raw:
            continue
        key = raw.lower().replace("-", "_")
        if key not in aliases:
            valid = ", ".join(sorted(set(aliases.values())))
            raise ValueError(f"Unknown {setting_name}: {raw!r}. Valid values include: {valid}")
        canonical = aliases[key]
        if canonical not in items:
            items.append(canonical)
    return items


def parse_args(argv: list[str] | None = None) -> SimpleNamespace:
    """
    Build the same args object as the old argparse version, but from the
    USER SETTINGS block at the top of this file.

    The argv argument is intentionally ignored. This script is configured by
    editing constants, not by command-line parameters.
    """
    args = SimpleNamespace(
        train_csv=TRAIN_CSV,
        test_csv=TEST_CSV,
        out_dir=OUT_DIR,
        label_col=LABEL_COL,
        exception_col=EXCEPTION_COL,
        exception_positive_value=EXCEPTION_POSITIVE_VALUE,
        models=_normalize_config_list(MODELS_TO_RUN, MODEL_ALIASES, "model"),
        feature_modes=_normalize_config_list(FEATURE_MODES_TO_RUN, FEATURE_MODE_ALIASES, "feature_mode"),
        seed=SEED,
        cv_folds=CV_FOLDS,
        group_aware_inner_cv=GROUP_AWARE_INNER_CV,
        beta=BETA,
        n_thresholds=N_THRESHOLDS,
        thr_low=THR_LOW,
        thr_high=THR_HIGH,
        n_jobs=N_JOBS,
        save_model=SAVE_MODEL,
        make_plots=MAKE_PLOTS,
        show_progress_bars=SHOW_PROGRESS_BARS,
        show_whole_process_progress=SHOW_WHOLE_PROCESS_PROGRESS,
        show_cv_fold_progress=SHOW_CV_FOLD_PROGRESS,
        show_threshold_progress=SHOW_THRESHOLD_PROGRESS,
        progress_mininterval_seconds=PROGRESS_MININTERVAL_SECONDS,
        parallel_hyperparameter_candidates=PARALLEL_HYPERPARAMETER_CANDIDATES,
        global_parallel_across_all_combinations=GLOBAL_PARALLEL_ACROSS_ALL_COMBINATIONS,
        max_parallel_candidates=MAX_PARALLEL_CANDIDATES,
        n_jobs_per_candidate=N_JOBS_PER_CANDIDATE,
        parallel_start_method=PARALLEL_START_METHOD,
        show_per_candidate_start_finish=SHOW_PER_CANDIDATE_START_FINISH,
        global_parallel_across_all_split_model_feature_jobs=(
            GLOBAL_PARALLEL_ACROSS_ALL_SPLIT_MODEL_FEATURE_JOBS
        ),
        max_total_parallel_jobs=MAX_TOTAL_PARALLEL_JOBS,
        n_jobs_per_global_job=N_JOBS_PER_GLOBAL_JOB,
        save_global_worker_logs=SAVE_GLOBAL_WORKER_LOGS,
        class_weight_balanced=CLASS_WEIGHT_BALANCED,
        continue_on_error=CONTINUE_ON_ERROR,
        hyperparameter_search=HYPERPARAMETER_SEARCH,
        lr_C=LR_C,
        lr_C_list=LR_C_LIST,
        lr_max_iter=LR_MAX_ITER,
        svm_C=SVM_C,
        svm_C_list=SVM_C_LIST,
        svm_max_iter=SVM_MAX_ITER,
        svm_calib_method=SVM_CALIB_METHOD,
        svm_calib_method_list=SVM_CALIB_METHOD_LIST,
        svm_calib_cv=SVM_CALIB_CV,
        tree_use_imputer=TREE_USE_IMPUTER,
        xgb_n_estimators=XGB_N_ESTIMATORS,
        xgb_learning_rate=XGB_LEARNING_RATE,
        xgb_max_depth=XGB_MAX_DEPTH,
        xgb_max_depth_list=XGB_MAX_DEPTH_LIST,
        xgb_min_child_weight=XGB_MIN_CHILD_WEIGHT,
        xgb_min_child_weight_list=XGB_MIN_CHILD_WEIGHT_LIST,
        xgb_subsample=XGB_SUBSAMPLE,
        xgb_subsample_list=XGB_SUBSAMPLE_LIST,
        xgb_colsample_bytree=XGB_COLSAMPLE_BYTREE,
        xgb_colsample_bytree_list=XGB_COLSAMPLE_BYTREE_LIST,
        xgb_reg_lambda=XGB_REG_LAMBDA,
        xgb_reg_lambda_list=XGB_REG_LAMBDA_LIST,
        xgb_reg_alpha=XGB_REG_ALPHA,
        xgb_reg_alpha_list=XGB_REG_ALPHA_LIST,
        xgb_gamma=XGB_GAMMA,
        xgb_gamma_list=XGB_GAMMA_LIST,
        xgb_tree_method=XGB_TREE_METHOD,
        lgbm_n_estimators=LGBM_N_ESTIMATORS,
        lgbm_learning_rate=LGBM_LEARNING_RATE,
        lgbm_num_leaves=LGBM_NUM_LEAVES,
        lgbm_num_leaves_list=LGBM_NUM_LEAVES_LIST,
        lgbm_max_depth=LGBM_MAX_DEPTH,
        lgbm_max_depth_list=LGBM_MAX_DEPTH_LIST,
        lgbm_min_child_samples=LGBM_MIN_CHILD_SAMPLES,
        lgbm_min_child_samples_list=LGBM_MIN_CHILD_SAMPLES_LIST,
        lgbm_subsample=LGBM_SUBSAMPLE,
        lgbm_subsample_list=LGBM_SUBSAMPLE_LIST,
        lgbm_subsample_freq=LGBM_SUBSAMPLE_FREQ,
        lgbm_colsample_bytree=LGBM_COLSAMPLE_BYTREE,
        lgbm_colsample_bytree_list=LGBM_COLSAMPLE_BYTREE_LIST,
        lgbm_reg_lambda=LGBM_REG_LAMBDA,
        lgbm_reg_lambda_list=LGBM_REG_LAMBDA_LIST,
        lgbm_reg_alpha=LGBM_REG_ALPHA,
        lgbm_reg_alpha_list=LGBM_REG_ALPHA_LIST,
    )

    if not args.models:
        raise ValueError("No models selected. Edit MODELS_TO_RUN near the top of the file.")
    if not args.feature_modes:
        raise ValueError("No feature modes selected. Edit FEATURE_MODES_TO_RUN near the top of the file.")
    if not str(args.exception_col).strip():
        raise ValueError("EXCEPTION_COL must be a non-empty column name.")
    if args.exception_col == args.label_col:
        raise ValueError("EXCEPTION_COL and LABEL_COL must be different columns.")
    if int(args.cv_folds) < 2:
        raise ValueError("CV_FOLDS must be >= 2")
    if int(args.max_total_parallel_jobs) < 1:
        raise ValueError("MAX_TOTAL_PARALLEL_JOBS must be >= 1")
    if int(args.n_jobs_per_global_job) != 1:
        raise ValueError(
            "N_JOBS_PER_GLOBAL_JOB must remain 1 when using the global multi-worker scheduler."
        )
    if args.beta <= 0:
        raise ValueError("BETA must be > 0")
    if args.n_thresholds < 2:
        raise ValueError("N_THRESHOLDS must be >= 2")
    if args.thr_low >= args.thr_high:
        raise ValueError("THR_LOW must be < THR_HIGH")
    if args.svm_calib_method not in {"sigmoid", "isotonic"}:
        raise ValueError('SVM_CALIB_METHOD must be "sigmoid" or "isotonic"')
    if int(args.svm_calib_cv) < 2:
        raise ValueError("SVM_CALIB_CV must be >= 2")

    for setting_name, values in {
        "LR_C_LIST": _as_list(args.lr_C_list),
        "SVM_C_LIST": _as_list(args.svm_C_list),
    }.items():
        if any(float(value) <= 0 for value in values):
            raise ValueError(f"Every value in {setting_name} must be > 0")

    for setting_name, values in {
        "XGB_SUBSAMPLE_LIST": _as_list(args.xgb_subsample_list),
        "XGB_COLSAMPLE_BYTREE_LIST": _as_list(args.xgb_colsample_bytree_list),
        "LGBM_SUBSAMPLE_LIST": _as_list(args.lgbm_subsample_list),
        "LGBM_COLSAMPLE_BYTREE_LIST": _as_list(args.lgbm_colsample_bytree_list),
    }.items():
        if any(not (0.0 < float(value) <= 1.0) for value in values):
            raise ValueError(f"Every value in {setting_name} must be in (0, 1]")

    if int(args.xgb_n_estimators) <= 0 or float(args.xgb_learning_rate) <= 0:
        raise ValueError("XGBoost n_estimators and learning_rate must be > 0")
    if int(args.lgbm_n_estimators) <= 0 or float(args.lgbm_learning_rate) <= 0:
        raise ValueError("LightGBM n_estimators and learning_rate must be > 0")
    if int(args.lgbm_subsample_freq) < 0:
        raise ValueError("LGBM_SUBSAMPLE_FREQ must be >= 0")
    if any(float(value) < 1.0 for value in _as_list(args.lgbm_subsample_list)) and int(
        args.lgbm_subsample_freq
    ) == 0:
        raise ValueError(
            "LightGBM subsample values below 1 require LGBM_SUBSAMPLE_FREQ > 0; "
            "otherwise row subsampling is disabled."
        )
    if any(int(value) <= 1 for value in _as_list(args.lgbm_num_leaves_list)):
        raise ValueError("Every value in LGBM_NUM_LEAVES_LIST must be > 1")
    if any(int(value) <= 0 for value in _as_list(args.lgbm_min_child_samples_list)):
        raise ValueError("Every value in LGBM_MIN_CHILD_SAMPLES_LIST must be > 0")

    return args

# =============================================================================
# Multi-split workflow: train/evaluate every accepted 70:30 group-aware split
# =============================================================================
def infer_primordium_from_filename(filename: str) -> str:
    """Infer tracked primordium from a snapshot filename prefix.

    Examples: sample4_36h.json -> sample4 and sample10_22h -> sample10.
    The final ``_<time>h`` token is removed; no external group-mapping CSV is used.
    """
    name = Path(str(filename).strip()).name
    stem = re.sub(r"\.[^.]+$", "", name)
    match = re.fullmatch(r"(.+)_\d+(?:\.\d+)?h", stem, flags=re.IGNORECASE)
    if match is None or not match.group(1).strip():
        raise ValueError(
            f"Cannot infer a primordium prefix from {filename!r}. Expected a snapshot "
            f"name such as 'sample4_36h.json'."
        )
    return match.group(1).strip()


def assign_primordia_from_filenames(df: pd.DataFrame) -> pd.DataFrame:
    """Add temporary _split_group using the prefix embedded in fileName."""
    if FILE_COL not in df.columns:
        raise ValueError(f"Required filename column {FILE_COL!r} was not found in ALL_DATA_CSV.")
    if df[FILE_COL].isna().any():
        raise ValueError(f"Column {FILE_COL!r} contains missing filenames.")

    out = df.copy()
    out["_split_group"] = out[FILE_COL].astype(str).map(infer_primordium_from_filename)
    if out["_split_group"].eq("").any():
        raise RuntimeError("At least one snapshot was assigned an empty primordium prefix.")
    return out


def parse_group_list_cell(value: Any) -> list[str]:
    """Parse semicolon-separated train_set/test_set cells (also accepts commas/space)."""
    if value is None or (isinstance(value, float) and np.isnan(value)):
        return []
    text_value = str(value).strip()
    if text_value == "" or text_value.lower() == "nan":
        return []
    groups = [x.strip() for x in re.split(r"[;,\s]+", text_value) if x.strip()]
    if len(groups) != len(set(groups)):
        raise ValueError(f"A split cell contains duplicated primordia: {value!r}")
    return groups


def make_train_test_csvs_for_one_accepted_split(
    full_df_with_groups: pd.DataFrame,
    split_row: pd.Series,
    split_index: int,
    split_root_dir: str,
    dataset_filter_debug: dict[str, Any],
    label_col: str,
    exception_col: str,
    exception_positive_value: int | float,
) -> dict[str, Any]:
    """Generate and validate train/test CSVs for one prefix-defined split."""
    split_name = str(split_row.get("split_id", f"split_{split_index + 1:03d}")).strip()
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", split_name):
        raise ValueError(f"Unsafe or empty split_id at row {split_index}: {split_name!r}")
    split_dir = os.path.join(split_root_dir, split_name)
    ensure_dir(split_dir)

    all_groups = set(full_df_with_groups["_split_group"].astype(str).unique())

    train_groups = set(parse_group_list_cell(split_row.get("train_set", "")))
    test_groups = set(parse_group_list_cell(split_row.get("test_set", "")))

    if not train_groups and not test_groups:
        raise ValueError(f"{split_name} has empty train_set and test_set cells.")
    if not train_groups or not test_groups:
        raise ValueError(f"{split_name} must explicitly define both train_set and test_set.")

    unknown = sorted((train_groups | test_groups) - all_groups)
    if unknown:
        raise ValueError(f"{split_name} contains primordia not present in ALL_DATA_CSV: {unknown}")

    overlap = sorted(train_groups & test_groups)
    if overlap:
        raise ValueError(f"{split_name} has primordia in both train and test: {overlap}")

    missing_from_partition = sorted(all_groups - train_groups - test_groups)
    if missing_from_partition:
        raise ValueError(f"{split_name} does not assign these primordia: {missing_from_partition}")

    train_df = full_df_with_groups[full_df_with_groups["_split_group"].isin(train_groups)].copy()
    test_df = full_df_with_groups[full_df_with_groups["_split_group"].isin(test_groups)].copy()
    if train_df.empty or test_df.empty:
        raise ValueError(f"{split_name} produced an empty train or test dataframe.")
    if len(train_df) + len(test_df) != len(full_df_with_groups):
        raise RuntimeError(f"{split_name} did not partition every analyzable row exactly once.")
    if (train_df[exception_col] == exception_positive_value).any() or (
        test_df[exception_col] == exception_positive_value
    ).any():
        raise RuntimeError(f"Integrated exception rows entered {split_name} after filtering.")

    train_csv = os.path.join(split_dir, "neighbor_pairs_train.csv")
    test_csv = os.path.join(split_dir, "neighbor_pairs_test.csv")
    assignment_csv = os.path.join(split_dir, "primordium_assignment.csv")
    split_metadata_json = os.path.join(split_dir, "split_metadata.json")

    # Export train/test without helper column, matching the old script's expected input.
    train_df.drop(columns=["_split_group"]).to_csv(train_csv, index=False)
    test_df.drop(columns=["_split_group"]).to_csv(test_csv, index=False)

    assignment_df = (
        full_df_with_groups.groupby("_split_group", as_index=False)
        .agg(
            n_rows=(FILE_COL, "size"),
            n_filenames=(FILE_COL, "nunique"),
            n_daughter_pairs=(label_col, "sum"),
            filenames=(FILE_COL, lambda x: ";".join(sorted(map(str, set(x))))),
        )
        .rename(columns={"_split_group": "primordium"})
    )
    assignment_df["split"] = assignment_df["primordium"].apply(
        lambda g: "train" if g in train_groups else "test"
    )
    assignment_df = assignment_df[
        ["primordium", "split", "n_rows", "n_filenames", "n_daughter_pairs", "filenames"]
    ]
    assignment_df.to_csv(assignment_csv, index=False)

    train_pos = int(train_df[label_col].sum())
    test_pos = int(test_df[label_col].sum())
    train_neg = int(len(train_df) - train_pos)
    test_neg = int(len(test_df) - test_pos)
    total_rows = int(len(train_df) + len(test_df))
    train_fraction = float(len(train_df) / total_rows)
    test_fraction = float(len(test_df) / total_rows)
    train_positive_rate = float(train_pos / len(train_df))
    test_positive_rate = float(test_pos / len(test_df))
    positive_rate_difference = float(abs(train_positive_rate - test_positive_rate))

    metadata = {
        "split_index": int(split_index),
        "split_id": split_name,
        "split_name": split_name,
        "train_csv": train_csv,
        "test_csv": test_csv,
        "assignment_csv": assignment_csv,
        "train_rows": int(len(train_df)),
        "test_rows": int(len(test_df)),
        "total_rows": total_rows,
        "total_input_rows": int(dataset_filter_debug["n_rows_original"]),
        "excluded_exception_rows": int(dataset_filter_debug["n_rows_removed_as_exceptions"]),
        "exception_column": exception_col,
        "exception_positive_value": exception_positive_value,
        "train_fraction": train_fraction,
        "test_fraction": test_fraction,
        "n_train_primordia": int(len(train_groups)),
        "n_test_primordia": int(len(test_groups)),
        "train_set": ";".join(sorted(train_groups)),
        "test_set": ";".join(sorted(test_groups)),
        "train_daughter_pairs": train_pos,
        "test_daughter_pairs": test_pos,
        "total_daughter_pairs": int(train_pos + test_pos),
        "train_non_daughter_pairs": train_neg,
        "test_non_daughter_pairs": test_neg,
        "train_daughter_pair_frequency": train_positive_rate,
        "test_daughter_pair_frequency": test_positive_rate,
        "daughter_pair_frequency_difference": positive_rate_difference,
    }

    integer_checks = {
        "n_train_primordia": metadata["n_train_primordia"],
        "n_test_primordia": metadata["n_test_primordia"],
        "train_rows": metadata["train_rows"],
        "test_rows": metadata["test_rows"],
        "total_rows": metadata["total_rows"],
        "total_input_rows": metadata["total_input_rows"],
        "excluded_exception_rows": metadata["excluded_exception_rows"],
        "train_daughter_pairs": train_pos,
        "test_daughter_pairs": test_pos,
        "total_daughter_pairs": train_pos + test_pos,
        "train_non_daughter_pairs": train_neg,
        "test_non_daughter_pairs": test_neg,
    }
    for column, calculated in integer_checks.items():
        if column in split_row.index and not pd.isna(split_row[column]):
            stated = int(split_row[column])
            if stated != int(calculated):
                raise ValueError(
                    f"{split_name}: split table {column}={stated}, but recalculation gives {calculated}."
                )

    float_checks = {
        "train_fraction": train_fraction,
        "test_fraction": test_fraction,
        "train_daughter_pair_frequency": train_positive_rate,
        "test_daughter_pair_frequency": test_positive_rate,
        "daughter_pair_frequency_difference": positive_rate_difference,
        "train_fraction_percent": 100.0 * train_fraction,
        "test_fraction_percent": 100.0 * test_fraction,
        "train_daughter_pair_frequency_percent": 100.0 * train_positive_rate,
        "test_daughter_pair_frequency_percent": 100.0 * test_positive_rate,
        "daughter_pair_frequency_difference_pp": 100.0 * positive_rate_difference,
    }
    for column, calculated in float_checks.items():
        if column in split_row.index and not pd.isna(split_row[column]):
            stated = float(split_row[column])
            if not np.isclose(stated, calculated, rtol=0.0, atol=1e-9):
                raise ValueError(
                    f"{split_name}: split table {column}={stated}, but recalculation gives {calculated}."
                )

    if "exception_column" in split_row.index:
        stated_exception_col = str(split_row["exception_column"]).strip()
        if stated_exception_col != exception_col:
            raise ValueError(
                f"{split_name}: split table exception_column={stated_exception_col!r}, "
                f"but the analysis setting is {exception_col!r}."
            )
    if "exception_positive_value" in split_row.index:
        if float(split_row["exception_positive_value"]) != float(exception_positive_value):
            raise ValueError(f"{split_name}: exception positive value does not match the analysis setting.")
    if "satisfies_conditions" in split_row.index and not bool(split_row["satisfies_conditions"]):
        raise ValueError(f"{split_name} is marked satisfies_conditions=False.")

    target_fraction = float(split_row.get("target_train_fraction", 0.70))
    fraction_tolerance = float(split_row.get("fraction_tolerance", 0.02))
    max_rate_difference = float(split_row.get("max_daughter_pair_frequency_difference", 0.01))
    if abs(train_fraction - target_fraction) > fraction_tolerance + 1e-12:
        raise ValueError(f"{split_name} violates the requested training-fraction tolerance.")
    if positive_rate_difference >= max_rate_difference:
        raise ValueError(f"{split_name} violates the daughter-pair-frequency criterion.")

    with open(split_metadata_json, "w", encoding="utf-8") as f:
        json.dump(metadata, f, indent=2)

    return metadata


def run_single_train_test_workflow(args: argparse.Namespace) -> tuple[list[TrainResult], list[dict[str, str]], str | None]:
    """
    Run the original one-split workflow and return results/failures/overall_summary_csv.
    This is factored out so the multi-split workflow can reuse it for every accepted split.
    """
    ensure_dir(args.out_dir)

    if (
        bool(getattr(args, "parallel_hyperparameter_candidates", False))
        and bool(getattr(args, "global_parallel_across_all_combinations", False))
    ):
        results, failures = run_global_parallel_search_across_all_combinations(args)
    else:
        results = []
        failures = []

        total_candidates = 0
        for feature_mode in args.feature_modes:
            for model_name in args.models:
                total_candidates += len(hyperparameter_candidates(model_name, args))
        print(f"Total hyperparameter candidates to evaluate: {total_candidates}")
        args._global_progress_bar = make_progress_bar(
            total=total_candidates,
            desc="Whole process",
            unit="candidate",
            args=args,
            leave=True,
            disable=not bool(getattr(args, "show_whole_process_progress", True)),
        )

        try:
            for feature_mode in args.feature_modes:
                for model_name in args.models:
                    try:
                        result = train_one_model_feature_mode(model_name=model_name, feature_mode=feature_mode, args=args)
                        results.append(result)
                    except Exception as e:
                        msg = f"FAILED model={model_name} feature_mode={feature_mode}: {e}"
                        print("\n[ERROR] " + msg)
                        failures.append({"model_name": model_name, "feature_mode": feature_mode, "error": str(e)})
                        if not args.continue_on_error:
                            raise
        finally:
            progress_close(getattr(args, "_global_progress_bar", None))

    overall_csv = None
    if results:
        overall_csv = write_overall_summary(results, args)

    if failures:
        failure_json = os.path.join(args.out_dir, "failures.json")
        with open(failure_json, "w", encoding="utf-8") as f:
            json.dump(failures, f, indent=2)
        print(f"Failures were saved to: {failure_json}")
        if not results:
            raise RuntimeError("All model runs failed. See failures.json.")

    return results, failures, overall_csv


def collect_split_overall_rows(overall_csv: str, split_metadata: dict[str, Any]) -> pd.DataFrame:
    """Read one split's overall_model_summary.csv and add split-level metadata columns."""
    df = pd.read_csv(overall_csv)
    for key, value in split_metadata.items():
        # Avoid overwriting model-level columns if any name collides.
        col = key if key not in df.columns else f"split_{key}"
        df[col] = value
    return df


def write_mean_sd_summary_across_splits(per_split_df: pd.DataFrame, out_dir: str) -> tuple[str, str]:
    """
    Write final summaries:
      1) one row per model + feature_mode, with mean ± SD for each metric;
      2) a long table that is easier for plotting/statistics.
    """
    ensure_dir(out_dir)

    group_cols = ["feature_mode", "model_name"]
    preferred_metric_cols = [
        "chosen_threshold",
        "train_cv_pr_auc",
        "train_cv_roc_auc",
        "train_cv_precision_after_matching",
        "train_cv_recall_after_matching",
        "train_cv_F1_after_matching",
        "test_precision_after_matching",
        "test_recall_after_matching",
        "test_F1_after_matching",
        "test_TP",
        "test_FP",
        "test_FN",
    ]
    metric_cols = [
        c for c in preferred_metric_cols
        if c in per_split_df.columns and pd.api.types.is_numeric_dtype(per_split_df[c])
    ]

    wide_rows = []
    long_rows = []

    for (feature_mode, model_name), g in per_split_df.groupby(group_cols, dropna=False):
        row = {
            "feature_mode": feature_mode,
            "model_name": model_name,
            "n_splits_successful": int(g["split_index"].nunique()) if "split_index" in g.columns else int(len(g)),
        }

        if "n_features" in g.columns:
            vals = pd.to_numeric(g["n_features"], errors="coerce").dropna().unique()
            if len(vals):
                row["n_features"] = int(vals[0])

        for metric in metric_cols:
            values = pd.to_numeric(g[metric], errors="coerce").dropna()
            n = int(len(values))
            mean = float(values.mean()) if n else np.nan
            sd = float(values.std(ddof=1)) if n > 1 else 0.0 if n == 1 else np.nan
            median = float(values.median()) if n else np.nan
            q025 = float(values.quantile(0.025)) if n else np.nan
            q975 = float(values.quantile(0.975)) if n else np.nan
            vmin = float(values.min()) if n else np.nan
            vmax = float(values.max()) if n else np.nan

            row[f"{metric}_mean"] = mean
            row[f"{metric}_sd"] = sd
            row[f"{metric}_mean_pm_sd"] = f"{mean:.6g} ± {sd:.6g}" if n else ""
            row[f"{metric}_median"] = median
            row[f"{metric}_q025"] = q025
            row[f"{metric}_q975"] = q975
            row[f"{metric}_min"] = vmin
            row[f"{metric}_max"] = vmax

            long_rows.append({
                "feature_mode": feature_mode,
                "model_name": model_name,
                "metric": metric,
                "n": n,
                "mean": mean,
                "sd": sd,
                "mean_pm_sd": f"{mean:.6g} ± {sd:.6g}" if n else "",
                "median": median,
                "q025": q025,
                "q975": q975,
                "min": vmin,
                "max": vmax,
            })

        wide_rows.append(row)

    wide_df = pd.DataFrame(wide_rows).sort_values(group_cols)
    long_df = pd.DataFrame(long_rows).sort_values(group_cols + ["metric"])

    wide_csv = os.path.join(out_dir, "final_summary_mean_sd_by_model_feature.csv")
    long_csv = os.path.join(out_dir, "final_summary_long_by_metric.csv")
    wide_df.to_csv(wide_csv, index=False)
    long_df.to_csv(long_csv, index=False)

    return wide_csv, long_csv


def write_hyperparameter_selection_frequency(
    per_split_df: pd.DataFrame,
    out_dir: str,
) -> str | None:
    """Summarize how often each training-selected configuration was chosen."""
    required = {"feature_mode", "model_name", "selected_hyperparameters"}
    if not required.issubset(per_split_df.columns):
        return None

    work = per_split_df[
        ["feature_mode", "model_name", "selected_hyperparameters"]
    ].copy()
    work["selected_hyperparameters"] = work["selected_hyperparameters"].fillna("{}").astype(str)
    counts = (
        work.groupby(
            ["feature_mode", "model_name", "selected_hyperparameters"],
            dropna=False,
        )
        .size()
        .reset_index(name="n_splits_selected")
    )
    totals = counts.groupby(["feature_mode", "model_name"])["n_splits_selected"].transform("sum")
    counts["selection_frequency_percent"] = 100.0 * counts["n_splits_selected"] / totals
    counts = counts.sort_values(
        ["feature_mode", "model_name", "n_splits_selected", "selected_hyperparameters"],
        ascending=[True, True, False, True],
    )
    out_csv = os.path.join(out_dir, "hyperparameter_selection_frequency.csv")
    counts.to_csv(out_csv, index=False)
    return out_csv


# =============================================================================
# One global process pool across split/model/feature-mode jobs
# =============================================================================
def run_one_global_split_model_feature_job(payload: dict[str, Any]) -> dict[str, Any]:
    """Run one complete scientific analysis job in one single-threaded worker.

    A job is uniquely identified by (split_id, model_name, feature_mode). It uses
    the same ``train_one_model_feature_mode`` function as the sequential workflow;
    only the scheduling layer is different. Returning paths and scalar metadata,
    rather than the fitted pipeline object, keeps inter-process transfers small.
    """
    import traceback

    os.environ["OMP_NUM_THREADS"] = "1"
    os.environ["OPENBLAS_NUM_THREADS"] = "1"
    os.environ["MKL_NUM_THREADS"] = "1"
    os.environ["NUMEXPR_NUM_THREADS"] = "1"
    os.environ["VECLIB_MAXIMUM_THREADS"] = "1"

    start = time.monotonic()
    split_id = str(payload["split_id"])
    split_index = int(payload["split_index"])
    model_name = str(payload["model_name"])
    feature_mode = str(payload["feature_mode"])
    job_index = int(payload["job_index"])
    total_jobs = int(payload["total_jobs"])
    worker_args = copy.copy(payload["args"])

    # The outer pool owns all parallelism. Disabling every inner pool is critical:
    # 36 outer workers x nested workers would oversubscribe CPU and memory.
    worker_args.parallel_hyperparameter_candidates = False
    worker_args.global_parallel_across_all_combinations = False
    worker_args.max_parallel_candidates = 1
    worker_args.n_jobs = int(payload.get("n_jobs_per_global_job", 1))
    worker_args.n_jobs_per_candidate = 1
    worker_args.show_progress_bars = False
    worker_args.show_whole_process_progress = False
    worker_args.show_cv_fold_progress = False
    worker_args.show_threshold_progress = False
    worker_args.show_per_candidate_start_finish = False

    model_out_dir = os.path.join(worker_args.out_dir, feature_mode, model_name)
    ensure_dir(model_out_dir)
    log_path = os.path.join(model_out_dir, "worker.log") if payload.get("save_worker_log", True) else None

    try:
        if log_path is not None:
            with open(log_path, "w", encoding="utf-8") as log_file:
                print(
                    f"Global job {job_index}/{total_jobs}: split={split_id}, "
                    f"model={model_name}, feature_mode={feature_mode}, pid={os.getpid()}",
                    file=log_file,
                )
                with redirect_stdout(log_file), redirect_stderr(log_file):
                    result = train_one_model_feature_mode(
                        model_name=model_name,
                        feature_mode=feature_mode,
                        args=worker_args,
                    )
        else:
            result = train_one_model_feature_mode(
                model_name=model_name,
                feature_mode=feature_mode,
                args=worker_args,
            )

        return {
            "ok": True,
            "job_index": job_index,
            "total_jobs": total_jobs,
            "split_index": split_index,
            "split_id": split_id,
            "model_name": result.model_name,
            "feature_mode": result.feature_mode,
            "out_dir": result.out_dir,
            "feature_cols": list(result.feature_cols),
            "chosen_threshold": float(result.chosen_threshold),
            "summary_json": result.summary_json,
            "worker_log": log_path,
            "worker_pid": int(os.getpid()),
            "elapsed_seconds": float(time.monotonic() - start),
        }
    except Exception as exc:
        traceback_text = traceback.format_exc()
        if log_path is not None:
            try:
                with open(log_path, "a", encoding="utf-8") as log_file:
                    print("\nWORKER FAILED", file=log_file)
                    print(traceback_text, file=log_file)
            except Exception:
                pass
        return {
            "ok": False,
            "job_index": job_index,
            "total_jobs": total_jobs,
            "split_index": split_index,
            "split_id": split_id,
            "model_name": model_name,
            "feature_mode": feature_mode,
            "worker_log": log_path,
            "worker_pid": int(os.getpid()),
            "error": str(exc),
            "traceback": traceback_text,
            "elapsed_seconds": float(time.monotonic() - start),
        }


def run_global_parallel_jobs_across_all_splits(
    args: argparse.Namespace,
    split_records: list[dict[str, Any]],
    root_out: str,
) -> tuple[list[pd.DataFrame], list[dict[str, Any]]]:
    """Run all split/model/feature combinations in one bounded process pool."""
    payloads: list[dict[str, Any]] = []
    total_jobs = len(split_records) * len(args.feature_modes) * len(args.models)
    job_index = 0

    for record in split_records:
        split_metadata = record["metadata"]
        split_args = record["args"]
        for feature_mode in args.feature_modes:
            for model_name in args.models:
                job_index += 1
                worker_args = prepare_args_for_worker(split_args)
                payloads.append({
                    "job_index": job_index,
                    "total_jobs": total_jobs,
                    "split_index": int(split_metadata["split_index"]),
                    "split_id": str(split_metadata["split_id"]),
                    "model_name": model_name,
                    "feature_mode": feature_mode,
                    "args": worker_args,
                    "n_jobs_per_global_job": int(args.n_jobs_per_global_job),
                    "save_worker_log": bool(args.save_global_worker_logs),
                })

    if not payloads:
        raise ValueError("The global scheduler received no split/model/feature jobs.")

    cpu_count = int(os.cpu_count() or 1)
    requested_workers = int(args.max_total_parallel_jobs)
    max_workers = max(1, min(requested_workers, cpu_count, len(payloads)))

    execution_plan = {
        "scheduler": "one global ProcessPoolExecutor across split/model/feature jobs",
        "parallel_unit": "one complete (split_id, model_name, feature_mode) analysis",
        "n_splits": int(len(split_records)),
        "n_models": int(len(args.models)),
        "n_feature_modes": int(len(args.feature_modes)),
        "total_jobs": int(len(payloads)),
        "hyperparameter_candidates_per_model": {
            model_name: int(len(hyperparameter_candidates(model_name, args)))
            for model_name in args.models
        },
        "total_candidate_evaluations": int(
            len(split_records)
            * len(args.feature_modes)
            * sum(len(hyperparameter_candidates(model_name, args)) for model_name in args.models)
        ),
        "requested_workers": requested_workers,
        "detected_logical_cpus": cpu_count,
        "workers_used": max_workers,
        "threads_per_job": int(args.n_jobs_per_global_job),
        "nested_process_pools": False,
    }
    execution_plan_json = os.path.join(root_out, "parallel_execution_plan.json")
    with open(execution_plan_json, "w", encoding="utf-8") as f:
        json.dump(execution_plan, f, indent=2)

    print("\n" + "=" * 90)
    print("GLOBAL MULTI-CORE SCHEDULER ENABLED")
    print(
        f"Jobs: {len(split_records)} splits x {len(args.models)} models x "
        f"{len(args.feature_modes)} feature modes = {len(payloads)}"
    )
    print(
        f"Worker processes: {max_workers} "
        f"(requested={requested_workers}, detected logical CPUs={cpu_count})"
    )
    print("Each worker is single-threaded; nested process pools are disabled.")
    print(f"Execution plan: {execution_plan_json}")

    start_method = str(getattr(args, "parallel_start_method", "fork"))
    try:
        mp_context = mp.get_context(start_method)
    except ValueError:
        print(f"[WARNING] Unknown PARALLEL_START_METHOD={start_method!r}; using Python default.")
        mp_context = None

    executor_kwargs: dict[str, Any] = {"max_workers": max_workers}
    if mp_context is not None:
        executor_kwargs["mp_context"] = mp_context

    results_by_split: dict[str, list[TrainResult]] = {
        str(record["metadata"]["split_id"]): [] for record in split_records
    }
    failures: list[dict[str, Any]] = []
    status_rows: list[dict[str, Any]] = []
    status_csv = os.path.join(root_out, "parallel_job_status.csv")

    global_bar = make_progress_bar(
        total=len(payloads),
        desc="All split/model/feature jobs",
        unit="job",
        args=args,
        leave=True,
        disable=not bool(getattr(args, "show_whole_process_progress", True)),
    )

    try:
        with ProcessPoolExecutor(**executor_kwargs) as executor:
            future_to_payload = {
                executor.submit(run_one_global_split_model_feature_job, payload): payload
                for payload in payloads
            }
            for future in as_completed(future_to_payload):
                payload = future_to_payload[future]
                try:
                    result = future.result()
                except Exception as exc:
                    import traceback
                    result = {
                        "ok": False,
                        "job_index": payload["job_index"],
                        "total_jobs": payload["total_jobs"],
                        "split_index": payload["split_index"],
                        "split_id": payload["split_id"],
                        "model_name": payload["model_name"],
                        "feature_mode": payload["feature_mode"],
                        "worker_log": None,
                        "worker_pid": None,
                        "error": str(exc),
                        "traceback": traceback.format_exc(),
                        "elapsed_seconds": 0.0,
                    }

                elapsed = float(result.get("elapsed_seconds", 0.0))
                status_row = {
                    "job_index": int(result["job_index"]),
                    "split_index": int(result["split_index"]),
                    "split_id": str(result["split_id"]),
                    "model_name": str(result["model_name"]),
                    "feature_mode": str(result["feature_mode"]),
                    "status": "succeeded" if result.get("ok", False) else "failed",
                    "elapsed_seconds": elapsed,
                    "worker_pid": result.get("worker_pid"),
                    "worker_log": result.get("worker_log"),
                    "summary_json": result.get("summary_json"),
                    "error": result.get("error"),
                }
                status_rows.append(status_row)
                # Keep a recoverable, live record that can also be inspected while
                # a long run is still in progress.
                pd.DataFrame(status_rows).sort_values("job_index").to_csv(status_csv, index=False)

                if result.get("ok", False):
                    train_result = TrainResult(
                        model_name=str(result["model_name"]),
                        feature_mode=str(result["feature_mode"]),
                        out_dir=str(result["out_dir"]),
                        pipeline=None,
                        feature_cols=list(result["feature_cols"]),
                        chosen_threshold=float(result["chosen_threshold"]),
                        summary_json=str(result["summary_json"]),
                    )
                    results_by_split[str(result["split_id"])].append(train_result)
                    progress_update(
                        global_bar,
                        1,
                        postfix=(
                            f"{result['split_id']} {result['feature_mode']}/{result['model_name']} "
                            f"finished in {format_duration(elapsed)}"
                        ),
                    )
                else:
                    failure = {
                        "job_index": int(result["job_index"]),
                        "split_index": int(result["split_index"]),
                        "split_id": str(result["split_id"]),
                        "model_name": str(result["model_name"]),
                        "feature_mode": str(result["feature_mode"]),
                        "error": str(result.get("error")),
                        "traceback": str(result.get("traceback", "")),
                        "worker_log": result.get("worker_log"),
                    }
                    failures.append(failure)
                    print(
                        "\n[ERROR] "
                        f"{failure['split_id']} {failure['feature_mode']}/{failure['model_name']} "
                        f"failed: {failure['error']}"
                    )
                    progress_update(
                        global_bar,
                        1,
                        postfix=(
                            f"FAILED {result['split_id']} "
                            f"{result['feature_mode']}/{result['model_name']}"
                        ),
                    )
    finally:
        progress_close(global_bar)

    status_df = pd.DataFrame(status_rows).sort_values("job_index")
    status_df.to_csv(status_csv, index=False)

    feature_order = {name: i for i, name in enumerate(args.feature_modes)}
    model_order = {name: i for i, name in enumerate(args.models)}
    split_rows_all: list[pd.DataFrame] = []
    expected_results_per_split = len(args.feature_modes) * len(args.models)

    for record in split_records:
        metadata = record["metadata"]
        split_args = record["args"]
        split_id = str(metadata["split_id"])
        split_results = results_by_split.get(split_id, [])
        split_results.sort(
            key=lambda r: (
                feature_order.get(r.feature_mode, 10**6),
                model_order.get(r.model_name, 10**6),
            )
        )

        if not split_results:
            continue
        if len(split_results) != expected_results_per_split:
            print(
                f"[WARNING] {split_id} has {len(split_results)}/"
                f"{expected_results_per_split} successful model/feature jobs."
            )

        overall_csv = write_overall_summary(split_results, split_args)
        split_rows_all.append(collect_split_overall_rows(overall_csv, metadata))

    if split_rows_all:
        per_split_df = pd.concat(split_rows_all, ignore_index=True)
        per_split_csv = os.path.join(root_out, "per_split_model_summary.csv")
        per_split_df.to_csv(per_split_csv, index=False)
        wide_csv, long_csv = write_mean_sd_summary_across_splits(per_split_df, root_out)
        hyperparameter_frequency_csv = write_hyperparameter_selection_frequency(
            per_split_df,
            root_out,
        )

        print("\n" + "=" * 90)
        print("GLOBAL MULTI-SPLIT WORKFLOW FINISHED")
        print(f"Parallel job status:            {status_csv}")
        print(f"Per-split model summary:        {per_split_csv}")
        print(f"Final mean ± SD wide summary:   {wide_csv}")
        print(f"Final long metric summary:      {long_csv}")
        if hyperparameter_frequency_csv is not None:
            print(f"Hyperparameter frequencies:     {hyperparameter_frequency_csv}")

    if failures:
        failure_json = os.path.join(root_out, "failures_all_splits.json")
        with open(failure_json, "w", encoding="utf-8") as f:
            json.dump(failures, f, indent=2)
        print(f"Failures across global jobs were saved to: {failure_json}")
        if not args.continue_on_error:
            raise RuntimeError(
                f"{len(failures)} global jobs failed. See failures_all_splits.json."
            )

    return split_rows_all, failures


def run_all_accepted_splits_workflow(args: argparse.Namespace) -> None:
    """
    Main multi-split workflow:
      - generate train/test CSVs for every accepted split;
      - run all requested models/feature modes for each split;
      - save per-split and final mean ± SD summaries.
    """
    root_out = MULTISPLIT_OUT_DIR
    ensure_dir(root_out)

    all_data_csv = resolve_input_path(ALL_DATA_CSV)
    accepted_splits_csv = resolve_input_path(ACCEPTED_SPLITS_CSV)

    if not os.path.exists(all_data_csv):
        raise FileNotFoundError(f"ALL_DATA_CSV not found: {all_data_csv}")
    if not os.path.exists(accepted_splits_csv):
        raise FileNotFoundError(f"ACCEPTED_SPLITS_CSV not found: {accepted_splits_csv}")

    print("\n" + "=" * 90)
    print("MULTI-SPLIT WORKFLOW ENABLED")
    print(f"ALL_DATA_CSV:        {all_data_csv}")
    print(f"ACCEPTED_SPLITS_CSV: {accepted_splits_csv}")
    print(
        f"Integrated exception filter: {args.exception_col} == "
        f"{args.exception_positive_value}"
    )
    print(f"MULTISPLIT_OUT_DIR:  {root_out}")

    split_table = pd.read_csv(accepted_splits_csv)
    required_split_columns = {"split_id", "train_set", "test_set"}
    missing_split_columns = sorted(required_split_columns - set(split_table.columns))
    if missing_split_columns:
        raise ValueError(
            f"ACCEPTED_SPLITS_CSV is missing required columns: {missing_split_columns}"
        )
    if split_table["split_id"].astype(str).duplicated().any():
        duplicated = sorted(
            split_table.loc[split_table["split_id"].astype(str).duplicated(False), "split_id"]
            .astype(str)
            .unique()
        )
        raise ValueError(f"ACCEPTED_SPLITS_CSV contains duplicated split_id values: {duplicated}")
    if "satisfies_conditions" in split_table.columns:
        condition_flags = split_table["satisfies_conditions"].map(_truthy_split_flag)
        if not condition_flags.all():
            bad_ids = split_table.loc[~condition_flags, "split_id"].astype(str).tolist()
            raise ValueError(
                "ACCEPTED_SPLITS_CSV contains rows marked satisfies_conditions=False: "
                f"{bad_ids}"
            )

    if EXPECTED_ACCEPTED_SPLIT_COUNT is not None and len(split_table) != int(EXPECTED_ACCEPTED_SPLIT_COUNT):
        raise ValueError(
            f"Expected {EXPECTED_ACCEPTED_SPLIT_COUNT} accepted splits, but "
            f"ACCEPTED_SPLITS_CSV contains {len(split_table)} rows."
        )

    if MAX_SPLITS_TO_RUN is not None:
        split_table = split_table.head(int(MAX_SPLITS_TO_RUN)).copy()
        print(f"DEBUG MODE: running only the first {len(split_table)} accepted splits.")

    full_df_raw = pd.read_csv(all_data_csv)
    if args.label_col not in full_df_raw.columns:
        raise ValueError(f"Label column {args.label_col!r} was not found in ALL_DATA_CSV.")
    labels = pd.to_numeric(full_df_raw[args.label_col], errors="coerce")
    if labels.isna().any() or not set(labels.unique()).issubset({0, 1}):
        raise ValueError(f"Label column {args.label_col!r} must contain only 0 and 1.")

    full_df_filtered, n_exceptions_removed, dataset_filter_debug = apply_integrated_exception_filter(
        full_df_raw,
        exception_col=args.exception_col,
        exception_positive_value=args.exception_positive_value,
        dataset_name="full input dataset",
    )
    full_df = assign_primordia_from_filenames(full_df_filtered)
    primordia = sorted(full_df["_split_group"].astype(str).unique())

    raw_dataset_signature = validate_raw_dataset_signature(
        full_df_raw=full_df_raw,
        full_df=full_df,
        primordia=primordia,
        label_col=args.label_col,
        n_exceptions_removed=n_exceptions_removed,
    )
    validated_split_design, outer_split_design = validate_and_summarize_split_table(
        split_table=split_table,
        full_df_with_groups=full_df,
        label_col=args.label_col,
    )
    validated_split_design_csv = os.path.join(root_out, "validated_split_design.csv")
    validated_split_design.to_csv(validated_split_design_csv, index=False)

    configuration = {
        "software_versions": collect_software_versions(),
        "script_provenance": collect_script_provenance(),
        "input_provenance": {
            "all_data_csv": all_data_csv,
            "all_data_csv_sha256": sha256_file(all_data_csv),
            "accepted_splits_csv": accepted_splits_csv,
            "accepted_splits_csv_sha256": sha256_file(accepted_splits_csv),
        },
        "methodological_references": METHODOLOGICAL_REFERENCES,
        "all_data_csv": all_data_csv,
        "accepted_splits_csv": accepted_splits_csv,
        "expected_accepted_split_count": EXPECTED_ACCEPTED_SPLIT_COUNT,
        "n_split_rows_to_run": int(len(split_table)),
        "raw_dataset_signature": raw_dataset_signature,
        "outer_split_design": outer_split_design,
        "validated_split_design_csv": validated_split_design_csv,
        "split_grouping": "fileName prefix before final _<time>h token",
        "primordia": primordia,
        "n_primordia": int(len(primordia)),
        "dataset_filtering": dataset_filter_debug,
        "n_daughter_pairs_after_exception_filter": int(full_df[args.label_col].sum()),
        "n_non_daughter_pairs_after_exception_filter": int(
            len(full_df) - full_df[args.label_col].sum()
        ),
        "models": list(args.models),
        "feature_modes": list(args.feature_modes),
        "all_feature_count_required": EXPECTED_ALL_FEATURE_COUNT,
        "selected_four_features": SELECTED_4_FEATURES,
        "cv": {
            "method": (
                "StratifiedGroupKFold on training primordia"
                if args.group_aware_inner_cv
                else "StratifiedKFold on training rows"
            ),
            "group_aware": bool(args.group_aware_inner_cv),
            "group_definition": "primordium inferred from fileName prefix",
            "folds": int(args.cv_folds),
            "shuffle": True,
            "seed": int(args.seed),
            "scope": "outer training partition only",
            "test_observations_used": False,
        },
        "threshold_selection": {
            "source": "cross-validated training probabilities only",
            "criterion": f"maximum F{args.beta} after exact maximum-weight matching",
            "tie_breakers": ["precision", "recall", "higher threshold"],
            "n_thresholds": int(args.n_thresholds),
            "range": [float(args.thr_low), float(args.thr_high)],
            "even_spacing": float(
                (args.thr_high - args.thr_low) / (args.n_thresholds - 1)
            ),
            "outer_test_observations_used": False,
            "test_application": "selected threshold frozen before held-out scoring",
        },
        "matching": {
            "method": "exact NetworkX maximum_weight_matching",
            "maxcardinality": False,
            "weight": "model positive-class score",
            "scope": "within each snapshot",
        },
        "hyperparameter_search_enabled": bool(args.hyperparameter_search),
        "hyperparameter_selection": {
            "source": "inner grouped cross-validated probabilities from outer training data only",
            "criterion": f"maximum F{args.beta} after exact maximum-weight matching",
            "tie_breakers": [
                "F1",
                "cross-validated PR-AUC",
                "first prespecified candidate (grids are ordered from simpler/stronger regularization)",
            ],
            "candidate_counts": {
                model_name: int(len(hyperparameter_candidates(model_name, args)))
                for model_name in args.models
            },
            "candidate_settings": {
                model_name: [
                    params for _candidate_args, params in hyperparameter_candidates(model_name, args)
                ]
                for model_name in args.models
            },
            "outer_test_observations_used": False,
        },
        "parallel_execution": {
            "global_across_split_model_feature_jobs": bool(
                args.global_parallel_across_all_split_model_feature_jobs
            ),
            "max_total_parallel_jobs": int(args.max_total_parallel_jobs),
            "threads_per_global_job": int(args.n_jobs_per_global_job),
            "nested_process_pools": False,
            "total_planned_jobs": int(
                len(split_table) * len(args.models) * len(args.feature_modes)
            ),
            "total_planned_hyperparameter_candidate_evaluations": int(
                len(split_table)
                * len(args.feature_modes)
                * sum(len(hyperparameter_candidates(model_name, args)) for model_name in args.models)
            ),
        },
        "model_settings": {
            "logistic_regression": {
                "imputation": "median fitted on training data",
                "scaling": "StandardScaler fitted on training data",
                "solver": "liblinear",
                "penalty": "l2",
                "C": float(args.lr_C),
                "class_weight": "balanced" if args.class_weight_balanced else None,
                "max_iter": int(args.lr_max_iter),
            },
            "linear_svm": {
                "imputation": "median fitted on training data",
                "scaling": "StandardScaler fitted on training data",
                "C": float(args.svm_C),
                "class_weight": "balanced" if args.class_weight_balanced else None,
                "probability_calibration": args.svm_calib_method,
                "calibration_cv": int(args.svm_calib_cv),
                "calibration_scope": "inside each model-fitting training partition only",
                "max_iter": int(args.svm_max_iter),
            },
            "xgboost": {
                "n_estimators": int(args.xgb_n_estimators),
                "learning_rate": float(args.xgb_learning_rate),
                "max_depth": int(args.xgb_max_depth),
                "min_child_weight": float(args.xgb_min_child_weight),
                "subsample": float(args.xgb_subsample),
                "colsample_bytree": float(args.xgb_colsample_bytree),
                "reg_lambda": float(args.xgb_reg_lambda),
                "reg_alpha": float(args.xgb_reg_alpha),
                "gamma": float(args.xgb_gamma),
                "class_imbalance": (
                    "scale_pos_weight = n_negative / n_positive, recalculated "
                    "inside each inner training fold"
                ),
            },
            "lightgbm": {
                "n_estimators": int(args.lgbm_n_estimators),
                "learning_rate": float(args.lgbm_learning_rate),
                "num_leaves": int(args.lgbm_num_leaves),
                "max_depth": int(args.lgbm_max_depth),
                "min_child_samples": int(args.lgbm_min_child_samples),
                "subsample": float(args.lgbm_subsample),
                "subsample_freq": int(args.lgbm_subsample_freq),
                "colsample_bytree": float(args.lgbm_colsample_bytree),
                "reg_lambda": float(args.lgbm_reg_lambda),
                "reg_alpha": float(args.lgbm_reg_alpha),
                "class_imbalance": (
                    "scale_pos_weight = n_negative / n_positive, recalculated "
                    "inside each inner training fold"
                ),
            },
        },
    }
    configuration_json = os.path.join(root_out, "analysis_configuration.json")
    with open(configuration_json, "w", encoding="utf-8") as f:
        json.dump(configuration, f, indent=2)
    reviewer_summary_txt = os.path.join(root_out, "reviewer_methods_summary.txt")
    write_reviewer_methods_summary(configuration, reviewer_summary_txt)

    print(
        f"Rows: input={len(full_df_raw)} removed_as_exceptions={n_exceptions_removed} "
        f"analyzable={len(full_df)}"
    )
    print(f"Primordia inferred from fileName prefixes ({len(primordia)}): {primordia}")
    print(f"Accepted split rows to run: {len(split_table)}")
    print(
        "Training primordia per accepted split: "
        f"{outer_split_design['training_primordia_count_distribution']}"
    )
    print(
        "Test primordia per accepted split: "
        f"{outer_split_design['test_primordia_count_distribution']}"
    )
    print(f"Validated split design: {validated_split_design_csv}")
    print(f"Reviewer methods summary: {reviewer_summary_txt}")

    if PREFLIGHT_ONLY:
        print("PREFLIGHT_ONLY=True: validation completed; model fitting was skipped.")
        return

    if nx is None:
        raise RuntimeError(
            "networkx is required for the formal model run. Install the listed dependencies "
            "before setting PREFLIGHT_ONLY=False."
        ) from NETWORKX_IMPORT_ERROR

    if bool(args.global_parallel_across_all_split_model_feature_jobs):
        print("Preparing validated train/test CSVs for the global job queue...")
        split_records: list[dict[str, Any]] = []
        split_prep_bar = make_progress_bar(
            total=len(split_table),
            desc="Preparing accepted splits",
            unit="split",
            args=args,
            leave=True,
            disable=not bool(getattr(args, "show_whole_process_progress", True)),
        )
        try:
            for split_index, split_row in split_table.iterrows():
                split_index_int = int(split_index)
                split_metadata = make_train_test_csvs_for_one_accepted_split(
                    full_df_with_groups=full_df,
                    split_row=split_row,
                    split_index=split_index_int,
                    split_root_dir=root_out,
                    dataset_filter_debug=dataset_filter_debug,
                    label_col=args.label_col,
                    exception_col=args.exception_col,
                    exception_positive_value=args.exception_positive_value,
                )
                split_model_out_dir = os.path.join(
                    root_out,
                    str(split_metadata["split_id"]),
                    "models",
                )
                split_args = copy.copy(args)
                split_args.train_csv = split_metadata["train_csv"]
                split_args.test_csv = split_metadata["test_csv"]
                split_args.out_dir = split_model_out_dir
                split_records.append({"metadata": split_metadata, "args": split_args})
                progress_update(
                    split_prep_bar,
                    1,
                    postfix=f"prepared {split_metadata['split_id']}",
                )
        finally:
            progress_close(split_prep_bar)

        run_global_parallel_jobs_across_all_splits(
            args=args,
            split_records=split_records,
            root_out=root_out,
        )
        return

    split_rows_all = []
    all_failures = []

    cumulative_summary_csv = os.path.join(root_out, "per_split_model_summary.csv")

    for split_index, split_row in split_table.iterrows():
        split_start = time.monotonic()
        split_index_int = int(split_index)
        split_name = str(split_row["split_id"]).strip()
        print("\n" + "#" * 90)
        print(f"Starting {split_name} ({split_index_int + 1}/{len(split_table)})")

        split_metadata = make_train_test_csvs_for_one_accepted_split(
            full_df_with_groups=full_df,
            split_row=split_row,
            split_index=split_index_int,
            split_root_dir=root_out,
            dataset_filter_debug=dataset_filter_debug,
            label_col=args.label_col,
            exception_col=args.exception_col,
            exception_positive_value=args.exception_positive_value,
        )

        split_name = split_metadata["split_name"]
        split_model_out_dir = os.path.join(root_out, split_name, "models")
        split_args = copy.copy(args)
        split_args.train_csv = split_metadata["train_csv"]
        split_args.test_csv = split_metadata["test_csv"]
        split_args.out_dir = split_model_out_dir

        try:
            _results, failures, overall_csv = run_single_train_test_workflow(split_args)
            if failures:
                for f in failures:
                    f = dict(f)
                    f["split_index"] = str(split_index_int)
                    f["split_name"] = split_name
                    all_failures.append(f)

            if overall_csv is not None and os.path.exists(overall_csv):
                split_summary_df = collect_split_overall_rows(overall_csv, split_metadata)
                split_rows_all.append(split_summary_df)
                pd.concat(split_rows_all, ignore_index=True).to_csv(cumulative_summary_csv, index=False)

        except Exception as e:
            import traceback
            failure = {
                "split_index": str(split_index_int),
                "split_name": split_name,
                "error": str(e),
                "traceback": traceback.format_exc(),
            }
            all_failures.append(failure)
            print("\n[ERROR] " + f"{split_name} failed: {e}")
            if not args.continue_on_error:
                failure_json = os.path.join(root_out, "failures_all_splits.json")
                with open(failure_json, "w", encoding="utf-8") as f:
                    json.dump(all_failures, f, indent=2)
                raise

        elapsed = time.monotonic() - split_start
        print(f"Finished {split_name} in {format_duration(elapsed)}")

    if split_rows_all:
        per_split_df = pd.concat(split_rows_all, ignore_index=True)
        per_split_csv = os.path.join(root_out, "per_split_model_summary.csv")
        per_split_df.to_csv(per_split_csv, index=False)

        wide_csv, long_csv = write_mean_sd_summary_across_splits(per_split_df, root_out)
        hyperparameter_frequency_csv = write_hyperparameter_selection_frequency(
            per_split_df,
            root_out,
        )

        print("\n" + "=" * 90)
        print("MULTI-SPLIT WORKFLOW FINISHED")
        print(f"Per-split model summary:        {per_split_csv}")
        print(f"Final mean ± SD wide summary:   {wide_csv}")
        print(f"Final long metric summary:      {long_csv}")
        if hyperparameter_frequency_csv is not None:
            print(f"Hyperparameter frequencies:     {hyperparameter_frequency_csv}")

    if all_failures:
        failure_json = os.path.join(root_out, "failures_all_splits.json")
        with open(failure_json, "w", encoding="utf-8") as f:
            json.dump(all_failures, f, indent=2)
        print(f"Failures across splits were saved to: {failure_json}")
        if not split_rows_all:
            raise RuntimeError("All split runs failed. See failures_all_splits.json.")


# Override the original main() so this same script can run either:
#   RUN_ALL_ACCEPTED_SPLITS=True  -> evaluate every row in ACCEPTED_SPLITS_CSV
#   RUN_ALL_ACCEPTED_SPLITS=False -> original one train/test split workflow
def main(argv: list[str] | None = None) -> None:
    args = parse_args(argv)
    if bool(RUN_ALL_ACCEPTED_SPLITS):
        run_all_accepted_splits_workflow(args)
    else:
        run_single_train_test_workflow(args)


if __name__ == "__main__":
    main()
    # Some scientific Python/OpenMP builds can leave non-daemon helper threads alive
    # after repeated cross-validation + matching runs. Flush outputs and force a clean
    # process exit so batch runs do not hang after all files are written.
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(0)
