from pathlib import Path
from itertools import combinations
import pandas as pd

# ============================================================
# Settings
# ============================================================

SCRIPT_DIR = Path(__file__).resolve().parent

INPUT_CSV = SCRIPT_DIR / "batch_neighbor_pair_geometry.csv"
OUTPUT_CSV = SCRIPT_DIR / "all_satisfying_primordia_splits.csv"

FILENAME_COL = "fileName"
LABEL_COL = "observed_division"
EXCEPTION_COL = "exception_label"
EXCEPTION_POSITIVE_VALUE = 1

# Requirement 1:
# training rows must be 70% +/- 2 percentage points
TARGET_TRAIN_FRACTION = 0.70
FRACTION_TOLERANCE = 0.02

# Requirement 2:
# absolute difference in daughter-pair frequency between
# training and test must be strictly < 1 percentage point
MAX_DAUGHTER_FREQ_DIFFERENCE = 0.01


def get_primordium_prefix(filename):
    """
    Extract primordium from fileName.

    Example:
        sample10_22h.json -> sample10
    """
    return str(filename).split("_", 1)[0]


def main():
    df = pd.read_csv(INPUT_CSV)

    if FILENAME_COL not in df.columns:
        raise ValueError(f"Missing column: {FILENAME_COL}")

    if LABEL_COL not in df.columns:
        raise ValueError(f"Missing column: {LABEL_COL}")

    if EXCEPTION_COL not in df.columns:
        raise ValueError(f"Missing column: {EXCEPTION_COL}")

    # --------------------------------------------------------
    # Exclude exception-labelled pairs BEFORE calculating any
    # primordium statistics or train/test balance criteria.
    #
    # Missing or non-numeric exception labels are treated as
    # non-exceptions, matching the threshold-analysis scripts.
    # --------------------------------------------------------
    total_input_rows = len(df)
    exception_values = pd.to_numeric(
        df[EXCEPTION_COL],
        errors="coerce",
    )
    exception_mask = exception_values.eq(EXCEPTION_POSITIVE_VALUE)
    excluded_exception_rows = int(exception_mask.sum())

    df = df.loc[~exception_mask].copy()

    if df.empty:
        raise ValueError(
            "No rows remain after excluding exception-labelled pairs."
        )

    print(f"Input rows: {total_input_rows}")
    print(f"Excluded exception rows: {excluded_exception_rows}")
    print(f"Rows used for splitting: {len(df)}")
    print()

    # Temporary grouping column.
    df["_primordium"] = df[FILENAME_COL].map(get_primordium_prefix)

    primordia = sorted(df["_primordium"].unique())
    n_primordia = len(primordia)

    print(f"Found {n_primordia} primordia:")
    print(", ".join(primordia))
    print()

    # --------------------------------------------------------
    # Per-primordium statistics
    # --------------------------------------------------------
    stats = {}

    for p in primordia:
        g = df[df["_primordium"] == p]

        n_rows = len(g)
        n_daughter = int((g[LABEL_COL] == 1).sum())

        stats[p] = {
            "rows": n_rows,
            "daughter": n_daughter,
            "non_daughter": n_rows - n_daughter,
        }

    total_rows = len(df)
    total_daughter = int((df[LABEL_COL] == 1).sum())

    valid_splits = []

    # --------------------------------------------------------
    # Test ALL possible train-set sizes.
    #
    # For 12 primordia this means train sizes 1 through 11.
    # In practice, only combinations whose ROW count is near
    # 70% will survive the fraction criterion.
    # --------------------------------------------------------
    for n_train in range(1, n_primordia):

        for train_tuple in combinations(primordia, n_train):

            train_set = set(train_tuple)
            test_set = set(primordia) - train_set

            # -------------------------
            # Row counts and fractions
            # -------------------------
            train_rows = sum(
                stats[p]["rows"] for p in train_set
            )
            test_rows = total_rows - train_rows

            train_fraction = train_rows / total_rows
            test_fraction = test_rows / total_rows

            train_fraction_error = abs(
                train_fraction - TARGET_TRAIN_FRACTION
            )

            # Requirement 1
            if train_fraction_error > FRACTION_TOLERANCE:
                continue

            # -------------------------
            # Daughter-pair statistics
            # -------------------------
            train_daughter = sum(
                stats[p]["daughter"] for p in train_set
            )
            test_daughter = total_daughter - train_daughter

            train_non_daughter = train_rows - train_daughter
            test_non_daughter = test_rows - test_daughter

            train_daughter_freq = train_daughter / train_rows
            test_daughter_freq = test_daughter / test_rows

            daughter_freq_difference = abs(
                train_daughter_freq - test_daughter_freq
            )

            # Requirement 2
            if daughter_freq_difference >= MAX_DAUGHTER_FREQ_DIFFERENCE:
                continue

            valid_splits.append({
                "test_set": ";".join(sorted(test_set)),
                "train_set": ";".join(sorted(train_set)),

                "n_test_primordia": len(test_set),
                "n_train_primordia": len(train_set),

                "test_rows": test_rows,
                "train_rows": train_rows,
                "total_rows": total_rows,

                "total_input_rows": total_input_rows,
                "excluded_exception_rows":
                    excluded_exception_rows,
                "exception_column": EXCEPTION_COL,
                "exception_positive_value":
                    EXCEPTION_POSITIVE_VALUE,

                "test_fraction": test_fraction,
                "train_fraction": train_fraction,

                "test_fraction_percent": test_fraction * 100,
                "train_fraction_percent": train_fraction * 100,

                "train_fraction_error_pp":
                    train_fraction_error * 100,

                "test_daughter_pairs": test_daughter,
                "train_daughter_pairs": train_daughter,
                "total_daughter_pairs": total_daughter,

                "test_non_daughter_pairs": test_non_daughter,
                "train_non_daughter_pairs": train_non_daughter,

                "test_daughter_pair_frequency":
                    test_daughter_freq,

                "train_daughter_pair_frequency":
                    train_daughter_freq,

                "test_daughter_pair_frequency_percent":
                    test_daughter_freq * 100,

                "train_daughter_pair_frequency_percent":
                    train_daughter_freq * 100,

                "daughter_pair_frequency_difference":
                    daughter_freq_difference,

                "daughter_pair_frequency_difference_pp":
                    daughter_freq_difference * 100,

                "target_train_fraction":
                    TARGET_TRAIN_FRACTION,

                "fraction_tolerance":
                    FRACTION_TOLERANCE,

                "max_daughter_pair_frequency_difference":
                    MAX_DAUGHTER_FREQ_DIFFERENCE,

                "satisfies_conditions": True,
            })

    # --------------------------------------------------------
    # Sort:
    # 1. closest to exactly 70% training rows
    # 2. smallest daughter-frequency difference
    # 3. deterministic test-set ordering
    # --------------------------------------------------------
    valid_splits.sort(
        key=lambda x: (
            x["train_fraction_error_pp"],
            x["daughter_pair_frequency_difference_pp"],
            x["test_set"],
        )
    )

    # Assign split IDs after sorting.
    for i, split in enumerate(valid_splits, start=1):
        split["split_id"] = f"split_{i:03d}"

    columns = [
        "split_id",
        "test_set",
        "train_set",

        "n_test_primordia",
        "n_train_primordia",

        "test_rows",
        "train_rows",
        "total_rows",

        "total_input_rows",
        "excluded_exception_rows",
        "exception_column",
        "exception_positive_value",

        "test_fraction",
        "train_fraction",

        "test_fraction_percent",
        "train_fraction_percent",
        "train_fraction_error_pp",

        "test_daughter_pairs",
        "train_daughter_pairs",
        "total_daughter_pairs",

        "test_non_daughter_pairs",
        "train_non_daughter_pairs",

        "test_daughter_pair_frequency",
        "train_daughter_pair_frequency",

        "test_daughter_pair_frequency_percent",
        "train_daughter_pair_frequency_percent",

        "daughter_pair_frequency_difference",
        "daughter_pair_frequency_difference_pp",

        "target_train_fraction",
        "fraction_tolerance",
        "max_daughter_pair_frequency_difference",

        "satisfies_conditions",
    ]

    result = pd.DataFrame(valid_splits, columns=columns)
    result.to_csv(OUTPUT_CSV, index=False)

    print(f"Satisfying splits found: {len(result)}")
    print()

    if len(result):
        print("Counts by train/test primordium number:")
        print(
            result.groupby(
                ["n_train_primordia", "n_test_primordia"]
            ).size()
        )
        print()

        print("Best-balanced split:")
        best = result.iloc[0]

        print("  split_id:", best["split_id"])
        print("  train_set:", best["train_set"])
        print("  test_set:", best["test_set"])
        print(
            f"  train/test rows: "
            f"{best['train_fraction_percent']:.3f}% / "
            f"{best['test_fraction_percent']:.3f}%"
        )
        print(
            f"  daughter-frequency difference: "
            f"{best['daughter_pair_frequency_difference_pp']:.3f} "
            f"percentage points"
        )

    print()
    print(f"Saved: {OUTPUT_CSV}")


if __name__ == "__main__":
    main()
