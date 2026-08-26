"""
Compact feature-distribution analysis for neighboring cell pairs.

For ANY selected feature, this script outputs only:
1. Daughter-pair mean +/- SD
2. Non-daughter-pair mean +/- SD

For JUNCTION-ANGLE-related features only, it additionally outputs:
3. Two-component axial von Mises fit: low/high component axial mean and kappa
4. Wasserstein closeness:
   - non-daughter pairs vs fitted low-angle component
   - daughter pairs vs fitted high-angle component

No overlap index, KS tests, cutoff statistics, bootstrap, posterior CSVs,
or figures are produced.

Requirements:
    pip install numpy pandas scipy
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

import numpy as np
import pandas as pd
from scipy.special import i0
from scipy.stats import wasserstein_distance


# =============================================================================
# User settings
# =============================================================================

INPUT_CSV = r"batch_neighbor_pair_geometry.csv"
TARGET_COLUMN = "normalizedSharedEdgeLength"
RESULT_DIR = "result_shared_edge_length"
OUT_SUMMARY_CSV = "feature_distribution_summary.csv"

OBSERVED_DIVISION_COLUMN = "auto"
OBSERVED_DIVISION_COLUMN_ALIASES = (
    "observed_division",
    "is_real_division",
    "division_real",
    "real_division",
)

EXCLUDE_EXCEPTION_ROWS = True
EXCEPTION_COLUMN = "exception_label"
EXCEPTION_COLUMN_ALIASES = ("exception_label", "exception")
EXCEPTION_POSITIVE_VALUE = 1


# Axial von Mises fitting is run ONLY for feature names containing both
# "junction" and "angle" after punctuation/case normalization.
VONMISES_N_INIT = 15
VONMISES_MAX_ITER = 400
SEED = 0


# =============================================================================
# Helpers
# =============================================================================

def _normalized_column_key(name: object) -> str:
    return re.sub(r"[^a-z0-9]+", "", str(name).casefold())


def resolve_column_name(
    df: pd.DataFrame,
    requested: str | None,
    aliases: tuple[str, ...] = (),
    *,
    required: bool,
    role: str,
) -> str | None:
    """Resolve exact, case-insensitive, or punctuation-insensitive column names."""
    candidates: list[str] = []
    if requested is not None and str(requested).strip().casefold() not in {"", "auto"}:
        candidates.append(str(requested))
    candidates.extend(str(x) for x in aliases)

    exact = {str(c): str(c) for c in df.columns}
    folded = {str(c).casefold(): str(c) for c in df.columns}
    normalized = {_normalized_column_key(c): str(c) for c in df.columns}

    for candidate in candidates:
        if candidate in exact:
            return exact[candidate]
        hit = folded.get(candidate.casefold())
        if hit is not None:
            return hit
        hit = normalized.get(_normalized_column_key(candidate))
        if hit is not None:
            return hit

    if required:
        raise ValueError(
            f"Could not resolve {role}. Requested={requested!r}. "
            f"Available columns: {list(df.columns)}"
        )
    return None


def exclude_exception_labeled_rows(
    df: pd.DataFrame,
    enabled: bool = True,
    exception_column: str = EXCEPTION_COLUMN,
    exception_positive_value: int | float | str = EXCEPTION_POSITIVE_VALUE,
) -> pd.DataFrame:
    """Exclude exception==1 rows when a supported exception column exists."""
    if not enabled:
        return df.copy()

    col = resolve_column_name(
        df,
        requested=exception_column,
        aliases=EXCEPTION_COLUMN_ALIASES,
        required=False,
        role="exception column",
    )
    if col is None:
        return df.copy()

    raw = df[col]
    numeric = pd.to_numeric(raw, errors="coerce")
    try:
        numeric_mask = numeric.eq(float(exception_positive_value))
    except (TypeError, ValueError):
        numeric_mask = pd.Series(False, index=df.index)

    string_target = str(exception_positive_value).strip().casefold()
    string_mask = (
        raw.astype("string")
        .str.strip()
        .str.casefold()
        .eq(string_target)
        .fillna(False)
    )
    mask = numeric_mask.fillna(False) | string_mask
    return df.loc[~mask].copy().reset_index(drop=True)


def is_junction_angle_feature(feature_name: str) -> bool:
    key = _normalized_column_key(feature_name)
    return "junction" in key and "angle" in key


def mean_sd(values: np.ndarray) -> tuple[float, float]:
    values = np.asarray(values, dtype=float)
    values = values[np.isfinite(values)]
    if values.size == 0:
        raise ValueError("Cannot calculate mean/SD for an empty group.")
    mean = float(np.mean(values))
    sd = float(np.std(values, ddof=1)) if values.size >= 2 else float("nan")
    return mean, sd


# =============================================================================
# Axial von Mises mixture (junction-angle features only)
# =============================================================================

def angle_deg_to_theta_rad(angle_deg: np.ndarray) -> np.ndarray:
    return (2.0 * np.deg2rad(angle_deg)) % (2.0 * np.pi)


def theta_rad_to_angle_deg(theta_rad: np.ndarray) -> np.ndarray:
    return (np.rad2deg(theta_rad) / 2.0) % 180.0


def kappa_from_R(R: np.ndarray) -> np.ndarray:
    R = np.asarray(R, dtype=float)
    R = np.clip(R, 1e-8, 1.0 - 1e-8)
    kappa = np.empty_like(R)

    mask1 = R < 0.53
    mask2 = (R >= 0.53) & (R < 0.85)
    mask3 = R >= 0.85

    kappa[mask1] = 2 * R[mask1] + R[mask1] ** 3 + (5 * R[mask1] ** 5) / 6
    kappa[mask2] = -0.4 + 1.39 * R[mask2] + 0.43 / (1 - R[mask2])
    kappa[mask3] = 1 / (R[mask3] ** 3 - 4 * R[mask3] ** 2 + 3 * R[mask3])
    return np.clip(kappa, 1e-6, 1e6)


def posterior_probabilities(
    theta: np.ndarray,
    weights: np.ndarray,
    mus: np.ndarray,
    kappas: np.ndarray,
) -> np.ndarray:
    theta = np.asarray(theta, dtype=float)
    K = len(weights)
    log_pdf = np.empty((theta.size, K), dtype=float)

    for k in range(K):
        log_norm = np.log(2.0 * np.pi) + np.log(i0(kappas[k]))
        log_pdf[:, k] = (
            np.log(weights[k] + 1e-300)
            + kappas[k] * np.cos(theta - mus[k])
            - log_norm
        )

    row_max = np.max(log_pdf, axis=1, keepdims=True)
    probs = np.exp(log_pdf - row_max)
    return probs / (np.sum(probs, axis=1, keepdims=True) + 1e-300)


def fit_vonmises_mixture(
    theta: np.ndarray,
    K: int = 2,
    n_init: int = VONMISES_N_INIT,
    max_iter: int = VONMISES_MAX_ITER,
    tol: float = 1e-6,
    seed: int = SEED,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """EM fit of a K-component von Mises mixture on doubled axial angles."""
    theta = np.asarray(theta, dtype=float)
    if theta.size < K:
        raise ValueError("Not enough observations for von Mises mixture fitting.")

    rng = np.random.default_rng(seed)
    N = theta.size
    sin_t = np.sin(theta)
    cos_t = np.cos(theta)

    best = None
    best_ll = -np.inf

    for _ in range(int(n_init)):
        mus = rng.choice(theta, size=K, replace=False)
        kappas = np.full(K, 5.0, dtype=float)
        weights = np.full(K, 1.0 / K, dtype=float)
        prev_ll = -np.inf

        for _ in range(int(max_iter)):
            resp = posterior_probabilities(theta, weights, mus, kappas)

            # Stable log-likelihood.
            log_pdf = np.empty((N, K), dtype=float)
            for k in range(K):
                log_norm = np.log(2.0 * np.pi) + np.log(i0(kappas[k]))
                log_pdf[:, k] = (
                    np.log(weights[k] + 1e-300)
                    + kappas[k] * np.cos(theta - mus[k])
                    - log_norm
                )
            row_max = np.max(log_pdf, axis=1, keepdims=True)
            ll = float(
                np.sum(
                    row_max[:, 0]
                    + np.log(np.sum(np.exp(log_pdf - row_max), axis=1) + 1e-300)
                )
            )

            if np.isfinite(prev_ll) and abs(ll - prev_ll) < tol:
                break
            prev_ll = ll

            Nk = resp.sum(axis=0) + 1e-300
            weights = Nk / N
            S = resp.T @ sin_t
            C = resp.T @ cos_t
            mus = np.arctan2(S, C) % (2.0 * np.pi)
            Rbar = np.sqrt(S**2 + C**2) / Nk
            kappas = kappa_from_R(Rbar)

        if ll > best_ll:
            best_ll = ll
            best = (weights.copy(), mus.copy(), kappas.copy())

    if best is None:
        raise RuntimeError("von Mises mixture fitting failed.")
    return best


def weighted_component_wasserstein(
    all_angles: np.ndarray,
    observed_angles: np.ndarray,
    component_weights: np.ndarray,
) -> float:
    """Wasserstein distance in degrees between a labeled group and weighted component."""
    all_angles = np.asarray(all_angles, dtype=float)
    observed_angles = np.asarray(observed_angles, dtype=float)
    component_weights = np.asarray(component_weights, dtype=float)

    valid = (
        np.isfinite(all_angles)
        & np.isfinite(component_weights)
        & (component_weights >= 0)
    )
    if observed_angles.size == 0 or valid.sum() == 0 or component_weights[valid].sum() <= 0:
        raise ValueError("Insufficient data for Wasserstein closeness calculation.")

    return float(
        wasserstein_distance(
            observed_angles,
            all_angles[valid],
            u_weights=None,
            v_weights=component_weights[valid],
        )
    )


# =============================================================================
# Main analysis
# =============================================================================

def main(
    csv: str,
    feature: str = TARGET_COLUMN,
    result_dir: str = RESULT_DIR,
    out_summary_csv: str = OUT_SUMMARY_CSV,
    observed_label_column: str = OBSERVED_DIVISION_COLUMN,
    exclude_exception_rows: bool = EXCLUDE_EXCEPTION_ROWS,
) -> Path:
    df = pd.read_csv(csv)

    feature_col = resolve_column_name(
        df,
        requested=feature,
        aliases=(),
        required=True,
        role="selected feature column",
    )
    label_col = resolve_column_name(
        df,
        requested=observed_label_column,
        aliases=OBSERVED_DIVISION_COLUMN_ALIASES,
        required=True,
        role="observed daughter/non-daughter label column",
    )

    df = exclude_exception_labeled_rows(df, enabled=exclude_exception_rows)

    x = pd.to_numeric(df[feature_col], errors="coerce")
    y = pd.to_numeric(df[label_col], errors="coerce")

    # Labeled rows are used for daughter/non-daughter descriptive statistics.
    # The junction-angle mixture itself uses all valid feature values, matching
    # the unsupervised fitting logic of the previous script.
    valid_feature = x.notna()
    valid_labeled = valid_feature & y.isin([0, 1])

    x_labeled = x[valid_labeled].astype(float).to_numpy()
    y_labeled = y[valid_labeled].astype(int).to_numpy()

    daughter = x_labeled[y_labeled == 1]
    non_daughter = x_labeled[y_labeled == 0]

    if daughter.size == 0 or non_daughter.size == 0:
        raise ValueError(
            f"Both observed classes are required. Daughter n={daughter.size}, "
            f"non-daughter n={non_daughter.size}."
        )

    d_mean, d_sd = mean_sd(daughter)
    n_mean, n_sd = mean_sd(non_daughter)

    summary: dict[str, object] = {
        "feature": feature_col,
        "daughter_pair_mean_plus_minus_sd": f"{d_mean:.6g} ± {d_sd:.6g}",
        "non_daughter_pair_mean_plus_minus_sd": f"{n_mean:.6g} ± {n_sd:.6g}",
    }

    junction_angle = is_junction_angle_feature(feature_col)

    if junction_angle:
        # Junction-angle values are axial orientations on [0, 180].
        angles = np.clip(x[valid_feature].astype(float).to_numpy(), 0.0, 180.0)
        daughter_angles = np.clip(daughter, 0.0, 180.0)
        non_daughter_angles = np.clip(non_daughter, 0.0, 180.0)
        theta = angle_deg_to_theta_rad(angles)

        weights, mus, kappas = fit_vonmises_mixture(theta)
        component_means = theta_rad_to_angle_deg(mus)

        # Sort so component 0 = low-angle and component 1 = high-angle.
        order = np.argsort(component_means)
        weights = weights[order]
        mus = mus[order]
        kappas = kappas[order]
        component_means = component_means[order]

        resp = posterior_probabilities(theta, weights, mus, kappas)

        w_non_low = weighted_component_wasserstein(
            angles,
            non_daughter_angles,
            resp[:, 0],
        )
        w_daughter_high = weighted_component_wasserstein(
            angles,
            daughter_angles,
            resp[:, 1],
        )

        summary.update(
            {
                "low_angle_component_axial_mean_deg": float(component_means[0]),
                "low_angle_component_kappa": float(kappas[0]),
                "high_angle_component_axial_mean_deg": float(component_means[1]),
                "high_angle_component_kappa": float(kappas[1]),
                "non_daughter_vs_low_component_wasserstein_deg": w_non_low,
                "daughter_vs_high_component_wasserstein_deg": w_daughter_high,
            }
        )

    out_dir = Path(result_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / out_summary_csv
    pd.DataFrame([summary]).to_csv(out_path, index=False)

    # Deliberately concise console output.
    print(f"Feature: {feature_col}")
    print(f"Daughter pair: {d_mean:.6g} ± {d_sd:.6g}")
    print(f"Non-daughter pair: {n_mean:.6g} ± {n_sd:.6g}")

    if junction_angle:
        print(
            "Axial von Mises low-angle component: "
            f"mean={summary['low_angle_component_axial_mean_deg']:.6f}°, "
            f"kappa={summary['low_angle_component_kappa']:.6f}"
        )
        print(
            "Axial von Mises high-angle component: "
            f"mean={summary['high_angle_component_axial_mean_deg']:.6f}°, "
            f"kappa={summary['high_angle_component_kappa']:.6f}"
        )
        print(
            "Wasserstein non-daughter vs low component: "
            f"{summary['non_daughter_vs_low_component_wasserstein_deg']:.6f}°"
        )
        print(
            "Wasserstein daughter vs high component: "
            f"{summary['daughter_vs_high_component_wasserstein_deg']:.6f}°"
        )

    print(f"Saved: {out_path}")
    return out_path


def parse_command_line() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Compact daughter/non-daughter feature comparison. All features: "
            "mean +/- SD. Junction-angle features additionally: axial von Mises "
            "fit and Wasserstein component-label closeness."
        )
    )
    parser.add_argument(
        "csv",
        nargs="?",
        default=INPUT_CSV,
        help="Input neighbor-pair geometry CSV.",
    )
    parser.add_argument(
        "--feature",
        default=TARGET_COLUMN,
        help="Feature column to analyze.",
    )
    parser.add_argument(
        "--result-dir",
        default=RESULT_DIR,
        help="Output directory.",
    )
    parser.add_argument(
        "--output",
        default=OUT_SUMMARY_CSV,
        help="Summary CSV filename.",
    )
    parser.add_argument(
        "--observed-label-column",
        default=OBSERVED_DIVISION_COLUMN,
        help="Observed daughter/non-daughter label column; default auto-detects supported aliases.",
    )
    parser.add_argument(
        "--include-exceptions",
        action="store_true",
        help="Do not exclude rows labeled as exceptions.",
    )
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_command_line()
    main(
        csv=args.csv,
        feature=args.feature,
        result_dir=args.result_dir,
        out_summary_csv=args.output,
        observed_label_column=args.observed_label_column,
        exclude_exception_rows=not args.include_exceptions,
    )
